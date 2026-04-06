#include "SDManager.h"
#include "AppState.h"
#include <SD.h>
#include <SPI.h>
#include <time.h>

void setupSD() {
    SPI.begin(40, 39, 14, 12); // SCK, MISO, MOSI, CS
    if (SD.begin(12, SPI, 25000000)) {
        sdReady = true;
        if (!SD.exists("/m5_health")) {
            SD.mkdir("/m5_health");
            File file = SD.open("/m5_health/data.csv", FILE_WRITE);
            if (file) {
                file.println("Time,StepDelta");
                file.close();
            }
        }
        loadSettings();
        loadActivityGraph();
    }
}

void saveSettings() {
    if (!sdReady) return;
    File file = SD.open("/m5_health/settings.txt", FILE_WRITE);
    if (file) {
        file.printf("goal=%d\n", dailyGoal);
        file.printf("height=%.1f\n", heightCm);
        file.printf("weight=%.1f\n", weightKg);
        file.printf("tz=%d\n", timezoneOffset);
        file.close();
    }
}

void loadSettings() {
    if (!sdReady) return;
    if (!SD.exists("/m5_health/settings.txt")) {
        saveSettings();
        return;
    }
    File file = SD.open("/m5_health/settings.txt", FILE_READ);
    if (file) {
        while (file.available()) {
            String line = file.readStringUntil('\n');
            line.trim();
            if (line.startsWith("goal=")) dailyGoal = line.substring(5).toInt();
            else if (line.startsWith("height=")) heightCm = line.substring(7).toFloat();
            else if (line.startsWith("weight=")) weightKg = line.substring(7).toFloat();
            else if (line.startsWith("tz=")) timezoneOffset = line.substring(3).toInt();
        }
        file.close();
    }
}

void logDataToSD() {
    if (!sdReady) return;
    uint32_t stepDelta = stepCount - stepsAtLastLog;

    // --- Update the live array so the graph stays current ---
    for (int i = 0; i < 71; i++) {
        activityGraph[i] = activityGraph[i+1];
    }
    activityGraph[71] = stepDelta;

    struct tm timeinfo;
    String timeString = "00:00";

    // If the clock has been synced, grab the real hours and minutes!
    if (getLocalTime(&timeinfo)) {
        char timeStr[10];
        strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
        timeString = String(timeStr);
    }

    File file = SD.open("/m5_health/data.csv", FILE_APPEND);
    if (file) {
        file.printf("%s,%d\n", timeString.c_str(), stepDelta);
        file.close();
    }
    stepsAtLastLog = stepCount;
}

String getSavedWiFiPassword(String ssid) {
    if (!sdReady || !SD.exists("/m5_health/networks.txt")) return "";

    File file = SD.open("/m5_health/networks.txt", FILE_READ);
    if (file) {
        while (file.available()) {
            String line = file.readStringUntil('\n');
            line.trim();
            int commaIndex = line.indexOf(',');
            if (commaIndex != -1) {
                String savedSSID = line.substring(0, commaIndex);
                if (savedSSID == ssid) {
                    file.close();
                    return line.substring(commaIndex + 1); // Return the password
                }
            }
        }
        file.close();
    }
    return "";
}

void saveWiFiNetwork(String ssid, String password) {
    if (!sdReady) return;
    File file = SD.open("/m5_health/networks.txt", FILE_APPEND);
    if (file) {
        file.printf("%s,%s\n", ssid.c_str(), password.c_str());
        file.close();
    }
}

void loadActivityGraph() {
    if (!sdReady) return;
    if (!SD.exists("/m5_health/data.csv")) return;
    
    File file = SD.open("/m5_health/data.csv", FILE_READ);
    if (!file) return;
    
    // Header should be skipped
    if (file.available()) {
        file.readStringUntil('\n');
    }
    
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;
        
        int commaIndex = line.indexOf(',');
        if (commaIndex != -1) {
            int stepDelta = line.substring(commaIndex + 1).toInt();
            
            // Shift elements to the left by 1 and put new element at the end
            for(int i = 0; i < 71; i++){
                activityGraph[i] = activityGraph[i+1];
            }
            activityGraph[71] = stepDelta;
        }
    }
    file.close();
}
