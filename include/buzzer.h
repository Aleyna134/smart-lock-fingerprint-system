#ifndef BUZZER_H
#define BUZZER_H

#include <Arduino.h>

/** Buzzer için varsayılan pin */
#define BUZZER_PIN 6

class BuzzerManager {
public:
    BuzzerManager(int pin = BUZZER_PIN) : _pin(pin) {}

    void init() {
        pinMode(_pin, OUTPUT);
        digitalWrite(_pin, LOW);
    }

    /** Başarılı işlem sesi (Kısa bip) */
    void beepSuccess() {
        digitalWrite(_pin, HIGH);
        delay(100);
        digitalWrite(_pin, LOW);
    }

    /** Hata sesi (İki kısa bip) */
    void beepError() {
        digitalWrite(_pin, HIGH);
        delay(100);
        digitalWrite(_pin, LOW);
        delay(100);
        digitalWrite(_pin, HIGH);
        delay(100);
        digitalWrite(_pin, LOW);
    }

    /** Açılış/Hoşgeldin sesi */
    void beepWelcome() {
        for(int i=0; i<3; i++) {
            digitalWrite(_pin, HIGH);
            delay(50);
            digitalWrite(_pin, LOW);
            delay(50);
        }
    }

private:
    int _pin;
};

#endif
