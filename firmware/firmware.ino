#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ======================= PIN TANIMLARI =======================
#define FP_RX_PIN       4
#define FP_TX_PIN       5

#define RELAY_PIN       1
#define BUZZER_PIN      0
#define GREEN_LED_PIN   10
#define RED_LED_PIN     3

#define SDA_PIN         8
#define SCL_PIN         9

// ======================= OLED =======================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ======================= FINGERPRINT =======================
HardwareSerial fingerSerial(1);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&fingerSerial);

// ======================= AYARLAR =======================
#define RELAY_ACTIVE_LEVEL HIGH   // Çalışmazsa LOW yap
#define RELAY_INACTIVE_LEVEL LOW

#define UNLOCK_TIME 5000

// ======================= YARDIMCI =======================
void showMessage(String l1, String l2 = "") {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println(l1);

  display.setCursor(0, 20);
  display.println(l2);

  display.display();
}

void beep(int t) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(t);
  digitalWrite(BUZZER_PIN, LOW);
}

void success() {
  digitalWrite(GREEN_LED_PIN, HIGH);
  digitalWrite(RED_LED_PIN, LOW);
  beep(100);
}

void fail() {
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, HIGH);
  beep(100);
  delay(100);
  beep(100);
}

void idle() {
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);
}

void unlockDoor() {
  digitalWrite(RELAY_PIN, RELAY_ACTIVE_LEVEL);
  showMessage("ACCESS GRANTED", "Door Open");
  success();

  delay(UNLOCK_TIME);

  digitalWrite(RELAY_PIN, RELAY_INACTIVE_LEVEL);
  idle();
  showMessage("Place Finger");
}

// ======================= FINGERPRINT =======================
int checkFingerprint() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) return -1;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return -1;

  p = finger.fingerSearch();

  if (p == FINGERPRINT_OK) {
    return finger.fingerID;
  } else if (p == FINGERPRINT_NOTFOUND) {
    return 0;
  } else {
    return -1;
  }
}

// ======================= SETUP =======================
void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);

  digitalWrite(RELAY_PIN, RELAY_INACTIVE_LEVEL);

  // OLED
  Wire.begin(SDA_PIN, SCL_PIN);
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  display.clearDisplay();

  showMessage("Smart Lock", "Starting...");

  // Fingerprint
  fingerSerial.begin(57600, SERIAL_8N1, FP_RX_PIN, FP_TX_PIN);
  finger.begin(57600);

  if (!finger.verifyPassword()) {
    showMessage("Sensor ERROR");
    while (1);
  }

  showMessage("Ready", "Place Finger");
}

// ======================= LOOP =======================
void loop() {
  int id = checkFingerprint();

  if (id > 0) {
    unlockDoor();
  }
  else if (id == 0) {
    showMessage("ACCESS DENIED");
    fail();
    delay(1500);
    idle();
    showMessage("Place Finger");
  }

  delay(100);
}
