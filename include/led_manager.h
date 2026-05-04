#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include <Arduino.h>

/** LED Pin Tanımlamaları - ESP32 DevKit */
#define LED_GREEN_PIN 27
#define LED_RED_PIN   25

class LedManager {
public:
    LedManager(int greenPin = LED_GREEN_PIN, int redPin = LED_RED_PIN) 
        : _greenPin(greenPin), _redPin(redPin) {}

    void init() {
        pinMode(_greenPin, OUTPUT);
        pinMode(_redPin, OUTPUT);
        allOff();
    }

    /** Başarılı durum: Yeşil LED'i yak, Kırmızıyı söndür */
    void success() {
        digitalWrite(_greenPin, HIGH);
        digitalWrite(_redPin, LOW);
    }

    /** Hata durumu: Kırmızı LED'i yak, Yeşili söndür */
    void error() {
        digitalWrite(_greenPin, LOW);
        digitalWrite(_redPin, HIGH);
    }

    /** Hepsi kapalı */
    void allOff() {
        digitalWrite(_greenPin, LOW);
        digitalWrite(_redPin, LOW);
    }

    /** Sadece Yeşil yak */
    void greenOn() {
        digitalWrite(_greenPin, HIGH);
    }

    /** Sadece Kırmızı yak */
    void redOn() {
        digitalWrite(_redPin, HIGH);
    }

private:
    int _greenPin;
    int _redPin;
};

#endif // LED_MANAGER_H
