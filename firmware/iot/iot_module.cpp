#include <Arduino.h>

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