#pragma once
#include <Arduino.h>
#include <SD.h>


void setupSD();
void saveSettings();
void loadSettings();
void loadActivityGraph();
void logDataToSD();
String getSavedWiFiPassword(String ssid);
void saveWiFiNetwork(String ssid, String password);
