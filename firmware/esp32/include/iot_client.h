#ifndef IOT_CLIENT_H
#define IOT_CLIENT_H

#include <Arduino.h>

struct EnrollmentCommand {
    bool available;
    int id;
    int templateId;
    String type;
};

class IotClient {
public:
    void begin();
    void updateWiFi();
    bool isConnected() const;

    EnrollmentCommand pollEnrollmentCommand();
    bool sendEnrollmentResult(int commandId, bool success, int templateId, const String& message);
    bool sendAccessLog(int userId, bool success, const String& status, int failCount);
    bool sendLockState(const String& lockStatus, const String& event);

private:
    bool ensureWiFi();
    String buildUrl(const String& path) const;
    String escapeJson(const String& value) const;
    int extractInt(const String& payload, const String& key) const;
    String extractString(const String& payload, const String& key) const;
};

#endif
