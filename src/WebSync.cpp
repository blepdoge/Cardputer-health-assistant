#include "WebSync.h"
#include "AppState.h"
#include <WiFi.h>
#include <time.h>

void scanWiFiNetworks() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    networkCount = WiFi.scanNetworks();
    if (networkCount > 15) networkCount = 15;
    for (int i = 0; i < networkCount; ++i) {
        networkSSIDs[i] = WiFi.SSID(i);
    }
}

bool connectToWiFi(String ssid, String password) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    int attempts = 0;
    // Wait up to 10 seconds for connection
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
    }
    return WiFi.status() == WL_CONNECTED;
}

void syncNTP() {
    // configTime takes offset in seconds (hours * 3600)
    configTime(timezoneOffset * 3600, 0, "pool.ntp.org", "time.nist.gov");

    struct tm timeinfo;
    int retry = 0;
    while (!getLocalTime(&timeinfo) && retry < 10) {
        delay(500);
        retry++;
    }
}
