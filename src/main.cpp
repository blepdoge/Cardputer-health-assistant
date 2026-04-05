#include <Arduino.h>
#include <M5Cardputer.h>
#include <M5Unified.h>
#include <Wire.h>
#include "SparkFun_BMI270_Arduino_Library.h"
#include <SD.h>
#include <SPI.h>

BMI270 imu;

// --- THEME & SPRITE ---
uint16_t ACCENT_COLOR = M5.Display.color565(0, 150, 136);
uint16_t PASTEL_GREEN = M5.Display.color565(170, 230, 170);
uint16_t GOLDEN_YELLOW = M5.Display.color565(255, 215, 0);
uint16_t SMOOTH_ORANGE = M5.Display.color565(255, 140, 0);
uint16_t ALERT_RED = M5.Display.color565(220, 50, 50);
M5Canvas canvas(&M5Cardputer.Display); // Create a memory buffer for drawing

// --- STATE MACHINE & UI ---
int currentPage = 0;
bool needsRedraw = true;
int settingsCursor = 0;
uint32_t lastBlinkTime = 0;

// --- EDITING LOGIC ---
bool isEditing = false;
String editBuffer = "";

// --- HEALTH DATA ---
uint32_t stepCount = 0;
uint32_t lastStepCount = 0;
float distanceKm = 0.0;
float caloriesBurned = 0.0;

float weightKg = 70.0;
float heightCm = 175.0;
int dailyGoal = 10000;

// --- POWER MANAGEMENT ---
uint32_t lastInteractionTime = 0;
bool isScreenOn = true;
const uint32_t SCREEN_TIMEOUT = 30000; // 30 seconds in milliseconds

// --- SD LOGGING ---
bool sdReady = false; // Flag to prevent crashes if no SD card is inserted
uint32_t lastLogTime = 0;
uint32_t stepsAtLastLog = 0;
const uint32_t LOG_INTERVAL = 1200000; // 20 minutes

// --- HELPER: RECALCULATE METRICS ---
void updateMetrics() {
    float strideLengthM = heightCm * 0.00414;
    distanceKm = (stepCount * strideLengthM) / 1000.0;
    caloriesBurned = weightKg * distanceKm * 1.036;
}

// --- SD CARD SETTINGS LOGIC ---
void saveSettings() {
    if (!sdReady) return; // Abort silently if no SD card

    File file = SD.open("/m5_health/settings.txt", FILE_WRITE);
    if (file) {
        file.printf("goal=%d\n", dailyGoal);
        file.printf("height=%.1f\n", heightCm);
        file.printf("weight=%.1f\n", weightKg);
        file.close();
    }
}

void loadSettings() {
    if (!sdReady) return; // Abort silently if no SD card

    if (!SD.exists("/m5_health")) SD.mkdir("/m5_health");

    if (!SD.exists("/m5_health/settings.txt")) {
        saveSettings(); // File doesn't exist, create it with defaults
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
    if (!sdReady) return; // Abort silently if no SD card

    uint32_t stepDelta = stepCount - stepsAtLastLog;

    File file = SD.open("/m5_health/data.csv", FILE_APPEND);
    if (file) {
        // Placeholder timestamp until Wi-Fi/NTP is added
        String timeString = "14:20";
        file.printf("%s,%d\n", timeString.c_str(), stepDelta);
        file.close();
    }

    stepsAtLastLog = stepCount;
}

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);

    canvas.createSprite(240, 135);
    canvas.setFont(&fonts::FreeSans9pt7b);

    M5Cardputer.Display.setTextDatum(middle_center);
    M5Cardputer.Display.drawString("Booting IMU...", 120, 67);

    bool imuFound = false;
    if (imu.beginI2C(0x68, Wire1) == BMI2_OK) imuFound = true;
    else if (imu.beginI2C(0x69, Wire1) == BMI2_OK) imuFound = true;

    if (!imuFound) {
        M5Cardputer.Display.clear();
        M5Cardputer.Display.setTextColor(RED);
        M5Cardputer.Display.drawString("IMU FAILED", 120, 67);
        while(1);
    }

    imu.enableFeature(BMI2_STEP_COUNTER);

    // --- MANUAL SD CARD INITIALIZATION ---
    SPI.begin(40, 39, 14, 12); // SCK, MISO, MOSI, CS (CardputerADV exact pins)

    // Initialize SD at 25MHz for maximum stability on the Cardputer bus
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

        // Load settings from SD on boot
        loadSettings();
    }

    updateMetrics();

    // Reset interaction timer so screen doesn't instantly die on boot
    lastInteractionTime = millis();
}

// --- UNIVERSAL STATUS BAR ---
void drawStatusBar(String title) {
    canvas.fillRect(0, 0, 240, 20, PASTEL_GREEN);

    canvas.setTextColor(BLACK, PASTEL_GREEN);
    canvas.setTextDatum(middle_left);
    canvas.drawString(" " + title, 0, 10);

    int batLevel = M5.Power.getBatteryLevel();
    canvas.setTextDatum(middle_right);
    canvas.drawString(String(batLevel) + "% ", 240, 10);
}

// --- PAGE 0: DASHBOARD ---
void drawDashboard() {
    drawStatusBar("Dashboard");

    canvas.setFont(&fonts::Orbitron_Light_32);
    canvas.setTextSize(1);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(WHITE);
    canvas.drawString(String(stepCount), 120, 45);

    // Switch back to FreeSans font for the rest of the screen
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextSize(1);
    canvas.setTextColor(ACCENT_COLOR);
    canvas.drawString("STEPS", 120, 70);

    int barX = 20;
    int barY = 85;
    int barWidthMax = 200;

    int fillWidth = 0;
    if (dailyGoal > 0) {
        fillWidth = (int)(((float)stepCount / dailyGoal) * barWidthMax);
    }
    if (fillWidth > barWidthMax) fillWidth = barWidthMax;

    canvas.drawRect(barX, barY, barWidthMax, 8, LIGHTGREY);
    if (fillWidth > 0) {
        canvas.fillRect(barX, barY, fillWidth, 8, ACCENT_COLOR);
    }

    canvas.setTextDatum(middle_center);
    canvas.setTextColor(LIGHTGREY);
    canvas.drawString("Goal: " + String(dailyGoal), 120, 103);

    canvas.setTextDatum(bottom_center);
    canvas.setTextColor(ORANGE);
    canvas.drawString(String(caloriesBurned, 1) + " kcal  |  " + String(distanceKm, 2) + " km", 120, 130);
}

// --- PAGE 1: GRAPH ---
void drawGraph() {
    drawStatusBar("Daily activity");

    int graphX = 12; // Center the 216px wide graph on the 240px screen
    int graphY = 115;
    int barWidth = 3;
    int spacing = 2;

    // Dynamically find the maximum steps in any 20-minute interval
    int maxStepsPerInterval = 2000; // Baseline minimum scale
    for(int i = 0; i < 72; i++) {
        if(activityGraph[i] > maxStepsPerInterval) {
            maxStepsPerInterval = activityGraph[i];
        }
    }

    for(int i = 0; i < 72; i++) {
            int barHeight = map(activityGraph[i], 0, maxStepsPerInterval, 0, 80);
            if(barHeight < 0) barHeight = 0;
            if(barHeight > 80) barHeight = 80;

            // Apply absolute threshold colors based on activity intensity
            uint16_t color;
            if (activityGraph[i] >= 1500) {
                color = ALERT_RED;       // High intensity
            } else if (activityGraph[i] >= 500) {
                color = SMOOTH_ORANGE;   // Moderate intensity
            } else {
                color = GOLDEN_YELLOW;   // Low intensity
            }

            canvas.fillRect(graphX + (i * spacing), graphY - barHeight, barWidth, barHeight, color);
        }

    // Draw the main baseline
    canvas.drawLine(0, 116, 240, 116, WHITE);

    canvas.setTextDatum(top_center);
    canvas.setTextColor(LIGHTGREY);

    for (int h = 0; h <= 24; h += 4) {
        // graphX is 12. Each hour is 9 pixels wide (216px total / 24h = 9px)
        int xPos = 12 + (h * 9);

        // Draw a tiny 2-pixel tick mark dropping down from the main line
        canvas.drawLine(xPos, 116, xPos, 118, WHITE);

        // Draw the hour label right below the tick mark
        canvas.drawString(String(h) + "h", xPos, 120);
    }
}

// --- PAGE 2: SETTINGS ---
void drawSettings() {
    drawStatusBar("Settings");

    String menuItems[4] = {
        "Daily Goal: " + String(dailyGoal),
        "Height: " + String(heightCm, 1) + " cm",
        "Weight: " + String(weightKg, 1) + " kg",
        "Start WebUI Sync"
    };

    canvas.setTextSize(1);
    canvas.setTextDatum(middle_left);

    for (int i = 0; i < 4; i++) {
        int yPos = 40 + (i * 25);

        if (i == settingsCursor) {
            canvas.fillRect(5, yPos - 12, 230, 24, ACCENT_COLOR);
            canvas.setTextColor(WHITE, ACCENT_COLOR);
        } else {
            canvas.setTextColor(LIGHTGREY, BLACK);
        }
        canvas.drawString(" " + menuItems[i], 10, yPos);
    }

    if (isEditing) {
        canvas.fillRect(30, 35, 180, 65, DARKGREY);
        canvas.drawRect(30, 35, 180, 65, WHITE);

        canvas.setTextColor(WHITE, DARKGREY);
        canvas.setTextDatum(top_center);
        canvas.drawString("Enter New Value:", 120, 42);

        canvas.fillRect(40, 60, 160, 25, BLACK);
        canvas.setTextDatum(middle_center);

        String cursorChar = ((millis() / 500) % 2 == 0) ? "_" : " ";
        canvas.drawString(editBuffer + cursorChar, 120, 72);
    }
}

// --- MAIN LOOP ---
void loop() {
    M5Cardputer.update();

    // 1. Always poll the hardware
    imu.getStepCount(&stepCount);

    // 2. Did we take a step?
    if (stepCount != lastStepCount) {
        updateMetrics();
        lastStepCount = stepCount;

        if (isScreenOn && currentPage == 0) {
            needsRedraw = true;
        }
    }

    // 3. 20-Minute CSV Data Logging Timer
    if (millis() - lastLogTime >= LOG_INTERVAL) {
        logDataToSD();
        lastLogTime = millis();
    }

    // 4. Handle Keyboard & Screen Wakeup
    if (M5Cardputer.Keyboard.isChange()) {
        if (M5Cardputer.Keyboard.isPressed()) {
            Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

            lastInteractionTime = millis();

            if (!isScreenOn) {
                M5Cardputer.Display.setBrightness(128); // Force brightness back up
                isScreenOn = true;
                needsRedraw = true;
                return;
            }

            if (isEditing) {
                if (status.del) {
                    if (editBuffer.length() > 0) editBuffer.remove(editBuffer.length() - 1);
                }
                else if (status.enter) {
                    if (settingsCursor == 0) dailyGoal = editBuffer.toInt();
                    else if (settingsCursor == 1) heightCm = editBuffer.toFloat();
                    else if (settingsCursor == 2) weightKg = editBuffer.toFloat();

                    isEditing = false;
                    updateMetrics();
                    saveSettings(); // Save to SD Card!
                }
                else {
                    for (auto key : status.word) {
                        if (isDigit(key) || key == '.') editBuffer += key;
                    }
                }
                needsRedraw = true;
            }
            else {
                for (auto key : status.word) {
                    if (key == '/') {
                        currentPage = (currentPage + 1) % 3;
                        needsRedraw = true;
                    }
                    if (key == ',') {
                        currentPage = (currentPage == 0) ? 2 : currentPage - 1;
                        needsRedraw = true;
                    }
                    if (currentPage == 2) {
                        if (key == '.') {
                            settingsCursor = (settingsCursor + 1) % 4;
                            needsRedraw = true;
                        }
                        if (key == ';') {
                            settingsCursor = (settingsCursor == 0) ? 3 : settingsCursor - 1;
                            needsRedraw = true;
                        }
                    }
                }

                if (status.enter && currentPage == 2) {
                    if (settingsCursor < 3) {
                        isEditing = true;
                        editBuffer = "";
                        needsRedraw = true;
                    }
                }
            }
        }
    }

    // 5. SCREEN TIMEOUT CHECK
    if (isScreenOn && (millis() - lastInteractionTime > SCREEN_TIMEOUT)) {
        M5Cardputer.Display.setBrightness(0);
        isScreenOn = false;
        isEditing = false;
    }

    // 6. Blinking Cursor Timer
    if (isEditing && isScreenOn && (millis() - lastBlinkTime >= 500)) {
        needsRedraw = true;
        lastBlinkTime = millis();
    }

    // --- RENDER SCREEN FROM MEMORY ---
    if (needsRedraw && isScreenOn) {
        canvas.clear();

        if (currentPage == 0) drawDashboard();
        else if (currentPage == 1) drawGraph();
        else if (currentPage == 2) drawSettings();

        canvas.pushSprite(0, 0);
        needsRedraw = false;
    }
}
