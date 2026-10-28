#pragma once

#include <Arduino.h>

// Joins the network saved in flash; if none is saved or it can't be reached, starts an access point.
// Also announces the device as http://<WIFI_HOSTNAME>.local via mDNS.
void beginWifi();

// Call from loop(): completes a join started with joinWifi().
void updateWifi();

// Tries `ssid` in the background while keeping the current access point or connection up, so a client can follow
// the outcome. The credentials are saved only once the connection succeeds; on failure the previous state is restored.
void joinWifi(const String& ssid, const String& password);

// Forgets the saved network. It takes effect at the next boot.
void forgetWifi();

// Saved station credentials, used at the next boot (for configuration export/import).
String savedWifiSsid();
String savedWifiPassword();
void saveWifi(const String& ssid, const String& password);

// First half of applying imported credentials. Returns true when joinWifi() should follow (call it after
// answering the HTTP request). On the access point the credentials are saved right away, since the network may be
// out of range now but valid at the next boot; when connected elsewhere they are left to joinWifi(), which saves
// them only on success so the controller stays reachable.
bool prepareWifiImport(const String& ssid, const String& password);

// {"ap":..., "station":..., "join":...} for the web UI.
String wifiStatusJson();

// Nearby networks. Starts a scan when there are no results yet (or `refresh` is set) and reports
// {"scanning":true} until it completes.
String wifiScanJson(bool refresh);

// Human-readable summary, e.g. "station MyNet 192.168.1.20" or "access point DJIController 192.168.4.1".
String wifiDescription();
