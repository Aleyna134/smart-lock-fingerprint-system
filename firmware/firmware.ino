#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>
#include "lock_logic.h"

// =====================================================
// PINLER
// Belgedeki tabloya göre:
// R307 TX -> GPIO16 (ESP RX)
// R307 RX -> GPIO17 (ESP TX)
// Solenoid/Relay -> GPIO26
// Red LED -> GPIO25
// Green LED -> GPIO27
// Buzzer -> GPIO14
// =====================================================
#define FP_RX_PIN      16
#define FP_TX_PIN      17
#define LOCK_PIN       26
#define RED_LED_PIN    25
#define GREEN_LED_PIN  27
#define BUZZER_PIN     14

// Test için 1 yapabilirsin
#define MOCK_SENSOR 0

HardwareSerial fingerSerial(1);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&fingerSerial);

LockLogic lockLogic(LOCK_PIN, RED_LED_PIN, GREEN_LED_PIN, BUZZER_PIN);

uint32_t getCurrentTimestamp() {
    // Şimdilik basit yaklaşım.
    // Sonradan NTP ile gerçek Unix time bağlanabilir.
    return millis() / 1000;
}

#if MOCK_SENSOR
FingerprintResult getMockFingerprintResult() {
    static int counter = 0;
    counter++;

    FingerprintResult result;
    result.timestamp = getCurrentTimestamp();
    result.confidence = 0;
    result.user_id = 0;
    result.matched = false;

    // örnek test akışı:
    // 1-2-3 başarısız, 4 başarılı
    if (counter % 4 == 0) {
        result.matched = true;
        result.user_id = 1;
        result.confidence = 87;
    }

    return result;
}
#endif

void setup() {
    Serial.begin(115200);
    delay(500);

    fingerSerial.begin(57600, SERIAL_8N1, FP_RX_PIN, FP_TX_PIN);

    lockLogic.begin();

#if MOCK_SENSOR
    Serial.println("MOCK_SENSOR mode active");
#else
    if (finger.verifyPassword()) {
        Serial.println("Fingerprint sensor found");
    } else {
        Serial.println("Fingerprint sensor not found");
        while (1) {
            delay(10);
        }
    }
#endif

    Serial.println("System ready");
}

void loop() {
    lockLogic.update();

    if (lockLogic.isLockoutActive()) {
        delay(20);
        return;
    }

#if MOCK_SENSOR
    FingerprintResult result = getMockFingerprintResult();
    lockLogic.processFingerprintResult(result);
    delay(1500);
#else
    uint8_t p = finger.getImage();

    if (p == FINGERPRINT_NOFINGER) {
        delay(50);
        return;
    }

    FingerprintResult result;
    result.timestamp = getCurrentTimestamp();
    result.user_id = 0;
    result.confidence = 0;
    result.matched = false;

    if (p != FINGERPRINT_OK) {
        delay(100);
        return;
    }

    p = finger.image2Tz();
    if (p != FINGERPRINT_OK) {
        lockLogic.processFingerprintResult(result);
        delay(500);
        return;
    }

    p = finger.fingerFastSearch();
    if (p == FINGERPRINT_OK) {
        result.matched = true;
        result.user_id = finger.fingerID;
        result.confidence = (finger.confidence > 100) ? 100 : finger.confidence;
    } else {
        result.matched = false;
    }

    lockLogic.processFingerprintResult(result);
    delay(500);
#endif
}
