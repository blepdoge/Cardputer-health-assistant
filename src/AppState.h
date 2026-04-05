#pragma once
#include <Arduino.h>
#include <M5Cardputer.h>
#include <M5Unified.h>
#include "SparkFun_BMI270_Arduino_Library.h"

// External objects
extern BMI270 imu;
extern M5Canvas canvas;

// Theme Colors
extern uint16_t ACCENT_COLOR;
extern uint16_t PASTEL_GREEN;
extern uint16_t GOLDEN_YELLOW;
extern uint16_t SMOOTH_ORANGE;
extern uint16_t ALERT_RED;

// State Machine
extern int currentPage;
extern bool needsRedraw;
extern int settingsCursor;
extern bool isEditing;
extern String editBuffer;

// Data
extern uint32_t stepCount;
extern uint32_t lastStepCount;
extern float distanceKm;
extern float caloriesBurned;
extern float weightKg;
extern float heightCm;
extern int dailyGoal;
extern int activityGraph[72];

// Power & SD
extern uint32_t lastInteractionTime;
extern bool isScreenOn;
extern const uint32_t SCREEN_TIMEOUT;
extern bool sdReady;
extern uint32_t lastLogTime;
extern uint32_t stepsAtLastLog;
extern const uint32_t LOG_INTERVAL;

// Shared Functions
void updateMetrics();
