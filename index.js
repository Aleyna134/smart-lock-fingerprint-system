const express = require('express');
const { PrismaClient } = require('@prisma/client');
const bcrypt = require('bcryptjs');
const crypto = require('crypto');
const fs = require('fs');
const http2 = require('http2');
const path = require('path');

const app = express();
const prisma = new PrismaClient();

app.use(express.json());

const DELIVERY_STATUS = {
    PENDING: 'PENDING',
    SENT: 'SENT',
    FAILED: 'FAILED',
    CANCELLED: 'CANCELLED'
};

const ENROLLMENT_STATUS = {
    PENDING: 'PENDING_ENROLLMENT',
    ENROLLED: 'ENROLLED',
    FAILED: 'ENROLL_FAILED',
    PENDING_DELETE: 'PENDING_DELETE',
    DELETE_FAILED: 'DELETE_FAILED'
};

const HARDWARE_COMMAND = {
    ENROLL_FINGERPRINT: 'ENROLL_FINGERPRINT',
    DELETE_FINGERPRINT: 'DELETE_FINGERPRINT'
};

const HARDWARE_COMMAND_STATUS = {
    PENDING: 'PENDING',
    CLAIMED: 'CLAIMED',
    DONE: 'DONE',
    FAILED: 'FAILED'
};

const LOCK_STATUS = {
    LOCKED: 'Locked',
    UNLOCKED: 'Unlocked'
};

const DEFAULT_LOCK_DEVICE_ID = process.env.LOCK_DEVICE_ID || 'lock-1';
const HARDWARE_COMMAND_CLAIM_TIMEOUT_MS = numberFromEnv('HARDWARE_COMMAND_CLAIM_TIMEOUT_MS', 120000);

const APNS_INVALID_REASONS = new Set([
    'BadDeviceToken',
    'DeviceTokenNotForTopic',
    'Unregistered',
    'TopicDisallowed'
]);

const APNS_RETRYABLE_STATUS_CODES = new Set([0, 429, 500, 503]);

function numberFromEnv(name, fallback, min = 1) {
    const raw = Number(process.env[name]);
    if (!Number.isFinite(raw) || raw < min) return fallback;
    return raw;
}

const PUSH_WORKER_INTERVAL_MS = numberFromEnv('PUSH_WORKER_INTERVAL_MS', 5000);
const PUSH_BATCH_SIZE = numberFromEnv('PUSH_BATCH_SIZE', 25);
const PUSH_MAX_ATTEMPTS = numberFromEnv('PUSH_MAX_ATTEMPTS', 6);
const PUSH_RETRY_BASE_MS = numberFromEnv('PUSH_RETRY_BASE_MS', 15000);
const PUSH_RETRY_MAX_MS = numberFromEnv('PUSH_RETRY_MAX_MS', 15 * 60 * 1000);
const APNS_EXPIRATION_SECONDS = numberFromEnv('APNS_EXPIRATION_SECONDS', 7 * 24 * 60 * 60);
const ALERT_PUSH_COOLDOWN_SECONDS = numberFromEnv('ALERT_PUSH_COOLDOWN_SECONDS', 30);

let queueWorkerHandle = null;
let queueProcessing = false;

function toBase64URL(value) {
    const input = Buffer.isBuffer(value) ? value : Buffer.from(String(value), 'utf8');
    return input
        .toString('base64')
        .replace(/\+/g, '-')
        .replace(/\//g, '_')
        .replace(/=+$/g, '');
}

function loadApnsPrivateKey() {
    if (process.env.APNS_PRIVATE_KEY && process.env.APNS_PRIVATE_KEY.trim().length > 0) {
        return process.env.APNS_PRIVATE_KEY.replace(/\\n/g, '\n');
    }

    if (process.env.APNS_PRIVATE_KEY_PATH && process.env.APNS_PRIVATE_KEY_PATH.trim().length > 0) {
        try {
            return fs.readFileSync(process.env.APNS_PRIVATE_KEY_PATH, 'utf8');
        } catch (error) {
            console.warn(`⚠️ APNS private key okunamadı: ${process.env.APNS_PRIVATE_KEY_PATH}`);
            return null;
        }
    }

    return null;
}

function hasApnsConfig() {
    return Boolean(
        process.env.APNS_TEAM_ID &&
        process.env.APNS_KEY_ID &&
        process.env.APNS_BUNDLE_ID &&
        loadApnsPrivateKey()
    );
}

function createApnsJwt() {
    const teamId = process.env.APNS_TEAM_ID;
    const keyId = process.env.APNS_KEY_ID;
    const privateKey = loadApnsPrivateKey();

    if (!teamId || !keyId || !privateKey) {
        throw new Error('APNS config eksik. APNS_TEAM_ID / APNS_KEY_ID / APNS_PRIVATE_KEY gerekir.');
    }

    const header = toBase64URL(JSON.stringify({ alg: 'ES256', kid: keyId }));
    const payload = toBase64URL(JSON.stringify({
        iss: teamId,
        iat: Math.floor(Date.now() / 1000)
    }));

    const unsignedToken = `${header}.${payload}`;
    const signature = crypto.sign('sha256', Buffer.from(unsignedToken), privateKey);

    return `${unsignedToken}.${toBase64URL(signature)}`;
}

function getApnsHost() {
    return process.env.APNS_PRODUCTION === 'true'
        ? 'api.push.apple.com'
        : 'api.sandbox.push.apple.com';
}

function retryDelayMs(attemptNumber) {
    const base = PUSH_RETRY_BASE_MS * Math.pow(2, Math.max(0, attemptNumber - 1));
    return Math.min(base, PUSH_RETRY_MAX_MS);
}

function isRetryableApnsResult(result) {
    if (result.skipped) return true;
    return APNS_RETRYABLE_STATUS_CODES.has(result.status);
}

function isInvalidTokenResult(result) {
    return result.status === 410 || APNS_INVALID_REASONS.has(result.reason);
}

function buildAlertPayloadFromLog(log) {
    if (log.success) return null;

    const isCritical = Number(log.fail_count || 0) >= 3;
    const type = isCritical ? 'MULTIPLE_FAILED_ATTEMPTS' : 'UNAUTHORIZED_ACCESS';
    const severity = isCritical ? 'CRITICAL' : 'WARNING';
    const title = isCritical ? 'Multiple failed access attempts' : 'Unauthorized access attempt';
    const detail = isCritical
        ? `${log.fail_count} failed access attempts were detected in a short period.`
        : `An unknown fingerprint attempt was detected. Status: ${log.status}`;

    return { type, severity, title, detail };
}

function normalizeAccessStatus(status) {
    const rawStatus = String(status || '').trim();
    const normalized = rawStatus
        .toLowerCase()
        .normalize('NFD')
        .replace(/[\u0300-\u036f]/g, '')
        .replace(/[\u0131\u0130]/g, 'i')
        .replace(/[\u015F\u015E]/g, 's')
        .replace(/[\u011F\u011E]/g, 'g')
        .replace(/[\u00FC\u00DC]/g, 'u')
        .replace(/[\u00F6\u00D6]/g, 'o')
        .replace(/[\u00E7\u00C7]/g, 'c');

    const labels = {
        'hatali parmak izi': 'Failed fingerprint',
        'parmak izi dogrulanamadi': 'Fingerprint could not be verified',
        'yetkisiz erisim': 'Unauthorized access',
        'yetkisiz erisim girisimi': 'Unauthorized access attempt',
        'yetkisiz giris': 'Unauthorized access',
        'yetkisiz giris girisimi': 'Unauthorized access attempt',
        'parmak izi dogrulanadi': 'Fingerprint verified',
        'coklu hatali giris denemesi': 'Multiple failed access attempts',
        'basarili giris': 'Successful access',
        'basarili erisim': 'Successful access',
        'kilit acildi': 'Unlocked',
        'kilitli': 'Locked'
    };

    return labels[normalized] || rawStatus;
}

function normalizeAlertForResponse(alert) {
    if (!alert) return alert;

    const title = normalizeAccessStatus(alert.title);
    const detail = String(alert.detail || '')
        .replace(/Tanimsiz parmak izi denemesi algilandi\. Durum: /g, 'An unknown fingerprint attempt was detected. Status: ')
        .replace(/Kisa surede (\d+) basarisiz giris denemesi algilandi\./g, '$1 failed access attempts were detected in a short period.')
        .replace(/Coklu hatali giris denemesi/g, 'Multiple failed access attempts')
        .replace(/Yetkisiz erisim girisimi/g, 'Unauthorized access attempt')
        .replace(/Yetkisiz erisim/g, 'Unauthorized access')
        .replace(/Parmak izi dogrulanamadi/g, 'Fingerprint could not be verified')
        .replace(/Parmak izi dogrulanadi/g, 'Fingerprint verified')
        .replace(/Hatali parmak izi/g, 'Failed fingerprint');

    return { ...alert, title, detail };
}

function normalizeLogForResponse(log) {
    if (!log) return log;
    return {
        ...log,
        status: normalizeAccessStatus(log.status),
        alert: normalizeAlertForResponse(log.alert)
    };
}

function buildApnsPayload(alert, unreadCount) {
    return {
        aps: {
            alert: {
                title: alert.title,
                body: alert.detail
            },
            badge: unreadCount,
            sound: 'alert.wav'
        },
        alert_id: alert.id,
        alert_type: alert.type,
        severity: alert.severity
    };
}

async function sendApnsNotification(token, alert, unreadCount) {
    if (!hasApnsConfig()) {
        return { ok: false, skipped: true, status: 0, reason: 'APNS_NOT_CONFIGURED' };
    }

    const jwt = createApnsJwt();
    const host = getApnsHost();
    const topic = process.env.APNS_BUNDLE_ID;
    const payload = buildApnsPayload(alert, unreadCount);
    const expiration = String(Math.floor(Date.now() / 1000) + APNS_EXPIRATION_SECONDS);

    return new Promise((resolve) => {
        let statusCode = 0;
        let responseBody = '';
        const client = http2.connect(`https://${host}`);

        client.on('error', (error) => {
            resolve({ ok: false, status: 0, reason: error.message });
        });

        const request = client.request({
            ':method': 'POST',
            ':path': `/3/device/${token}`,
            authorization: `bearer ${jwt}`,
            'apns-topic': topic,
            'apns-push-type': 'alert',
            'apns-priority': '10',
            'apns-expiration': expiration
        });

        request.setEncoding('utf8');
        request.on('response', (headers) => {
            statusCode = Number(headers[':status'] || 0);
        });
        request.on('data', (chunk) => {
            responseBody += chunk;
        });
        request.on('end', () => {
            client.close();

            if (statusCode >= 200 && statusCode < 300) {
                resolve({ ok: true, status: statusCode, reason: null });
                return;
            }

            try {
                const parsed = JSON.parse(responseBody || '{}');
                resolve({ ok: false, status: statusCode, reason: parsed.reason || 'APNS_ERROR' });
            } catch {
                resolve({ ok: false, status: statusCode, reason: responseBody || 'APNS_ERROR' });
            }
        });
        request.on('error', (error) => {
            client.close();
            resolve({ ok: false, status: 0, reason: error.message });
        });
        request.end(JSON.stringify(payload));
    });
}

async function createAlertForLog(log) {
    const payload = buildAlertPayloadFromLog(log);
    if (!payload) return null;

    return prisma.alert.create({
        data: {
            type: payload.type,
            severity: payload.severity,
            title: payload.title,
            detail: payload.detail,
            log_id: log.id
        }
    });
}

async function enqueueDeliveriesForAlert(alertId) {
    const activeTokens = await prisma.deviceToken.findMany({
        where: { is_active: true, platform: 'ios' },
        select: { id: true }
    });

    if (activeTokens.length === 0) return 0;

    await prisma.$transaction(
        activeTokens.map((token) =>
            prisma.pushDelivery.upsert({
                where: {
                    alert_id_device_token_id: {
                        alert_id: alertId,
                        device_token_id: token.id
                    }
                },
                update: {
                    status: DELIVERY_STATUS.PENDING,
                    next_retry_at: new Date(),
                    last_error: null
                },
                create: {
                    alert_id: alertId,
                    device_token_id: token.id
                }
            })
        )
    );

    return activeTokens.length;
}

async function isPushSuppressedByCooldown(alert) {
    const threshold = new Date(Date.now() - ALERT_PUSH_COOLDOWN_SECONDS * 1000);

    const similarRecentAlert = await prisma.alert.findFirst({
        where: {
            id: { not: alert.id },
            type: alert.type,
            created_at: { gte: threshold }
        },
        orderBy: { created_at: 'desc' },
        select: { id: true }
    });

    return Boolean(similarRecentAlert);
}

async function markDeliveryFailed(delivery, reason, retryable) {
    const nextAttempts = delivery.attempts + 1;
    const exhausted = nextAttempts >= PUSH_MAX_ATTEMPTS || !retryable;
    const attemptsToStore = exhausted ? PUSH_MAX_ATTEMPTS : nextAttempts;
    const nextRetryAt = new Date(Date.now() + retryDelayMs(nextAttempts));

    await prisma.pushDelivery.update({
        where: { id: delivery.id },
        data: {
            status: DELIVERY_STATUS.FAILED,
            attempts: attemptsToStore,
            last_error: String(reason || 'APNS_ERROR'),
            next_retry_at: nextRetryAt
        }
    });
}

async function processDelivery(delivery, unreadCount) {
    const token = delivery.deviceToken;
    const alert = delivery.alert;

    if (!token || !alert) {
        await prisma.pushDelivery.update({
            where: { id: delivery.id },
            data: {
                status: DELIVERY_STATUS.CANCELLED,
                attempts: delivery.attempts + 1,
                last_error: 'MISSING_ALERT_OR_TOKEN',
                next_retry_at: new Date()
            }
        });
        return;
    }

    if (!token.is_active) {
        await prisma.pushDelivery.update({
            where: { id: delivery.id },
            data: {
                status: DELIVERY_STATUS.CANCELLED,
                attempts: delivery.attempts + 1,
                last_error: 'TOKEN_INACTIVE',
                next_retry_at: new Date()
            }
        });
        return;
    }

    try {
        const result = await sendApnsNotification(token.token, alert, unreadCount);

        if (result.ok) {
            await prisma.pushDelivery.update({
                where: { id: delivery.id },
                data: {
                    status: DELIVERY_STATUS.SENT,
                    attempts: delivery.attempts + 1,
                    last_error: null,
                    sent_at: new Date(),
                    next_retry_at: new Date()
                }
            });
            return;
        }

        if (isInvalidTokenResult(result)) {
            await prisma.$transaction([
                prisma.deviceToken.update({
                    where: { id: token.id },
                    data: { is_active: false }
                }),
                prisma.pushDelivery.update({
                    where: { id: delivery.id },
                    data: {
                        status: DELIVERY_STATUS.CANCELLED,
                        attempts: delivery.attempts + 1,
                        last_error: String(result.reason || 'INVALID_TOKEN'),
                        next_retry_at: new Date()
                    }
                })
            ]);
            return;
        }

        await markDeliveryFailed(delivery, result.reason || 'APNS_ERROR', isRetryableApnsResult(result));
    } catch (error) {
        await markDeliveryFailed(delivery, error.message || 'APNS_THROW', true);
    }
}

async function processPushQueueOnce() {
    if (queueProcessing) return;
    if (!hasApnsConfig()) return;

    queueProcessing = true;
    try {
        const dueDeliveries = await prisma.pushDelivery.findMany({
            where: {
                status: { in: [DELIVERY_STATUS.PENDING, DELIVERY_STATUS.FAILED] },
                attempts: { lt: PUSH_MAX_ATTEMPTS },
                next_retry_at: { lte: new Date() }
            },
            include: {
                alert: true,
                deviceToken: true
            },
            orderBy: { next_retry_at: 'asc' },
            take: PUSH_BATCH_SIZE
        });

        if (dueDeliveries.length === 0) return;

        const unreadCount = await prisma.alert.count({ where: { status: 'UNREAD' } });

        for (const delivery of dueDeliveries) {
            await processDelivery(delivery, unreadCount);
        }
    } catch (error) {
        console.error('❌ Push queue işlenemedi:', error);
    } finally {
        queueProcessing = false;
    }
}

function startPushQueueWorker() {
    if (queueWorkerHandle) return;

    queueWorkerHandle = setInterval(() => {
        void processPushQueueOnce();
    }, PUSH_WORKER_INTERVAL_MS);

    if (typeof queueWorkerHandle.unref === 'function') {
        queueWorkerHandle.unref();
    }

    void processPushQueueOnce();
}

function stopPushQueueWorker() {
    if (!queueWorkerHandle) return;
    clearInterval(queueWorkerHandle);
    queueWorkerHandle = null;
}

// 1. LOG KAYDETME (ESP32'den gelen veriler için)
app.post('/api/access-log', async (req, res) => {
    try {
        const { success, status, fail_count, user_id } = req.body;

        const newLog = await prisma.log.create({
            data: {
                success: Boolean(success),
                status: normalizeAccessStatus(status),
                fail_count: Number(fail_count || 0),
                user_id: user_id ? Number(user_id) : null
            }
        });

        let queued = 0;
        const alert = await createAlertForLog(newLog);
        if (alert) {
            const pushSuppressed = await isPushSuppressedByCooldown(alert);
            if (!pushSuppressed) {
                queued = await enqueueDeliveriesForAlert(alert.id);
                void processPushQueueOnce();
            } else {
                console.log(`🔕 Push cooldown nedeniyle bastırıldı alert_id=${alert.id}`);
            }
        }

        console.log('Yeni log kaydedildi:', newLog);
        if (alert) {
            console.log(`Push kuyruğuna eklendi: ${queued} cihaz`);
        }

        res.status(201).json(newLog);
    } catch (error) {
        console.error('Log kaydetme hatasi:', error);
        res.status(500).json({ error: 'Log could not be saved.' });
    }
});

// 2. CİHAZ TOKEN KAYDI (iOS APNs)
app.post('/api/device-token', async (req, res) => {
    try {
        const { user_id, token } = req.body;
        const normalizedToken = String(token || '').trim().toLowerCase();

        if (!normalizedToken) {
            return res.status(400).json({ error: 'Token is required.' });
        }

        if (!/^[a-f0-9]{40,256}$/.test(normalizedToken)) {
            return res.status(400).json({ error: 'Token format is invalid.' });
        }

        let resolvedUserId = null;
        if (user_id !== undefined && user_id !== null) {
            resolvedUserId = Number(user_id);
            if (Number.isNaN(resolvedUserId)) {
                return res.status(400).json({ error: 'user_id must be numeric.' });
            }

            const user = await prisma.user.findUnique({ where: { id: resolvedUserId } });
            if (!user) {
                console.warn(`⚠️ user_id bulunamadı (${resolvedUserId}), token anonim kaydedilecek.`);
                resolvedUserId = null;
            }
        }

        const existing = await prisma.deviceToken.findUnique({
            where: { token: normalizedToken },
            select: { id: true }
        });

        const savedToken = await prisma.deviceToken.upsert({
            where: { token: normalizedToken },
            update: {
                user_id: resolvedUserId,
                is_active: true,
                platform: 'ios'
            },
            create: {
                token: normalizedToken,
                user_id: resolvedUserId,
                platform: 'ios'
            }
        });

        // Token register çağrısı her login/launch'ta tekrar gelir.
        // Geçmiş unread alertleri burada geri doldurmak spam ürettiği için kapatıldı.
        const queuedUnread = 0;

        console.log(
            `📲 Device token kaydedildi id=${savedToken.id} user_id=${resolvedUserId ?? 'null'} queued_unread=${queuedUnread}`
        );

        return res.status(existing ? 200 : 201).json({
            message: existing ? 'Token updated.' : 'Token registered.',
            token_id: savedToken.id,
            queued_unread_alerts: queuedUnread
        });
    } catch (error) {
        console.error('Device token kaydetme hatasi:', error);
        return res.status(500).json({ error: 'Device token could not be registered.' });
    }
});

// 3. YENİ KULLANICI EKLEME (Sadece ADMIN Yetkisiyle)
app.post('/api/users', async (req, res) => {
    try {
        const { name, email, password, role, admin_id } = req.body;

        if (!admin_id) {
            return res.status(401).json({ error: 'Acting administrator ID is required.' });
        }

        const requester = await prisma.user.findUnique({ where: { id: Number(admin_id) } });
        if (!requester || requester.role !== 'ADMIN') {
            return res.status(403).json({ error: 'Unauthorized action. Only administrators can add users.' });
        }

        const hashedPassword = await bcrypt.hash(password, 10);

        const { newUser, command } = await prisma.$transaction(async (tx) => {
            const createdUser = await tx.user.create({
                data: {
                    name: String(name),
                    email: String(email),
                    password: hashedPassword,
                    role: role ? String(role).toUpperCase() : 'USER',
                    enrollment_status: ENROLLMENT_STATUS.PENDING
                }
            });

            const createdCommand = await tx.hardwareCommand.create({
                data: {
                    type: HARDWARE_COMMAND.ENROLL_FINGERPRINT,
                    device_id: DEFAULT_LOCK_DEVICE_ID,
                    user_id: createdUser.id
                }
            });

            return { newUser: createdUser, command: createdCommand };
        });

        res.status(201).json({
            message: 'User created. Waiting for fingerprint enrollment.',
            enrollment_command_id: command.id,
            user: {
                id: newUser.id,
                name: newUser.name,
                email: newUser.email,
                role: newUser.role,
                enrolled_at: newUser.enrolled_at,
                enrollment_status: newUser.enrollment_status
            }
        });
    } catch (error) {
        console.error(error);
        res.status(500).json({ error: 'User could not be created. The email address may already be in use.' });
    }
});

// 4. GİRİŞ YAPMA (Login Paneli İçin)
app.post('/api/login', async (req, res) => {
    try {
        const { email, password } = req.body;
        const user = await prisma.user.findUnique({ where: { email: String(email) } });

        if (user && await bcrypt.compare(String(password), user.password)) {
            res.json({ message: 'Login successful.', user_id: user.id, name: user.name, role: user.role });
        } else {
            res.status(401).json({ error: 'Invalid email or password.' });
        }
    } catch (error) {
        res.status(500).json({ error: 'An error occurred during login.' });
    }
});

// 5. KULLANICILARI LİSTELEME
app.get('/api/users', async (req, res) => {
    try {
        const users = await prisma.user.findMany({
            select: { id: true, name: true, email: true, role: true, enrolled_at: true, enrollment_status: true }
        });
        res.json(users);
    } catch (error) {
        res.status(500).json({ error: 'Users could not be loaded.' });
    }
});

// 6. KULLANICI SİLME (ID ile)
app.delete('/api/users/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const userId = Number(id);
        if (Number.isNaN(userId)) {
            return res.status(400).json({ error: 'Invalid user ID.' });
        }

        console.log(`[HW_COMMAND] Delete requested. user_id=${userId}`);

        const user = await prisma.user.findUnique({ where: { id: userId } });
        if (!user) {
            console.warn(`[HW_COMMAND] Delete failed — user not found. user_id=${userId}`);
            return res.status(404).json({ error: 'User not found.' });
        }

        const isRetry = user.enrollment_status === ENROLLMENT_STATUS.DELETE_FAILED;
        if (isRetry) {
            console.log(`[HW_COMMAND] Delete retry detected. user_id=${userId} previous_status=${user.enrollment_status}`);
        }

        const { command } = await prisma.$transaction(async (tx) => {
            await tx.user.update({
                where: { id: userId },
                data: { enrollment_status: ENROLLMENT_STATUS.PENDING_DELETE }
            });

            const existingCommand = await tx.hardwareCommand.findFirst({
                where: {
                    user_id: userId,
                    type: HARDWARE_COMMAND.DELETE_FINGERPRINT,
                    status: { in: [HARDWARE_COMMAND_STATUS.PENDING, HARDWARE_COMMAND_STATUS.CLAIMED] }
                },
                orderBy: { created_at: 'asc' }
            });

            if (existingCommand) {
                console.log(`[HW_COMMAND] Delete — reusing existing command. user_id=${userId} command_id=${existingCommand.id}`);
                return { command: existingCommand };
            }

            const createdCommand = await tx.hardwareCommand.create({
                data: {
                    type: HARDWARE_COMMAND.DELETE_FINGERPRINT,
                    device_id: DEFAULT_LOCK_DEVICE_ID,
                    user_id: userId
                }
            });

            return { command: createdCommand };
        });

        console.log(`[HW_COMMAND] Delete queued. user_id=${userId} command_id=${command.id}`);
        res.json({
            message: 'User deletion queued. Waiting for ESP32 fingerprint removal.',
            deletion_command_id: command.id,
            user: {
                id: userId,
                enrollment_status: ENROLLMENT_STATUS.PENDING_DELETE
            }
        });
    } catch (error) {
        console.error('User delete queue error:', error);
        res.status(500).json({ error: 'User could not be deleted.' });
    }
});

app.post('/api/users/:id/enrollment/retry', async (req, res) => {
    try {
        const userId = Number(req.params.id);
        if (Number.isNaN(userId)) {
            return res.status(400).json({ error: 'Invalid user ID.' });
        }

        console.log(`[HW_COMMAND] Enrollment retry requested. user_id=${userId}`);

        const user = await prisma.user.findUnique({ where: { id: userId } });
        if (!user) {
            console.warn(`[HW_COMMAND] Enrollment retry failed — user not found. user_id=${userId}`);
            return res.status(404).json({ error: 'User not found.' });
        }
        if (user.enrollment_status !== ENROLLMENT_STATUS.FAILED) {
            console.warn(`[HW_COMMAND] Enrollment retry rejected — wrong status. user_id=${userId} status=${user.enrollment_status}`);
            return res.status(409).json({
                error: `Enrollment retry is only allowed when status is ENROLL_FAILED. Current status: ${user.enrollment_status}`
            });
        }

        const { updatedUser, command } = await prisma.$transaction(async (tx) => {
            const updated = await tx.user.update({
                where: { id: userId },
                data: { enrollment_status: ENROLLMENT_STATUS.PENDING }
            });

            const existingCommand = await tx.hardwareCommand.findFirst({
                where: {
                    user_id: userId,
                    type: HARDWARE_COMMAND.ENROLL_FINGERPRINT,
                    status: { in: [HARDWARE_COMMAND_STATUS.PENDING, HARDWARE_COMMAND_STATUS.CLAIMED] }
                }
            });

            if (existingCommand) {
                console.log(`[HW_COMMAND] Enrollment retry — reusing existing command. user_id=${userId} command_id=${existingCommand.id}`);
                return { updatedUser: updated, command: existingCommand };
            }

            const createdCommand = await tx.hardwareCommand.create({
                data: {
                    type: HARDWARE_COMMAND.ENROLL_FINGERPRINT,
                    device_id: DEFAULT_LOCK_DEVICE_ID,
                    user_id: userId
                }
            });

            return { updatedUser: updated, command: createdCommand };
        });

        console.log(`[HW_COMMAND] Enrollment retry queued. user_id=${userId} command_id=${command.id}`);
        return res.json({
            message: 'Enrollment retry queued. Waiting for fingerprint enrollment.',
            enrollment_command_id: command.id,
            user: {
                id: updatedUser.id,
                name: updatedUser.name,
                email: updatedUser.email,
                role: updatedUser.role,
                enrollment_status: updatedUser.enrollment_status
            }
        });
    } catch (error) {
        console.error('Enrollment retry error:', error);
        return res.status(500).json({ error: 'Enrollment retry could not be queued.' });
    }
});

app.get('/api/hardware/commands/next', async (req, res) => {
    try {
        const deviceId = String(req.query.device_id || DEFAULT_LOCK_DEVICE_ID);
        const staleClaimedBefore = new Date(Date.now() - HARDWARE_COMMAND_CLAIM_TIMEOUT_MS);

        const command = await prisma.hardwareCommand.findFirst({
            where: {
                device_id: deviceId,
                type: { in: [HARDWARE_COMMAND.ENROLL_FINGERPRINT, HARDWARE_COMMAND.DELETE_FINGERPRINT] },
                OR: [
                    { status: HARDWARE_COMMAND_STATUS.PENDING },
                    { status: HARDWARE_COMMAND_STATUS.CLAIMED, claimed_at: { lt: staleClaimedBefore } }
                ]
            },
            orderBy: { created_at: 'asc' }
        });

        if (!command) {
            return res.json({ command: null });
        }

        if (!command.user_id) {
            await prisma.hardwareCommand.update({
                where: { id: command.id },
                data: {
                    status: HARDWARE_COMMAND_STATUS.FAILED,
                    error: 'Command has no user ID.',
                    completed_at: new Date()
                }
            });
            console.warn(`[HW_COMMAND] Skipped command without user_id. command_id=${command.id}`);
            return res.json({ command: null });
        }

        const claimed = await prisma.hardwareCommand.update({
            where: { id: command.id },
            data: {
                status: HARDWARE_COMMAND_STATUS.CLAIMED,
                attempts: { increment: 1 },
                claimed_at: new Date(),
                error: null
            }
        });

        console.log(`[HW_COMMAND] Claimed. command_id=${claimed.id} type=${claimed.type} user_id=${claimed.user_id} attempts=${claimed.attempts}`);
        return res.json({
            command: {
                id: claimed.id,
                type: claimed.type,
                user_id: claimed.user_id,
                template_id: claimed.user_id,
                device_id: claimed.device_id,
                status: claimed.status,
                attempts: claimed.attempts
            }
        });
    } catch (error) {
        console.error('Hardware command fetch error:', error);
        return res.status(500).json({ error: 'Hardware command could not be loaded.' });
    }
});

app.post('/api/hardware/commands/:id/result', async (req, res) => {
    try {
        const commandId = Number(req.params.id);
        const { success, template_id, message } = req.body;

        if (Number.isNaN(commandId)) {
            return res.status(400).json({ error: 'Invalid command ID.' });
        }

        const command = await prisma.hardwareCommand.findUnique({ where: { id: commandId } });
        if (!command) {
            return res.status(404).json({ error: 'Hardware command not found.' });
        }
        if (!command.user_id) {
            return res.status(409).json({ error: 'Hardware command is no longer attached to a user.' });
        }

        const templateId = template_id !== undefined && template_id !== null ? Number(template_id) : command.user_id;
        const matchedTemplate = templateId === command.user_id;
        const succeeded = Boolean(success) && matchedTemplate;
        const errorMessage = succeeded
            ? null
            : (matchedTemplate ? String(message || 'Hardware command failed.') : 'Template ID does not match user ID.');

        console.log(`[HW_COMMAND] Result received. command_id=${command.id} type=${command.type} success=${Boolean(success)} template_id=${templateId}`);

        if (command.type === HARDWARE_COMMAND.DELETE_FINGERPRINT) {
            if (succeeded) {
                const [updatedCommand] = await prisma.$transaction([
                    prisma.hardwareCommand.update({
                        where: { id: command.id },
                        data: {
                            status: HARDWARE_COMMAND_STATUS.DONE,
                            error: null,
                            completed_at: new Date()
                        }
                    }),
                    prisma.user.delete({ where: { id: command.user_id } })
                ]);

                console.log(`[HW_COMMAND] Fingerprint delete completed. command_id=${updatedCommand.id} user_id=${command.user_id}`);
                return res.json({
                    command: {
                        id: updatedCommand.id,
                        type: updatedCommand.type,
                        status: updatedCommand.status,
                        error: updatedCommand.error
                    },
                    user: {
                        id: command.user_id,
                        deleted: true
                    }
                });
            }

            const [updatedCommand, updatedUser] = await prisma.$transaction([
                prisma.hardwareCommand.update({
                    where: { id: command.id },
                    data: {
                        status: HARDWARE_COMMAND_STATUS.FAILED,
                        error: errorMessage,
                        completed_at: new Date()
                    }
                }),
                prisma.user.update({
                    where: { id: command.user_id },
                    data: { enrollment_status: ENROLLMENT_STATUS.DELETE_FAILED }
                })
            ]);

            console.warn(`[HW_COMMAND] Fingerprint delete failed. command_id=${updatedCommand.id} user_id=${updatedUser.id} error=${errorMessage}`);
            return res.json({
                command: {
                    id: updatedCommand.id,
                    type: updatedCommand.type,
                    status: updatedCommand.status,
                    error: updatedCommand.error
                },
                user: {
                    id: updatedUser.id,
                    enrollment_status: updatedUser.enrollment_status
                }
            });
        }

        if (command.type !== HARDWARE_COMMAND.ENROLL_FINGERPRINT) {
            return res.status(400).json({ error: 'Unsupported hardware command type.' });
        }

        const [updatedCommand, updatedUser] = await prisma.$transaction([
            prisma.hardwareCommand.update({
                where: { id: command.id },
                data: {
                    status: succeeded ? HARDWARE_COMMAND_STATUS.DONE : HARDWARE_COMMAND_STATUS.FAILED,
                    error: errorMessage,
                    completed_at: new Date()
                }
            }),
            prisma.user.update({
                where: { id: command.user_id },
                data: {
                    enrollment_status: succeeded ? ENROLLMENT_STATUS.ENROLLED : ENROLLMENT_STATUS.FAILED
                }
            })
        ]);

        return res.json({
            command: {
                id: updatedCommand.id,
                type: updatedCommand.type,
                status: updatedCommand.status,
                error: updatedCommand.error
            },
            user: {
                id: updatedUser.id,
                enrollment_status: updatedUser.enrollment_status
            }
        });
    } catch (error) {
        console.error('Hardware command result error:', error);
        return res.status(500).json({ error: 'Hardware command result could not be saved.' });
    }
});

app.post('/api/hardware/state', async (req, res) => {
    try {
        const lockStatus = String(req.body.lock_status || '').trim();
        const event = req.body.event !== undefined && req.body.event !== null
            ? String(req.body.event).trim()
            : null;

        if (![LOCK_STATUS.LOCKED, LOCK_STATUS.UNLOCKED].includes(lockStatus)) {
            console.warn('[HW_STATE] Invalid lock state payload:', req.body);
            return res.status(400).json({ error: 'lock_status must be Locked or Unlocked.' });
        }

        console.log(`[HW_STATE] Received lock state. status=${lockStatus} event=${event || 'none'}`);
        const state = await prisma.systemState.upsert({
            where: { id: 1 },
            update: {
                lock_status: lockStatus,
                last_state_event: event || null
            },
            create: {
                id: 1,
                lock_status: lockStatus,
                last_state_event: event || null
            }
        });

        console.log(`[HW_STATE] Persisted lock state. status=${state.lock_status} event=${state.last_state_event || 'none'} updated_at=${state.updated_at.toISOString()}`);
        return res.json({
            lock_status: state.lock_status,
            last_state_event: state.last_state_event,
            updated_at: state.updated_at
        });
    } catch (error) {
        console.error('Hardware state update error:', error);
        return res.status(500).json({ error: 'Hardware state could not be saved.' });
    }
});

// 7. TÜM LOGLARI LİSTELEME
app.get('/api/logs', async (req, res) => {
    try {
        const logs = await prisma.log.findMany({
            include: {
                user: { select: { id: true, name: true, email: true, role: true } },
                alert: true
            },
            orderBy: { time: 'desc' }
        });
        res.json(logs.map(normalizeLogForResponse));
    } catch (error) {
        res.status(500).json({ error: 'Logs could not be loaded.' });
    }
});

// 8. ALERT LİSTESİ
app.get('/api/alerts', async (req, res) => {
    try {
        const limit = Math.min(Math.max(Number(req.query.limit || 50), 1), 200);

        const alerts = await prisma.alert.findMany({
            orderBy: { created_at: 'desc' },
            take: limit
        });

        const unreadCount = alerts.filter(a => a.status === 'UNREAD').length;
        console.log(`[ALERT] GET /api/alerts — total=${alerts.length} unread=${unreadCount} limit=${limit}`);
        res.json(alerts.map(normalizeAlertForResponse));
    } catch (error) {
        console.error('[ALERT] Failed to load alerts:', error);
        res.status(500).json({ error: 'Alerts could not be loaded.' });
    }
});

// 9. TÜM ALERTLERI OKUNDU YAP
app.patch('/api/alerts/read-all', async (req, res) => {
    try {
        const result = await prisma.alert.updateMany({
            where: { status: 'UNREAD' },
            data: { status: 'READ' }
        });
        console.log(`[ALERT] read-all — marked ${result.count} alerts as READ`);
        return res.json({ updated: result.count });
    } catch (error) {
        console.error('[ALERT] Failed to mark all alerts read:', error);
        return res.status(500).json({ error: 'Alerts could not be updated.' });
    }
});

// 10. ALERT OKUNDU GÜNCELLEME
app.patch('/api/alerts/:id/read', async (req, res) => {
    try {
        const alertId = Number(req.params.id);
        if (Number.isNaN(alertId)) {
            return res.status(400).json({ error: 'Invalid alert ID.' });
        }

        const updated = await prisma.alert.update({
            where: { id: alertId },
            data: { status: 'READ' }
        });

        console.log(`[ALERT] alert_id=${alertId} marked READ`);
        return res.json(updated);
    } catch (error) {
        console.error(`[ALERT] Failed to mark alert read. id=${req.params.id}:`, error);
        return res.status(500).json({ error: 'Alert could not be updated.' });
    }
});

// 10. PUSH QUEUE DURUMU
app.get('/api/push-queue/status', async (_req, res) => {
    try {
        const [pending, sent, failed, cancelled, dueNow] = await Promise.all([
            prisma.pushDelivery.count({ where: { status: DELIVERY_STATUS.PENDING } }),
            prisma.pushDelivery.count({ where: { status: DELIVERY_STATUS.SENT } }),
            prisma.pushDelivery.count({ where: { status: DELIVERY_STATUS.FAILED } }),
            prisma.pushDelivery.count({ where: { status: DELIVERY_STATUS.CANCELLED } }),
            prisma.pushDelivery.count({
                where: {
                    status: { in: [DELIVERY_STATUS.PENDING, DELIVERY_STATUS.FAILED] },
                    attempts: { lt: PUSH_MAX_ATTEMPTS },
                    next_retry_at: { lte: new Date() }
                }
            })
        ]);

        return res.json({
            pending,
            sent,
            failed,
            cancelled,
            due_now: dueNow,
            apns_configured: hasApnsConfig()
        });
    } catch (error) {
        return res.status(500).json({ error: 'Queue status could not be loaded.' });
    }
});

// 11. SİSTEM DURUMU
app.get('/api/status', async (req, res) => {
    try {
        const [lastLog, systemState] = await Promise.all([
            prisma.log.findFirst({ orderBy: { time: 'desc' } }),
            prisma.systemState.findUnique({ where: { id: 1 } })
        ]);

        const status = {
            lock_status: systemState?.lock_status || LOCK_STATUS.LOCKED,
            last_event: normalizeAccessStatus(lastLog?.status) || 'No activity',
            fail_count: lastLog?.fail_count || 0
        };

        console.log(`[STATUS] lock_status=${status.lock_status} last_event=${status.last_event} fail_count=${status.fail_count}`);
        res.json(status);
    } catch (error) {
        console.error('System status load error:', error);
        res.status(500).json({ error: 'System status could not be loaded.' });
    }
});

// Web Paneli (React build)
const webDist = path.join(__dirname, 'web/dist');
app.use(express.static(webDist));
app.get('*splat', (req, res) => {
    res.sendFile(path.join(webDist, 'index.html'));
});

function startServer(port = Number(process.env.PORT || 3000)) {
    startPushQueueWorker();

    const server = app.listen(port, () => {
        console.log(`🚀 Sunucu http://localhost:${port} adresinde çalışıyor...`);
    });

    server.on('close', () => {
        stopPushQueueWorker();
    });

    return server;
}

if (require.main === module) {
    startServer();
}

module.exports = {
    app,
    startServer,
    processPushQueueOnce
};
