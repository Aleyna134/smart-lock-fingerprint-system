/**
 * @file keypad_manager.h
 * @brief 4x4 Matrix Keypad Yönetim Modülü
 *
 * Bu dosya, KeypadManager sınıfının tanımını içerir.
 * ESP32 üzerinden 4x4 Matrix Keypad ile PIN tabanlı erişim kontrolü sağlar.
 * PIN'ler EEPROM (Preferences) içinde kalıcı olarak saklanır.
 *
 * Tuş Haritası:
 *   1 2 3 A    → A = Admin/Kayıt modu
 *   4 5 6 B    → B = Şifre değiştir
 *   7 8 9 C    → C = İptal / Geri
 *   * 0 # D    → # = Onayla/Giriş, * = Sil (son karakter), D = Kapı aç (doğrulama sonrası)
 *
 * @author Akıllı Kilit Projesi
 * @date 2026
 */

#ifndef KEYPAD_MANAGER_H
#define KEYPAD_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include "display_manager.h"

// ============================================================================
// Keypad Pin Tanımlamaları - ESP32 DevKit
// ============================================================================

// 4x4 Keypad → 8 pin gerekir (4 satır + 4 sütun)
// Kullanılmayan ESP32 pinleri seçildi (I2C, UART2, Röle, LED, Buzzer dışında)

// Satır pinleri (ROW) - Keypad'den çıkış olarak taranır
#define KP_ROW1  13
#define KP_ROW2  12
#define KP_ROW3  15  // Not: Boot sırasında dikkat, ama runtime'da sorunsuz
#define KP_ROW4   4

// Sütun pinleri (COL) - Input olarak okunur (INPUT_PULLUP)
// Tüm pinlerde dahili pull-up var, harici direnç GEREKMEZ
#define KP_COL1  32
#define KP_COL2  33
#define KP_COL3  18
#define KP_COL4  19

// ============================================================================
// Sabitler
// ============================================================================

#define PIN_MIN_LENGTH   4      ///< Minimum PIN uzunluğu
#define PIN_MAX_LENGTH   8      ///< Maksimum PIN uzunluğu
#define MAX_WRONG_ATTEMPTS 3    ///< Kilitleme öncesi max yanlış deneme
#define LOCKOUT_DURATION  30000 ///< Kilitleme süresi (ms) = 30 saniye
#define KEY_DEBOUNCE_MS   200   ///< Tuş sıçrama önleme süresi (ms)
#define INPUT_TIMEOUT_MS  10000 ///< PIN giriş zaman aşımı (ms) = 10 saniye

// Keypad boyutları
#define KP_ROWS 4
#define KP_COLS 4

// ============================================================================
// KeypadManager Sınıfı
// ============================================================================

/**
 * @brief 4x4 Matrix Keypad ile PIN Erişim Kontrol Sınıfı
 *
 * Fonksiyonlar:
 *  - PIN ile giriş doğrulama
 *  - Yeni PIN kaydetme (admin modunda)
 *  - PIN değiştirme (eski PIN doğrulaması ile)
 *  - Yanlış deneme sayacı ve kilitleme mekanizması
 *  - EEPROM (Preferences) ile kalıcı PIN saklama
 *
 * Kullanım:
 * @code
 *   KeypadManager keypad;
 *   keypad.init();
 *   char key = keypad.getKey();
 *   if (key) { ... }
 * @endcode
 */
class KeypadManager {
public:
    KeypadManager() : _wrongAttempts(0), _lockoutStart(0), _isLockedOut(false) {}

    /**
     * @brief Keypad'i başlatır ve EEPROM'dan PIN'i yükler
     */
    void init() {
        // Satır pinlerini OUTPUT olarak ayarla
        int rowPins[KP_ROWS] = {KP_ROW1, KP_ROW2, KP_ROW3, KP_ROW4};
        int colPins[KP_COLS] = {KP_COL1, KP_COL2, KP_COL3, KP_COL4};

        for (int i = 0; i < KP_ROWS; i++) {
            _rowPins[i] = rowPins[i];
            pinMode(_rowPins[i], OUTPUT);
            digitalWrite(_rowPins[i], HIGH);
        }

        // Sütun pinlerini INPUT_PULLUP olarak ayarla (tüm pinlerde dahili pullup var)
        for (int i = 0; i < KP_COLS; i++) {
            _colPins[i] = colPins[i];
            pinMode(_colPins[i], INPUT_PULLUP);
        }

        // EEPROM'dan kayıtlı PIN'i yükle
        _loadPinFromEEPROM();

        Serial.println("[KEYPAD] Keypad baslatildi.");
        Serial.printf("[KEYPAD] Kayitli PIN uzunlugu: %d\n", _storedPin.length());
    }

    // ========================================================================
    // Tuş Okuma
    // ========================================================================

    /**
     * @brief Keypad'den tek bir tuşa basılıp basılmadığını kontrol eder
     * @return Basılan tuş karakteri, basılmadıysa '\0'
     *
     * Tuş haritası:
     *   '1','2','3','A'
     *   '4','5','6','B'
     *   '7','8','9','C'
     *   '*','0','#','D'
     */
    char getKey() {
        static unsigned long lastKeyTime = 0;

        // Debounce kontrolü
        if (millis() - lastKeyTime < KEY_DEBOUNCE_MS) return '\0';

        // 4x4 tuş haritası
        const char keys[KP_ROWS][KP_COLS] = {
            {'1', '2', '3', 'A'},
            {'4', '5', '6', 'B'},
            {'7', '8', '9', 'C'},
            {'*', '0', '#', 'D'}
        };

        for (int row = 0; row < KP_ROWS; row++) {
            // Bu satırı LOW yap (aktif et)
            digitalWrite(_rowPins[row], LOW);

            for (int col = 0; col < KP_COLS; col++) {
                if (digitalRead(_colPins[col]) == LOW) {
                    // Tuşun bırakılmasını bekle
                    while (digitalRead(_colPins[col]) == LOW) {
                        delay(10);
                    }
                    // Satırı tekrar HIGH yap
                    digitalWrite(_rowPins[row], HIGH);
                    lastKeyTime = millis();
                    return keys[row][col];
                }
            }

            // Satırı tekrar HIGH yap
            digitalWrite(_rowPins[row], HIGH);
        }

        return '\0'; // Tuşa basılmadı
    }

    // ========================================================================
    // PIN Giriş Fonksiyonları
    // ========================================================================

    /**
     * @brief Kullanıcıdan PIN girişi alır (LCD'de * ile maskelenir)
     * @param prompt Serial'e yazdırılacak açıklama
     * @param enteredPin Girilen PIN'in yazılacağı referans
     * @return true: PIN girildi, false: zaman aşımı veya iptal
     *
     * # = Onayla, C = İptal, * = Son karakteri sil
     */
    bool readPinInput(String& enteredPin, DisplayManager& disp, char initialKey = '\0') {
        enteredPin = "";
        unsigned long startTime = millis();

        // --- Yardımcı: tüm PIN'i yıldız olarak göster ---
        auto showStars = [&]() {
            String s = "";
            for (unsigned int i = 0; i < enteredPin.length(); i++) s += '*';
            while (s.length() < 16) s += ' ';
            disp.showLine2(s);
        };

        // --- Yardımcı: Son karakteri rakam olarak göster, kalanlar yıldız ---
        auto showLastDigit = [&]() {
            String s = "";
            for (unsigned int i = 0; i + 1 < enteredPin.length(); i++) s += '*';
            if (enteredPin.length() > 0) s += enteredPin[enteredPin.length() - 1];
            while (s.length() < 16) s += ' ';
            disp.showLine2(s);
        };

        // --- Başlangıç tuşu gelmişse işle ---
        if (initialKey >= '0' && initialKey <= '9') {
            enteredPin += initialKey;
            Serial.print(initialKey); // Seri porta gerçek rakamı yaz
            showLastDigit();          // LCD'ye rakamı göster
            delay(600);               // 600ms görünür kal
            Serial.print("\b*");       // Seri portaki rakamı * ile değiştir
            showStars();              // LCD'yi yıldıza çevir
        }

        while (millis() - startTime < INPUT_TIMEOUT_MS) {
            char key = getKey();
            if (key == '\0') { delay(10); continue; }

            if (key == '#') {
                // --- Onayla ---
                if (enteredPin.length() >= PIN_MIN_LENGTH) {
                    Serial.println();
                    return true;
                } else {
                    Serial.printf("\n[!] En az %d hane gerekli!\n", PIN_MIN_LENGTH);
                    // LCD'de uyarı göster
                    String warn = "Min " + String(PIN_MIN_LENGTH) + " hane!  ";
                    while (warn.length() < 16) warn += ' ';
                    disp.showLine2(warn);
                    delay(1000);
                    showStars(); // Geri yıldızlara dön
                    startTime = millis();
                }
            }
            else if (key == 'C') {
                // --- İptal ---
                Serial.println("\n[KEYPAD] Iptal.");
                enteredPin = "";
                return false;
            }
            else if (key == '*') {
                // --- Son karakteri sil ---
                if (enteredPin.length() > 0) {
                    enteredPin.remove(enteredPin.length() - 1);
                    showStars();
                    // Seri port güncelle
                    Serial.print("\r[PIN] ");
                    for (unsigned int i = 0; i < enteredPin.length(); i++) Serial.print('*');
                    Serial.print("< "); // Silindi göstergesi
                    startTime = millis();
                }
            }
            else if (key >= '0' && key <= '9') {
                // --- Rakam girişi ---
                if (enteredPin.length() < PIN_MAX_LENGTH) {
                    enteredPin += key;
                    Serial.print(key);   // Önce rakamı yaz
                    showLastDigit();     // LCD'ye rakamı göster
                    delay(600);          // 600ms görünür
                    Serial.print("\b*"); // Seri portaki rakamı * ile ört
                    showStars();         // LCD yıldıza çevir
                    startTime = millis();
                }
            }
            // A, B, D tuşları bu modda yoksayılır
        }

        Serial.println("\n[KEYPAD] Zaman asimi!");
        return false;
    }

    // ========================================================================
    // Erişim Kontrol
    // ========================================================================

    /**
     * @brief Girilen PIN'i doğrular
     * @param inputPin Kontrol edilecek PIN
     * @return true: doğru PIN, false: yanlış PIN
     */
    bool verifyPin(const String& inputPin) {
        // Kilitleme kontrolü
        if (_isLockedOut) {
            if (millis() - _lockoutStart < LOCKOUT_DURATION) {
                unsigned long remaining = (LOCKOUT_DURATION - (millis() - _lockoutStart)) / 1000;
                Serial.printf("[KEYPAD] SISTEM KILITLI! %lu saniye bekleyin.\n", remaining);
                return false;
            } else {
                // Kilitleme süresi doldu
                _isLockedOut = false;
                _wrongAttempts = 0;
                Serial.println("[KEYPAD] Kilitleme suresi doldu. Tekrar deneyebilirsiniz.");
            }
        }

        if (inputPin == _storedPin) {
            _wrongAttempts = 0;
            Serial.println("[KEYPAD] PIN DOGRU - Erisim Onaylandi!");
            return true;
        } else {
            _wrongAttempts++;
            Serial.printf("[KEYPAD] YANLIS PIN! (Deneme: %d/%d)\n", _wrongAttempts, MAX_WRONG_ATTEMPTS);

            if (_wrongAttempts >= MAX_WRONG_ATTEMPTS) {
                _isLockedOut = true;
                _lockoutStart = millis();
                Serial.printf("[KEYPAD] SISTEM KILITLENDI! %d saniye bekleyin.\n", LOCKOUT_DURATION / 1000);
            }
            return false;
        }
    }

    // ========================================================================
    // PIN Yönetimi
    // ========================================================================

    /**
     * @brief Yeni PIN kaydeder (Admin modu - doğrudan)
     * @param newPin Kaydedilecek yeni PIN
     * @return true: başarılı, false: geçersiz PIN uzunluğu
     */
    bool setNewPin(const String& newPin) {
        if (newPin.length() < PIN_MIN_LENGTH || newPin.length() > PIN_MAX_LENGTH) {
            Serial.printf("[KEYPAD] PIN %d-%d hane olmali!\n", PIN_MIN_LENGTH, PIN_MAX_LENGTH);
            return false;
        }

        // Sadece rakam kontrolü
        for (unsigned int i = 0; i < newPin.length(); i++) {
            if (newPin[i] < '0' || newPin[i] > '9') {
                Serial.println("[KEYPAD] PIN sadece rakam icermelidir!");
                return false;
            }
        }

        _storedPin = newPin;
        _savePinToEEPROM();
        Serial.println("[KEYPAD] Yeni PIN kaydedildi!");
        return true;
    }

    /**
     * @brief PIN değiştirir (eski PIN doğrulaması gerekir)
     * @param oldPin Mevcut PIN
     * @param newPin Yeni PIN
     * @return true: başarılı, false: eski PIN yanlış veya yeni PIN geçersiz
     */
    bool changePin(const String& oldPin, const String& newPin) {
        if (oldPin != _storedPin) {
            Serial.println("[KEYPAD] Eski PIN yanlis! Degisiklik reddedildi.");
            return false;
        }
        return setNewPin(newPin);
    }

    /**
     * @brief PIN'i fabrika varsayılanına sıfırlar (1234)
     */
    void resetToDefault() {
        _storedPin = "1234";
        _savePinToEEPROM();
        _wrongAttempts = 0;
        _isLockedOut = false;
        Serial.println("[KEYPAD] PIN fabrika ayarlarina sifirlandi (1234).");
    }

    // ========================================================================
    // Durum Sorgulama
    // ========================================================================

    /** Kilitleme durumunda mı? */
    bool isLockedOut() const { return _isLockedOut && (millis() - _lockoutStart < LOCKOUT_DURATION); }

    /** Kalan kilitleme süresi (saniye) */
    unsigned long getLockoutRemaining() const {
        if (!isLockedOut()) return 0;
        return (LOCKOUT_DURATION - (millis() - _lockoutStart)) / 1000;
    }

    /** Yanlış deneme sayısı */
    int getWrongAttempts() const { return _wrongAttempts; }

    /** Kayıtlı PIN uzunluğu */
    int getStoredPinLength() const { return _storedPin.length(); }

    /** PIN hâlâ fabrika varsayılanı "1234" mi? (İlk kurulum kontrolü) */
    bool isDefaultPin() const { return _storedPin == "1234"; }

private:
    int _rowPins[KP_ROWS];
    int _colPins[KP_COLS];

    String _storedPin;             ///< EEPROM'dan yüklenen kayıtlı PIN
    int _wrongAttempts;            ///< Yanlış deneme sayacı
    unsigned long _lockoutStart;   ///< Kilitleme başlangıç zamanı
    bool _isLockedOut;             ///< Kilitleme durumu

    Preferences _prefs;            ///< ESP32 NVS (Non-Volatile Storage)

    /**
     * @brief EEPROM (NVS) üzerinden PIN'i yükler
     * İlk çalışmada varsayılan PIN "1234" olarak kaydedilir.
     */
    void _loadPinFromEEPROM() {
        _prefs.begin("smartlock", true); // Read-only
        _storedPin = _prefs.getString("pin", "");
        _prefs.end();

        if (_storedPin.length() == 0) {
            // İlk çalışma - varsayılan PIN kaydet
            Serial.println("[KEYPAD] Ilk calisma! Varsayilan PIN: 1234");
            _storedPin = "1234";
            _savePinToEEPROM();
        } else {
            Serial.println("[KEYPAD] PIN EEPROM'dan yuklendi.");
        }
    }

    /**
     * @brief PIN'i EEPROM (NVS) üzerine kaydeder
     */
    void _savePinToEEPROM() {
        _prefs.begin("smartlock", false); // Read-Write
        _prefs.putString("pin", _storedPin);
        _prefs.end();
        Serial.println("[KEYPAD] PIN EEPROM'a kaydedildi.");
    }
};

#endif // KEYPAD_MANAGER_H
