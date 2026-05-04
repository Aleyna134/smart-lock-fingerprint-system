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
#include <Preferences.h>

// ESP32 NVS depolama — sensörün bozuk flash'ını bypass eder
static Preferences fpStore;

// UpChar: Sensörün CharBuffer'ından veriyi ESP32'ye yükle
// Adafruit kütüphanesinin paket buffer'ı 64 byte — sensör 128+ byte paket gönderiyor.
// Bu yüzden Serial2'den doğrudan (raw) okuyoruz.
static bool upCharToESP32(Adafruit_Fingerprint* finger, uint8_t bufId, uint8_t* outBuf, uint16_t* outLen) {
    // UpChar komutunu gönder (kütüphanenin getModel() fonksiyonu)
    uint8_t p = finger->getModel();
    Serial.printf("[FP] UpChar ACK: %d\n", p);
    if (p != FINGERPRINT_OK) {
        delay(500);
        while (Serial2.available()) Serial2.read();
        return false;
    }

    // Ham UART okuma — R307 paket formatı:
    // [0xEF 0x01] [4-byte adres] [1-byte tip] [2-byte uzunluk] [veri...] [2-byte checksum]
    *outLen = 0;
    int pktCount = 0;
    unsigned long totalTimeout = millis() + 8000;

    while (millis() < totalTimeout && pktCount < 20) {
        // Header bul: 0xEF 0x01
        bool headerFound = false;
        unsigned long hdrTimeout = millis() + 3000;
        while (millis() < hdrTimeout) {
            if (!Serial2.available()) { delay(1); continue; }
            if (Serial2.read() != 0xEF) continue;
            // 0xEF bulundu, 0x01 bekle
            unsigned long t2 = millis() + 500;
            while (!Serial2.available() && millis() < t2) delay(1);
            if (!Serial2.available()) break;
            if (Serial2.read() == 0x01) { headerFound = true; break; }
        }
        if (!headerFound) break;

        // Adres (4 byte) — oku ve atla
        for (int i = 0; i < 4; i++) {
            unsigned long t = millis() + 500;
            while (!Serial2.available() && millis() < t) delay(1);
            if (Serial2.available()) Serial2.read();
        }

        // Tip (1 byte)
        unsigned long t = millis() + 500;
        while (!Serial2.available() && millis() < t) delay(1);
        uint8_t pktType = Serial2.available() ? Serial2.read() : 0xFF;

        // Uzunluk (2 byte) — checksum dahil
        t = millis() + 500;
        while (Serial2.available() < 2 && millis() < t) delay(1);
        uint8_t lenH = Serial2.available() ? Serial2.read() : 0;
        uint8_t lenL = Serial2.available() ? Serial2.read() : 0;
        uint16_t pktLen = ((uint16_t)lenH << 8) | lenL;
        uint16_t payloadLen = (pktLen > 2) ? (pktLen - 2) : 0;

        // Veri (payloadLen byte)
        for (uint16_t i = 0; i < payloadLen; i++) {
            t = millis() + 500;
            while (!Serial2.available() && millis() < t) delay(1);
            if (!Serial2.available()) break;
            uint8_t b = Serial2.read();
            if (*outLen < 1024) outBuf[(*outLen)++] = b;
        }

        // Checksum (2 byte) — oku ve atla
        for (int i = 0; i < 2; i++) {
            t = millis() + 500;
            while (!Serial2.available() && millis() < t) delay(1);
            if (Serial2.available()) Serial2.read();
        }

        pktCount++;
        Serial.printf("[FP] UpChar pkt %d: type=0x%02X, payload=%d, total=%d\n",
                      pktCount, pktType, payloadLen, *outLen);

        if (pktType == 0x08) break; // ENDDATAPACKET
    }

    // Kalan veriyi temizle
    delay(100);
    while (Serial2.available()) Serial2.read();

    Serial.printf("[FP] UpChar toplam: %d paket, %d byte\n", pktCount, *outLen);
    return (*outLen > 0);
}

// DownChar: ESP32'den sensörün CharBuffer'ına veri indir
// R307 DownChar komutu (0x09): host → sensör, data paketleri halinde
static bool downCharToSensor(Adafruit_Fingerprint* finger, uint8_t bufId, const uint8_t* data, uint16_t len) {
    uint8_t cmd[] = {0x09, bufId};
    Adafruit_Fingerprint_Packet pkt(FINGERPRINT_COMMANDPACKET, sizeof(cmd), cmd);
    finger->writeStructuredPacket(pkt);
    if (finger->getStructuredPacket(&pkt) != FINGERPRINT_OK) return false;
    if (pkt.data[0] != FINGERPRINT_OK) return false;

    // Veriyi 64-byte'lık parçalar halinde gönder (kütüphanenin paket boyutu 64)
    // Son paket ENDDATAPACKET, diğerleri DATAPACKET
    uint16_t offset = 0;
    while (offset < len) {
        uint16_t chunkLen = len - offset;
        if (chunkLen > 64) chunkLen = 64;
        uint8_t pktType = (offset + chunkLen >= len) ? FINGERPRINT_ENDDATAPACKET : FINGERPRINT_DATAPACKET;
        Adafruit_Fingerprint_Packet dataPkt(pktType, chunkLen, (uint8_t*)(data + offset));
        finger->writeStructuredPacket(dataPkt);
        offset += chunkLen;
        delay(5);
    }
    return true;
}

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

    // Adafruit Fingerprint nesnesini oluştur
    _finger = new Adafruit_Fingerprint(_serial);

    // Sensörün uyanması için bekliyoruz (ESP32 hızlı açılır, sensör gecikmeli)
    Serial.println("[FP] Sensorun acilmasi bekleniyor (2 sn)...");
    delay(2000);

    // --- BAUD RATE OTOMATIK TARAMA ---
    // Fabrikadan gelen R307/ZFM sensörler 9600 veya 57600 baud ile gelebilir.
    // Her iki hızı da deniyoruz.
    const uint32_t baudRates[] = {57600, 9600, 115200};
    const int baudCount = sizeof(baudRates) / sizeof(baudRates[0]);
    bool found = false;

    for (int i = 0; i < baudCount && !found; i++) {
        uint32_t baud = baudRates[i];
        Serial.printf("[FP] Deneniyor: %d baud...\n", baud);

        // Portu bu hızda başlat
        _serial->end();
        delay(100);
        _serial->begin(baud, SERIAL_8N1, _rxPin, _txPin);
        delay(200);

        // Adafruit kütüphanesini bu hızda başlat
        _finger->begin(baud);
        delay(300);

        // Buffer'ı temizle
        while (_serial->available()) _serial->read();

        // Şifreyi doğrula (bu komut sensörün cevap verip vermediğini test eder)
        if (_finger->verifyPassword()) {
            Serial.printf("[FP] BASARILI! Sensor %d baud hizinda BULUNDU.\n", baud);
            found = true;

            // --- GÜVENLİK SEVİYESİ AYARI ---
            _initialized = true;
            _finger->getParameters();
            // _finger->setSecurityLevel(1); // KLON SENSÖRLERDE ARAMAYI BOZABİLİR, KALDIRILDI!
            delay(100);

            // Buffer temizle
            while (_serial->available()) _serial->read();

            // Zaman aşımını artır (gürültülü hat için)
            _serial->setTimeout(1000);

            // Bilgileri yazdır
            _finger->getParameters();
            Serial.printf("[FP] Kapasite: %d | Guvenlik: %d\n",
                          _finger->capacity, _finger->security_level);
        } else {
            Serial.printf("[FP] %d baud: cevap yok.\n", baud);
            delay(200);
        }
    }

    if (!found) {
        Serial.println("[FP] HATA: Hicbir baud hizinda sensor bulunamadi!");
        Serial.println("[FP] Kontrol listesi:");
        Serial.println("[FP]  1. Sari(TX) -> GPIO16, Gri(RX) -> GPIO17 mi?");
        Serial.println("[FP]  2. Kirmizi -> 3.3V, Siyah -> GND mi?");
        Serial.println("[FP]  3. Kablo baglantilari gevşek olmali, soguk lehim?");
        Serial.printf( "[FP]  4. Kullanilan pinler: RX=%d TX=%d\n", _rxPin, _txPin);
        _initialized = false;
        return false;
    }

    return true;
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

    // ----- Adım 1: Görüntü al ve karakter dosyası oluştur -----
    int result = captureAndCreateCharFile(1);
    if (result != FINGERPRINT_OK) {
        Serial.println("[FP] HATA: Goruntu alinamadi");
        return false;
    }
    Serial.println("[FP] Goruntu alindi.");

    // ----- Adım 2: UpChar ile karakter dosyasını ESP32'ye çek -----
    // Sensörün flash'ı bozuk, bu yüzden veriyi ESP32 NVS'e kaydediyoruz.
    uint8_t charData[1024];
    uint16_t charLen = 0;
    if (!upCharToESP32(_finger, 0x01, charData, &charLen)) {
        Serial.println("[FP] HATA: UpChar basarisiz!");
        return false;
    }
    Serial.printf("[FP] UpChar OK - %d byte okundu\n", charLen);

    // ----- Adım 3: ESP32 NVS'e kaydet -----
    fpStore.begin("fingerprints", false);
    String key = "fp_" + String(id);
    fpStore.putBytes(key.c_str(), charData, charLen);
    // Kayıt sayısını güncelle
    int count = fpStore.getInt("count", 0);
    // ID daha önce yoksa sayacı artır
    String existKey = "ex_" + String(id);
    if (!fpStore.getBool(existKey.c_str(), false)) {
        fpStore.putInt("count", count + 1);
        fpStore.putBool(existKey.c_str(), true);
    }
    fpStore.end();

    Serial.printf("[FP] BASARILI! ID %d kaydedildi (%d byte NVS'e yazildi).\n", id, charLen);
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

    // Adım 1: Görüntü al (10 saniye bekle)
    int p = -1;
    unsigned long startTime = millis();
    Serial.println("[FP] Parmak bekleniyor (10 sn)...");
    
    while (p != FINGERPRINT_OK) {
        p = _finger->getImage();
        if (p == FINGERPRINT_NOFINGER) {
            if (millis() - startTime > 10000) {
                result.message = "Zaman asimi: Parmak algilanmadi";
                Serial.println("[FP] " + result.message);
                return result;
            }
            delay(100);
        } else if (p != FINGERPRINT_OK) {
            result.message = "Goruntu alma hatasi: " + errorToString(p);
            Serial.println("[FP] HATA: " + result.message);
            return result;
        }
    }
    Serial.println("[FP] [1/3] Goruntu alindi OK");

    // Adım 2: Yeni parmağın karakter dosyasını oluştur (CharBuffer2'ye)
    delay(50);
    p = _finger->image2Tz(2);
    if (p != FINGERPRINT_OK) { result.message = "image2Tz hatasi"; return result; }
    Serial.println("[FP] [2/3] Karakter dosyasi olusturuldu");

    // Adım 3: NVS'den kayıtlı verileri tek tek DownChar ile sensöre yükle, Match yap
    // Yeni parmak Buffer2'de. Kayıtlıyı Buffer1'e yüklüyoruz.
    // İkisi de karakter dosyası formatında → Match çalışır!
    Serial.println("[FP] [3/3] Eslestirme basliyor...");
    bool found = false;

    fpStore.begin("fingerprints", true);  // read-only

    for (int id = 1; id <= FP_MAX_TEMPLATES && !found; id++) {
        String existKey = "ex_" + String(id);
        if (!fpStore.getBool(existKey.c_str(), false)) continue;

        String key = "fp_" + String(id);
        uint8_t storedData[1024];
        size_t storedLen = fpStore.getBytes(key.c_str(), storedData, sizeof(storedData));
        if (storedLen == 0) continue;

        // DownChar: kayıtlı karakter dosyasını Buffer1'e yükle
        if (!downCharToSensor(_finger, 0x01, storedData, storedLen)) {
            Serial.printf("[FP]   ID %d DownChar basarisiz\n", id);
            continue;
        }

        // Match (0x03): Buffer1 (NVS'den yüklenen) vs Buffer2 (yeni parmak)
        uint8_t matchCmd[] = {0x03};
        Adafruit_Fingerprint_Packet matchPkt(FINGERPRINT_COMMANDPACKET, sizeof(matchCmd), matchCmd);
        _finger->writeStructuredPacket(matchPkt);

        if (_finger->getStructuredPacket(&matchPkt) != FINGERPRINT_OK) continue;
        if (matchPkt.type != FINGERPRINT_ACKPACKET) continue;

        Serial.printf("[FP]   ID %d -> Match: %d\n", id, matchPkt.data[0]);

        if (matchPkt.data[0] == FINGERPRINT_OK) {
            uint16_t score = ((uint16_t)matchPkt.data[1] << 8) | matchPkt.data[2];
            result.matched    = true;
            result.user_id    = id;
            result.confidence = score;
            found = true;
            Serial.printf("[FP] ✓ ID %d eslesti! Guven: %d\n", id, score);
        }
    }

    fpStore.end();

    if (!found) {
        result.message = "Eslesme bulunamadi";
        Serial.println("[FP] ✗ " + result.message);
    } else {
        result.message = "Parmak izi eslesti! ID: " + String(result.user_id)
                       + " | Guven: " + String(result.confidence);
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

    // Sensör flash'ını da temizle (bozuk olsa bile)
    _finger->emptyDatabase();

    // ESP32 NVS'i temizle (asıl verilerimiz burada)
    fpStore.begin("fingerprints", false);
    fpStore.clear();
    fpStore.end();

    Serial.println("[FP] Tum kayitlar silindi (NVS + Sensor).");
    return true;
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
