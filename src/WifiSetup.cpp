#include "WifiSetup.h"

#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFi.h>

namespace {

constexpr const char* kPrefsNamespace = "wifi";
constexpr uint32_t kConnectTimeoutMs = 10000;

}  // namespace

void beginWifi() {
  Preferences prefs;
  prefs.begin(kPrefsNamespace, true);
  const String ssid = prefs.getString("ssid", "");
  const String password = prefs.getString("password", "");
  prefs.end();

  WiFi.setHostname(WIFI_HOSTNAME);
  if (ssid.length() > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < kConnectTimeoutMs) delay(100);
  }
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);
  }
  MDNS.begin(WIFI_HOSTNAME);
  MDNS.addService("http", "tcp", 80);
}

bool saveWifiCredentials(const String& ssid, const String& password) {
  if (ssid.length() == 0) return false;
  Preferences prefs;
  prefs.begin(kPrefsNamespace, false);
  const bool ok = prefs.putString("ssid", ssid) > 0;
  prefs.putString("password", password);
  prefs.end();
  return ok;
}

void clearWifiCredentials() {
  Preferences prefs;
  prefs.begin(kPrefsNamespace, false);
  prefs.clear();
  prefs.end();
}

String wifiDescription() {
  if (WiFi.getMode() == WIFI_AP) {
    return String("access point ") + WIFI_AP_SSID + " " + WiFi.softAPIP().toString();
  }
  return String("station ") + WiFi.SSID() + " " + WiFi.localIP().toString();
}
