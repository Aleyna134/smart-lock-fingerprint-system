# 🔒 Akıllı Parmak İzi Kilidi Sistemi (Smart Lock)

Bu proje, yüksek güvenlikli, gerçek zamanlı izleme özellikli ve çok platformlu bir IoT akıllı kilit çözümüdür. Sistem; **ESP32 tabanlı donanım**, **Node.js/Prisma backend** ve **React tabanlı web paneli** olmak üzere üç ana bileşenden oluşmaktadır.

---

## 🚀 Proje Genel Bakışı

Akıllı Kilit Sistemi, geleneksel anahtarların yerini biyometrik (parmak izi) ve dijital (PIN) kimlik doğrulama ile değiştirir. Kullanıcılar kapı durumunu dünyanın her yerinden izleyebilir, yeni kullanıcılar tanımlayabilir ve erişim loglarını takip edebilirler.

### 🏛️ Sistem Mimarisi

1.  **Donanım (Firmware):** ESP32 mikrodenetleyici, R307/R503 parmak izi sensörü, 4x4 Keypad, I2C LCD ekran ve kilit mekanizması (Röle).
2.  **Backend:** Node.js, Express ve Prisma (SQLite/PostgreSQL) kullanılarak geliştirilmiş API. Sistem durumu, kullanıcı yönetimi ve loglama işlemlerini yürütür.
3.  **Web Paneli:** React ve Vite ile oluşturulmuş, sistem yönetimi, log takibi ve kullanıcı işlemleri için kullanılan merkezi yönetim arayüzü.

---

## ✨ Ana Özellikler

*   **Çift Faktörlü Erişim:** Parmak izi ve/veya PIN kodu ile güvenli giriş.
*   **Gerçek Zamanlı İzleme:** Kapı durumunun (Kilitli/Açık) ve son erişim olaylarının anlık takibi.
*   **Uzaktan Yönetim:** Web paneli üzerinden yeni parmak izi kaydetme süreci başlatma veya mevcut kullanıcıları silme.
*   **Gelişmiş Loglama:** Tüm başarılı ve başarısız giriş denemelerinin tarih, saat ve kullanıcı bilgisiyle veritabanında saklanması.
*   **Güvenlik Kilidi (Lockout):** Üst üste hatalı denemelerde sistemin kendini otomatik olarak belirli bir süre boyunca kilitlemesi.
*   **E-posta Uyarıları:** Kritik güvenlik olaylarında (çoklu hatalı giriş vb.) adminlere otomatik e-posta bildirimi.

---

## 🛠️ Teknik Bileşenler

### 🔌 Donanım (ESP32)
*   **Dil:** C++ (PlatformIO / Arduino Framework)
*   **Özellikler:** Otomatik parmak izi tarama, Keypad ile PIN yönetimi, LCD üzerinden kullanıcı geri bildirimi.
*   **Haberleşme:** WiFi üzerinden REST API ile backend senkronizasyonu.

### ⚙️ Backend (API)
*   **Teknoloji:** Node.js, Express, Prisma ORM
*   **Veritabanı:** SQLite (Geliştirme için)
*   **Güvenlik:** Bcrypt şifreleme, Admin/User yetkilendirme rolleri.
*   **Entegrasyon:** SMTP (E-posta) ve Cloudflare Tunnel desteği.

### 💻 Web Paneli
*   **Teknoloji:** React, Vite
*   **Özellikler:** Reaktif dashboard, interaktif log tabloları, kullanıcı yönetim arayüzü ve sistem durumu göstergeleri.

---

## 📦 Kurulum ve Çalıştırma

### 1. Backend Kurulumu
```bash
cd smart-lock-fingerprint-system
npm install
# .env dosyasını oluşturun ve gerekli değişkenleri (Veritabanı, SMTP vb.) doldurun
npx prisma migrate dev
npm start
```

### 2. Web Paneli
Backend çalışırken web arayüzüne tarayıcı üzerinden erişebilirsiniz. Geliştirme modu için:
```bash
cd smart-lock-fingerprint-system/web
npm install
npm run dev
```

### 3. Donanım (ESP32) Yazılımı
*   `firmware/esp32` dizinindeki projeyi PlatformIO ile açın.
*   `include/network_config.local.h` dosyasını oluşturup WiFi ve Backend URL bilgilerinizi girin.
*   ESP32 kartınıza yükleme yapın.

---

## 👥 Katkıda Bulunanlar

*   **Teknolojiler:** ESP32, Node.js, React

---

*Bu proje Mikroişlemciler dersi kapsamında bir prototip olarak geliştirilmiştir.*

