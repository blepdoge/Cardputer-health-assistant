#include "WebSync.h"
#include "AppState.h"
#include <WiFi.h>
#include <time.h>
#include <WebServer.h>
#include <ESPmDNS.h>

WebServer server(80);

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

void startWebUI_Server() {
    // 1. Start the mDNS responder for "cardputer.local"
    if (!MDNS.begin("cardputer")) {
        Serial.println("Error setting up MDNS responder!");
    }

    // 2. Define what happens when someone visits the root URL "/"
    server.on("/", HTTP_GET, []() {
        server.send(200, "text/plain", "Hi Cardputer");
    });

    // 3. Start the server
    server.begin();
    MDNS.addService("http", "tcp", 80);
}

void handleWebUI() {
    server.handleClient(); // Listen for incoming browser requests
}

void stopWebUI_Server() {
    server.stop();
    MDNS.end();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}
