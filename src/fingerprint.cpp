/**
 * @file fingerprint.cpp
 * @brief FingerprintManager sınıfının implementasyonu
 *
 * R307/ZFM-20 parmak izi sensörünü Adafruit kütüphanesi üzerinden
 * kontrol eden fonksiyonların gerçekleştirimi.
 *
 * MOCK_MODE tanımlıysa sensör yerine sahte veriler döndürülür,
 * böylece donanım olmadan firmware test edilebilir.
 *
 * @author Kişi 2 - Parmak İzi Firmware
 * @date 2026
 */

#include "fingerprint.h"

// ============================================================================
// Mock Mod Yardımcıları
// ============================================================================
#ifdef MOCK_MODE

// Mock modda sahte sonuçlar üretmek için sayaç
static int _mockCallCount = 0;

/**
 * @brief Mock doğrulama sonucu üretir
 *
 * Her çağrıda dönüşümlü olarak başarılı/başarısız sonuç döndürür.
 * Başarılı durumda sahte bir kullanıcı ID'si ve güven skoru verir.
 */
static FingerprintResult mockVerify() {
    _mockCallCount++;
    FingerprintResult result;

    // Çift sayılarda eşleşme başarılı, tek sayılarda başarısız
    if (_mockCallCount % 2 == 0) {
        result.matched    = true;
        result.user_id    = (_mockCallCount / 2) % 5 + 1;  // 1-5 arası ID döndür
        result.confidence = 180 + (_mockCallCount * 7) % 120; // 180-300 arası skor
        result.message    = "[MOCK] Parmak izi eslesti - Kullanici ID: " + String(result.user_id);
    } else {
        result.matched    = false;
        result.user_id    = 0;
        result.confidence = 0;
        result.message    = "[MOCK] Parmak izi eslesmedi";
    }

    return result;
}

/**
 * @brief Mock sensör bilgisi üretir
 */
static SensorInfo mockSensorData() {
    SensorInfo info;
    info.connected      = true;
    info.template_count = 3;       // Sahte: 3 kayıtlı parmak izi
    info.capacity       = FP_MAX_TEMPLATES;
    info.security_level = 3;
    info.status_message = "[MOCK] Sensor bagli (sanal)";
    return info;
}

#endif // MOCK_MODE

// ============================================================================
// Constructor & Destructor
// ============================================================================

FingerprintManager::FingerprintManager()
    :
#ifndef MOCK_MODE
      _finger(nullptr),
#endif
      _serial(nullptr),
      _initialized(false),
      _rxPin(FP_RX_PIN),
      _txPin(FP_TX_PIN)
{
}

FingerprintManager::~FingerprintManager() {
#ifndef MOCK_MODE
    if (_finger) {
        delete _finger;
        _finger = nullptr;
    }
#endif
}

// ============================================================================
// init() - Sensörü Başlatma
// ============================================================================

/**
 * @brief Sensörü başlatır, UART pinlerini ayarlar ve bağlantıyı doğrular
 *
 * ESP32 HardwareSerial Kullanımı:
 *   ESP32'de 3 adet HardwareSerial var: Serial (USB), Serial1, Serial2.
 *   Parmak izi sensörü için Serial2 önerilir:
 *
 *     FingerprintManager fp;
 *     fp.init(&Serial2, 16, 17);  // RX=GPIO16, TX=GPIO17
 *
 *   Pin numaraları değiştirilebilir, ESP32 herhangi bir GPIO'yu
 *   UART olarak kullanabilir.
 *
 * @param serial HardwareSerial pointer'ı (örn: &Serial2)
 * @param rxPin  RX pin numarası
 * @param txPin  TX pin numarası
 * @return true: sensör bağlı ve hazır
 */
bool FingerprintManager::init(HardwareSerial* serial, int rxPin, int txPin) {
    _serial = serial;
    _rxPin  = rxPin;
    _txPin  = txPin;

#ifdef MOCK_MODE
    // ---- MOCK MOD: Sensöre bağlanmadan başarılı döndür ----
    Serial.println("[MOCK] FingerprintManager baslatildi (sanal mod)");
    _initialized = true;
    return true;
#else
    // ---- GERÇEK MOD: UART'ı başlat ve sensörü bul ----

    // İŞLETİM SİSTEMİ BAĞIMSIZ BAŞLATMA (HardwareSerial):
    // Ekip arkadaşlarının Windows (COM3) veya Linux (/dev/ttyUSB0) kullanması KESİNLİKLE FARK ETMEZ.
    // İşletim sistemine özgü port isimleri ("COM3", "/dev/tty...") KULLANMIYORUZ.
    // Doğrudan ESP32'nin donanımsal UART pinlerini (GPIO) argüman olarak vererek başlatıyoruz.
    // Böylece kod herkesin bilgisayarında ve işletim sisteminde sıfır çakışmayla çalışır.
    _serial->begin(FP_BAUD_RATE, SERIAL_8N1, _rxPin, _txPin);

    // Adafruit Fingerprint nesnesini oluştur
    _finger = new Adafruit_Fingerprint(_serial);

    // Sensör ile el sıkışma (handshake)
    _finger->begin(FP_BAUD_RATE);

    // Sensör doğrulama denemesi
    if (_finger->verifyPassword()) {
        Serial.println("[FP] Parmak izi sensoru bulundu ve dogrulandi.");
        _initialized = true;

        // Sensör parametrelerini oku
        _finger->getParameters();
        Serial.printf("[FP] Kapasite: %d | Guvenlik: %d\n",
                      _finger->capacity, _finger->security_level);

        return true;
    } else {
        Serial.println("[FP] HATA: Parmak izi sensoru bulunamadi!");
        Serial.println("[FP] Kablo baglantilarini kontrol edin:");
        Serial.printf("[FP]   RX Pin: %d, TX Pin: %d, Baud: %d\n",
                      _rxPin, _txPin, FP_BAUD_RATE);
        _initialized = false;
        return false;
    }
#endif
}

// ============================================================================
// enrollFingerprint() - Yeni Parmak İzi Kayıt
// ============================================================================

/**
 * @brief Yeni parmak izi kayıt işlemi
 *
 * İşlem adımları:
 *   1) Kullanıcıdan parmağını koymasını iste → görüntü al → slot 1'e kaydet
 *   2) Parmağı kaldır
 *   3) Aynı parmağı tekrar koy → görüntü al → slot 2'ye kaydet
 *   4) İki şablonu karşılaştırıp model oluştur
 *   5) Modeli verilen ID ile sensör belleğine kaydet
 *
 * @param id Kayıt edilecek ID (1 - 127)
 * @return true: kayıt başarılı
 */
bool FingerprintManager::enrollFingerprint(int id) {
    if (id < 1 || id > FP_MAX_TEMPLATES) {
        Serial.printf("[FP] HATA: Gecersiz ID (%d). Aralik: 1-%d\n", id, FP_MAX_TEMPLATES);
        return false;
    }

#ifdef MOCK_MODE
    // ---- MOCK MOD ----
    Serial.printf("[MOCK] Parmak izi kaydedildi - ID: %d\n", id);
    return true;
#else
    // ---- GERÇEK MOD ----
    if (!_initialized) {
        Serial.println("[FP] HATA: Sensor baslatilmamis! Once init() cagirin.");
        return false;
    }

    Serial.printf("[FP] Kayit baslatiliyor - ID: %d\n", id);
    Serial.println("[FP] Lutfen parmaGInIzI sensore koyun...");

    // ----- Adım 1: İlk görüntüyü al -----
    int result = captureAndCreateCharFile(1);
    if (result != FINGERPRINT_OK) {
        Serial.println("[FP] HATA: Ilk goruntu alinamadi: " + errorToString(result));
        return false;
    }
    Serial.println("[FP] Ilk goruntu alindi. Lutfen parmaGInIzI kaldirin...");
    delay(2000);

    // Parmağın kaldırılmasını bekle
    while (_finger->getImage() != FINGERPRINT_NOFINGER) {
        delay(100);
    }

    // ----- Adım 2: İkinci görüntüyü al -----
    Serial.println("[FP] Ayni parmaGI tekrar koyun...");
    result = captureAndCreateCharFile(2);
    if (result != FINGERPRINT_OK) {
        Serial.println("[FP] HATA: Ikinci goruntu alinamadi: " + errorToString(result));
        return false;
    }

    // ----- Adım 3: İki şablondan model oluştur -----
    result = _finger->createModel();
    if (result != FINGERPRINT_OK) {
        Serial.println("[FP] HATA: Model olusturulamadi (parmaklar uyusmadi): "
                       + errorToString(result));
        return false;
    }

    // ----- Adım 4: Modeli sensör belleğine kaydet -----
    result = _finger->storeModel(id);
    if (result != FINGERPRINT_OK) {
        Serial.println("[FP] HATA: Model kaydedilemedi: " + errorToString(result));
        return false;
    }

    Serial.printf("[FP] BASARILI! Parmak izi ID %d olarak kaydedildi.\n", id);
    return true;
#endif
}

// ============================================================================
// verifyFingerprint() - Parmak İzi Doğrulama
// ============================================================================

/**
 * @brief Parmak izi okuyup veritabanında arar
 *
 * Islem akisi:
 *   1) Sensorden goruntu al
 *   2) Goruntugu sablona donustur
 *   3) Kayitli sablonlarla karsilastir (1:N arama)
 *   4) Sonucu FingerprintResult struct'i olarak dondur
 *
 * Kilit Mantigi modulu bu fonksiyonun donus degerini kullanir:
 *   FingerprintResult r = fpManager.verifyFingerprint();
 *   if (r.matched) { kilidiAc(); }
 *
 * @return FingerprintResult - matched, user_id, confidence, message alanlari
 */
FingerprintResult FingerprintManager::verifyFingerprint() {
    FingerprintResult result;
    result.matched    = false;
    result.user_id    = 0;
    result.confidence = 0;

#ifdef MOCK_MODE
    // ---- MOCK MOD: Sahte sonuç döndür ----
    result = mockVerify();
    Serial.println(result.message);
    return result;
#else
    // ---- GERÇEK MOD ----
    if (!_initialized) {
        result.message = "Sensor baslatilmamis!";
        return result;
    }

    // Adım 1: Görüntü al
    int p = _finger->getImage();
    if (p == FINGERPRINT_NOFINGER) {
        result.message = "Parmak algilanmadi";
        return result;
    }
    if (p != FINGERPRINT_OK) {
        result.message = "Goruntu alma hatasi: " + errorToString(p);
        return result;
    }

    // Adım 2: Görüntüyü şablon dosyasına dönüştür
    p = _finger->image2Tz();
    if (p != FINGERPRINT_OK) {
        result.message = "Sablon olusturma hatasi: " + errorToString(p);
        return result;
    }

    // Adım 3: Veritabanında ara (1:N fast search)
    p = _finger->fingerSearch();
    if (p == FINGERPRINT_OK) {
        // Eşleşme bulundu!
        result.matched    = true;
        result.user_id    = _finger->fingerID;
        result.confidence = _finger->confidence;
        result.message    = "Parmak izi eslesti - ID: " + String(result.user_id)
                          + " (Guven: " + String(result.confidence) + ")";
        Serial.println("[FP] " + result.message);
    } else if (p == FINGERPRINT_NOTFOUND) {
        result.message = "Parmak izi kayitli degil";
        Serial.println("[FP] Eslesme bulunamadi.");
    } else {
        result.message = "Arama hatasi: " + errorToString(p);
        Serial.println("[FP] " + result.message);
    }

    return result;
#endif
}

// ============================================================================
// deleteID() - Parmak İzi Silme
// ============================================================================

/**
 * @brief Belirli bir ID'deki parmak izi kaydını siler
 *
 * @param id Silinecek kayıt ID'si (1 - 127)
 * @return true: silme başarılı
 */
bool FingerprintManager::deleteID(int id) {
    if (id < 1 || id > FP_MAX_TEMPLATES) {
        Serial.printf("[FP] HATA: Gecersiz ID (%d)\n", id);
        return false;
    }

#ifdef MOCK_MODE
    Serial.printf("[MOCK] Parmak izi silindi - ID: %d\n", id);
    return true;
#else
    if (!_initialized) {
        Serial.println("[FP] HATA: Sensor baslatilmamis!");
        return false;
    }

    int result = _finger->deleteModel(id);
    if (result == FINGERPRINT_OK) {
        Serial.printf("[FP] ID %d basariyla silindi.\n", id);
        return true;
    } else {
        Serial.println("[FP] HATA: Silme basarisiz: " + errorToString(result));
        return false;
    }
#endif
}

// ============================================================================
// deleteAll() - Tüm Kayıtları Silme
// ============================================================================

/**
 * @brief Sensördeki tüm parmak izi kayıtlarını siler
 * @return true: başarılı
 */
bool FingerprintManager::deleteAll() {
#ifdef MOCK_MODE
    Serial.println("[MOCK] Tum parmak izleri silindi");
    return true;
#else
    if (!_initialized) {
        Serial.println("[FP] HATA: Sensor baslatilmamis!");
        return false;
    }

    int result = _finger->emptyDatabase();
    if (result == FINGERPRINT_OK) {
        Serial.println("[FP] Tum kayitlar silindi.");
        return true;
    } else {
        Serial.println("[FP] HATA: Toplu silme basarisiz: " + errorToString(result));
        return false;
    }
#endif
}

// ============================================================================
// getSensorData() - Sensör Durum Bilgisi
// ============================================================================

/**
 * @brief Sensörden ham durum bilgisi okur
 *
 * Bağlantı durumu, kayıtlı şablon sayısı, toplam kapasite
 * ve güvenlik seviyesi gibi bilgileri döndürür.
 *
 * @return SensorInfo yapısı
 */
SensorInfo FingerprintManager::getSensorData() {
#ifdef MOCK_MODE
    return mockSensorData();
#else
    SensorInfo info;

    if (!_initialized) {
        info.connected      = false;
        info.template_count = 0;
        info.capacity       = 0;
        info.security_level = 0;
        info.status_message = "Sensor baslatilmamis";
        return info;
    }

    // Sensör parametrelerini güncelle
    _finger->getTemplateCount();
    _finger->getParameters();

    info.connected      = true;
    info.template_count = _finger->templateCount;
    info.capacity       = _finger->capacity;
    info.security_level = _finger->security_level;
    info.status_message = "Sensor bagli ve calisir durumda";

    return info;
#endif
}

// ============================================================================
// isReady() - Hazırlık Durumu
// ============================================================================

/**
 * @brief Sensörün başlatılmış ve hazır olup olmadığını döndürür
 */
bool FingerprintManager::isReady() const {
    return _initialized;
}

// ============================================================================
// getStoredCount() - Kayıtlı Parmak İzi Sayısı
// ============================================================================

/**
 * @brief Sensörde kayıtlı parmak izi sayısını döndürür
 */
int FingerprintManager::getStoredCount() {
#ifdef MOCK_MODE
    return 3; // Sahte değer
#else
    if (!_initialized) return -1;
    _finger->getTemplateCount();
    return _finger->templateCount;
#endif
}

// ============================================================================
// Dahili Yardımcı Fonksiyonlar (Private)
// ============================================================================

/**
 * @brief Parmak izi görüntüsü alır ve karakter dosyası oluşturur
 *
 * Kayıt (enroll) sırasında iki kez çağrılır: slot 1 ve slot 2.
 * Kullanıcının parmağını koymasını bekler.
 *
 * @param slot Karakter dosyası slot numarası (1 veya 2)
 * @return FINGERPRINT_OK başarılı, diğer değerler hata
 */
int FingerprintManager::captureAndCreateCharFile(int slot) {
#ifdef MOCK_MODE
    return 0; // FINGERPRINT_OK = 0
#else
    int p = -1;

    // Parmağın koyulmasını bekle (5 saniye timeout)
    unsigned long startTime = millis();
    while (p != FINGERPRINT_OK) {
        p = _finger->getImage();
        if (p == FINGERPRINT_NOFINGER) {
            // Henüz parmak yok, beklemeye devam
            if (millis() - startTime > 5000) {
                Serial.println("[FP] Zaman asimi: Parmak algilanmadi");
                return FINGERPRINT_NOFINGER;
            }
            delay(100);
            continue;
        }
        if (p != FINGERPRINT_OK) {
            return p; // Hata varsa döndür
        }
    }

    // Görüntüyü karakter dosyasına dönüştür
    p = _finger->image2Tz(slot);
    return p;
#endif
}

/**
 * @brief Adafruit hata kodlarını okunabilir Türkçe mesaja çevirir
 * @param code Adafruit kütüphanesinden dönen hata kodu
 * @return Hata açıklaması
 */
String FingerprintManager::errorToString(int code) {
#ifdef MOCK_MODE
    return "[MOCK] Hata kodu: " + String(code);
#else
    switch (code) {
        case FINGERPRINT_OK:             return "Basarili";
        case FINGERPRINT_PACKETRECIEVEERR: return "Iletisim hatasi";
        case FINGERPRINT_NOFINGER:       return "Parmak algilanmadi";
        case FINGERPRINT_IMAGEFAIL:      return "Goruntu alinamadi";
        case FINGERPRINT_IMAGEMESS:      return "Goruntu cok karisik";
        case FINGERPRINT_FEATUREFAIL:    return "Ozellik cikarilmadi";
        case FINGERPRINT_NOMATCH:        return "Parmak izleri uyusmadi";
        case FINGERPRINT_NOTFOUND:       return "Eslesme bulunamadi";
        case FINGERPRINT_ENROLLMISMATCH: return "Kayit eslesmesi basarisiz";
        case FINGERPRINT_BADLOCATION:    return "Gecersiz kayit konumu";
        case FINGERPRINT_DBRANGEFAIL:    return "Veritabani aralik hatasi";
        case FINGERPRINT_FLASHERR:       return "Flash yazma hatasi";
        default:                         return "Bilinmeyen hata: " + String(code);
    }
#endif
}
