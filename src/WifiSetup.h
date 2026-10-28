#pragma once

#include <Arduino.h>

// Joins the network saved in flash; if none is saved or it can't be reached, starts an access point.
// Also announces the device as http://<WIFI_HOSTNAME>.local via mDNS.
void beginWifi();

// Saves station credentials to flash. They take effect at the next boot.
bool saveWifiCredentials(const String& ssid, const String& password);
void clearWifiCredentials();

// Human-readable summary, e.g. "station MyNet 192.168.1.20" or "access point DJIController 192.168.4.1".
String wifiDescription();
