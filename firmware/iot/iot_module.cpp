#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <vector>

std::vector<AccessLog> logQueue;

struct AccessLog {
  int user_id;
  bool success;
  String status;
  int fail_count;
  String time;
};

String logToJson(AccessLog log) {
  String json = "{";
  json += "\"user_id\":" + String(log.user_id) + ",";
  json += "\"success\":" + String(log.success ? "true" : "false") + ",";
  json += "\"status\":\"" + log.status + "\",";
  json += "\"fail_count\":" + String(log.fail_count) + ",";
  json += "\"time\":\"" + log.time + "\"";
  json += "}";
  return json;
}

void sendLog(AccessLog log) {

  if (WiFi.status() == WL_CONNECTED) {

    HTTPClient http;

    http.begin("https://httpbin.org/post");
    http.addHeader("Content-Type", "application/json");

    String json = logToJson(log);

    Serial.println("Gönderilen JSON:");
    Serial.println(json);

    int responseCode = http.POST(json);

    if (responseCode > 0) {
      Serial.print("HTTP Response: ");
      Serial.println(responseCode);
    } else {
      Serial.print("Hata: ");
      Serial.println(responseCode);
    }

    http.end();

  } else {
    Serial.println("WiFi bağlı değil!");
  }
}

void testLog() {

  AccessLog log;

  log.user_id = 2;
  log.success = true;
  log.status = "unlocked";
  log.fail_count = 0;
  log.time = "2026-03-19T15:00:00Z";

  Serial.println("Test log oluşturuldu");

  processLog(log);
}

void processLog(AccessLog log) {

 if (log.status == "lockout") {
    sendAlert();
  }

  if (WiFi.status() == WL_CONNECTED) {
    sendLog(log);
  } else {
    Serial.println("WiFi yok → kuyruğa eklendi");
    logQueue.push_back(log);
  }
}

void flushQueue() {

  if (WiFi.status() != WL_CONNECTED) return;

  Serial.println("Kuyruk gönderiliyor...");

  while (!logQueue.empty()) {
    sendLog(logQueue.front());
    logQueue.erase(logQueue.begin());
}

  logQueue.clear();
}

void checkConnection() {

  if (WiFi.status() == WL_CONNECTED) {
    flushQueue();
  } else {
    Serial.println("WiFi hala yok...");
  }
}

void sendAlert() {

  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;

  http.begin("https://your-api.com/api/alert");
  http.addHeader("Content-Type", "application/json");

  String json = "{\"message\":\"LOCKOUT DETECTED\"}";

  int code = http.POST(json);

  Serial.print("Alert gönderildi: ");
  Serial.println(code);

  http.end();
}