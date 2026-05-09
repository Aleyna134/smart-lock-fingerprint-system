# 📱 Smart Lock iOS Application — Deep Technical Overview

Bu doküman, Smart Lock ekosisteminin mobil ayağını oluşturan iOS uygulamasının yazılım mimarisini, veri akış protokollerini ve ileri seviye sistem entegrasyonlarını detaylandırmaktadır.

---

## 🛠️ Yazılım Mimarisi ve State Yönetimi

Uygulama, Apple'ın en güncel geliştirme standartları (iOS 17+) baz alınarak tasarlanmıştır.

### 1. Modern MVVM & Observation
*   **Reaktif Yapı:** `@Observable` makrosu (Swift 5.9+) kullanılarak verimli bir veri bağlama (Data Binding) sağlanmıştır. Bu sayede sadece değişen alanlar UI'da yeniden render edilir, bu da pil ve performans tasarrufu sağlar.
*   **Separation of Concerns:** View'lar sadece arayüz deklarasyonundan sorumludur; tüm iş mantığı (Business Logic) ViewModel katmanında soyutlanmıştır.

### 2. Dependency Injection (DI)
*   **SessionManager:** Kullanıcı oturumu, ağ durumu ve global uygulama ayarları `Environment` üzerinden tüm hiyerarşiye dağıtılır.
*   **Servis Katmanı:** API servisleri (`APIAuthService`, `APIUserService` vb.) protokol tabanlı (`Protocol-Oriented`) tasarlanmıştır, bu da kolay birim test (Unit Test) imkanı sağlar.

---

## 🚀 İleri Seviye Sistem Entegrasyonları

### 1. Live Activities & Dynamic Island Lifecycle
Uygulama, kapıdaki kritik olayları anlık olarak kilit ekranına taşır:
*   **Başlatma:** Backend'den gelen `push-to-start` token'ı ile uygulama kapalı olsa dahi Live Activity başlatılabilir.
*   **Güncelleme:** Kapı açıldığında veya kilitlendiğinde Dynamic Island içeriği asenkron olarak güncellenir.
*   **Sonlandırma:** Olay çözüldüğünde veya zaman aşımına uğradığında otomatik temizleme mekanizması çalışır.

### 2. Güvenlik ve Kimlik Doğrulama (Keychain)
*   **Güvenli Depolama:** Kullanıcı şifreleri veya token'ları `UserDefaults` yerine donanım seviyesinde şifrelenmiş **Keychain** içerisinde saklanır.
*   **Biyometrik Yerel Kilitleme:** Uygulama açılışında `LocalAuthentication` framework'ü ile cihaz sahibi doğrulaması yapılır.

### 3. Push Bildirimleri ve APNs (Apple Push Notification service)
*   **Özel Payload İşleme:** Gelen bildirimlerin içindeki `alert_id` ve `severity` bilgileri, `NotificationServiceExtension` tarafından okunur.
*   **Rich Notifications:** Bildirimler sadece metin değil, alarm sesleri (`alert.wav`) ve aksiyon butonları içerir.

---

## 🎨 UI/UX Bileşenleri ve Animasyonlar

*   **Custom View Components:**
    *   `StatusCard`: Gerçek zamanlı sensör verilerini gösteren cam (Glassmorphism) efektli kartlar.
    *   `AccessLogRow`: Morphing animasyonları ile açılan detaylı log satırları.
    *   `AvatarSystem`: İsimden hash üreterek kişiye özel deterministik renk atayan avatar motoru.
*   **Haptic Feedback:** Başarılı girişlerde `UINotificationFeedbackGenerator(.success)`, hatalarda `.error` ile fiziksel geri bildirim sağlanır.
*   **Motion Design:** SwiftUI `MatchedGeometryEffect` kullanılarak Dashboard ve Detay ekranları arasında akıcı geçişler sağlanmıştır.

---

## 📡 Detaylı API İletişim Şeması

Uygulama, backend ile JSON tabanlı RESTful protokol üzerinden haberleşir.

**Örnek Login Akışı:**
```swift
// Request Payload
struct LoginRequest: Encodable {
    let email: String
    let password: String
}

// Response Handling
if let user = try? JSONDecoder().decode(User.self, from: data) {
    self.sessionManager.login(user)
    self.notificationManager.registerDeviceToken()
}
```

---

## 📁 Klasör Yapısı (Project Structure)

```text
Microprocessors_Project_IOS/
├── Auth/               # Giriş, Kayıt, Şifre işlemleri
├── Main/               # TabBar ve Ana Navigasyon
├── Screens/            # Dashboard, Logs, Users, Alerts, Settings
├── Models/             # Veri yapıları (User, Log, Alert)
├── Services/           # API ve Push servisleri
├── LiveActivity/       # WidgetKit ve Dynamic Island kodları
└── Assets/             # İkonlar, Renkler ve Alarm sesleri
```

---

## 📦 Dağıtım ve Test
*   **TestFlight:** Uygulama beta testleri için hazır haldedir.
*   **CI/CD:** GitHub Actions üzerinden otomatik build altyapısı kurgulanabilir.

---

*Bu uygulama, endüstriyel standartlarda bir IoT kullanıcı deneyimi sunmak üzere optimize edilmiştir.*
