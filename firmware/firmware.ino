#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>

HardwareSerial fingerSerial(1);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&fingerSerial);

#define RX_PIN 4
#define TX_PIN 5
#define RELAY_PIN 6

void setup() {
  Serial.begin(115200);

  fingerSerial.begin(57600, SERIAL_8N1, RX_PIN, TX_PIN);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  if (finger.verifyPassword()) {
    Serial.println("Fingerprint sensor found");
  } else {
    Serial.println("Sensor not found");
    while (1);
  }
}

void loop() {
  if (getFingerprintID() == FINGERPRINT_OK) {
    unlockDoor();
  }
  delay(50);
}

uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) return p;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return p;

  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK) return p;

  return FINGERPRINT_OK;
}

void unlockDoor() {
  Serial.println("ACCESS GRANTED");

  digitalWrite(RELAY_PIN, HIGH);
  delay(3000);
  digitalWrite(RELAY_PIN, LOW);
}
