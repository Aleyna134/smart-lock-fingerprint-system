# 📱 Smart Lock iOS Application — Teknik Dokümantasyon

Bu doküman, Akıllı Parmak İzi Kilidi Sistemi'nin iOS istemcisinin mimarisini, kullanılan teknolojileri ve özelliklerini detaylandırmaktadır. Uygulama, modern Swift pratikleri ve Apple'ın en yeni framework'leri kullanılarak geliştirilmiştir.

---

## 🏛️ Mimari ve State Yönetimi

Uygulama, **MVVM (Model-View-ViewModel)** mimari deseni üzerine inşa edilmiştir.

*   **SwiftUI:** Deklaratif kullanıcı arayüzü ile reaktif ve akıcı bir deneyim sağlanır.
*   **Observation Framework:** Swift 5.9+ ile gelen `@Observable` makrosu kullanılarak, geleneksel `ObservableObject` yerine daha performanslı ve modern bir state yönetimi uygulanmıştır.
*   **Dependency Injection:** `SessionManager` gibi merkezi servisler, `@Environment` aracılığıyla tüm görünümlere enjekte edilir.

---

## ✨ Öne Çıkan Özellikler

### 1. Canlı Etkinlikler (Live Activities) & Dynamic Island
*   **WidgetKit:** Kapıdaki başarısız giriş denemeleri veya kritik güvenlik olayları, iPhone kilit ekranında ve Dynamic Island'da gerçek zamanlı olarak izlenebilir.
*   **Push-to-Start:** Backend'den gelen özel bir push bildirimi ile Live Activity uzaktan başlatılabilir.

### 2. Gelişmiş Bildirim Sistemi (APNs)
*   **Anlık Uyarılar:** Kapıda yetkisiz bir erişim denemesi olduğunda kullanıcıya özel alarm sesi (`alert.wav`) ile bildirim gider.
*   **Notification Service Extension:** Gelen bildirimlerin içeriği cihaz tarafında işlenerek zenginleştirilir.
*   **Deep Linking:** Bildirime tıklandığında kullanıcı doğrudan ilgili uyarının (Alert) detay ekranına yönlendirilir.

### 3. Biyometrik ve Güvenli Oturum
*   **Keychain Integration:** Kullanıcı oturum bilgileri Apple'ın güvenli depolama birimi olan Keychain'de şifreli olarak saklanır; uygulama silinip yüklense bile (isteğe bağlı) oturum korunabilir.
*   **Biometric Access Control:** Uygulama açılışında FaceID/TouchID desteği için altyapı mevcuttur.

---

## 🛠️ Teknik Bileşenler

### UI/UX Tasarım Dili
*   **Semantik Renkler:** Light/Dark mode desteği için sistem renkleri kullanılmıştır.
*   **SF Symbols:** Tüm ikonlar Apple'ın standart kütüphanesinden seçilerek görsel tutarlılık sağlanmıştır.
*   **Animasyonlar:** SwiftUI'ın `spring()` ve `easeOut` animasyonları ile interaktif geçişler (örn: Yanlış girişte sallanma efekti - `ShakeEffect`) optimize edilmiştir.

### Network Katmanı
*   **Async/Await:** Tüm ağ istekleri Swift'in modern concurrency yapısı ile yönetilir.
*   **URLSession:** Üçüncü taraf kütüphane (Alamofire vb.) kullanılmadan, native ve hafif bir network katmanı oluşturulmuştur.

---

## 📡 Backend Haberleşme Protokolü

| Endpoint | Method | Amaç |
| :--- | :--- | :--- |
| `/api/login` | POST | Kullanıcı girişi ve session oluşturma. |
| `/api/status` | GET | Kilidin anlık durumunu Dashboard'da gösterme. |
| `/api/logs` | GET | Erişim tarihçesini kronolojik listeleme. |
| `/api/device-token` | POST | APNs token kaydı (Bildirimler için). |
| `/api/alerts/read-all` | PATCH | Tüm güvenlik uyarılarını okundu işaretleme. |

---

## 📦 Kurulum ve Gereksinimler

1.  **Xcode:** 15.0+ sürümü gereklidir.
2.  **iOS Sürümü:** Target iOS 17.0+ olarak belirlenmiştir.
3.  **Konfigürasyon:** `NetworkConfig.swift` dosyasındaki `baseURL` değerini backend sunucu adresinizle güncelleyin.
4.  **Push Bildirimleri:** Push bildirimlerini test etmek için bir **Apple Developer Program** üyeliği ve fiziksel bir iOS cihazı gereklidir.

---

*Bu mobil uygulama, Smart Lock ekosisteminin son kullanıcıya temas eden en kritik ayağını oluşturmaktadır.*
