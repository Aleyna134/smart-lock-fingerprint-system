#ifndef NETWORK_CONFIG_H
#define NETWORK_CONFIG_H

// Create include/network_config.local.h to override these values locally.
// Do not commit real WiFi credentials. BACKEND_URL must be reachable from the
// ESP32 on the same WiFi network.
#if __has_include("network_config.local.h")
#include "network_config.local.h"
#else
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define BACKEND_URL "http://192.168.1.100:3000"
#define DEVICE_ID "lock-1"
#endif

#define IOT_POLL_INTERVAL_MS 5000UL
#define WIFI_CONNECT_TIMEOUT_MS 5000UL

#ifndef VERIFY_SCAN_INTERVAL_MS
#define VERIFY_SCAN_INTERVAL_MS 300UL
#endif

#ifndef VERIFY_COOLDOWN_MS
#define VERIFY_COOLDOWN_MS 2500UL
#endif

#ifndef UNLOCK_DURATION_MS
#define UNLOCK_DURATION_MS 3000UL
#endif

#endif
