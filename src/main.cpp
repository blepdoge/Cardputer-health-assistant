#include "AppState.h"
#include "SDManager.h"
#include "DisplayUI.h"

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

uint32_t stepCount = 0;
uint32_t lastStepCount = 0;
float distanceKm = 0.0;
float caloriesBurned = 0.0;
float weightKg = 70.0;
float heightCm = 175.0;
int dailyGoal = 10000;
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

void loop() {
    M5Cardputer.update();

    if (millis() - lastImuPoll >= 2000) {
        imu.getStepCount(&stepCount);
        lastImuPoll = millis();
        if (stepCount != lastStepCount) {
            updateMetrics();
            lastStepCount = stepCount;
            if (isScreenOn && currentPage == 0) needsRedraw = true;
        }
    }

    if (millis() - lastLogTime >= LOG_INTERVAL) {
        logDataToSD();
        lastLogTime = millis();
    }

    if (M5Cardputer.Keyboard.isChange()) {
        if (M5Cardputer.Keyboard.isPressed()) {
            Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
            lastInteractionTime = millis();

            if (!isScreenOn) {
                M5Cardputer.Display.setBrightness(128);
                isScreenOn = true;
                needsRedraw = true;
            } else if (isEditing) {
                if (status.del && editBuffer.length() > 0) editBuffer.remove(editBuffer.length() - 1);
                else if (status.enter) {
                    if (settingsCursor == 0) dailyGoal = editBuffer.toInt();
                    else if (settingsCursor == 1) heightCm = editBuffer.toFloat();
                    else if (settingsCursor == 2) weightKg = editBuffer.toFloat();
                    isEditing = false;
                    updateMetrics();
                    saveSettings();
                } else {
                    for (auto key : status.word) {
                        if (isDigit(key) || key == '.') editBuffer += key;
                    }
                }
                needsRedraw = true;
            } else {
                for (auto key : status.word) {
                    if (key == '/') currentPage = (currentPage + 1) % 3;
                    if (key == ',') currentPage = (currentPage == 0) ? 2 : currentPage - 1;
                    if (currentPage == 2) {
                        if (key == '.') settingsCursor = (settingsCursor + 1) % 4;
                        if (key == ';') settingsCursor = (settingsCursor == 0) ? 3 : settingsCursor - 1;
                    }
                }
                needsRedraw = true;
                if (status.enter && currentPage == 2 && settingsCursor < 3) {
                    isEditing = true;
                    editBuffer = "";
                }
            }
        }
    }

    if (isScreenOn && (millis() - lastInteractionTime > SCREEN_TIMEOUT)) {
        M5Cardputer.Display.setBrightness(0);
        isScreenOn = false;
        isEditing = false;
    }

    if (needsRedraw && isScreenOn) {
        canvas.clear();
        if (currentPage == 0) drawDashboard();
        else if (currentPage == 1) drawGraph();
        else if (currentPage == 2) drawSettings();
        canvas.pushSprite(0, 0);
        needsRedraw = false;
    }

    delay(50); // The sleepy time is now outside the keyboard block!
}
