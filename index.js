const express = require('express');
const { PrismaClient } = require('@prisma/client');
const bcrypt = require('bcryptjs');

const app = express();
const prisma = new PrismaClient();

app.use(express.json());

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

        console.log("Yeni log kaydedildi:", newLog);
        res.status(201).json(newLog);
    } catch (error) {
        console.error("Log kaydetme hatasi:", error);
        res.status(500).json({ error: "Log kaydedilemedi." });
    }
});

// 2. YENİ KULLANICI EKLEME (Sadece ADMIN Yetkisiyle)
app.post('/api/users', async (req, res) => {
    try {
        const { name, email, password, role, admin_id } = req.body;

        // Güvenlik Kontrolü: İstek atan kişi admin_id gönderdi mi?
        if (!admin_id) {
            return res.status(401).json({ error: "İşlemi yapan admin_id belirtilmedi." });
        }

        // Güvenlik Kontrolü: İstek atan kişi gerçekten ADMIN mi?
        const requester = await prisma.user.findUnique({ where: { id: Number(admin_id) } });
        if (!requester || requester.role !== 'ADMIN') {
            return res.status(403).json({ error: "Yetkisiz işlem. Sadece adminler kullanıcı ekleyebilir." });
        }

        // Şifreyi güvenlik için hashle (şifrele)
        const hashedPassword = await bcrypt.hash(password, 10);

        const newUser = await prisma.user.create({
            data: {
                name: String(name),
                email: String(email),
                password: hashedPassword,
                role: role ? String(role).toUpperCase() : "USER"
            }
        });

        res.status(201).json({
            message: "Kullanıcı başarıyla oluşturuldu",
            user: { id: newUser.id, name: newUser.name, email: newUser.email, role: newUser.role }
        });
    } catch (error) {
        console.error(error);
        res.status(500).json({ error: "Kullanıcı oluşturulamadı. Mail adresi kullanımda olabilir." });
    }
});

// 3. GİRİŞ YAPMA (Login Paneli İçin)
app.post('/api/login', async (req, res) => {
    try {
        const { email, password } = req.body;
        const user = await prisma.user.findUnique({ where: { email: String(email) } });

        // Şifreler eşleşiyor mu kontrol et
        if (user && await bcrypt.compare(String(password), user.password)) {
            res.json({ message: "Giriş başarılı", user_id: user.id, name: user.name, role: user.role });
        } else {
            res.status(401).json({ error: "Hatalı mail veya şifre" });
        }
    } catch (error) {
        res.status(500).json({ error: "Giriş işlemi sırasında bir hata oluştu." });
    }
});

// 4. KULLANICILARI LİSTELEME
app.get('/api/users', async (req, res) => {
    try {
        const users = await prisma.user.findMany({
            // Güvenlik için şifreleri frontend'e göndermiyoruz
            select: { id: true, name: true, email: true, role: true, enrolled_at: true }
        });
        res.json(users);
    } catch (error) {
        res.status(500).json({ error: "Kullanıcılar getirilemedi." });
    }
});

// 5. KULLANICI SİLME (ID ile)
app.delete('/api/users/:id', async (req, res) => {
    try {
        const { id } = req.params;
        await prisma.user.delete({ where: { id: Number(id) } });
        res.json({ message: "Kullanıcı başarıyla silindi." });
    } catch (error) {
        res.status(500).json({ error: "Kullanıcı silinirken bir hata oluştu." });
    }
});

// 6. TÜM LOGLARI LİSTELEME
app.get('/api/logs', async (req, res) => {
    try {
        const logs = await prisma.log.findMany({
            include: {
                user: { select: { name: true, email: true, role: true } } // Şifreyi gizle
            },
            orderBy: { time: 'desc' }
        });
        res.json(logs);
    } catch (error) {
        res.status(500).json({ error: "Loglar getirilemedi." });
    }
});

// 7. SİSTEM DURUMU
app.get('/api/status', async (req, res) => {
    try {
        const lastLog = await prisma.log.findFirst({ orderBy: { time: 'desc' } });
        res.json({
            lock_status: lastLog?.success ? "Unlocked" : "Locked",
            last_event: lastLog?.status || "No activity",
            fail_count: lastLog?.fail_count || 0
        });
    } catch (error) {
        res.status(500).json({ error: "Sistem durumu alınamadı." });
    }
});

// Sunucuyu Başlat
const PORT = 3000;
app.listen(PORT, () => {
    console.log(`🚀 Sunucu http://localhost:${PORT} adresinde çalışıyor...`);
});