#ifndef IOT_CLIENT_H
#define IOT_CLIENT_H

#include <Arduino.h>

#define LOG_BUFFER_MAX 20

struct EnrollmentCommand {
    bool available;
    int id;
    int templateId;
    String type;
    String userName;
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
    void flushBufferedLogs();
    void saveUserName(int templateId, const String& name);
    String loadUserName(int templateId);
    String fetchUserName(int templateId);
    int keypadEnrollUser();
    bool keypadDeleteUser(int templateId);
    bool keypadDeleteAllUsers();

private:
    bool ensureWiFi();
    bool postAccessLog(int userId, bool success, const String& status, int failCount);
    void bufferAccessLog(int userId, bool success, const String& status, int failCount);
    int getBufferCount();
    void clearBuffer();
    String buildUrl(const String& path) const;
    String escapeJson(const String& value) const;
    int extractInt(const String& payload, const String& key) const;
    String extractString(const String& payload, const String& key) const;
};

#endif
