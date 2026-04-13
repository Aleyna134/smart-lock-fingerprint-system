#include <Arduino.h>
#include "fingerprint.h"
#include "lock.h"
#include "buzzer.h"
#include "display_manager.h"
#include "led_manager.h"

// ============================================================================
// Global Nesneler
// ============================================================================

FingerprintManager fpManager;
LockManager lockController;
BuzzerManager buzzer;
DisplayManager display;
LedManager leds;

// ============================================================================
// ESP32-C3 Pin Yapılandırması (GÜNCELLENDİ)
// ============================================================================
//
//   R307 Sensör Bağlantısı:
//   TX (Sarı)     -> GPIO 4 (RX1)
//   RX (Beyaz)    -> GPIO 5 (TX1)
//
//   Röle (Kilit):
//   Sinyal        -> GPIO 7
//
//   Buzzer:
//   Sinyal        -> GPIO 6
//
//   Ekran (OLED I2C):
//   SDA           -> GPIO 8 (C3 Default)
//   SCL           -> GPIO 9 (C3 Default)
//
//   LED'ler:
//   Yeşil         -> GPIO 10
//   Kırmızı       -> GPIO 3
//
// ----------------------------------------------------------------------------

void setup() {
    Serial.begin(115200);
    delay(2000); // C3 USB-CDC'nin oturması için biraz daha uzun bekleme

    Serial.println("=================================");
    Serial.println("  Akilli Kilit - Prototip V1.1");
    Serial.println("=================================");

#ifdef MOCK_MODE
    Serial.println("[!] MOCK MOD AKTIF - Sanal sensor kullaniliyor");
#endif

    // Modülleri Başlat
    lockController.init();
    buzzer.init();
    display.init();
    leds.init();
    
    display.showMessage("Sistem Basliyor...", "Lutfen Bekleyin");
    buzzer.beepWelcome();

    // Parmak izi sensörünü başlat (C3'te Serial1 kullanılır)
    bool ok = fpManager.init(&Serial1, 4, 5); 

    if (ok) {
        Serial.println("[OK] Sensor hazir!");
        SensorInfo info = fpManager.getSensorData();
        display.showMessage("Sensor Hazir", "Parmak Okutun");
        leds.greenOn(); delay(200); leds.allOff(); // Başlangıç testi
    } else {
        Serial.println("[HATA] Sensor baslatilamadi!");
        display.showMessage("HATA!", "Sensor Yok");
        leds.error();
        buzzer.beepError();
        while (true) { delay(1000); }
    }

    Serial.println("\nKomutlar: 'v' (Dogrula), 'e' (Kayit), 'd' (Sil), 's' (Durum)");
    Serial.println("=================================\n");
}

void loop() {
    if (Serial.available()) {
        char cmd = Serial.read();

        switch (cmd) {
            case 'v':
            case 'V': {
                Serial.println("\n[>] Parmak izinizi okutun...");
                display.showMessage("Parmak Bekleniyor", "Lutfen Okutun");
                FingerprintResult result = fpManager.verifyFingerprint();

                if (result.matched) {
                    Serial.printf("[OK] Kullanici %d tanindi.\n", result.user_id);
                    display.showMessage("ERISIM ONAYLANDI", "Hosgeldiniz ID:" + String(result.user_id));
                    leds.success();
                    buzzer.beepSuccess();
                    lockController.unlock();
                    delay(3500); // 3.5 saniye açık tut
                    lockController.lock();
                    leds.allOff();
                    display.showMessage("Kilitlendi", "Parmak Okutun");
                } else {
                    Serial.println("[HATA] Parmak izi gecersiz!");
                    display.showMessage("ERISIM REDDEDILDI", "Gecersiz Parmak");
                    leds.error();
                    buzzer.beepError();
                    delay(2000);
                    leds.allOff();
                    display.showMessage("Parmak Okutun");
                }
                break;
            }

            case 'e':
            case 'E': {
                Serial.println("\n[>] Yeni Kayit - Lutfen parmak izinizi 2 kez okutun.");
                display.showMessage("YENI KAYIT", "Parmak Bekleniyor");
                bool ok = fpManager.enrollFingerprint(1); 
                if (ok) {
                    display.showMessage("KAYIT BASARILI", "ID: 1");
                    leds.success();
                    buzzer.beepSuccess();
                } else {
                    display.showMessage("KAYIT HATASI", "Tekrar Deneyin");
                    leds.error();
                    buzzer.beepError();
                }
                delay(2000);
                leds.allOff();
                display.showMessage("Parmak Okutun");
                break;
            }

            case 'd':
            case 'D': {
                Serial.println("\n[>] ID 1 siliniyor...");
                bool ok = fpManager.deleteID(1);
                if (ok) buzzer.beepSuccess(); else buzzer.beepError();
                break;
            }

            case 's':
            case 'S': {
                SensorInfo info = fpManager.getSensorData();
                Serial.println("\n--- Sistem Durumu ---");
                Serial.printf("  Sensor:   %s\n", info.connected ? "BAGLI" : "HATA");
                Serial.printf("  Kayitli:  %d\n", info.template_count);
                Serial.printf("  Kilit:    %s\n", lockController.isLocked() ? "KILITLI" : "ACIK");
                Serial.println("----------------------\n");
                break;
            }
        }
    }
    delay(50);
}

