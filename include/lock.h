#ifndef LOCK_H
#define LOCK_H

#include <Arduino.h>

/** Röle (Kilit) için pin - ESP32 DevKit */
#define LOCK_RELAY_PIN 26

class LockManager {
public:
    LockManager(int pin = LOCK_RELAY_PIN) : _pin(pin), _isLocked(true) {}

    void init() {
        pinMode(_pin, OUTPUT);
        digitalWrite(_pin, HIGH); // Başlangıçta kilidi kapalı tut (HIGH = Röle Bırakır)
        _isLocked = true;
    }

    void unlock() {
        digitalWrite(_pin, LOW);  // Röleyi tetikle (Aktif Et)
        _isLocked = false;
        Serial.println("[LOCK] Kapı ACILDI");
    }

    void lock() {
        digitalWrite(_pin, HIGH); // Röleyi bırak (Pasif Et)
        _isLocked = true;
        Serial.println("[LOCK] Kapı KILITLI");
    }

    bool isLocked() const { return _isLocked; }

private:
    int _pin;
    bool _isLocked;
};

#endif
