#include "WebSync.h"
#include "AppState.h"
#include "SDManager.h"   // To trigger saveSettings()
#include "WebUI.h"  // Our baked-in website
#include <WiFi.h>
#include <time.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <SD.h>          // To read the CSV

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
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
    }
    return WiFi.status() == WL_CONNECTED;
}

void syncNTP() {
    configTime(timezoneOffset * 3600, 0, "pool.ntp.org", "time.nist.gov");
    struct tm timeinfo;
    int retry = 0;
    while (!getLocalTime(&timeinfo) && retry < 10) {
        delay(500);
        retry++;
    }
}

// --- WEB SERVER ENDPOINTS ---

void handleRoot() {
    // Serve the main HTML page
    server.send(200, "text/html", index_html);
}

void handleGetSettings() {
    // Build a JSON response with the live variables
    String json = "{";
    json += "\"ssid\":\"" + networkSSIDs[selectedNetworkIndex] + "\",";
    json += "\"goal\":" + String(dailyGoal) + ",";
    json += "\"height\":" + String(heightCm, 1) + ",";
    json += "\"weight\":" + String(weightKg, 1) + ",";
    json += "\"tz\":" + String(timezoneOffset);
    json += "}";
    server.send(200, "application/json", json);
}

void handlePostSettings() {
    // Update variables from the submitted HTML form
    if (server.hasArg("goal")) dailyGoal = server.arg("goal").toInt();
    if (server.hasArg("height")) heightCm = server.arg("height").toFloat();
    if (server.hasArg("weight")) weightKg = server.arg("weight").toFloat();
    if (server.hasArg("tz")) timezoneOffset = server.arg("tz").toInt();

    updateMetrics();
    saveSettings();     // Save them to the SD card!
    needsRedraw = true; // Flag the UI to update when we exit the WebUI

    server.send(200, "text/plain", "Settings Updated Successfully");
}

void handleGetCSV() {
    // Stream the data log directly from the SD card to the browser!
    if (!sdReady || !SD.exists("/m5_health/data.csv")) {
        server.send(404, "text/plain", "No data found");
        return;
    }
    File file = SD.open("/m5_health/data.csv", FILE_READ);
    server.streamFile(file, "text/csv");
    file.close();
}

// --- SERVER LIFECYCLE ---

void startWebUI_Server() {
    if (!MDNS.begin("cardputer")) {
        Serial.println("Error setting up MDNS responder!");
    }

    // Attach endpoints to functions
    server.on("/", HTTP_GET, handleRoot);
    server.on("/api/settings", HTTP_GET, handleGetSettings);
    server.on("/api/settings", HTTP_POST, handlePostSettings);
    server.on("/api/data.csv", HTTP_GET, handleGetCSV);

    server.begin();
    MDNS.addService("http", "tcp", 80);
}

void handleWebUI() {
    server.handleClient();
}

void stopWebUI_Server() {
    server.stop();
    MDNS.end();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}
