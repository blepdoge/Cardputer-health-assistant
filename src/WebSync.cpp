#include "WebSync.h"
#include "AppState.h"
#include <WiFi.h>

void scanWiFiNetworks() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // This function halts the code for ~2 seconds while the hardware scans
    networkCount = WiFi.scanNetworks();

    if (networkCount > 15) networkCount = 15; // Cap to our array size

    for (int i = 0; i < networkCount; ++i) {
        networkSSIDs[i] = WiFi.SSID(i);
    }
}
