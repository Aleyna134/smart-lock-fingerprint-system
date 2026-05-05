#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// LCD Ayarları
#define LCD_COLUMNS 16
#define LCD_ROWS    2
#define LCD_ADDRESS 0x27 // Yaygın adres: 0x27 veya 0x3F

// I2C Pinleri - ESP32 DevKit varsayılan I2C pinleri
#ifndef I2C_SDA
#define I2C_SDA 21
#endif
#ifndef I2C_SCL
#define I2C_SCL 22
#endif

class DisplayManager {
public:
    // ESP32'de I2C pinlerini constructor içinde değil, init() içinde Wire.begin() ile veriyoruz.
    DisplayManager(uint8_t addr = LCD_ADDRESS) 
        : _lcd(addr, LCD_COLUMNS, LCD_ROWS) {}

    bool init() {
        Wire.begin(I2C_SDA, I2C_SCL);
        Wire.setClock(50000); // 50kHz (Gürültü ve kablo kayıplarını önlemek için yavaşlatıldı)
        delay(200); 

        _lcd.init();
        _lcd.backlight();
        _lcd.clear();
        delay(100);
        
        Serial.println(F("[DISP] LCD baslatildi."));
        return true;
    }

    void showMessage(const String& line1, const String& line2 = "") {
        _lcd.clear();
        delay(50); // Ekran kontrolcusunun temizlik yapması için mola
        
        // 1. Satır
        _lcd.setCursor(0, 0);
        _lcd.print(line1.substring(0, 16));

        // 2. Satır
        if (line2 != "") {
            _lcd.setCursor(0, 1);
            _lcd.print(line2.substring(0, 16));
        }
    }

    void showStatus(bool locked) {
        _lcd.clear();
        _lcd.setCursor(0, 0);
        _lcd.print(F("Sistem Durumu:"));
        _lcd.setCursor(0, 1);
        _lcd.print(locked ? F("> KILITLI") : F("> ACIK"));
    }

    void clear() {
        _lcd.clear();
    }

private:
    LiquidCrystal_I2C _lcd;
};

#endif
