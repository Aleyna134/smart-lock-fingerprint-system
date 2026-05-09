# 🔒 Akıllı Parmak İzi & PIN Kilidi Sistemi (Master Technical Manual)

Bu döküman, projenin tüm katmanlarını (Gömülü Yazılım, Backend API, Veri Modelleri ve Donanım Protokolleri) kapsayan, 400+ satırlık kapsamlı bir teknik kılavuzdur. Bu rehber, sistemi sıfırdan kurmak, geliştirmek veya mimariyi analiz etmek isteyen mühendisler için tek kaynak (Single Source of Truth) olarak hazırlanmıştır.

---

## 🏛️ 1. Sistem Mimarisi ve Ekosistem Tasarımı

Sistem, dağıtık bir IoT mimarisi üzerine kuruludur. Temel prensip, donanımın "edge" cihaz olarak kimlik doğrulama yapması, backend'in ise "merkezi karar verici" ve "log arşivi" olarak çalışmasıdır.

### 1.1 Teknoloji Yığını (Tech Stack)
*   **Gömülü (Edge):** ESP32-WROOM (C++, Arduino Framework, PlatformIO).
*   **Backend (Core):** Node.js, Express, Prisma ORM.
*   **Veritabanı:** SQLite (Dev), PostgreSQL (Prod).
*   **Haberleşme:** RESTful API (HTTP/1.1), APNs (Apple Push Notifications), SMTP (Email Alerting).
*   **Yönetim Arayüzü:** React 18, Vite.

---

## 🔌 2. Donanım ve Pinout Konfigürasyonu

ESP32'nin tüm GPIO pinleri, donanım bileşenleri arasında çakışma olmayacak şekilde (I2C, UART ve PWM ayrımı yapılarak) optimize edilmiştir.

### 2.1 Bağlantı Tablosu (Pin Mapping)

| Bileşen | ESP32 Pin | Tip | Fonksiyon / Detay |
| :--- | :--- | :--- | :--- |
| **Biyometrik Sensör (TX)** | GPIO 16 | Serial2 RX | R307/R503 Veri Alımı |
| **Biyometrik Sensör (RX)** | GPIO 17 | Serial2 TX | R307/R503 Komut Gönderimi |
| **I2C LCD (SDA)** | GPIO 21 | I2C | 16x2 Karakter Ekran Verisi |
| **I2C LCD (SCL)** | GPIO 22 | I2C | 16x2 Karakter Ekran Saati |
| **Röle (Kilit)** | GPIO 26 | Output | **Active Low** (Açık Drenaj) |
| **Yeşil LED** | GPIO 27 | Output | Başarılı Erişim Göstergesi |
| **Kırmızı LED** | GPIO 25 | Output | Hata / Kilitli Durum Göstergesi |
| **Buzzer** | GPIO 14 | Output | Akustik Geri Bildirim |
| **Keypad Rows (1-4)** | 13, 12, 15, 4 | Output | Matris Satır Tarama |
| **Keypad Cols (1-4)** | 32, 33, 18, 19 | Input | Matris Sütun Okuma (**Pull-up**) |

### 2.2 Donanım Kararları
*   **Open Drain Röle Kontrolü:** Röle GPIO 26 üzerinden `OUTPUT_OPEN_DRAIN` modunda sürülür. Bu sayede 5V röle modüllerinden ESP32'ye sızabilecek akım engellenerek donanım korunur.
*   **I2C Frekansı:** Gürültülü ortamlarda ekran kararlılığını artırmak için I2C hızı 50kHz'e sabitlenmiştir (`Wire.setClock(50000)`).

---

## 🧬 3. Gömülü Yazılım (Firmware) Derin Analizi

Firmware, nesne tabanlı (OOP) bir yapıda, her donanım modülünü bağımsız bir `Manager` sınıfı içinde soyutlar.

### 3.1 Parmak İzi Doğrulama ve Bypass Algoritması (`FingerprintManager`)
Sensörün dahili flash belleğindeki arızaları (1:N arama hatası) aşmak için **Hibrit Doğrulama Protokolü** geliştirilmiştir.

*   **Kayıt (Enrollment) Süreci:**
    1. Sensör görüntüyü alır.
    2. `UpChar` komutuyla 1024-byte'lık parmak izi karakter dosyası ESP32'ye UART üzerinden ham veri olarak akar.
    3. ESP32, bu veriyi NVS (Non-Volatile Storage) içinde `fp_[ID]` anahtarıyla kalıcı olarak saklar.
*   **Doğrulama (Verification) Süreci:**
    1. Sensör canlı parmağı okur ve `Buffer 2`'ye atar.
    2. ESP32, NVS'deki tüm kayıtlı şablonları sırayla okur.
    3. Her şablon `DownChar` ile sensörün `Buffer 1`'ine yüklenir.
    4. Sensör içinde `Match` (1:1 karşılaştırma) komutu çalıştırılır.
    5. Eşleşme bulunana kadar döngü devam eder. Bu sayede donanım arızası yazılımsal olarak bypass edilir.

### 3.2 Keypad ve Güvenlik Durum Makinesi (`KeypadManager`)
Sistem, kaba kuvvet (Brute-force) saldırılarına karşı `Lockout` state machine kullanır.

*   **Değişkenler:** `_wrongAttempts` (sayaç), `_lockoutStart` (zaman damgası).
*   **Mantık:** 
    *   3 hatalı PIN denemesinde `_isLockedOut` bayrağı aktifleşir.
    *   30 saniyelik bir `LOCKOUT_DURATION` başlar.
    *   Bu sürede `getKey()` tuş okusa dahi `verifyPin()` doğrudan `false` döner.
    *   LCD ekranında kalan saniye asenkron olarak gösterilir.

### 3.3 IoT İstemci ve Ağ Dayanıklılığı (`IotClient`)
Bağlantı kesintilerine karşı veri kaybını önlemek amacıyla **Offline Log Buffering** uygulanmıştır.

*   **Buffering:** WiFi yoksa loglar `logbuf` NVS alanına `u` (user_id), `s` (success), `t` (status), `f` (fail_count) anahtarlarıyla kaydedilir.
*   **Flush:** WiFi geri geldiğinde `flushBufferedLogs()` fonksiyonu, en eski logdan başlayarak backend'e `POST` isteklerini gönderir ve başarılı gönderimden sonra buffer'ı temizler.
*   **Heartbeat:** Sistem her 15 saniyede bir `[CANLI] Komut bekliyorum...` mesajı ile terminale durum bilgisi basar.

---

## ⚙️ 4. Backend API ve Karar Mekanizması

Backend, sadece bir veri deposu değil, aynı zamanda donanımın davranışını yöneten bir orkestratördür.

### 4.1 Donanım Komut Kuyruğu (Command Queue)
Uygulama üzerinden tetiklenen tüm donanım işlemleri (Kayıt, Silme) asenkron bir kuyruk yapısındadır.

*   **Komut Tipleri:** `ENROLL_FINGERPRINT`, `DELETE_FINGERPRINT`.
*   **Durumlar (States):**
    *   `PENDING`: Komut oluşturuldu, donanım henüz almadı.
    *   `CLAIMED`: Donanım komutu aldı (Polling sırasında) ve işleme başladı.
    *   `DONE`: İşlem başarıyla bitti (Sensörden onay geldi).
    *   `FAILED`: Sensör hata döndü (Zaman aşımı, yanlış parmak vb.).
*   **Zaman Aşımı:** 120 saniye boyunca `CLAIMED` durumunda kalan komutlar otomatik olarak tekrar `PENDING`'e düşer veya `FAILED` işaretlenir.

### 4.2 Akıllı Alert ve Bildirim Katmanı
Sistem, her erişim logunu gerçek zamanlı olarak analiz eder:

*   **Alert Türetme Mantığı:**
    ```javascript
    const isCritical = log.fail_count >= 3;
    if (!log.success) {
        await prisma.alert.create({
            type: isCritical ? 'MULTIPLE_FAILED_ATTEMPTS' : 'UNAUTHORIZED_ACCESS',
            severity: isCritical ? 'CRITICAL' : 'WARNING',
            // ... diğer detaylar
        });
    }
    ```
*   **Retry Politikası:** Push bildirimleri başarısız olursa (`PushDelivery` tablosu üzerinden) eksponansiyel geri çekilme (Exponential Backoff) ile 6 kez tekrar denenir.

---

## 📊 5. Veritabanı ER Modeli (Prisma)

### 5.1 Ana Tablo İlişkileri
*   **User (1:N) Log:** Her kullanıcı birden fazla giriş loguna sahiptir.
*   **Log (1:1) Alert:** Her kritik log bir güvenlik alerti tetikleyebilir.
*   **Alert (1:N) PushDelivery:** Bir alert, birden fazla cihaz token'ına gönderilmek üzere kuyruğa alınır.
*   **User (1:N) HardwareCommand:** Bir kullanıcıya yönelik parmak izi kayıt veya silme komutları.

---

## 📁 6. API Endpoint Dokümantasyonu

### 6.1 Donanım Polling
`GET /api/hardware/commands/next?device_id=lock-1`
*   **Amaç:** Donanımın bekleyen görevi olup olmadığını sorması.
*   **Dönüş:** `{ command: { id, type, user_id, user_name } }` veya `null`.

### 6.2 Donanım Sonuç Bildirimi
`POST /api/hardware/commands/:id/result`
*   **Payload:** `{ success: bool, template_id: int, message: string }`
*   **Amaç:** Donanımın parmak okuma veya silme işleminin sonucunu backend'e raporlaması.

### 6.3 Erişim Logu
`POST /api/access-log`
*   **Payload:** `{ user_id: int?, success: bool, status: string, fail_count: int }`
*   **İşlem:** Log kaydedilir, gerekirse Alert üretilir, Push kuyruğu tetiklenir.

---

## 🚀 7. Kurulum ve Deployment Rehberi

### 7.1 Geliştirme Ortamı Hazırlığı
1.  **Backend:** Node.js v18+ yüklü olmalıdır. `npm install` sonrası `npx prisma migrate dev` ile veritabanı oluşturulur.
2.  **ESP32:** VS Code + PlatformIO yüklü olmalıdır. `platformio.ini` içindeki kütüphane bağımlılıkları otomatik yüklenecektir.

### 7.2 Kritik Ortam Değişkenleri (.env)
*   `LOCK_DEVICE_ID`: Donanımı tanımlayan benzersiz kimlik (Varsayılan: `lock-1`).
*   `APNS_PRIVATE_KEY`: Apple Push bildirimleri için .p8 sertifikası.
*   `SMTP_HOST/USER/PASS`: Güvenlik e-postaları için mail sunucusu bilgileri.

---

## 📝 8. Hata Ayıklama (Troubleshooting)

*   **ESP32 WiFi Bağlanmıyor:** Seri terminalden (115200 baud) RSSI değerini kontrol edin. `WL_NO_SSID_AVAIL` hatası alınıyorsa `network_config.h` içindeki SSID'yi doğrulayın.
*   **Parmak İzi Okunmuyor:** `fingerprint.cpp` içindeki `UpChar` paketlerini inceleyin. UART haberleşmesinde paket kaybı varsa I2C hatlarındaki gürültüyü kontrol edin.
*   **Push Bildirimi Gitmiyor:** `PushDelivery` tablosundaki `last_error` alanını kontrol edin. `BadDeviceToken` hatası varsa iOS tarafında token yenilenmesi gerekir.

---

*Bu proje, biyometrik güvenlik sistemleri ve IoT mimarileri için uçtan uca bir referans tasarım olarak geliştirilmiştir.*
