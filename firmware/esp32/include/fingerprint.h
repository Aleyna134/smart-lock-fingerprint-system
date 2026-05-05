/**
 * @file fingerprint.h
 * @brief Parmak İzi Sensör Yönetim Modülü (R307/ZFM-20)
 *
 * Bu dosya, FingerprintManager sınıfının tanımını içerir.
 * Adafruit Fingerprint Sensor kütüphanesini kullanarak ESP32 üzerinden
 * UART haberleşmesi ile R307/ZFM-20 parmak izi sensörünü kontrol eder.
 *
 * Derleyici bayrağı: -DMOCK_MODE ile sensör bağlı olmadan test edilebilir.
 *
 * @author Kişi 2 - Parmak İzi Firmware
 * @date 2026
 */

#ifndef FINGERPRINT_H
#define FINGERPRINT_H

#include <Arduino.h>

// Mock mod aktif değilse gerçek kütüphaneyi dahil et
#ifndef MOCK_MODE
#include <Adafruit_Fingerprint.h>
#endif

// ============================================================================
// Sabitler
// ============================================================================

/** Parmak izi sensörü varsayılan baud rate */
#define FP_BAUD_RATE 57600

/** Kayıt edilebilecek maksimum parmak izi sayısı (sensöre bağlıdır) */
#define FP_MAX_TEMPLATES 127

/** ESP32 DevKit pinleri - Serial2 (UART2) kullanılır */
#define FP_RX_PIN 16
#define FP_TX_PIN 17

// ============================================================================
// Veri Yapıları
// ============================================================================

/**
 * @brief Parmak izi doğrulama sonuç yapısı
 *
 * Kilit Mantığı modülü bu struct'ı kullanarak
 * doğrulama sonucuna ve kullanıcı bilgisine erişir.
 *
 * Örnek kullanım:
 *   FingerprintResult result = fpManager.verifyFingerprint();
 *   if (result.matched) {
 *       Serial.printf("Kullanıcı %d doğrulandı! (Güven: %d)\n",
 *                      result.user_id, result.confidence);
 *   }
 */
struct FingerprintResult {
    bool matched;       ///< Eşleşme bulundu mu?
    int user_id;        ///< Eşleşen kullanıcının kayıtlı ID'si (0 = eşleşme yok)
    int confidence;     ///< Eşleşme güven skoru (0-300 arası, yüksek = daha iyi)
    String message;     ///< Durum / hata mesajı (debug için)
};

/**
 * @brief Sensör durum bilgisi yapısı
 *
 * getSensorData() fonksiyonunun döndürdüğü ham sensör verileri.
 */
struct SensorInfo {
    bool connected;         ///< Sensör bağlı mı?
    int template_count;     ///< Kayıtlı parmak izi sayısı
    int capacity;           ///< Toplam kayıt kapasitesi
    int security_level;     ///< Sensör güvenlik seviyesi (1-5)
    String status_message;  ///< Durum açıklaması
};

// ============================================================================
// FingerprintManager Sınıfı
// ============================================================================

/**
 * @brief R307/ZFM-20 Parmak İzi Sensör Yönetim Sınıfı
 *
 * Bu sınıf, sensörle ilgili tüm işlemleri kapsüller (encapsulate).
 * UART üzerinden ESP32 HardwareSerial kullanarak sensörle iletişim kurar.
 *
 * Kullanım:
 * @code
 *   FingerprintManager fpManager;
 *   fpManager.init(&Serial2, FP_RX_PIN, FP_TX_PIN);
 *
 *   // Doğrulama
 *   FingerprintResult result = fpManager.verifyFingerprint();
 *
 *   // Kayıt
 *   bool ok = fpManager.enrollFingerprint(1);
 *
 *   // Silme
 *   bool deleted = fpManager.deleteID(1);
 * @endcode
 */
class FingerprintManager {
public:
    FingerprintManager();
    ~FingerprintManager();

    // ---- Başlatma ----

    /**
     * @brief Sensörü başlatır ve UART bağlantısını kurar
     * @param serial HardwareSerial portu (ESP32'de &Serial2 önerilir)
     * @param rxPin UART RX pini (varsayılan: 16)
     * @param txPin UART TX pini (varsayılan: 17)
     * @return true: sensör bulundu ve hazır, false: bağlantı hatası
     */
    bool init(HardwareSerial* serial, int rxPin = FP_RX_PIN, int txPin = FP_TX_PIN);

    // ---- Temel İşlemler ----

    /**
     * @brief Yeni parmak izi kaydı yapar
     *
     * Kullanıcıdan iki kez parmak basmasını ister, model oluşturur
     * ve sensörün flash belleğine kaydeder.
     *
     * @param id Kayıt edilecek parmak izi ID'si (1 - FP_MAX_TEMPLATES)
     * @return true: kayıt başarılı, false: kayıt başarısız
     */
    bool enrollFingerprint(int id);

    /**
     * @brief Parmak izi okuyup veritabanında arar
     *
     * Sensörden görüntü alır, şablon oluşturur ve kayıtlı
     * parmak izleriyle karşılaştırır.
     *
     * @return FingerprintResult yapısı: {matched, user_id, confidence, message}
     */
    FingerprintResult verifyFingerprint();

    /**
     * @brief Sensorde parmak olup olmadigini hizli kontrol eder.
     *
     * Bu fonksiyon sadece hafif bir getImage() kontrolu yapar. Parmak yoksa
     * false doner ve failed access log uretilmemelidir.
     */
    bool hasFinger();

    /**
     * @brief Belirli bir ID'deki parmak izini siler
     * @param id Silinecek parmak izi ID'si
     * @return true: silme başarılı, false: hata
     */
    bool deleteID(int id);

    /**
     * @brief Tüm kayıtlı parmak izlerini siler
     * @return true: başarılı, false: hata
     */
    bool deleteAll();

    /**
     * @brief Sensörden ham durum bilgisi okur
     * @return SensorInfo yapısı: bağlantı durumu, şablon sayısı, kapasite vb.
     */
    SensorInfo getSensorData();

    // ---- Yardımcı ----

    /**
     * @brief Sensörün bağlı ve hazır olup olmadığını döndürür
     */
    bool isReady() const;

    /**
     * @brief Kayıtlı parmak izi sayısını döndürür
     */
    int getStoredCount();

private:
    // Mock moddayken Adafruit nesnesi oluşturulmuyor
#ifndef MOCK_MODE
    Adafruit_Fingerprint* _finger;  ///< Adafruit kütüphane nesnesi
#endif

    HardwareSerial* _serial;        ///< Kullanılan UART portu
    bool _initialized;              ///< Sensör başlatıldı mı?
    int _rxPin;                     ///< UART RX pin numarası
    int _txPin;                     ///< UART TX pin numarası

    // ---- Dahili yardımcı fonksiyonlar ----

    /** Parmak izi görüntüsü al ve karakter dosyası oluştur */
    int captureAndCreateCharFile(int slot);

    /** Adafruit hata kodlarını okunabilir mesaja çevir */
    String errorToString(int code);
};

#endif // FINGERPRINT_H
