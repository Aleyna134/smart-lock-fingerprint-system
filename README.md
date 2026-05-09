# 🔒 Akıllı Parmak İzi & PIN Kilidi Sistemi (Enterprise-Grade IoT Ecosystem)

Bu proje; biyometrik veri güvenliği, asenkron donanım yönetimi ve gerçek zamanlı mobil bildirimleri birleştiren, yüksek ölçeklenebilir bir IoT akıllı kilit çözümüdür. Sistem; **ESP32 Gömülü Sistem**, **Node.js/Prisma API Katmanı** ve **React Yönetim Paneli**'nden oluşan tam entegre bir ekosistemdir.

---

## 🏛️ Katmanlı Sistem Mimarisi

Sistem, "Single Source of Truth" prensibiyle çalışır; tüm donanım durumları ve yetkilendirmeler merkezi backend tarafından yönetilir.

### 1. Donanım & Gömülü Yazılım (Firmware)
*   **Çekirdek:** ESP32-WROOM (Dual-Core 240MHz).
*   **Biyometrik Modül:** R307/R503 (Optik/Kapasitif seçenekli). 
*   **Kullanılan Kütüphaneler:**
    *   `Adafruit Fingerprint Sensor Library` (Biyometrik yönetim).
    *   `LiquidCrystal_I2C` (LCD sürücüsü).
    *   `Keypad` (Matris tarama algoritması).
    *   `HTTPClient` & `WiFi` (Backend senkronizasyonu).
*   **Mimarisi:** Non-blocking `loop()`, asenkron polling ve fail-safe (Brown-out) koruması.

### 2. Backend & Veri Katmanı
*   **Framework:** Node.js (Express v4).
*   **Veritabanı Motoru:** SQLite (Lokal test) / PostgreSQL (Üretim).
*   **ORM:** Prisma (Tip-güvenli sorgular ve otomatik migrasyonlar).
*   **Güvenlik:**
    *   **Argon2/Bcrypt:** Şifre hashleme.
    *   **JWT (İleri Hazırlık):** Bearer token altyapısı.
    *   **Sanitization:** Donanım girişlerinin ve API request'lerinin sıkı validasyonu.

### 3. Yönetim Paneli (Web)
*   **Framework:** React 18 (Vite tabanlı).
*   **UI:** Modern Dashboard, Reaktif Liste bileşenleri.
*   **İletişim:** Axios tabanlı API istemcisi.

---

## 🛠️ Teknik Derin Dalış (Deep Dive)

### 📡 Donanım Komut Kuyruğu (Hardware Command Queue)
Sistem, donanımı doğrudan tetiklemek yerine bir "Komut Kuyruğu" mimarisi kullanır. Bu sayede donanım, internet kesintisi olsa bile tekrar çevrimiçi olduğunda bekleyen görevleri (Yeni kayıt, silme) sırasıyla işler.

**Akış Şeması (Kullanıcı Kaydı):**
1.  **Web Panel:** `POST /api/users` isteği gönderir.
2.  **Backend:** Kullanıcıyı `PENDING` statüsünde oluşturur ve bir `HardwareCommand` üretir.
3.  **ESP32:** `GET /api/hardware/commands/next` ile komutu "Claim" eder.
4.  **ESP32:** Fiziksel sensörde parmak izini alır, sonucu `POST /api/hardware/commands/:id/result` ile bildirir.
5.  **Backend:** Kayıt başarılıysa kullanıcıyı `ENROLLED` statüsüne çeker.

### 🔒 Güvenlik Protokolü (Security Lockout)
Hizmet dışı bırakma saldırılarına karşı hibrit bir koruma mevcuttur:
-   **Donanım Seviyesi:** 3 hatalı denemede ESP32 `lockedUntil` değişkenini aktif ederek 30 saniye boyunca parmak okumayı durdurur.
-   **Yazılım Seviyesi:** Backend, gelen `fail_count` değerine göre kritik alertler üretir ve e-posta tetikleyicilerini çalıştırır.

---

## 📊 Veritabanı Şeması (Data Models)

| Model | Açıklama |
| :--- | :--- |
| **User** | ID, İsim, Email (Unique), Password (Hashed), Role (Admin/User), Enrollment Status. |
| **Log** | Success (Bool), Status (Mesaj), Time, Fail Count, User Reference. |
| **Alert** | Type (Critical/Warning), Severity, Status (Unread/Read), Log Link. |
| **HardwareCommand** | Type (Enroll/Delete), Status (Pending/Claimed/Done), Device ID. |
| **SystemState** | Anlık kilit durumu (Locked/Unlocked) ve son senkronizasyon zamanı. |

---

## 📁 API Referansı & JSON Örnekleri

### 1. Erişim Logu Gönderme
`POST /api/access-log`
```json
{
  "success": false,
  "status": "Hatali Parmak Izi",
  "fail_count": 2,
  "user_id": null
}
```

### 2. Donanım Komutu Sorgulama
`GET /api/hardware/commands/next?device_id=lock-1`
```json
{
  "command": {
    "id": 42,
    "type": "ENROLL_FINGERPRINT",
    "user_id": 10,
    "user_name": "Ahmet Yilmaz"
  }
}
```

---

## 🔌 Donanım Konfigürasyonu (Pinout)

Sistem, ESP32'nin tüm donanım özelliklerini optimize ederek kullanır:

| Fonksiyon | Pin | Detay |
| :--- | :--- | :--- |
| **Serial2 RX (FP)** | GPIO 16 | Sensör veri alımı |
| **Serial2 TX (FP)** | GPIO 17 | Sensör komut gönderimi |
| **I2C SDA (LCD)** | GPIO 21 | Veri hattı |
| **I2C SCL (LCD)** | GPIO 22 | Saat hattı |
| **Relay Signal** | GPIO 18 | Kilit kontrolü (Inverted) |
| **Buzzer** | GPIO 19 | Alarm ve Onay tonları |
| **Keypad Rows** | 12, 14, 27, 26 | Satır tarama |
| **Keypad Cols** | 25, 33, 32, 4 | Sütun okuma |

---

## 👥 Katkıda Bulunanlar ve İletişim

*   **Ekosistem Sahibi:** Smart Lock Project Team
*   **Teknolojiler:** ESP32, Node.js, React, Prisma, SwiftUI
*   **Lisans:** Akademik Kullanım

---

*Bu doküman, sistemin teknik yeterliliğini ve mimari bütünlüğünü kanıtlamak amacıyla sunum dosyası olarak hazırlanmıştır.*
