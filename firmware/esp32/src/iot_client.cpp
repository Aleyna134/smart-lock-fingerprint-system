#include "iot_client.h"
#include "network_config.h"

#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>

static Preferences logPrefs;

namespace {
const char* wifiStatusName(wl_status_t status) {
    switch (status) {
        case WL_IDLE_STATUS: return "WL_IDLE_STATUS";
        case WL_NO_SSID_AVAIL: return "WL_NO_SSID_AVAIL";
        case WL_SCAN_COMPLETED: return "WL_SCAN_COMPLETED";
        case WL_CONNECTED: return "WL_CONNECTED";
        case WL_CONNECT_FAILED: return "WL_CONNECT_FAILED";
        case WL_CONNECTION_LOST: return "WL_CONNECTION_LOST";
        case WL_DISCONNECTED: return "WL_DISCONNECTED";
        default: return "UNKNOWN";
    }
}

void printNearbyNetworks() {
    Serial.println("[IOT][WiFi] Scanning nearby networks...");
    int count = WiFi.scanNetworks();
    if (count <= 0) {
        Serial.printf("[IOT][WiFi] Scan found no networks. result=%d\n", count);
        return;
    }

    Serial.printf("[IOT][WiFi] Scan found %d networks:\n", count);
    for (int i = 0; i < count; i++) {
        Serial.printf("[IOT][WiFi]   ssid='%s' rssi=%d channel=%d enc=%d\n",
                      WiFi.SSID(i).c_str(),
                      WiFi.RSSI(i),
                      WiFi.channel(i),
                      WiFi.encryptionType(i));
    }
}
}

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
    command.userName = extractString(payload, "user_name");
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

bool IotClient::postAccessLog(int userId, bool success, const String& status, int failCount) {
    HTTPClient http;
    String url = buildUrl("/api/access-log");
    String body = "{";
    body += "\"success\":" + String(success ? "true" : "false") + ",";
    body += "\"status\":\"" + escapeJson(status) + "\",";
    body += "\"fail_count\":" + String(failCount) + ",";
    body += "\"user_id\":";
    body += (userId > 0) ? String(userId) : "null";
    body += "}";

    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(body);
    http.end();

    Serial.printf("[IOT] Access log sent. HTTP %d\n", code);
    return (code >= 200 && code < 300);
}

int IotClient::getBufferCount() {
    logPrefs.begin("logbuf", true);
    int n = logPrefs.getInt("n", 0);
    logPrefs.end();
    return n;
}

void IotClient::bufferAccessLog(int userId, bool success, const String& status, int failCount) {
    logPrefs.begin("logbuf", false);
    int n = logPrefs.getInt("n", 0);
    if (n >= LOG_BUFFER_MAX) {
        Serial.printf("[IOT][BUF] Buffer dolu (%d), en eski log silindi.\n", LOG_BUFFER_MAX);
        for (int i = 0; i < n - 1; i++) {
            logPrefs.putInt(("u" + String(i)).c_str(), logPrefs.getInt(("u" + String(i + 1)).c_str(), 0));
            logPrefs.putBool(("s" + String(i)).c_str(), logPrefs.getBool(("s" + String(i + 1)).c_str(), false));
            logPrefs.putString(("t" + String(i)).c_str(), logPrefs.getString(("t" + String(i + 1)).c_str(), ""));
            logPrefs.putInt(("f" + String(i)).c_str(), logPrefs.getInt(("f" + String(i + 1)).c_str(), 0));
        }
        n = LOG_BUFFER_MAX - 1;
    }
    logPrefs.putInt(("u" + String(n)).c_str(), userId);
    logPrefs.putBool(("s" + String(n)).c_str(), success);
    logPrefs.putString(("t" + String(n)).c_str(), status.substring(0, 62));
    logPrefs.putInt(("f" + String(n)).c_str(), failCount);
    logPrefs.putInt("n", n + 1);
    logPrefs.end();
    Serial.printf("[IOT][BUF] Log buffera alindi. Toplam: %d\n", n + 1);
}

void IotClient::clearBuffer() {
    logPrefs.begin("logbuf", false);
    logPrefs.clear();
    logPrefs.end();
}

void IotClient::flushBufferedLogs() {
    if (!ensureWiFi()) return;

    int n = getBufferCount();
    if (n == 0) return;

    Serial.printf("[IOT][BUF] %d bekleyen log gonderiliyor...\n", n);

    logPrefs.begin("logbuf", true);
    int sent = 0;
    for (int i = 0; i < n; i++) {
        int uid    = logPrefs.getInt(("u" + String(i)).c_str(), 0);
        bool ok    = logPrefs.getBool(("s" + String(i)).c_str(), false);
        String st  = logPrefs.getString(("t" + String(i)).c_str(), "");
        int fc     = logPrefs.getInt(("f" + String(i)).c_str(), 0);
        logPrefs.end();

        if (postAccessLog(uid, ok, st, fc)) {
            sent++;
        }
        logPrefs.begin("logbuf", true);
    }
    logPrefs.end();

    if (sent == n) {
        clearBuffer();
        Serial.printf("[IOT][BUF] Tum %d log gonderildi, buffer temizlendi.\n", sent);
    } else {
        Serial.printf("[IOT][BUF] %d/%d log gonderilemedi, buffer korundu.\n", n - sent, n);
    }
}

bool IotClient::sendAccessLog(int userId, bool success, const String& status, int failCount) {
    if (!ensureWiFi()) {
        Serial.println("[IOT] WiFi yok, log buffera aliniyor.");
        bufferAccessLog(userId, success, status, failCount);
        return false;
    }

    flushBufferedLogs();
    return postAccessLog(userId, success, status, failCount);
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

    static unsigned long lastDebugAt = 0;
    const bool shouldDebug = millis() - lastDebugAt > 15000;
    if (shouldDebug) {
        lastDebugAt = millis();
        Serial.printf("[IOT][WiFi] Connecting to ssid='%s' backend='%s'\n", WIFI_SSID, BACKEND_URL);
    }

    WiFi.disconnect(false);
    delay(100);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    unsigned long startedAt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startedAt < WIFI_CONNECT_TIMEOUT_MS) {
        delay(250);
    }

    wl_status_t status = WiFi.status();
    if (status == WL_CONNECTED) {
        Serial.printf("[IOT][WiFi] Connected. ip=%s rssi=%d\n",
                      WiFi.localIP().toString().c_str(),
                      WiFi.RSSI());
        return true;
    }

    if (shouldDebug) {
        Serial.printf("[IOT][WiFi] Connect failed. status=%d (%s)\n", status, wifiStatusName(status));
        printNearbyNetworks();
    }

    return false;
}

int IotClient::keypadEnrollUser() {
    if (!ensureWiFi()) {
        Serial.println("[IOT][KEYPAD] WiFi yok, kullanici olusturulamadi.");
        return -1;
    }

    HTTPClient http;
    http.begin(buildUrl("/api/keypad-enroll"));
    http.addHeader("Content-Type", "application/json");
    int code = http.POST("{}");
    String response = http.getString();
    http.end();

    if (code != 201) {
        Serial.printf("[IOT][KEYPAD] Kullanici olusturulamadi. HTTP %d\n", code);
        return -1;
    }

    int userId = extractInt(response, "user_id");
    Serial.printf("[IOT][KEYPAD] Bos kullanici olusturuldu. user_id=%d\n", userId);
    return userId;
}

bool IotClient::keypadDeleteUser(int templateId) {
    if (!ensureWiFi()) {
        Serial.println("[IOT][KEYPAD] WiFi yok, kullanici silinemedi.");
        return false;
    }

    HTTPClient http;
    http.begin(buildUrl("/api/keypad-delete/" + String(templateId)));
    int code = http.sendRequest("DELETE");
    http.end();

    Serial.printf("[IOT][KEYPAD] Kullanici silindi. user_id=%d HTTP %d\n", templateId, code);
    return (code >= 200 && code < 300);
}

bool IotClient::keypadDeleteAllUsers() {
    if (!ensureWiFi()) {
        Serial.println("[IOT][KEYPAD] WiFi yok, toplu silme yapilamadi.");
        return false;
    }

    HTTPClient http;
    http.begin(buildUrl("/api/keypad-delete-all"));
    int code = http.sendRequest("DELETE");
    http.end();

    Serial.printf("[IOT][KEYPAD] Tum kullanicilar silindi. HTTP %d\n", code);
    return (code >= 200 && code < 300);
}

void IotClient::saveUserName(int templateId, const String& name) {
    Preferences p;
    p.begin("usernames", false);
    p.putString(String(templateId).c_str(), name.substring(0, 20));
    p.end();
}

String IotClient::fetchUserName(int templateId) {
    if (!ensureWiFi()) return loadUserName(templateId);

    HTTPClient http;
    http.begin(buildUrl("/api/users/" + String(templateId) + "/name"));
    int code = http.GET();
    if (code != 200) {
        http.end();
        return loadUserName(templateId);
    }

    String payload = http.getString();
    http.end();

    String name = extractString(payload, "name");
    if (name.length() > 0) {
        saveUserName(templateId, name);
    }
    return name.length() > 0 ? name : loadUserName(templateId);
}

String IotClient::loadUserName(int templateId) {
    Preferences p;
    p.begin("usernames", true);
    String name = p.getString(String(templateId).c_str(), "");
    p.end();
    return name;
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
