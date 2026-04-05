#pragma once
#include <Arduino.h>

void scanWiFiNetworks();
bool connectToWiFi(String ssid, String password);
void syncNTP();
