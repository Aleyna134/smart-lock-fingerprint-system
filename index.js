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
    const title = isCritical ? 'Coklu hatali giris denemesi' : 'Yetkisiz erisim denemesi';
    const detail = isCritical
        ? `Kisa surede ${log.fail_count} basarisiz giris denemesi algilandi.`
        : `Tanimsiz parmak izi denemesi algilandi. Durum: ${log.status}`;

    return { type, severity, title, detail };
}

function buildApnsPayload(alert, unreadCount) {
    return {
        aps: {
            alert: {
                title: alert.title,
                body: alert.detail
            },
            badge: unreadCount,
            sound: 'default'
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
                status: String(status),
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
        res.status(500).json({ error: 'Log kaydedilemedi.' });
    }
});

// 2. CİHAZ TOKEN KAYDI (iOS APNs)
app.post('/api/device-token', async (req, res) => {
    try {
        const { user_id, token } = req.body;
        const normalizedToken = String(token || '').trim().toLowerCase();

        if (!normalizedToken) {
            return res.status(400).json({ error: 'token zorunludur.' });
        }

        if (!/^[a-f0-9]{40,256}$/.test(normalizedToken)) {
            return res.status(400).json({ error: 'token formatı geçersiz.' });
        }

        let resolvedUserId = null;
        if (user_id !== undefined && user_id !== null) {
            resolvedUserId = Number(user_id);
            if (Number.isNaN(resolvedUserId)) {
                return res.status(400).json({ error: 'user_id sayısal olmalıdır.' });
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
            message: existing ? 'Token güncellendi.' : 'Token kaydedildi.',
            token_id: savedToken.id,
            queued_unread_alerts: queuedUnread
        });
    } catch (error) {
        console.error('Device token kaydetme hatasi:', error);
        return res.status(500).json({ error: 'Device token kaydedilemedi.' });
    }
});

// 3. YENİ KULLANICI EKLEME (Sadece ADMIN Yetkisiyle)
app.post('/api/users', async (req, res) => {
    try {
        const { name, email, password, role, admin_id } = req.body;

        if (!admin_id) {
            return res.status(401).json({ error: 'İşlemi yapan admin_id belirtilmedi.' });
        }

        const requester = await prisma.user.findUnique({ where: { id: Number(admin_id) } });
        if (!requester || requester.role !== 'ADMIN') {
            return res.status(403).json({ error: 'Yetkisiz işlem. Sadece adminler kullanıcı ekleyebilir.' });
        }

        const hashedPassword = await bcrypt.hash(password, 10);

        const newUser = await prisma.user.create({
            data: {
                name: String(name),
                email: String(email),
                password: hashedPassword,
                role: role ? String(role).toUpperCase() : 'USER'
            }
        });

        res.status(201).json({
            message: 'Kullanıcı başarıyla oluşturuldu',
            user: { id: newUser.id, name: newUser.name, email: newUser.email, role: newUser.role }
        });
    } catch (error) {
        console.error(error);
        res.status(500).json({ error: 'Kullanıcı oluşturulamadı. Mail adresi kullanımda olabilir.' });
    }
});

// 4. GİRİŞ YAPMA (Login Paneli İçin)
app.post('/api/login', async (req, res) => {
    try {
        const { email, password } = req.body;
        const user = await prisma.user.findUnique({ where: { email: String(email) } });

        if (user && await bcrypt.compare(String(password), user.password)) {
            res.json({ message: 'Giriş başarılı', user_id: user.id, name: user.name, role: user.role });
        } else {
            res.status(401).json({ error: 'Hatalı mail veya şifre' });
        }
    } catch (error) {
        res.status(500).json({ error: 'Giriş işlemi sırasında bir hata oluştu.' });
    }
});

// 5. KULLANICILARI LİSTELEME
app.get('/api/users', async (req, res) => {
    try {
        const users = await prisma.user.findMany({
            select: { id: true, name: true, email: true, role: true, enrolled_at: true }
        });
        res.json(users);
    } catch (error) {
        res.status(500).json({ error: 'Kullanıcılar getirilemedi.' });
    }
});

// 6. KULLANICI SİLME (ID ile)
app.delete('/api/users/:id', async (req, res) => {
    try {
        const { id } = req.params;
        await prisma.user.delete({ where: { id: Number(id) } });
        res.json({ message: 'Kullanıcı başarıyla silindi.' });
    } catch (error) {
        res.status(500).json({ error: 'Kullanıcı silinirken bir hata oluştu.' });
    }
});

// 7. TÜM LOGLARI LİSTELEME
app.get('/api/logs', async (req, res) => {
    try {
        const logs = await prisma.log.findMany({
            include: {
                user: { select: { id: true, name: true, email: true, role: true } }
            },
            orderBy: { time: 'desc' }
        });
        res.json(logs);
    } catch (error) {
        res.status(500).json({ error: 'Loglar getirilemedi.' });
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

        res.json(alerts);
    } catch (error) {
        res.status(500).json({ error: 'Alert listesi alınamadı.' });
    }
});

// 9. TÜM ALERTLERI OKUNDU YAP
app.patch('/api/alerts/read-all', async (req, res) => {
    try {
        const result = await prisma.alert.updateMany({
            where: { status: 'UNREAD' },
            data: { status: 'READ' }
        });
        return res.json({ updated: result.count });
    } catch (error) {
        return res.status(500).json({ error: 'Alertler güncellenemedi.' });
    }
});

// 10. ALERT OKUNDU GÜNCELLEME
app.patch('/api/alerts/:id/read', async (req, res) => {
    try {
        const alertId = Number(req.params.id);
        if (Number.isNaN(alertId)) {
            return res.status(400).json({ error: 'Geçersiz alert id.' });
        }

        const updated = await prisma.alert.update({
            where: { id: alertId },
            data: { status: 'READ' }
        });

        return res.json(updated);
    } catch (error) {
        return res.status(500).json({ error: 'Alert güncellenemedi.' });
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
        return res.status(500).json({ error: 'Queue durumu alınamadı.' });
    }
});

// 11. SİSTEM DURUMU
app.get('/api/status', async (req, res) => {
    try {
        const lastLog = await prisma.log.findFirst({ orderBy: { time: 'desc' } });
        res.json({
            lock_status: lastLog?.success ? 'Unlocked' : 'Locked',
            last_event: lastLog?.status || 'No activity',
            fail_count: lastLog?.fail_count || 0
        });
    } catch (error) {
        res.status(500).json({ error: 'Sistem durumu alınamadı.' });
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
