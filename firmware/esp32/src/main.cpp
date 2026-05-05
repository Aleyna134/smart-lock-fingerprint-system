#include <Arduino.h>
#include "fingerprint.h"
#include "lock.h"
#include "buzzer.h"
#include "display_manager.h"
#include "led_manager.h"
#include "keypad_manager.h"
#include "iot_client.h"
#include "network_config.h"

// Nesneler
FingerprintManager fpManager;
LockManager lockController;
BuzzerManager buzzer;
DisplayManager display;
LedManager leds;
IotClient iotClient;
KeypadManager keypad;
int failedAttempts = 0;
bool enrollmentActive = false;
bool waitingForFingerRelease = false;
unsigned long lastAutoScanAt = 0;
unsigned long nextVerifyAllowedAt = 0;

#define LOCKOUT_THRESHOLD    3
#define LOCKOUT_DURATION_MS  30000UL
unsigned long lockedUntil = 0;

bool autoTestActive = false; // Otomatik test başlangıçta KAPALI

void showReady();
void printRuntimeConfig();
void pollHardwareCommands();
void handleAutomaticFingerprintScan();
void handleFingerprintAccess();
void handlePinLogin(char startKey = '\0');
void handleChangePin();
void handleAdminMode();

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) delay(10);
    
    Serial.println("\n[SISTEM] Akilli Kilit Baslatiliyor...");
    Serial.flush();

    // 1. GPIO Modülleri Başlat
    leds.init();
    lockController.init();
    buzzer.init();
    buzzer.beepWelcome(); // Açılış sesi

    // 2. LCD Başlat (Wire.begin() display.init() içinde çağrılır)
    if(display.init()) {
        display.showMessage("Baslatiliyor...", "Lutfen Bekleyin");
    } else {
        Serial.println("[HATA] LCD baslatilamadi!");
    }

    // 3. I2C Tarayıcı (Wire zaten display.init() tarafından başlatıldı)
    Serial.println("[...] I2C cihazlari taraniyor...");
    for(byte address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        if (Wire.endTransmission() == 0) {
            Serial.printf("[I2C] Cihaz bulundu: 0x%02X\n", address);
        }
    }

    // 4. Keypad Başlat
    keypad.init();

    // 5. Parmak İzi Sensörü (Serial2: RX=16, TX=17)
    if(fpManager.init(&Serial2, FP_RX_PIN, FP_TX_PIN)) {
        Serial.println("[OK] Parmak izi sensoru hazir.");
    } else {
        Serial.println("[YOK] Sensor bulunamadi.");
    }

    Serial.println("[IOT] WiFi baglantisi baslatiliyor...");
    iotClient.begin();
    Serial.println(iotClient.isConnected() ? "[IOT] WiFi baglandi." : "[IOT] WiFi yok, offline devam.");
    printRuntimeConfig();

    display.showMessage("Sistem Hazir", "Parmak/PIN");
    Serial.println("\n[HAZIR] Komutlar:");
    Serial.println("  v = Parmak Dogrula   e = Yeni Kayit");
    Serial.println("  s = Durum Sorgula    c = Tum Kayitlari Sil");
    Serial.println("  o = Kapi Ac          k = Kapi Kilitle");
    Serial.println("  l = Donanim Testi    t = Otomatik Test");
    Serial.println("\nKeypad Tuslari:");
    Serial.println("  0-9 = PIN girisi     # = Onayla/Giris");
    Serial.println("  A   = Admin Modu     B = Sifre Degistir");
    Serial.println("  C   = Iptal          * = Durum");
}

void showReady() {
    display.showMessage("Sistem Hazir", "Parmak/PIN");
}

void printRuntimeConfig() {
    Serial.println("[CFG] Otomatik parmak tarama aktif.");
    Serial.printf("[CFG] Backend URL: %s | Device ID: %s\n", BACKEND_URL, DEVICE_ID);
    Serial.printf("[CFG] Command poll: %lu ms | Scan interval: %lu ms | Verify cooldown: %lu ms | Unlock: %lu ms\n",
                  IOT_POLL_INTERVAL_MS,
                  VERIFY_SCAN_INTERVAL_MS,
                  VERIFY_COOLDOWN_MS,
                  UNLOCK_DURATION_MS);
}

void pollHardwareCommands() {
    static unsigned long lastCommandPoll = 0;
    if (!enrollmentActive && millis() - lastCommandPoll > IOT_POLL_INTERVAL_MS) {
        lastCommandPoll = millis();
        Serial.println("[IOT] Backend command polling...");
        EnrollmentCommand command = iotClient.pollEnrollmentCommand();

        if (command.available) {
            enrollmentActive = true;
            Serial.printf("[AUTO] Normal fingerprint verify durduruldu: hardware command basliyor. type=%s\n",
                          command.type.c_str());
            Serial.printf("[IOT] Backend komutu alindi. type=%s ID=%d command_id=%d\n",
                          command.type.c_str(), command.templateId, command.id);

            if (command.type == "DELETE_FINGERPRINT") {
                display.showMessage("Deleting user", "ID: " + String(command.templateId));
                Serial.printf("[IOT] Fingerprint delete command baslatiliyor. template_id=%d\n", command.templateId);
                bool deleted = fpManager.deleteID(command.templateId);

                if (deleted) {
                    display.showMessage("Delete OK", "ID: " + String(command.templateId));
                    leds.success();
                    buzzer.beepSuccess();
                    Serial.println("[IOT] Fingerprint delete result success gonderiliyor...");
                    iotClient.sendEnrollmentResult(command.id, true, command.templateId, "Fingerprint deleted");
                } else {
                    display.showMessage("Delete Fail", "Try Again");
                    leds.error();
                    buzzer.beepError();
                    Serial.println("[IOT] Fingerprint delete result failed gonderiliyor...");
                    iotClient.sendEnrollmentResult(command.id, false, command.templateId, "Fingerprint delete failed");
                }

                delay(2000);
                leds.allOff();
            } else if (!fpManager.isReady()) {
                Serial.println("[HATA] Sensor hazir degil, kayit komutu fail bildiriliyor.");
                display.showMessage("SENSOR HATASI", "KAYIT YOK");
                iotClient.sendEnrollmentResult(command.id, false, command.templateId, "Fingerprint sensor not ready");
            } else if (command.type == "ENROLL_FINGERPRINT") {
                display.showMessage("New enrollment", "ID: " + String(command.templateId));
                bool enrolled = fpManager.enrollFingerprint(command.templateId);

                if (enrolled) {
                    display.showMessage("Enrollment OK", "ID: " + String(command.templateId));
                    leds.success();
                    buzzer.beepSuccess();
                    iotClient.sendEnrollmentResult(command.id, true, command.templateId, "Fingerprint enrolled");
                } else {
                    display.showMessage("Enrollment Fail", "Try Again");
                    leds.error();
                    buzzer.beepError();
                    iotClient.sendEnrollmentResult(command.id, false, command.templateId, "Fingerprint enrollment failed");
                }

                delay(2000);
                leds.allOff();
            } else {
                Serial.printf("[IOT] Desteklenmeyen komut tipi: %s\n", command.type.c_str());
                iotClient.sendEnrollmentResult(command.id, false, command.templateId, "Unsupported hardware command");
            }

            showReady();
            waitingForFingerRelease = true;
            nextVerifyAllowedAt = millis() + VERIFY_COOLDOWN_MS;
            Serial.println("[AUTO] Hardware command bitti. Parmak kaldirilana kadar otomatik verify bekletiliyor.");
            enrollmentActive = false;
        }
    }
}

// ============================================================================
// Keypad İşlem Fonksiyonları
// ============================================================================

void handlePinLogin(char startKey) {
    if (keypad.isLockedOut()) {
        unsigned long remaining = keypad.getLockoutRemaining();
        display.showMessage("SISTEM KILITLI", String(remaining) + " sn bekle");
        buzzer.beepError();
        delay(2000);
        showReady();
        return;
    }

    display.showMessage("PIN Giriniz:", "");
    Serial.println("\n[>] PIN giris modu aktif. # = Onayla, C = Iptal");

    String enteredPin = "";
    if (keypad.readPinInput(enteredPin, display, startKey)) {
        if (keypad.verifyPin(enteredPin)) {
            Serial.println("[KAPI] PIN ile ERISIM ONAYLANDI!");
            display.showMessage("ERISIM ONAYLANDI", "Hosgeldiniz");
            leds.success();
            buzzer.beepSuccess();
            iotClient.sendAccessLog(0, true, "PIN access granted", 0);
            lockController.unlock();
            iotClient.sendLockState("Unlocked", "PIN unlock");
            delay(UNLOCK_DURATION_MS);
            lockController.lock();
            iotClient.sendLockState("Locked", "Auto relock");
            leds.allOff();
        } else {
            Serial.println("[KAPI] ERISIM REDDEDILDI! (Yanlis PIN)");
            iotClient.sendAccessLog(0, false, "Failed PIN", keypad.getWrongAttempts());

            if (keypad.isLockedOut()) {
                display.showMessage("SISTEM KILITLI!", String(LOCKOUT_DURATION / 1000) + " sn bekle");
                buzzer.beepError();
                delay(100);
                buzzer.beepError();
                iotClient.sendLockState("Locked", "PIN Lockout");
            } else {
                int remaining = MAX_WRONG_ATTEMPTS - keypad.getWrongAttempts();
                display.showMessage("YANLIS PIN!", "Kalan: " + String(remaining));
                leds.error();
                buzzer.beepError();
                delay(2000);
                leds.allOff();
            }
        }
    } else {
        display.showMessage("Iptal/Timeout", "");
        delay(1000);
    }

    showReady();
}

void handleChangePin() {
    Serial.println("\n[>] PIN degistirme modu aktif.");

    display.showMessage("Eski PIN:", "");
    String oldPin = "";
    if (!keypad.readPinInput(oldPin, display)) {
        display.showMessage("Iptal Edildi", "");
        delay(1000);
        showReady();
        return;
    }

    if (!keypad.verifyPin(oldPin)) {
        display.showMessage("ESKI PIN YANLIS", "Reddedildi!");
        leds.error();
        buzzer.beepError();
        delay(2000);
        leds.allOff();
        showReady();
        return;
    }

    display.showMessage("Yeni PIN:", "");
    String newPin = "";
    if (!keypad.readPinInput(newPin, display)) {
        display.showMessage("Iptal Edildi", "");
        delay(1000);
        showReady();
        return;
    }

    display.showMessage("Tekrar Girin:", "");
    String confirmPin = "";
    if (!keypad.readPinInput(confirmPin, display)) {
        display.showMessage("Iptal Edildi", "");
        delay(1000);
        showReady();
        return;
    }

    if (newPin != confirmPin) {
        display.showMessage("PIN UYUMSUZ!", "Tekrar Deneyin");
        leds.error();
        buzzer.beepError();
        delay(2000);
        leds.allOff();
        showReady();
        return;
    }

    if (keypad.setNewPin(newPin)) {
        display.showMessage("PIN DEGISTIRILDI", "Basarili!");
        leds.success();
        buzzer.beepSuccess();
    } else {
        display.showMessage("PIN HATASI", "4-8 hane olmali");
        leds.error();
        buzzer.beepError();
    }
    delay(2000);
    leds.allOff();
    showReady();
}

void handleAdminMode() {
    Serial.println("\n[>] Admin modu aktif.");

    if (keypad.isDefaultPin()) {
        display.showMessage("ILK KURULUM", "PIN Belirleyin");
        Serial.println("[ADMIN] Ilk kurulum - PIN dogrulamasi atlaniyor.");
        delay(1500);
    } else {
        display.showMessage("ADMIN MODU", "");
        String adminPin = "";
        if (!keypad.readPinInput(adminPin, display)) {
            display.showMessage("Iptal Edildi", "");
            delay(1000);
            showReady();
            return;
        }
        if (!keypad.verifyPin(adminPin)) {
            display.showMessage("YETKISIZ ERISIM", "Reddedildi!");
            leds.error();
            buzzer.beepError();
            delay(2000);
            leds.allOff();
            showReady();
            return;
        }
    }

    display.showMessage("1:PIN 3:Parmak+", "2:Rst 4:TumuSil");
    Serial.println("[ADMIN] 1=Yeni PIN, 2=Fabrika Sifirla, 3=Parmak Ekle, 4=Tumunu Sil, 5=Durum, C=Cikis");

    unsigned long menuStart = millis();
    while (millis() - menuStart < INPUT_TIMEOUT_MS) {
        char mKey = keypad.getKey();
        if (mKey == '\0') continue;

        if (mKey == '1') {
            display.showMessage("Yeni PIN Girin:", "");
            String newPin = "";
            if (keypad.readPinInput(newPin, display)) {
                if (keypad.setNewPin(newPin)) {
                    display.showMessage("PIN KAYDEDILDI", "Basarili!");
                    leds.success();
                    buzzer.beepSuccess();
                    delay(2000);
                    leds.allOff();
                }
            }
            break;
        }
        else if (mKey == '2') {
            keypad.resetToDefault();
            display.showMessage("PIN SIFIRLANDI", "Yeni PIN: 1234");
            buzzer.beepWelcome();
            delay(3000);
            break;
        }
        else if (mKey == '3') {
            if (!fpManager.isReady()) {
                display.showMessage("SENSOR HATASI", "BAGLANTI YOK");
                delay(2000);
            } else {
                int count = fpManager.getStoredCount();
                if (count < 0) {
                    display.showMessage("SENSOR HATASI", "");
                    delay(2000);
                } else if (count >= FP_MAX_TEMPLATES) {
                    display.showMessage("KAPASITE DOLU", "Silmek icin 4");
                    buzzer.beepError();
                    delay(2000);
                } else {
                    int nextId = count + 1;
                    display.showMessage("YENI KAYIT", "Parmak Koyun");
                    if (fpManager.enrollFingerprint(nextId)) {
                        display.showMessage("KAYIT BASARILI", "ID: " + String(nextId));
                        leds.success(); buzzer.beepSuccess();
                    } else {
                        display.showMessage("KAYIT HATASI", "Tekrar Deneyin");
                        leds.error(); buzzer.beepError();
                    }
                    delay(2000); leds.allOff();
                }
            }
            break;
        }
        else if (mKey == '4') {
            display.showMessage("SILIYOR...", "Lutfen Bekleyin");
            if (fpManager.deleteAll()) {
                display.showMessage("TUMU SILINDI", "Sistem Sifirlandi");
                leds.success(); buzzer.beepWelcome(); delay(1000); leds.allOff();
            } else {
                display.showMessage("SILME HATASI", "");
                leds.error(); buzzer.beepError(); delay(1000); leds.allOff();
            }
            break;
        }
        else if (mKey == '5') {
            if (!fpManager.isReady()) {
                display.showMessage("SENSOR: YOK", "BAGLAYIN");
            } else {
                SensorInfo info = fpManager.getSensorData();
                display.showMessage("Kayitli:" + String(info.template_count), "Kapasite:" + String(info.capacity));
            }
            delay(3000);
            break;
        }
        else if (mKey == 'C' || mKey == 'c') {
            Serial.println("[ADMIN] Cikis yapildi.");
            break;
        }
        menuStart = millis();
    }

    showReady();
}

// ============================================================================
// Parmak İzi Erişim
// ============================================================================

void handleFingerprintAccess() {
    unsigned long now = millis();
    if (now < lockedUntil) {
        unsigned long remaining = (lockedUntil - now + 999) / 1000;
        Serial.printf("[SEC] Erisim reddedildi — sistem kilitli. Kalan: %lu sn\n", remaining);
        display.showMessage("SISTEM KILITLI", String(remaining) + " sn kaldi");
        return;
    }

    Serial.println("\n[>] Parmak okuma modu aktif.");
    if (!fpManager.isReady()) {
        Serial.println("[HATA] Sensor hazir degil!");
        display.showMessage("SENSOR HATASI", "BAGLANTI YOK");
        nextVerifyAllowedAt = millis() + VERIFY_COOLDOWN_MS;
        return;
    }

    display.showMessage("Parmak Bekleniyor", "Lutfen Okutun");
    FingerprintResult result = fpManager.verifyFingerprint();

    if (result.matched) {
        failedAttempts = 0;
        Serial.printf("[KAPI] ERISIM ONAYLANDI! (ID: %d, Guven: %d)\n", result.user_id, result.confidence);
        display.showMessage("ERISIM ONAYLANDI", "Hosgeldiniz");
        leds.success();
        buzzer.beepSuccess();
        Serial.println("[LOG] Backend success access log gonderiliyor...");
        iotClient.sendAccessLog(result.user_id, true, "Access granted", 0);
        Serial.printf("[LOCK] Kapi %lu ms acik kalacak.\n", UNLOCK_DURATION_MS);
        lockController.unlock();
        Serial.println("[STATE] Backend lock state Unlocked gonderiliyor...");
        iotClient.sendLockState("Unlocked", "Access granted");
        delay(UNLOCK_DURATION_MS);
        lockController.lock();
        Serial.println("[STATE] Backend lock state Locked gonderiliyor...");
        iotClient.sendLockState("Locked", "Auto relock");
        leds.allOff();
    } else {
        failedAttempts++;
        Serial.println("[KAPI] ERISIM REDDEDILDI! (Gecersiz Parmak)");
        Serial.printf("[SEC] Basarisiz deneme sayisi: %d\n", failedAttempts);
        display.showMessage("ERISIM REDDEDILDI", "Gecersiz Parmak");
        leds.error();
        buzzer.beepError();
        Serial.println("[LOG] Backend failed access log gonderiliyor...");
        iotClient.sendAccessLog(0, false, "Failed fingerprint", failedAttempts);
        delay(1000);
        leds.allOff();

        if (failedAttempts >= LOCKOUT_THRESHOLD) {
            lockedUntil = millis() + LOCKOUT_DURATION_MS;
            Serial.printf("[SEC] LOCKOUT baslatildi! %d basarisiz deneme. %lu sn kilitli.\n",
                          failedAttempts, LOCKOUT_DURATION_MS / 1000);
            display.showMessage("SISTEM KILITLI", "30 sn bekleyin");
            buzzer.beepError();
            delay(300);
            buzzer.beepError();
            Serial.println("[STATE] Backend lock state Locked (Lockout) gonderiliyor...");
            iotClient.sendLockState("Locked", "Lockout");
            failedAttempts = 0;
        }
    }

    nextVerifyAllowedAt = millis() + VERIFY_COOLDOWN_MS;
    waitingForFingerRelease = true;
    Serial.printf("[AUTO] Verify cooldown basladi: %lu ms. Parmak kaldirma bekleniyor.\n", VERIFY_COOLDOWN_MS);
    if (millis() < lockedUntil) {
        unsigned long remaining = (lockedUntil - millis() + 999) / 1000;
        display.showMessage("SISTEM KILITLI", String(remaining) + " sn kaldi");
    } else {
        showReady();
    }
}

void handleAutomaticFingerprintScan() {
    if (enrollmentActive || !fpManager.isReady()) {
        return;
    }

    unsigned long now = millis();

    if (now < lockedUntil) {
        static unsigned long lastLockoutDisplay = 0;
        if (now - lastLockoutDisplay > 1000) {
            lastLockoutDisplay = now;
            unsigned long remaining = (lockedUntil - now + 999) / 1000;
            Serial.printf("[SEC] Sistem kilitli. Kalan: %lu sn\n", remaining);
            display.showMessage("SISTEM KILITLI", String(remaining) + " sn kaldi");
        }
        return;
    }

    // Lockout yeni bittiyse hazir ekranina don
    static unsigned long prevLockedUntil = 0;
    if (prevLockedUntil > 0) {
        Serial.println("[SEC] Lockout sona erdi. Sistem yeniden hazir.");
        showReady();
        prevLockedUntil = 0;
    }
    if (lockedUntil > 0) {
        prevLockedUntil = lockedUntil;
        lockedUntil = 0;
    }

    if (now - lastAutoScanAt < VERIFY_SCAN_INTERVAL_MS) {
        return;
    }
    lastAutoScanAt = now;

    if (waitingForFingerRelease) {
        if (!fpManager.hasFinger()) {
            waitingForFingerRelease = false;
            nextVerifyAllowedAt = millis() + VERIFY_COOLDOWN_MS;
            Serial.println("[AUTO] Parmak kaldirildi. Yeni verify icin sistem yeniden hazirlanacak.");
        }
        return;
    }

    if ((long)(now - nextVerifyAllowedAt) < 0) {
        return;
    }

    if (!fpManager.hasFinger()) {
        return;
    }

    Serial.println("[AUTO] Parmak algilandi, otomatik verify baslatiliyor.");
    handleFingerprintAccess();
}

void loop() {
    pollHardwareCommands();

    // 1. KEYPAD KONTROLÜ (Öncelikli)
    char key = keypad.getKey();
    if (key != '\0') {
        Serial.printf("[KEYPAD] Tus: '%c'\n", key);
        switch (key) {
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
            case '#':
                handlePinLogin(key);
                break;
            case 'A':
                handleAdminMode();
                break;
            case 'B':
                handleChangePin();
                break;
            case 'D':
                Serial.println("\n[>] Kapi acmak icin PIN girin.");
                handlePinLogin();
                break;
            case 'C':
                display.showMessage("Sistem Hazir", "Parmak/PIN");
                Serial.println("[KEYPAD] Ana ekrana donuldu.");
                break;
            case '*':
                if (keypad.isLockedOut()) {
                    display.showMessage("SISTEM KILITLI", String(keypad.getLockoutRemaining()) + " sn");
                } else {
                    display.showMessage("Sistem Hazir", "Parmak/PIN");
                }
                break;
        }
    }

    // 2. Otomatik Parmak İzi Tarama
    handleAutomaticFingerprintScan();

    // 3. Manuel Komut Kontrolü (Serial)
    if (Serial.available()) {
        char cmd = Serial.read();

        // Yeni satır / boşluk karakterlerini atla
        if (cmd == '\n' || cmd == '\r' || cmd == ' ') return;

        switch (cmd) {
            case 'v': case 'V': {
                handleFingerprintAccess();
                break;
            }
            case 'e': case 'E': {
                Serial.println("\n[>] Kayit modu aktif.");
                if (!fpManager.isReady()) {
                    Serial.println("[HATA] Sensor hazir degil!");
                    display.showMessage("SENSOR HATASI", "BAGLANTI YOK");
                    break;
                }
                
                // Sıradaki boş ID'yi bul
                // NOT: Bu yöntem yalnızca sıralı kayıt + toplu silme (c komutu) ile doğru çalışır.
                // Tekli silme (deleteID) kullanılırsa ID çakışması olabilir.
                int count = fpManager.getStoredCount();
                if (count < 0) {
                    Serial.println("[HATA] Kayit sayisi alinamadi!");
                    display.showMessage("SENSOR HATASI", "");
                    break;
                }
                if (count >= FP_MAX_TEMPLATES) {
                    Serial.println("[HATA] Kayit kapasitesi dolu!");
                    display.showMessage("KAPASITE DOLU", "Sil: 'c' komutu");
                    buzzer.beepError();
                    delay(2000);
                    display.showMessage("Sistem Hazir", "Parmak/PIN");
                    break;
                }
                int nextId = count + 1;
                Serial.printf("[>] Yeni kullanici ID: %d olarak belirlendi.\n", nextId);
                
                display.showMessage("YENI KAYIT", "Parmak Koyun");
                if (fpManager.enrollFingerprint(nextId)) {
                    display.showMessage("KAYIT BASARILI", "ID: " + String(nextId));
                    leds.success(); buzzer.beepSuccess();
                } else {
                    display.showMessage("KAYIT HATASI", "Tekrar Deneyin");
                    leds.error(); buzzer.beepError();
                }
                delay(2000); leds.allOff();
                waitingForFingerRelease = true;
                nextVerifyAllowedAt = millis() + VERIFY_COOLDOWN_MS;
                display.showMessage("Sistem Hazir", "Parmak/PIN");
                break;
            }
            case 's': case 'S': {
                if (!fpManager.isReady()) {
                    Serial.println("[DURUM] Sensor: YOK");
                    display.showMessage("SENSOR: YOK", "BAGLAYIN");
                    break;
                }
                SensorInfo info = fpManager.getSensorData();
                Serial.printf("\n[DURUM] Sensor: %s | Kayitli: %d | Kapasite: %d | Guvenlik: %d\n", 
                             info.connected ? "BAGLI" : "YOK", info.template_count,
                             info.capacity, info.security_level);
                display.showMessage("Kayitli:" + String(info.template_count), "Guvenlik:" + String(info.security_level));
                delay(2000);
                display.showMessage("Sistem Hazir", "Parmak Okutun");
                break;
            }
            case 'l': case 'L': {
                Serial.println("\n[TEST] Donanim testi...");
                display.showMessage("TEST BASLADI", "LED & ROLE");
                leds.greenOn(); delay(500); leds.allOff();
                leds.error(); delay(500); leds.allOff();
                buzzer.beepSuccess(); delay(200);
                lockController.unlock();
                delay(1000);
                lockController.lock();
                display.showMessage("TEST TAMAM", "Sistem Hazir");
                delay(1000);
                display.showMessage("Sistem Hazir", "Parmak Okutun");
                break;
            }
            case 'o': case 'O': {
                Serial.println("\n[MANUEL] Kapi ACILIYOR...");
                display.showMessage("KAPI DURUMU:", "ACIK (MANUEL)");
                lockController.unlock();
                Serial.println("[STATE] Backend lock state Unlocked gonderiliyor...");
                iotClient.sendLockState("Unlocked", "Manual unlock");
                leds.greenOn();
                buzzer.beepSuccess();
                break;
            }
            case 'k': case 'K': {
                Serial.println("\n[MANUEL] Kapi KILITLENIYOR...");
                display.showMessage("KAPI DURUMU:", "KILITLI");
                lockController.lock();
                Serial.println("[STATE] Backend lock state Locked gonderiliyor...");
                iotClient.sendLockState("Locked", "Manual lock");
                leds.allOff();
                delay(1500);
                display.showMessage("Sistem Hazir", "Parmak Okutun");
                break;
            }
            case 'c': case 'C': {
                Serial.println("\n[>] Tum parmak izi kayitlari siliniyor...");
                display.showMessage("SILIYOR...", "Lutfen Bekleyin");
                if (fpManager.deleteAll()) {
                    display.showMessage("TUMU SILINDI", "Sistem Sifirlandi");
                    Serial.println("[OK] Veritabani tertemiz.");
                    leds.success(); buzzer.beepWelcome(); delay(1000); leds.allOff();
                } else {
                    display.showMessage("SILME HATASI", "");
                    leds.error(); buzzer.beepError(); delay(1000); leds.allOff();
                }
                delay(1000);
                display.showMessage("Sistem Hazir", "Parmak Okutun");
                break;
            }
            case 't': case 'T': {
                autoTestActive = !autoTestActive;
                Serial.printf("\n[MOD] Otomatik Test: %s\n", autoTestActive ? "ACIK" : "KAPALI");
                display.showMessage("OTO TEST:", autoTestActive ? "ACIK" : "KAPALI");
                break;
            }
            case 'p': case 'P': {
                Serial.println("\n[>] PIN fabrika ayarlarina sifirlaniyor...");
                keypad.resetToDefault();
                display.showMessage("PIN SIFIRLANDI", "Varsayilan: 1234");
                buzzer.beepWelcome();
                delay(2000);
                display.showMessage("Sistem Hazir", "Parmak/PIN");
                break;
            }
            default: {
                Serial.printf("[?] Bilinmeyen komut: '%c'\n", cmd);
                Serial.println("[?] Gecerli komutlar: v e s c o k l t p");
                break;
            }
        }
    }

    // 2. OTOMATİK DÖNGÜ TESTİ (5 saniyede bir, millis ile)
    // Bu kısım kapının LOW/HIGH durumlarını periyodik test eder.
    static unsigned long lastTest = 0;
    static bool testState = false;
    
    // Sadece terminale 't' yazıldığında çalışır
    if (autoTestActive && (millis() - lastTest > 5000)) {
        testState = !testState;
        if (testState) {
            Serial.println("\n--- OTO TEST: ACILIYOR (Pin LOW) ---");
            display.showMessage("OTO: ACILIYOR", "Sinyal: LOW");
            lockController.unlock(); 
            leds.greenOn();
        } else {
            Serial.println("\n--- OTO TEST: KILITLENIYOR (Pin HIGH) ---");
            display.showMessage("OTO: KILITLI", "Sinyal: HIGH");
            lockController.lock();
            leds.allOff();
        }
        lastTest = millis();
    }

    // 3. Heartbeat
    static unsigned long hb = 0;
    if(millis() - hb > 15000) {
        Serial.println("[CANLI] Komut bekliyorum...");
        hb = millis();
    }
    delay(50);
}
