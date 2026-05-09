# 🔒 Akıllı Parmak İzi Kilidi Sistemi (Smart Lock)

Bu proje, yüksek güvenlikli, gerçek zamanlı izleme özellikli ve çok platformlu bir IoT akıllı kilit çözümüdür. Sistem; **ESP32 tabanlı donanım**, **Node.js/Prisma backend**, **React tabanlı web paneli** ve **Native iOS (SwiftUI) uygulaması** olmak üzere dört ana bileşenden oluşmaktadır.

---

## 🚀 Proje Genel Bakışı

Akıllı Kilit Sistemi, geleneksel anahtarların yerini biyometrik (parmak izi) ve dijital (PIN) kimlik doğrulama ile değiştirir. Kullanıcılar kapı durumunu dünyanın her yerinden izleyebilir, yeni kullanıcılar tanımlayabilir ve yetkisiz erişim girişimlerinde anlık bildirim alabilirler.

### 🏛️ Sistem Mimarisi

1.  **Donanım (Firmware):** ESP32 mikrodenetleyici, R307/R503 parmak izi sensörü, 4x4 Keypad, I2C LCD ekran ve kilit mekanizması (Röle).
2.  **Backend:** Node.js, Express ve Prisma (SQLite/PostgreSQL) kullanılarak geliştirilmiş API. APNs (Apple Push Notification service) ve SMTP entegrasyonuna sahiptir.
3.  **iOS Uygulaması:** SwiftUI ile geliştirilmiş, Live Activities ve Dynamic Island desteği sunan modern mobil uygulama.
4.  **Web Paneli:** React ve Vite ile oluşturulmuş, sistem yönetimi ve log takibi için kullanılan yönetim arayüzü.

---

## ✨ Ana Özellikler

*   **Çift Faktörlü Erişim:** Parmak izi ve/veya PIN kodu ile güvenli giriş.
*   **Gerçek Zamanlı Bildirimler:** Başarısız giriş denemelerinde iOS cihazlara anlık push bildirimleri ve adminlere e-posta uyarısı.
*   **Canlı Etkinlikler (Live Activities):** Kapıdaki kritik durumları iPhone kilit ekranında ve Dynamic Island'da anlık takip etme.
*   **Uzaktan Yönetim:** Web veya mobil uygulama üzerinden yeni parmak izi kaydetme veya mevcut kullanıcıları silme.
*   **Gelişmiş Loglama:** Tüm başarılı ve başarısız giriş denemelerinin tarih, saat ve kullanıcı bilgisiyle kaydedilmesi.
*   **Güvenlik Kilidi (Lockout):** 3 kez üst üste hatalı denemede sistemin kendini otomatik olarak 30 saniye boyunca kilitlemesi.

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
*   **Entegrasyon:** APNs (Push), Nodemailer (E-posta), Cloudflare Tunnel desteği.

### 📱 iOS Uygulaması
*   **Teknoloji:** Swift, SwiftUI, Observation Framework (iOS 17+)
*   **Mimari:** MVVM (Model-View-ViewModel)
*   **Özellikler:** Keychain ile güvenli oturum, APNs entegrasyonu, WidgetKit ile Live Activities.

### 💻 Web Paneli
*   **Teknoloji:** React, Vite, Tailwind CSS (opsiyonel)
*   **Özellikler:** Reaktif dashboard, interaktif log tabloları, kullanıcı yönetim arayüzü.

---

## 📦 Kurulum ve Çalıştırma

### 1. Backend Kurulumu
```bash
cd smart-lock-fingerprint-system
npm install
# .env dosyasını oluşturun ve gerekli değişkenleri (APNS, SMTP vb.) doldurun
npx prisma migrate dev
npm start
```

### 2. Donanım (ESP32) Yazılımı
*   `firmware/esp32` dizinindeki projeyi PlatformIO ile açın.
*   `include/network_config.local.h` dosyasını oluşturup WiFi ve Backend URL bilgilerinizi girin.
*   ESP32 kartınıza yükleme yapın.

### 3. iOS Uygulaması
*   `Microprocessors_Project_IOS` dizinindeki `.xcodeproj` dosyasını Xcode ile açın.
*   `NetworkConfig.swift` dosyasındaki `baseURL` değerini güncelleyin.
*   Uygulamayı fiziksel bir iOS cihazında (Push bildirimleri için) çalıştırın.

---

## 📝 Ekran Görüntüleri ve UI/UX

Sistem, hem mobil hem de web tarafında tutarlı bir tasarım diline sahiptir:
*   **Yeşil:** Başarılı giriş / Sistem çevrimiçi.
*   **Kırmızı:** Başarısız deneme / Kritik uyarı.
*   **Mavi:** Admin yetkileri / Genel aksiyonlar.

---

## 👥 Katkıda Bulunanlar

*   **Proje Geliştiricisi:** Hamza
*   **Teknolojiler:** ESP32, Node.js, React, Swift

---

*Bu proje Mikroişlemciler dersi kapsamında bir prototip olarak geliştirilmiştir.*
