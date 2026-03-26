const express = require('express');
const { PrismaClient } = require('@prisma/client');

const app = express();
const prisma = new PrismaClient();

app.use(express.json());

// 1. Log Kaydetme
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

// 2. Yeni Kullanici Ekleme
app.post('/api/users', async (req, res) => {
    try {
        const { name } = req.body;
        const newUser = await prisma.user.create({
            data: { name: String(name) }
        });
        res.status(201).json(newUser);
    } catch (error) {
        res.status(500).json({ error: "Kullanici olusturulamadi." });
    }
});

// 3. Kullanicilari Listeleme
app.get('/api/users', async (req, res) => {
    try {
        const users = await prisma.user.findMany();
        res.json(users);
    } catch (error) {
        res.status(500).json({ error: "Kullanicilar getirilemedi." });
    }
});

// 4. Kullanici Silme (ID ile)
app.delete('/api/users/:id', async (req, res) => {
    try {
        const { id } = req.params;
        await prisma.user.delete({ where: { id: Number(id) } });
        res.json({ message: "Kullanici basariyla silindi." });
    } catch (error) {
        res.status(500).json({ error: "Kullanici silinirken bir hata olustu." });
    }
});

// 5. Tum Loglari Listeleme (Kullanici bilgisi ile)
app.get('/api/logs', async (req, res) => {
    try {
        const logs = await prisma.log.findMany({
            include: { user: true },
            orderBy: { time: 'desc' }
        });
        res.json(logs);
    } catch (error) {
        res.status(500).json({ error: "Loglar getirilemedi." });
    }
});

// 6. Sistem Durumu
app.get('/api/status', async (req, res) => {
    try {
        const lastLog = await prisma.log.findFirst({ orderBy: { time: 'desc' } });
        res.json({
            lock_status: lastLog?.success ? "Unlocked" : "Locked",
            last_event: lastLog?.status || "No activity",
            fail_count: lastLog?.fail_count || 0
        });
    } catch (error) {
        res.status(500).json({ error: "Sistem durumu alinamadi." });
    }
});

// Sunucuyu Baslat
const PORT = 3000;
app.listen(PORT, () => {
    console.log(`Sunucu http://localhost:${PORT} adresinde calisiyor...`);
});