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

void setup() {
    Serial.begin(115200);
    Serial.setTxTimeoutMs(0);
    while (!Serial && millis() < 5000) delay(10);
    
    Serial.println("\n[SISTEM] LCD Modu Baslatiliyor...");
    Serial.flush();

    // 1. Modülleri Başlat
    leds.init();
    lockController.init();
    buzzer.init();

    // 2. I2C Tarayıcı (LCD ve Sensör bulmak için)
    Serial.println("[...] I2C cihazlari taraniyor...");
    Wire.begin(I2C_SDA, I2C_SCL);
    for(byte address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        if (Wire.endTransmission() == 0) {
            Serial.printf("[I2C] Cihaz bulundu: 0x%02X\n", address);
        }
    }

    // 3. LCD Başlat
    if(display.init()) {
        display.showMessage("Sistem Hazir", "Parmak Okutun");
    } else {
        Serial.println("[HATA] LCD baslatilamadi!");
    }

    // 4. Parmak İzi Sensörü (Yeni pinler: RX=4, TX=7)
    if(fpManager.init(&Serial1, 4, 7)) {
        Serial.println("[OK] Parmak izi sensoru hazir.");
    } else {
        Serial.println("[YOK] Sensor bulunamadi.");
    }

    Serial.println("\n[HAZIR] Komut bekliyorum (v/e/s)...");
}

void loop() {
    if (Serial.available()) {
        char cmd = Serial.read();
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
                    display.showMessage("ERISIM ONAYLANDI", "Hosgeldiniz");
                    leds.success(); buzzer.beepSuccess();
                    lockController.unlock();
                    delay(3000);
                    lockController.lock(); leds.allOff();
                } else {
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
                display.showMessage("YENI KAYIT", "Parmak Koyun");
                if (fpManager.enrollFingerprint(1)) {
                    display.showMessage("KAYIT BASARILI", "ID: 1");
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
                Serial.printf("\n[DURUM] Sensor: %s | Kayitli: %d\n", 
                             info.connected ? "BAGLI" : "YOK", info.template_count);
                display.showMessage("KAYITLI SAYISI:", String(info.template_count));
                delay(2000);
                display.showMessage("Sistem Hazir", "Parmak Okutun");
                break;
            }
            case 'l': case 'L': {
                Serial.println("\n[TEST] Donanim testi...");
                display.showMessage("TEST BASLADI", "LED & ROLE");
                
                // LED'ler
                leds.greenOn(); delay(500); leds.allOff();
                leds.error(); delay(500); leds.allOff();
                
                // RÖLE TESTİ (1 saniye tetikle)
                Serial.println("-> ROLE TETIKLENDI (Tık sesi bekleniyor)");
                lockController.unlock(); // GPIO 1: HIGH
                delay(1000);
                lockController.lock();   // GPIO 1: LOW
                Serial.println("-> ROLE DURDU");

                display.showMessage("TEST TAMAM", "Sistem Hazir");
                break;
            }
            case 'o': case 'O': {
                Serial.println("\n[MANUEL] Kapi ACILIYOR...");
                display.showMessage("KAPI DURUMU:", "ACIK (MANUEL)");
                lockController.unlock();
                leds.greenOn();
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
        }
    }
    
    // Heartbeat
    static unsigned long hb = 0;
    if(millis() - hb > 15000) {
        Serial.println("[CANLI] Komut bekliyorum...");
        hb = millis();
    }
    delay(50);
}
