#include "iot_client.h"
#include "network_config.h"

#include <HTTPClient.h>
#include <WiFi.h>

void IotClient::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    ensureWiFi();
}

void IotClient::updateWiFi() {
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
}

bool IotClient::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

EnrollmentCommand IotClient::pollEnrollmentCommand() {
    EnrollmentCommand command = { false, 0, 0, "" };

    if (!ensureWiFi()) {
        Serial.println("[IOT] WiFi unavailable, command polling skipped.");
        return command;
    }

    HTTPClient http;
    String url = buildUrl("/api/hardware/commands/next?device_id=" + String(DEVICE_ID));
    http.begin(url);
    int code = http.GET();

    if (code != HTTP_CODE_OK) {
        Serial.printf("[IOT] Command poll failed. HTTP %d\n", code);
        http.end();
        return command;
    }

    String payload = http.getString();
    http.end();

    if (payload.indexOf("\"command\":null") >= 0) {
        return command;
    }

    command.id = extractInt(payload, "id");
    command.templateId = extractInt(payload, "template_id");
    command.type = extractString(payload, "type");
    command.available = command.id > 0
        && command.templateId > 0
        && (command.type == "ENROLL_FINGERPRINT" || command.type == "DELETE_FINGERPRINT");

    if (command.available) {
        Serial.printf("[IOT] Hardware command received. command_id=%d type=%s template_id=%d\n",
                      command.id, command.type.c_str(), command.templateId);
    } else {
        Serial.printf("[IOT] Unsupported or invalid command payload: %s\n", payload.c_str());
    }

    return command;
}

bool IotClient::sendEnrollmentResult(int commandId, bool success, int templateId, const String& message) {
    if (!ensureWiFi()) {
        Serial.println("[IOT] WiFi unavailable, enrollment result not sent.");
        return false;
    }

    HTTPClient http;
    String url = buildUrl("/api/hardware/commands/" + String(commandId) + "/result");
    String body = "{";
    body += "\"success\":" + String(success ? "true" : "false") + ",";
    body += "\"template_id\":" + String(templateId) + ",";
    body += "\"message\":\"" + escapeJson(message) + "\"";
    body += "}";

    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(body);
    String response = http.getString();
    http.end();

    Serial.printf("[IOT] Enrollment result sent. HTTP %d\n", code);
    if (code < 200 || code >= 300) {
        Serial.println(response);
        return false;
    }
    return true;
}

bool IotClient::sendAccessLog(int userId, bool success, const String& status, int failCount) {
    if (!ensureWiFi()) {
        Serial.println("[IOT] WiFi unavailable, access log not sent.");
        return false;
    }

    HTTPClient http;
    String url = buildUrl("/api/access-log");
    String body = "{";
    body += "\"success\":" + String(success ? "true" : "false") + ",";
    body += "\"status\":\"" + escapeJson(status) + "\",";
    body += "\"fail_count\":" + String(failCount) + ",";
    body += "\"user_id\":";
    if (userId > 0) {
        body += String(userId);
    } else {
        body += "null";
    }
    body += "}";

    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(body);
    String response = http.getString();
    http.end();

    Serial.printf("[IOT] Access log sent. HTTP %d\n", code);
    if (code < 200 || code >= 300) {
        Serial.println(response);
        return false;
    }
    return true;
}

bool IotClient::sendLockState(const String& lockStatus, const String& event) {
    if (!ensureWiFi()) {
        Serial.println("[IOT] WiFi unavailable, lock state not sent.");
        return false;
    }

    HTTPClient http;
    String url = buildUrl("/api/hardware/state");
    String body = "{";
    body += "\"lock_status\":\"" + escapeJson(lockStatus) + "\",";
    body += "\"event\":\"" + escapeJson(event) + "\"";
    body += "}";

    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    Serial.printf("[IOT] Sending lock state. url=%s body=%s\n", url.c_str(), body.c_str());
    int code = http.POST(body);
    String response = http.getString();
    http.end();

    Serial.printf("[IOT] Lock state sent. status=%s event=%s HTTP %d\n",
                  lockStatus.c_str(), event.c_str(), code);
    if (response.length() > 0) {
        Serial.printf("[IOT] Lock state response: %s\n", response.c_str());
    }
    if (code < 200 || code >= 300) {
        return false;
    }
    return true;
}

bool IotClient::ensureWiFi() {
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    unsigned long startedAt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startedAt < WIFI_CONNECT_TIMEOUT_MS) {
        delay(250);
    }

    return WiFi.status() == WL_CONNECTED;
}

String IotClient::buildUrl(const String& path) const {
    String base = String(BACKEND_URL);
    if (base.endsWith("/") && path.startsWith("/")) {
        base.remove(base.length() - 1);
    }
    return base + path;
}

String IotClient::escapeJson(const String& value) const {
    String escaped;
    escaped.reserve(value.length());

    for (size_t i = 0; i < value.length(); i++) {
        char c = value.charAt(i);
        if (c == '"' || c == '\\') {
            escaped += '\\';
        }
        escaped += c;
    }

    return escaped;
}

int IotClient::extractInt(const String& payload, const String& key) const {
    String token = "\"" + key + "\":";
    int start = payload.indexOf(token);
    if (start < 0) {
        return 0;
    }

    start += token.length();
    while (start < payload.length() && payload.charAt(start) == ' ') {
        start++;
    }

    int end = start;
    while (end < payload.length() && isDigit(payload.charAt(end))) {
        end++;
    }

    return payload.substring(start, end).toInt();
}

String IotClient::extractString(const String& payload, const String& key) const {
    String token = "\"" + key + "\":\"";
    int start = payload.indexOf(token);
    if (start < 0) {
        return "";
    }

    start += token.length();
    int end = payload.indexOf("\"", start);
    if (end < 0) {
        return "";
    }

    return payload.substring(start, end);
}
