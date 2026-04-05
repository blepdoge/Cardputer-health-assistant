#include "DisplayUI.h"
#include "AppState.h"

void drawStatusBar(String title) {
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextSize(1.0);

    canvas.fillRect(0, 0, 240, 20, PASTEL_GREEN);

    canvas.setTextColor(BLACK, PASTEL_GREEN);
    canvas.setTextDatum(middle_left);
    canvas.drawString(" " + title, 0, 10);

    int batLevel = M5.Power.getBatteryLevel();
    canvas.setTextDatum(middle_right);
    canvas.drawString(String(batLevel) + "% ", 240, 10);
}

void drawDashboard() {
    drawStatusBar("Dashboard");
    canvas.setFont(&fonts::Orbitron_Light_32);
    canvas.setTextSize(1);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(WHITE);
    canvas.drawString(String(stepCount), 120, 45);

    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextSize(1);
    canvas.setTextColor(ACCENT_COLOR);
    canvas.drawString("STEPS", 120, 70);

    int barX = 20; int barY = 85; int barWidthMax = 200;
    int fillWidth = 0;
    if (dailyGoal > 0) fillWidth = (int)(((float)stepCount / dailyGoal) * barWidthMax);
    if (fillWidth > barWidthMax) fillWidth = barWidthMax;

    canvas.drawRect(barX, barY, barWidthMax, 8, LIGHTGREY);
    if (fillWidth > 0) canvas.fillRect(barX, barY, fillWidth, 8, ACCENT_COLOR);

    canvas.setTextDatum(middle_center);
    canvas.setTextColor(LIGHTGREY);
    canvas.drawString("Goal: " + String(dailyGoal), 120, 103);
    canvas.setTextDatum(bottom_center);
    canvas.setTextColor(ORANGE);
    canvas.drawString(String(caloriesBurned, 1) + " kcal  |  " + String(distanceKm, 2) + " km", 120, 130);
}

void drawGraph() {
    drawStatusBar("Daily activity");
    int graphX = 12; int graphY = 115;
    int barWidth = 3; int spacing = 2;

    int maxStepsPerInterval = 1800;
    for(int i = 0; i < 72; i++) {
        if(activityGraph[i] > maxStepsPerInterval) maxStepsPerInterval = activityGraph[i];
    }

    for(int i = 0; i < 72; i++) {
        int barHeight = map(activityGraph[i], 0, maxStepsPerInterval, 0, 80);
        if(barHeight < 0) barHeight = 0;
        if(barHeight > 80) barHeight = 80;

        uint16_t color;
        if (activityGraph[i] >= 1500) color = ALERT_RED;
        else if (activityGraph[i] >= 500) color = SMOOTH_ORANGE;
        else color = GOLDEN_YELLOW;

        canvas.fillRect(graphX + (i * spacing), graphY - barHeight, barWidth, barHeight, color);
    }

    canvas.drawLine(0, 116, 240, 116, WHITE);
    canvas.setTextDatum(top_center);
    canvas.setTextSize(0.8);
    canvas.setTextColor(LIGHTGREY);

    for (int h = 0; h <= 24; h += 4) {
        int xPos = 12 + (h * 9);
        canvas.drawLine(xPos, 116, xPos, 118, WHITE);
        canvas.drawString(String(h) + "h", xPos, 120);
    }
}

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
        canvas.drawString(editBuffer + "_", 120, 72);
    }
}
