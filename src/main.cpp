#include <Arduino.h>
#include "fingerprint.h"
#include "lock.h"
#include "buzzer.h"
#include "display_manager.h"
#include "led_manager.h"

// Nesneler
FingerprintManager fpManager;
LockManager lockController;
BuzzerManager buzzer;
DisplayManager display;
LedManager leds;

bool autoTestActive = false; // Otomatik test başlangıçta KAPALI

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) delay(10);
    
    Serial.println("\n[SISTEM] Akilli Kilit Baslatiliyor...");
    Serial.flush();

    // 1. GPIO Modülleri Başlat
    leds.init();
    lockController.init();
    buzzer.init();
    buzzer.beepWelcome(); // Açılış sesi

    // 2. LCD Başlat (Wire.begin() display.init() içinde çağrılır)
    if(display.init()) {
        display.showMessage("Baslatiliyor...", "Lutfen Bekleyin");
    } else {
        Serial.println("[HATA] LCD baslatilamadi!");
    }

    // 3. I2C Tarayıcı (Wire zaten display.init() tarafından başlatıldı)
    Serial.println("[...] I2C cihazlari taraniyor...");
    for(byte address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        if (Wire.endTransmission() == 0) {
            Serial.printf("[I2C] Cihaz bulundu: 0x%02X\n", address);
        }
    }

    // 4. Parmak İzi Sensörü (Serial2: RX=16, TX=17)
    if(fpManager.init(&Serial2, FP_RX_PIN, FP_TX_PIN)) {
        Serial.println("[OK] Parmak izi sensoru hazir.");
    } else {
        Serial.println("[YOK] Sensor bulunamadi.");
    }

    display.showMessage("Sistem Hazir", "Parmak Okutun");
    Serial.println("\n[HAZIR] Komutlar:");
    Serial.println("  v = Parmak Dogrula   e = Yeni Kayit");
    Serial.println("  s = Durum Sorgula    c = Tum Kayitlari Sil");
    Serial.println("  o = Kapi Ac          k = Kapi Kilitle");
    Serial.println("  l = Donanim Testi    t = Otomatik Test");
}

void loop() {
    // 1. Manuel Komut Kontrolü (Serial)
    if (Serial.available()) {
        char cmd = Serial.read();

        // Yeni satır / boşluk karakterlerini atla
        if (cmd == '\n' || cmd == '\r' || cmd == ' ') return;

        switch (cmd) {
            case 'v': case 'V': {
                Serial.println("\n[>] Parmak okuma modu aktif.");
                if (!fpManager.isReady()) {
                    Serial.println("[HATA] Sensor hazir degil!");
                    display.showMessage("SENSOR HATASI", "BAGLANTI YOK");
                    break;
                }
                display.showMessage("Parmak Bekleniyor", "Lutfen Okutun");
                FingerprintResult result = fpManager.verifyFingerprint();
                
                if (result.matched) {
                    Serial.printf("[KAPI] ERISIM ONAYLANDI! (ID: %d, Guven: %d)\n", result.user_id, result.confidence);
                    display.showMessage("ERISIM ONAYLANDI", "Hosgeldiniz");
                    leds.success(); buzzer.beepSuccess();
                    lockController.unlock();
                    delay(3000);
                    lockController.lock(); leds.allOff();
                } else {
                    Serial.println("[KAPI] ERISIM REDDEDILDI! (Gecersiz Parmak)");
                    display.showMessage("ERISIM REDDEDILDI", "Gecersiz Parmak");
                    leds.error(); buzzer.beepError();
                    delay(2000); leds.allOff();
                }
                display.showMessage("Sistem Hazir", "Parmak Okutun");
                break;
            }
            case 'e': case 'E': {
                Serial.println("\n[>] Kayit modu aktif.");
                if (!fpManager.isReady()) {
                    Serial.println("[HATA] Sensor hazir degil!");
                    display.showMessage("SENSOR HATASI", "BAGLANTI YOK");
                    break;
                }
                
                // Sıradaki boş ID'yi bul
                // NOT: Bu yöntem yalnızca sıralı kayıt + toplu silme (c komutu) ile doğru çalışır.
                // Tekli silme (deleteID) kullanılırsa ID çakışması olabilir.
                int count = fpManager.getStoredCount();
                if (count < 0) {
                    Serial.println("[HATA] Kayit sayisi alinamadi!");
                    display.showMessage("SENSOR HATASI", "");
                    break;
                }
                if (count >= FP_MAX_TEMPLATES) {
                    Serial.println("[HATA] Kayit kapasitesi dolu!");
                    display.showMessage("KAPASITE DOLU", "Sil: 'c' komutu");
                    buzzer.beepError();
                    delay(2000);
                    display.showMessage("Sistem Hazir", "Parmak Okutun");
                    break;
                }
                int nextId = count + 1;
                Serial.printf("[>] Yeni kullanici ID: %d olarak belirlendi.\n", nextId);
                
                display.showMessage("YENI KAYIT", "Parmak Koyun");
                if (fpManager.enrollFingerprint(nextId)) {
                    display.showMessage("KAYIT BASARILI", "ID: " + String(nextId));
                    leds.success(); buzzer.beepSuccess();
                } else {
                    display.showMessage("KAYIT HATASI", "Tekrar Deneyin");
                    leds.error(); buzzer.beepError();
                }
                delay(2000); leds.allOff();
                display.showMessage("Sistem Hazir", "Parmak Okutun");
                break;
            }
            case 's': case 'S': {
                if (!fpManager.isReady()) {
                    Serial.println("[DURUM] Sensor: YOK");
                    display.showMessage("SENSOR: YOK", "BAGLAYIN");
                    break;
                }
                SensorInfo info = fpManager.getSensorData();
                Serial.printf("\n[DURUM] Sensor: %s | Kayitli: %d | Kapasite: %d | Guvenlik: %d\n", 
                             info.connected ? "BAGLI" : "YOK", info.template_count,
                             info.capacity, info.security_level);
                display.showMessage("Kayitli:" + String(info.template_count), "Guvenlik:" + String(info.security_level));
                delay(2000);
                display.showMessage("Sistem Hazir", "Parmak Okutun");
                break;
            }
            case 'l': case 'L': {
                Serial.println("\n[TEST] Donanim testi...");
                display.showMessage("TEST BASLADI", "LED & ROLE");
                leds.greenOn(); delay(500); leds.allOff();
                leds.error(); delay(500); leds.allOff();
                buzzer.beepSuccess(); delay(200);
                lockController.unlock();
                delay(1000);
                lockController.lock();
                display.showMessage("TEST TAMAM", "Sistem Hazir");
                delay(1000);
                display.showMessage("Sistem Hazir", "Parmak Okutun");
                break;
            }
            case 'o': case 'O': {
                Serial.println("\n[MANUEL] Kapi ACILIYOR...");
                display.showMessage("KAPI DURUMU:", "ACIK (MANUEL)");
                lockController.unlock();
                leds.greenOn();
                buzzer.beepSuccess();
                break;
            }
            case 'k': case 'K': {
                Serial.println("\n[MANUEL] Kapi KILITLENIYOR...");
                display.showMessage("KAPI DURUMU:", "KILITLI");
                lockController.lock();
                leds.allOff();
                delay(1500);
                display.showMessage("Sistem Hazir", "Parmak Okutun");
                break;
            }
            case 'c': case 'C': {
                Serial.println("\n[>] Tum parmak izi kayitlari siliniyor...");
                display.showMessage("SILIYOR...", "Lutfen Bekleyin");
                if (fpManager.deleteAll()) {
                    display.showMessage("TUMU SILINDI", "Sistem Sifirlandi");
                    Serial.println("[OK] Veritabani tertemiz.");
                    leds.success(); buzzer.beepWelcome(); delay(1000); leds.allOff();
                } else {
                    display.showMessage("SILME HATASI", "");
                    leds.error(); buzzer.beepError(); delay(1000); leds.allOff();
                }
                delay(1000);
                display.showMessage("Sistem Hazir", "Parmak Okutun");
                break;
            }
            case 't': case 'T': {
                autoTestActive = !autoTestActive;
                Serial.printf("\n[MOD] Otomatik Test: %s\n", autoTestActive ? "ACIK" : "KAPALI");
                display.showMessage("OTO TEST:", autoTestActive ? "ACIK" : "KAPALI");
                break;
            }
            default: {
                // Tanınmayan komutlar için yardım göster
                Serial.printf("[?] Bilinmeyen komut: '%c'\n", cmd);
                Serial.println("[?] Gecerli komutlar: v e s c o k l t");
                break;
            }
        }
    }

    // 2. OTOMATİK DÖNGÜ TESTİ (5 saniyede bir, millis ile)
    // Bu kısım kapının LOW/HIGH durumlarını periyodik test eder.
    static unsigned long lastTest = 0;
    static bool testState = false;
    
    // Sadece terminale 't' yazıldığında çalışır
    if (autoTestActive && (millis() - lastTest > 5000)) {
        testState = !testState;
        if (testState) {
            Serial.println("\n--- OTO TEST: ACILIYOR (Pin LOW) ---");
            display.showMessage("OTO: ACILIYOR", "Sinyal: LOW");
            lockController.unlock(); 
            leds.greenOn();
        } else {
            Serial.println("\n--- OTO TEST: KILITLENIYOR (Pin HIGH) ---");
            display.showMessage("OTO: KILITLI", "Sinyal: HIGH");
            lockController.lock();
            leds.allOff();
        }
        lastTest = millis();
    }

    // 3. Heartbeat
    static unsigned long hb = 0;
    if(millis() - hb > 15000) {
        Serial.println("[CANLI] Komut bekliyorum...");
        hb = millis();
    }
    delay(50);
}
