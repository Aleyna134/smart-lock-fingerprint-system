# 🔒 Akıllı Parmak İzi & PIN Kilidi Sistemi (Enterprise IoT Solution)

Bu proje, biyometrik veri güvenliği ve gerçek zamanlı IoT haberleşmesini temel alan, uçtan uca entegre bir akıllı kilit çözümüdür. Sistem; **ESP32 tabanlı gömülü yazılım**, **Node.js/Prisma API katmanı** ve **React yönetim paneli** üzerinden tam kontrol sağlar.

---

## 🏛️ Sistem Mimarisi ve Teknoloji Yığını

Sistem, dağıtık bir yapıda üç ana katmandan oluşur:

### 1. Gömülü Sistem (Hardware & Firmware)
*   **Mikrodenetleyici:** ESP32-WROOM (Dual-core, WiFi/BT desteği).
*   **Biyometrik Sensör:** R307/R503 Parmak İzi Sensörü (512 parmak kapasitesi, <0.3s tanıma süresi).
*   **Arayüz:** 4x4 Matris Keypad (PIN girişi), 16x2 I2C LCD (Kullanıcı etkileşimi), Buzzer & LED (Görsel-işitsel geri bildirim).
*   **Yazılım:** C++ (Arduino/PlatformIO), Non-blocking event loop mimarisi.

### 2. Backend API Katmanı (Cloud & Local)
*   **Runtime:** Node.js (Express Framework).
*   **ORM:** Prisma (Tip güvenli veritabanı erişimi).
*   **Veritabanı:** SQLite/PostgreSQL (İlişkisel veri yönetimi).
*   **Haberleşme:** RESTful API katmanı üzerinden donanım-backend senkronizasyonu.
*   **Bildirim Motoru:** SMTP (Email) entegrasyonu.

### 3. Yönetim Paneli (Web Interface)
*   **Frontend:** React (Vite).
*   **State Yönetimi:** Hooks & Context API.
*   **Veri Görselleştirme:** Dinamik log tabloları ve sistem durumu kartları.

---

## 🛠️ Teknik Detaylar ve Protokoller

### 🔑 Güvenlik ve Kimlik Doğrulama
*   **Biyometrik Veri:** Parmak izi şablonları sensörün güvenli bölgesinde tutulur; backend sadece `template_id` ve eşleşme sonucunu yönetir.
*   **Şifreleme:** Kullanıcı şifreleri veritabanında **Bcrypt** (cost: 10) algoritması ile tuzlanarak (salted) saklanır.
*   **Lockout Mekanizması:** 3 kez hatalı giriş denemesinde donanım ve yazılım seviyesinde 30 saniyelik "Cool-down" süreci tetiklenir.
*   **Admin Yetkilendirme:** Sadece Admin rolüne sahip kullanıcılar yeni kullanıcı ekleyebilir, silebilir veya sistem ayarlarını değiştirebilir.

### 📡 Donanım-Backend Haberleşmesi
Sistem, donanım komutlarını yönetmek için bir **Command Queue** yapısı kullanır:
1.  Admin web panelinden "Kullanıcı Ekle" butonuna basar.
2.  Backend, veritabanında bir `HardwareCommand (ENROLL)` kaydı oluşturur.
3.  ESP32, her 5 saniyede bir `/api/hardware/commands/next` endpoint'ini pollar.
4.  ESP32 komutu alır, sensörü aktif eder ve sonucu `/api/hardware/commands/:id/result` üzerinden backend'e bildirir.

---

## 🔌 Donanım Bağlantı Şeması (Pinout)

| Bileşen | ESP32 Pin | Açıklama |
| :--- | :--- | :--- |
| **Parmak İzi Sensörü (RX)** | GPIO 16 | Serial2 RX |
| **Parmak İzi Sensörü (TX)** | GPIO 17 | Serial2 TX |
| **I2C LCD (SDA)** | GPIO 21 | Standard I2C |
| **I2C LCD (SCL)** | GPIO 22 | Standard I2C |
| **Röle (Kilit)** | GPIO 18 | Aktif Düşük (Active Low) |
| **Buzzer** | GPIO 19 | PWM/Digital Out |
| **Keypad** | GPIO 12, 14, 27, 26, 25, 33, 32, 4 | Matrix Rows/Cols |

---

## 📁 API Endpoint Dokümantasyonu

### Kullanıcı İşlemleri
*   `POST /api/login`: Kimlik doğrulama.
*   `GET /api/users`: Tüm kullanıcıları listeleme.
*   `POST /api/users`: Yeni kullanıcı oluşturma (Admin yetkisi gerekir).
*   `DELETE /api/users/:id`: Kullanıcı silme (Donanım komut kuyruğuna eklenir).

### Donanım ve Loglama
*   `POST /api/access-log`: Donanımdan gelen başarılı/başarısız erişim verisi.
*   `GET /api/status`: Kilidin anlık durumu (Locked/Unlocked) ve sistem sağlığı.
*   `POST /api/hardware/state`: Donanımın fiziksel kilit durumunu backend'e bildirmesi.
*   `GET /api/logs`: Tüm erişim tarihçesi.

---

## 🚀 Kurulum ve Yapılandırma

### 1. Sunucu Tarafı
```bash
# Bağımlılıkları yükle
npm install

# Veritabanı şemasını hazırla
npx prisma migrate dev --name init

# Sunucuyu başlat
npm start
```

### 2. Gömülü Sistem Tarafı
1.  `firmware/esp32` dizinindeki `network_config.local.h` dosyasını oluşturun.
2.  `WIFI_SSID`, `WIFI_PASS` ve `BACKEND_URL` tanımlamalarını yapın.
3.  PlatformIO üzerinden "Upload" butonuna basarak kodu ESP32'ye yükleyin.

### 3. Web Arayüzü
```bash
cd web
npm install
npm run dev
```

---

## 📈 Gelecek Geliştirmeler
- [ ] OTP (Tek kullanımlık şifre) desteği.
- [ ] MQTT protokolüne geçiş (Daha düşük gecikme süresi).
- [ ] Yüz tanıma modülü entegrasyonu.

---

*Bu proje, mikroişlemci tabanlı güvenlik sistemlerinin tasarımı ve uygulanması amacıyla geliştirilmiş bir akademik çalışmadır.*
