#ifndef LOCK_H
#define LOCK_H

#include <Arduino.h>

/** Röle (Kilit) için varsayılan pin */
#define LOCK_RELAY_PIN 7

class LockManager {
public:
    LockManager(int pin = LOCK_RELAY_PIN) : _pin(pin), _isLocked(true) {}

    void init() {
        pinMode(_pin, OUTPUT);
        lock(); // Başlangıçta kilitli tut
    }

    void unlock() {
        digitalWrite(_pin, HIGH); // Röle tipine göre LOW/HIGH değişebilir
        _isLocked = false;
        Serial.println("[LOCK] Kapı ACILDI");
    }

    void lock() {
        digitalWrite(_pin, LOW);
        _isLocked = true;
        Serial.println("[LOCK] Kapı KILITLI");
    }

    bool isLocked() const { return _isLocked; }

private:
    int _pin;
    bool _isLocked;
};

#endif
