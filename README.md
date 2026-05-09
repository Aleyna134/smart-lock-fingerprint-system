# 🔒 Akıllı Parmak İzi & PIN Kilidi Sistemi (Exhaustive Technical Manual)

Bu döküman, projenin kaynak kodlarının (Gömülü Yazılım ve Backend) derinlemesine analiziyle hazırlanmış, sistemin tüm iç mantığını açıklayan teknik bir kılavuzdur. Bu dökümanı okuyan bir geliştirici, kaynak kodlara bakmadan tüm sistem akışını anlayabilir.

---

## 🏛️ 1. Gömülü Yazılım (ESP32 Firmware) Analizi

ESP32 yazılımı, `main.cpp` üzerinde koşan asenkron bir `event loop` mimarisine sahiptir. Donanım bileşenleri (Keypad, Parmak İzi, LCD) birbirinden bağımsız sınıflar (`Manager`) tarafından yönetilir.

### 🧬 1.1 Parmak İzi Yönetimi (`FingerprintManager`)
Sistemin en kritik parçasıdır. R307 sensörünün flash belleği bozuk olduğundan, veriler **ESP32 NVS (Non-Volatile Storage)** üzerinde saklanır.

*   **Veri Depolama Stratejisi:**
    *   Parmak izi şablonu (template) sensörden `UpChar` komutuyla 1024-byte'lık paketler halinde çekilir.
    *   Bu paketler ESP32'nin NVS alanına `fp_[ID]` anahtarıyla kaydedilir.
    *   **Doğrulama (Verification) Akışı:** 
        1. Yeni parmak sensöre konur ve geçici belleğe (Buffer 2) alınır.
        2. ESP32, NVS'deki tüm kayıtlı şablonları döngüye sokar.
        3. Her şablon `DownChar` ile sensörün Buffer 1'ine geri yüklenir.
        4. Sensör içinde `Match` (1:1) işlemi yapılır. Bu işlem, sensörün 1:N arama özelliğinin donanımsal arızasından dolayı yazılımsal olarak simüle edilmesidir.

### ⌨️ 1.2 Keypad ve PIN Güvenliği (`KeypadManager`)
4x4 matris keypad üzerinden 4-8 hane arası PIN girişi yönetilir.

*   **PIN Doğrulama Mantığı:**
    *   **Giriş:** `#` tuşu onayla, `C` iptal, `*` silme işlemini yapar.
    *   **Maskeleme:** LCD ekranında rakamlar 600ms görünüp sonra `*` karakterine dönüşür.
    *   **Lockout (Kilitleme) State Machine:**
        1. `_wrongAttempts` sayacı her hatalı girişte artar.
        2. Sayaç 3'e ulaştığında `_isLockedOut` true olur ve `_lockoutStart` zamanı kaydedilir.
        3. 30 saniye boyunca `verifyPin()` fonksiyonu doğrudan false döner ve donanım hiçbir girişi kabul etmez.

### 🌐 1.3 IoT Haberleşme Katmanı (`IotClient`)
HTTP/REST protokolü üzerinden backend ile konuşur.

*   **Offline Loglama (Buffering):** İnternet kesilirse loglar kaybolmaz. ESP32 NVS içinde `logbuf` alanına 50 adede kadar log kaydedilir. İnternet geldiğinde `flushBufferedLogs()` fonksiyonu bu logları sırayla backend'e basar.
*   **Polling Mekanizması:** Donanım her 5-10 saniyede bir `/api/hardware/commands/next` endpoint'ini kontrol eder. Bu, donanımın arkasında (NAT/Firewall) olduğu durumlarda backend'den komut alabilmesini sağlar.

---

## ⚙️ 2. Backend & API Analizi (`index.js`)

Node.js tabanlı backend, donanım ile kullanıcı arayüzü (Web/Mobil) arasında bir orkestratör görevi görür.

### 📝 2.1 Donanım Komut Kuyruğu (Hardware Command Queue)
Donanım üzerinde yapılan işlemler (Kayıt silme, yeni parmak ekleme) asenkron çalışır.

*   **Akış:**
    1. Kullanıcı isteği backend'e gelir → `HardwareCommand` tablosuna `PENDING` olarak yazılır.
    2. Donanım bu komutu `poll` eder → `CLAIMED` durumuna geçer.
    3. İşlem donanımda biter → Donanım `/result` endpoint'ine sonuç döner → Komut `DONE` veya `FAILED` olur.
    4. Bu yapı sayesinde donanım anlık offline olsa bile işlem kuyrukta bekler.

### 🔔 2.2 Bildirim ve Alert Sistemi
Backend, her log girişini analiz ederek kritiklik seviyesini belirler.

*   **Alert Türetme:** Eğer bir logda `success: false` ve `fail_count >= 3` ise otomatik olarak `CRITICAL` seviyesinde bir `Alert` nesnesi oluşturulur.
*   **Push Notification (APNs):** Yeni bir alert oluştuğunda backend, `DeviceToken` tablosundaki tüm aktif iOS cihazlara Apple Push Notification servisi üzerinden bildirim gönderir.
*   **Email Alert:** `CRITICAL` alertler, admin e-posta adresine SMTP üzerinden anında raporlanır.

---

## 🔗 3. Sistem Entegrasyon Akışları (Sequence Diagrams)

### 3.1 Yeni Kullanıcı Kayıt Akışı
1.  **Web:** `POST /api/users` (Admin).
2.  **Backend:** `Prisma.User.create` + `Prisma.HardwareCommand.create(ENROLL)`.
3.  **ESP32:** `IotClient::pollEnrollmentCommand()` → "Kayıt Modu" aktifleşir.
4.  **Hardware:** LCD: "Parmak Koyun" → Sensör parmağı NVS'ye kaydeder.
5.  **ESP32:** `IotClient::sendEnrollmentResult(success: true)`.
6.  **Backend:** `Prisma.User.update(status: ENROLLED)`.

### 3.2 Yetkisiz Erişim Akışı
1.  **Hardware:** Yanlış parmak okutuldu (Deneme 3).
2.  **ESP32:** `IotClient::sendAccessLog(success: false, fail_count: 3)`.
3.  **Backend:**
    *   `Log` tablosuna kaydet.
    *   `Alert` tablosuna `CRITICAL` uyarısı oluştur.
    *   `PushDelivery` kuyruğuna tüm adminleri ekle.
    *   SMTP üzerinden Admin'e "Güvenlik İhlali" maili at.

---

## 🛠️ 4. Veritabanı Modeli Detayları

*   **`User`**: Kimlik bilgilerini ve `enrollment_status` (Kayıt aşaması) bilgisini tutar.
*   **`HardwareCommand`**: Donanıma gönderilen görevleri ve bunların durumlarını (Pending/Claimed/Done) takip eder.
*   **`PushDelivery`**: Push bildirimlerinin başarılı olup olmadığını, kaç kez denendiğini ve hata loglarını saklar.
*   **`SystemState`**: Kilidin fiziksel durumunu (Locked/Unlocked) tutan tekil satırlık (Singleton) bir tablodur.

---

*Bu döküman, sistemin tüm katmanlarını teknik olarak şeffaf hale getirmek için kod bazlı analizle oluşturulmuştur.*
