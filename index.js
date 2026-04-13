const express = require('express');
const { PrismaClient } = require('@prisma/client');
const bcrypt = require('bcryptjs');
const nodemailer = require('nodemailer');

const app = express();
const prisma = new PrismaClient();

app.use(express.json());

// --- MAİL AYARLARI (GMAIL - GÖNDERİCİ) ---
const transporter = nodemailer.createTransport({
    service: 'gmail',
    auth: {
        user: 'hamzatufekci1234@gmail.com', // BURAYA KENDİ MAİLİNİ YAZ (Gönderici)
        pass: 'vzkakqdntsgxmnjc' // BURAYA UYGULAMA ŞİFRENİ YAZ
    }
});

// 1. LOG KAYDETME (ESP32)
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

        console.log("Yeni log kaydedildi:", newLog);
        res.status(201).json(newLog);
    } catch (error) {
        console.error("Log kaydetme hatasi:", error);
        res.status(500).json({ error: "Log kaydedilemedi." });
    }
});

// 2. GÜNCELLENEN: ALERT UÇ NOKTASI (Sadece Adminlere Mail Atar)
app.post('/api/alert', async (req, res) => {
    try {
        const { message } = req.body;
        console.log("🚨 ESP32'den Uyarı Geldi:", message);

        // Veritabanından rolü 'ADMIN' olan kullanıcıyı (Hamza'yı) bul
        const admins = await prisma.user.findMany({
            where: { role: 'ADMIN' },
            select: { email: true }
        });

        // Eğer sistemde admin yoksa işlemi durdur
        if (admins.length === 0) {
            console.log("Sistemde ADMIN bulunamadı, mail atılamıyor.");
            return res.status(404).json({ error: "Mail gönderilecek admin bulunamadı." });
        }

        // Admin(lerin) mail adreslerini al (Eğer birden fazla admin varsa hepsine atar)
        const adminEmails = admins.map(admin => admin.email).join(',');

        // Mail içeriği
        const mailOptions = {
            from: 'kendi.mail.adresin@gmail.com', // Kimden gidiyor (Yukarıdakiyle aynı)
            to: adminEmails,                      // KİME GİDİYOR: Veritabanından çekilen Admin Maili (Hamza)
            subject: '⚠️ AKILLI KİLİT SİSTEMİ - GÜVENLİK UYARISI ⚠️',
            text: `Sistemden yeni bir güvenlik uyarısı aldınız!\n\nDurum: ${message}\nTarih: ${new Date().toLocaleString('tr-TR')}`
        };

        // Maili Gönder
        await transporter.sendMail(mailOptions);
        
        console.log(`✅ Uyarı maili başarıyla şu ADMIN adresine gönderildi: ${adminEmails}`);
        res.status(200).json({ success: true, message: "Uyarı alındı ve admine mail gönderildi." });

    } catch (error) {
        console.error("Mail gönderme hatası:", error);
        res.status(500).json({ error: "Uyarı alındı ama mail atılamadı." });
    }
});

// 3. YENİ KULLANICI EKLEME (Sadece ADMIN Yetkisiyle)
app.post('/api/users', async (req, res) => {
    try {
        const { name, email, password, role, admin_id } = req.body;

        if (!admin_id) return res.status(401).json({ error: "Admin ID eksik." });

        const requester = await prisma.user.findUnique({ where: { id: Number(admin_id) } });
        if (!requester || requester.role !== 'ADMIN') {
            return res.status(403).json({ error: "Yetkisiz işlem." });
        }

        const hashedPassword = await bcrypt.hash(password, 10);
        const newUser = await prisma.user.create({
            data: {
                name: String(name),
                email: String(email),
                password: hashedPassword,
                role: role ? String(role).toUpperCase() : "USER"
            }
        });

        res.status(201).json({ message: "Kullanıcı oluşturuldu", user: newUser });
    } catch (error) {
        res.status(500).json({ error: "Kullanıcı oluşturulamadı." });
    }
});

// 4. GİRİŞ YAPMA (Login)
app.post('/api/login', async (req, res) => {
    try {
        const { email, password } = req.body;
        const user = await prisma.user.findUnique({ where: { email: String(email) } });

        if (user && await bcrypt.compare(String(password), user.password)) {
            res.json({ message: "Giriş başarılı", user_id: user.id, role: user.role });
        } else {
            res.status(401).json({ error: "Hatalı mail veya şifre" });
        }
    } catch (error) {
        res.status(500).json({ error: "Giriş hatası." });
    }
});

// 5. KULLANICILARI LİSTELEME
app.get('/api/users', async (req, res) => {
    const users = await prisma.user.findMany({ select: { id: true, name: true, email: true, role: true } });
    res.json(users);
});

// 6. KULLANICI SİLME
app.delete('/api/users/:id', async (req, res) => {
    await prisma.user.delete({ where: { id: Number(req.params.id) } });
    res.json({ message: "Kullanıcı silindi." });
});

// 7. TÜM LOGLARI LİSTELEME
app.get('/api/logs', async (req, res) => {
    const logs = await prisma.log.findMany({ include: { user: { select: { name: true } } }, orderBy: { time: 'desc' } });
    res.json(logs);
});

// 8. SİSTEM DURUMU
app.get('/api/status', async (req, res) => {
    const lastLog = await prisma.log.findFirst({ orderBy: { time: 'desc' } });
    res.json({
        lock_status: lastLog?.success ? "Unlocked" : "Locked",
        last_event: lastLog?.status || "No activity",
        fail_count: lastLog?.fail_count || 0
    });
});

const PORT = 3000;
app.listen(PORT, () => console.log(`🚀 Sunucu http://localhost:${PORT} adresinde çalışıyor...`));