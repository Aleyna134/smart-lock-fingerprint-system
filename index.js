const express = require('express');
const { PrismaClient } = require('@prisma/client');

const app = express();
// En sade ve en doğru bağlantı şekli:
const prisma = new PrismaClient(); 

const port = 3000;
app.use(express.json());

app.get('/', (req, res) => {
  res.send('Smart Lock Backend API tıkır tıkır çalışıyor! 🚀');
});

app.post('/api/access-log', async (req, res) => {
  try {
    const { success, status, fail_count } = req.body;
    
    const yeniKayit = await prisma.log.create({
      data: {
        success: Boolean(success),
        status: String(status),
        fail_count: Number(fail_count || 0)
      }
    });

    console.log("Veritabanına kaydedildi:", yeniKayit);
    res.status(201).json({ message: "Başarıyla kaydedildi!", veri: yeniKayit });

  } catch (error) {
    console.error("Hata Detayı:", error);
    res.status(500).json({ error: "Veritabanı hatası." });
  }
});

app.listen(port, () => {
  console.log(`Sunucu http://localhost:${port} adresinde dinleniyor...`);
});