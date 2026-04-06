#include "AppState.h"
#include "WebSync.h"
#include "SDManager.h"
#include "DisplayUI.h"
#include <WiFi.h>

// --- VARIABLE INSTANTIATION ---
BMI270 imu;
M5Canvas canvas(&M5Cardputer.Display);

uint16_t ACCENT_COLOR = M5.Display.color565(0, 150, 136);
uint16_t PASTEL_GREEN = M5.Display.color565(170, 230, 170);
uint16_t GOLDEN_YELLOW = M5.Display.color565(255, 215, 0);
uint16_t SMOOTH_ORANGE = M5.Display.color565(255, 140, 0);
uint16_t ALERT_RED = M5.Display.color565(220, 50, 50);

int currentPage = 0;
bool needsRedraw = true;
int settingsCursor = 0;
bool isEditing = false;
String editBuffer = "";

// Wi-Fi Variables
bool isScanningWiFi = false;
int wifiCursor = 0;
int selectedNetworkIndex = -1;
String wifiPasswordBuffer = "";
bool isEnteringWiFiPassword = false;
int networkCount = 0;
bool isConnectingWiFi = false;
bool isSyncingNTP = false;
bool isWebUIRunning = false;
String networkSSIDs[15];

uint32_t stepCount = 0;
uint32_t lastStepCount = 0;
float distanceKm = 0.0;
float caloriesBurned = 0.0;
float weightKg = 70.0;
float heightCm = 175.0;
int dailyGoal = 10000;
int timezoneOffset = 0; // Default to UTC time
int activityGraph[72] = {
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  50, 100, 200, 150, 300, 400, 500, 200, 100,
  0, 0, 50, 0, 0, 20, 10, 0, 0,
  200, 300, 250, 100, 50, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0,
  400, 600, 800, 1600, 500, 100, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0
};

uint32_t lastInteractionTime = 0;
bool isScreenOn = true;
const uint32_t SCREEN_TIMEOUT = 30000;

bool sdReady = false;
uint32_t lastLogTime = 0;
uint32_t stepsAtLastLog = 0;
const uint32_t LOG_INTERVAL = 1200000;
uint32_t lastImuPoll = 0;

void updateMetrics() {
    float strideLengthM = heightCm * 0.00414;
    distanceKm = (stepCount * strideLengthM) / 1000.0;
    caloriesBurned = weightKg * distanceKm * 1.036;
}

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    setCpuFrequencyMhz(80);

    canvas.createSprite(240, 135);
    canvas.setFont(&fonts::FreeSans9pt7b);

    M5Cardputer.Display.setTextDatum(middle_center);
    M5Cardputer.Display.drawString("Booting IMU...", 120, 67);

    if (imu.beginI2C(0x68, Wire1) != BMI2_OK && imu.beginI2C(0x69, Wire1) != BMI2_OK) {
        M5Cardputer.Display.clear();
        M5Cardputer.Display.setTextColor(RED);
        M5Cardputer.Display.drawString("IMU FAILED", 120, 67);
        while(1);
    }
    imu.enableFeature(BMI2_STEP_COUNTER);

    setupSD(); // Hands off initialization to SDManager
    updateMetrics();

    lastInteractionTime = millis();
}

// --- MAIN LOOP ---
void loop() {
    M5Cardputer.update();
    if (isWebUIRunning) {
            handleWebUI();
        }

    // 1. Poll the hardware ONLY once every 2s
    if (millis() - lastImuPoll >= 2000) {
        imu.getStepCount(&stepCount);
        lastImuPoll = millis();

        // 2. Did we take a step?
        if (stepCount != lastStepCount) {
            updateMetrics();
            lastStepCount = stepCount;

            if (isScreenOn && currentPage == 0) {
                needsRedraw = true;
            }
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
            if (isWebUIRunning) {
                // Intercept keyboard entirely if WebUI is live
                if (status.del) { // Backspace key
                    stopWebUI_Server();
                    setCpuFrequencyMhz(80); // Drop CPU back down to save battery
                    isWebUIRunning = false;
                    currentPage = 0; // Go back to Dashboard
                    needsRedraw = true;
                }
                return; // Stop processing other keys
            }

            if (!isScreenOn) {
                M5Cardputer.Display.setBrightness(128); // Force brightness back up
                isScreenOn = true;
                needsRedraw = true;
            }
            else if (isEditing) {
                // --- SETTINGS EDITING LOGIC ---
                if (status.del && editBuffer.length() > 0) {
                    editBuffer.remove(editBuffer.length() - 1);
                }
                else if (status.enter) {
                    if (settingsCursor == 0) dailyGoal = editBuffer.toInt();
                    else if (settingsCursor == 1) heightCm = editBuffer.toFloat();
                    else if (settingsCursor == 2) weightKg = editBuffer.toFloat();
                    else if (settingsCursor == 3) timezoneOffset = editBuffer.toInt();

                    isEditing = false;
                    updateMetrics();
                    saveSettings(); // Save to SD Card!
                }
                else {
                    for (auto key : status.word) {
                        if (isDigit(key) || key == '.' || key == '-') editBuffer += key;
                    }
                }
                needsRedraw = true;
            }
            else if (currentPage == 3) {
                // --- WI-FI MENU LOGIC ---
                if (isEnteringWiFiPassword) {
                    if (status.del && wifiPasswordBuffer.length() > 0) {
                        wifiPasswordBuffer.remove(wifiPasswordBuffer.length() - 1);
                    }
                    else if (status.enter) {
                        isEnteringWiFiPassword = false;
                        saveWiFiNetwork(networkSSIDs[selectedNetworkIndex], wifiPasswordBuffer);
                        // Password found! Start connecting.
                        isConnectingWiFi = true;
                    }
                    else {
                        for (auto key : status.word) wifiPasswordBuffer += key;
                    }
                    needsRedraw = true;
                } else {
                    for (auto key : status.word) {
                        if (key == ',') { currentPage = 2; needsRedraw = true; } // Back to settings
                        if (key == '.') { wifiCursor = (wifiCursor + 1) % networkCount; needsRedraw = true; }
                        if (key == ';') { wifiCursor = (wifiCursor == 0) ? networkCount - 1 : wifiCursor - 1; needsRedraw = true; }
                    }
                    if (status.enter && networkCount > 0) {
                        selectedNetworkIndex = wifiCursor;
                        String savedPw = getSavedWiFiPassword(networkSSIDs[selectedNetworkIndex]);

                        if (savedPw != "") {
                            // Password found! Start connecting.
                            wifiPasswordBuffer = savedPw;
                            isConnectingWiFi = true;
                        } else {
                            // Password not found, open prompt
                            isEnteringWiFiPassword = true;
                            wifiPasswordBuffer = "";
                        }
                        needsRedraw = true;
                    }
                }
            }
            else {
                // --- NORMAL UI NAVIGATION (Pages 0, 1, 2) ---
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
                            settingsCursor = (settingsCursor + 1) % 5;
                            needsRedraw = true;
                        }
                        if (key == ';') {
                            settingsCursor = (settingsCursor == 0) ? 4 : settingsCursor - 1;
                            needsRedraw = true;
                        }
                    }
                }

                if (status.enter && currentPage == 2) {
                    if (settingsCursor < 4) {
                        isEditing = true;
                        editBuffer = "";
                        needsRedraw = true;
                    } else if (settingsCursor == 4) {
                        // Start WebUI clicked!
                        currentPage = 3;
                        isScanningWiFi = true;
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
        isEnteringWiFiPassword = false; // Safety reset just in case
    }

    // --- SPECIAL HOOK: FORCE DRAW BEFORE SCANNING ---
    if (currentPage == 3 && isScanningWiFi) {
        canvas.clear();
        drawWiFiScanner(); // Draws "Scanning..."
        canvas.pushSprite(0, 0);

        scanWiFiNetworks(); // Blocks the CPU for 2 seconds to scan

        isScanningWiFi = false;
        wifiCursor = 0;
        needsRedraw = true;
    }

    // --- SPECIAL HOOK: FORCE DRAW BEFORE CONNECTING & NTP ---
    if (currentPage == 3 && isConnectingWiFi) {
        canvas.clear();
        drawWiFiScanner(); // Draws "Connecting to Wi-Fi..."
        canvas.pushSprite(0, 0);

        setCpuFrequencyMhz(240); // 🚀 Boost CPU for Wi-Fi!

        bool connected = connectToWiFi(networkSSIDs[selectedNetworkIndex], wifiPasswordBuffer);

        if (connected) {
            isConnectingWiFi = false;
            isSyncingNTP = true;

            canvas.clear();
            drawWiFiScanner(); // Draws "Syncing Time..."
            canvas.pushSprite(0, 0);

            syncNTP(); // Reach out to the time servers

            isSyncingNTP = false;
            isWebUIRunning = true;
            startWebUI_Server();
        } else {
            // Connection failed! Go back to password prompt.
            setCpuFrequencyMhz(80);
            isConnectingWiFi = false;
            isEnteringWiFiPassword = true;
            wifiPasswordBuffer = "";
        }
        needsRedraw = true;
    }

    // --- RENDER SCREEN FROM MEMORY ---
    if (needsRedraw && isScreenOn && !isScanningWiFi) {
        canvas.clear();

        if (currentPage == 0) drawDashboard();
        else if (currentPage == 1) drawGraph();
        else if (currentPage == 2) drawSettings();
        else if (currentPage == 3) drawWiFiScanner();

        canvas.pushSprite(0, 0);
        needsRedraw = false;
    }

    delay(50); //sleepy time for cpu
}
