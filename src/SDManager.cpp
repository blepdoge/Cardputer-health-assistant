#include "SDManager.h"
#include "AppState.h"
#include <SD.h>
#include <SPI.h>

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
    }
}

void saveSettings() {
    if (!sdReady) return;
    File file = SD.open("/m5_health/settings.txt", FILE_WRITE);
    if (file) {
        file.printf("goal=%d\n", dailyGoal);
        file.printf("height=%.1f\n", heightCm);
        file.printf("weight=%.1f\n", weightKg);
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
        }
        file.close();
    }
}

void logDataToSD() {
    if (!sdReady) return;
    uint32_t stepDelta = stepCount - stepsAtLastLog;
    File file = SD.open("/m5_health/data.csv", FILE_APPEND);
    if (file) {
        String timeString = "14:20"; // Placeholder until NTP
        file.printf("%s,%d\n", timeString.c_str(), stepDelta);
        file.close();
    }
    stepsAtLastLog = stepCount;
}
