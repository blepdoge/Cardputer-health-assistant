#pragma once
#include <Arduino.h>

void scanWiFiNetworks();
bool connectToWiFi(String ssid, String password);
void syncNTP();

// NEW WEBUI FUNCTIONS
void startWebUI_Server();
void handleWebUI();
void stopWebUI_Server();
