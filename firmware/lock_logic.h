#ifndef LOCK_LOGIC_H
#define LOCK_LOGIC_H

#include <Arduino.h>

// Kişi 2'den gelen sabit arayüz
struct FingerprintResult {
    bool matched;         // true = doğrulama başarılı
    uint16_t user_id;     // 0 = eşleşme yok
    uint8_t confidence;   // 0-100
    uint32_t timestamp;   // Unix time
};

// Kişi 3'ün ürettiği log yapısı
struct AccessLog {
    uint16_t user_id;     
    bool success;         
    char status[16];      // "unlocked" | "denied" | "lockout"
    uint8_t fail_count;   
    uint32_t timestamp;   
};

class LockLogic {
private:
    int lockPin;
    int redLedPin;
    int greenLedPin;
    int buzzerPin;

    uint8_t failCount;
    bool lockoutActive;
    unsigned long lockoutStartMillis;
    unsigned long lastBlinkMillis;
    bool redLedState;

    static const uint8_t MAX_FAILS = 3;
    static const unsigned long UNLOCK_TIME_MS = 3000;
    static const unsigned long LOCKOUT_TIME_MS = 60000;
    static const unsigned long BLINK_INTERVAL_MS = 300;

    static const int BUZZER_CHANNEL = 0;
    static const int BUZZER_FREQ = 2000;
    static const int BUZZER_RESOLUTION = 8;
    static const int BUZZER_DUTY = 128; // %50 duty

    void openLock();
    void closeLock();
    void setIdleOutputs();
    void startLockout();
    void stopLockout();
    void blinkRedLed();
    void printLog(const AccessLog& log);

public:
    LockLogic(int lockPin, int redLedPin, int greenLedPin, int buzzerPin);

    void begin();
    void update();
    void processFingerprintResult(const FingerprintResult& result);
    void handleFailedAttempt(uint32_t timestamp);

    AccessLog createAccessLog(uint16_t user_id, bool success, const char* status, uint8_t fail_count, uint32_t timestamp);
    void enqueueAccessLog(const AccessLog& log);

    bool isLockoutActive() const;
    uint8_t getFailCount() const;
};

#endif
