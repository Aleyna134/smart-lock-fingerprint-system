#include "lock_logic.h"
#include <string.h>

LockLogic::LockLogic(int lockPin, int redLedPin, int greenLedPin, int buzzerPin)
    : lockPin(lockPin),
      redLedPin(redLedPin),
      greenLedPin(greenLedPin),
      buzzerPin(buzzerPin),
      failCount(0),
      lockoutActive(false),
      lockoutStartMillis(0),
      lastBlinkMillis(0),
      redLedState(false) {
}

void LockLogic::begin() {
    pinMode(lockPin, OUTPUT);
    pinMode(redLedPin, OUTPUT);
    pinMode(greenLedPin, OUTPUT);

    closeLock();
    setIdleOutputs();

    ledcSetup(BUZZER_CHANNEL, BUZZER_FREQ, BUZZER_RESOLUTION);
    ledcAttachPin(buzzerPin, BUZZER_CHANNEL);

    failCount = 0;
    lockoutActive = false;
    lockoutStartMillis = 0;
    lastBlinkMillis = 0;
    redLedState = false;

    Serial.println("LockLogic initialized");
}

void LockLogic::openLock() {
    digitalWrite(lockPin, HIGH);
}

void LockLogic::closeLock() {
    digitalWrite(lockPin, LOW);
}

void LockLogic::setIdleOutputs() {
    digitalWrite(redLedPin, LOW);
    digitalWrite(greenLedPin, LOW);
    ledcWrite(BUZZER_CHANNEL, 0);
}

void LockLogic::startLockout() {
    lockoutActive = true;
    lockoutStartMillis = millis();
    lastBlinkMillis = 0;
    redLedState = false;

    Serial.println("LOCKOUT STARTED");
}

void LockLogic::stopLockout() {
    lockoutActive = false;
    failCount = 0;
    setIdleOutputs();

    Serial.println("LOCKOUT ENDED");
}

void LockLogic::blinkRedLed() {
    if (millis() - lastBlinkMillis >= BLINK_INTERVAL_MS) {
        lastBlinkMillis = millis();
        redLedState = !redLedState;
        digitalWrite(redLedPin, redLedState ? HIGH : LOW);
    }
}

void LockLogic::update() {
    if (!lockoutActive) {
        return;
    }

    blinkRedLed();
    ledcWrite(BUZZER_CHANNEL, BUZZER_DUTY);

    if (millis() - lockoutStartMillis >= LOCKOUT_TIME_MS) {
        stopLockout();
    }
}

void LockLogic::processFingerprintResult(const FingerprintResult& result) {
    if (lockoutActive) {
        return;
    }

    if (result.matched) {
        failCount = 0;

        Serial.print("ACCESS GRANTED - User ID: ");
        Serial.println(result.user_id);

        digitalWrite(redLedPin, LOW);
        digitalWrite(greenLedPin, HIGH);

        openLock();
        delay(UNLOCK_TIME_MS);
        closeLock();

        digitalWrite(greenLedPin, LOW);

        AccessLog log = createAccessLog(
            result.user_id,
            true,
            "unlocked",
            failCount,
            result.timestamp
        );
        enqueueAccessLog(log);
    } else {
        handleFailedAttempt(result.timestamp);
    }
}

void LockLogic::handleFailedAttempt(uint32_t timestamp) {
    if (lockoutActive) {
        return;
    }

    failCount++;

    Serial.print("ACCESS DENIED - fail_count = ");
    Serial.println(failCount);

    digitalWrite(redLedPin, HIGH);
    delay(200);
    digitalWrite(redLedPin, LOW);

    if (failCount >= MAX_FAILS) {
        startLockout();

        AccessLog log = createAccessLog(
            0,
            false,
            "lockout",
            failCount,
            timestamp
        );
        enqueueAccessLog(log);
    } else {
        AccessLog log = createAccessLog(
            0,
            false,
            "denied",
            failCount,
            timestamp
        );
        enqueueAccessLog(log);
    }
}

AccessLog LockLogic::createAccessLog(uint16_t user_id, bool success, const char* status, uint8_t fail_count, uint32_t timestamp) {
    AccessLog log;
    log.user_id = user_id;
    log.success = success;
    log.fail_count = fail_count;
    log.timestamp = timestamp;

    strncpy(log.status, status, sizeof(log.status) - 1);
    log.status[sizeof(log.status) - 1] = '\0';

    return log;
}

void LockLogic::printLog(const AccessLog& log) {
    Serial.println("----- ACCESS LOG -----");
    Serial.print("user_id: ");
    Serial.println(log.user_id);

    Serial.print("success: ");
    Serial.println(log.success ? "true" : "false");

    Serial.print("status: ");
    Serial.println(log.status);

    Serial.print("fail_count: ");
    Serial.println(log.fail_count);

    Serial.print("timestamp: ");
    Serial.println(log.timestamp);
    Serial.println("----------------------");
}

void LockLogic::enqueueAccessLog(const AccessLog& log) {
    // Şimdilik Kişi 6 entegrasyonu yoksa serial'a yazıyoruz.
    // Sonradan burası queue/sendLog ile bağlanabilir.
    printLog(log);
}

bool LockLogic::isLockoutActive() const {
    return lockoutActive;
}

uint8_t LockLogic::getFailCount() const {
    return failCount;
}
