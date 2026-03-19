#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

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

    http.begin("https://httpbin.org/post"); // test server
    http.addHeader("Content-Type", "application/json");

    String json = logToJson(log);

    int responseCode = http.POST(json);

    Serial.print("HTTP Response: ");
    Serial.println(responseCode);

    http.end();

  } else {
    Serial.println("WiFi bağlı değil!");
  }
}