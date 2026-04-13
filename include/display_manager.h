#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Wire.h>

#ifndef MOCK_MODE
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#endif

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C

// I2C Pinleri (ESP32-C3 Default)
#define I2C_SDA 8
#define I2C_SCL 9

class DisplayManager {
public:
    DisplayManager() 
#ifndef MOCK_MODE
    : _display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET) 
#endif
    {}

    bool init() {
#ifdef MOCK_MODE
        Serial.println(F("[MOCK] Ekran baslatildi (Sanal)"));
        return true;
#else
        Wire.begin(I2C_SDA, I2C_SCL);
        if(!_display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
            Serial.println(F("[DISP] SSD1306 hatasi!"));
            return false;
        }
        _display.clearDisplay();
        _display.setTextSize(1);
        _display.setTextColor(SSD1306_WHITE);
        _display.display();
        return true;
#endif
    }

    void showMessage(const String& line1, const String& line2 = "") {
#ifdef MOCK_MODE
        Serial.println("[MOCK-SCREEN] " + line1 + " | " + line2);
#else
        _display.clearDisplay();
        _display.setCursor(0, 0);
        _display.setTextSize(1);
        _display.println(line1);
        if (line2 != "") {
            _display.setCursor(0, 16);
            _display.println(line2);
        }
        _display.display();
#endif
    }

    void showStatus(bool locked) {
#ifdef MOCK_MODE
        Serial.print(F("[MOCK-SCREEN] Durum: "));
        Serial.println(locked ? F("KILITLI") : F("ACIK"));
#else
        _display.clearDisplay();
        _display.setCursor(0, 0);
        _display.setTextSize(1);
        _display.println(F("Sistem Durumu:"));
        _display.setCursor(0, 16);
        _display.setTextSize(2);
        _display.println(locked ? F("KILITLI") : F("ACIK"));
        _display.display();
#endif
    }

    void clear() {
#ifndef MOCK_MODE
        _display.clearDisplay();
        _display.display();
#endif
    }

private:
#ifndef MOCK_MODE
    Adafruit_SSD1306 _display;
#endif
};

#endif
