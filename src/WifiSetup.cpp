#include "WifiSetup.h"

#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFi.h>

#include <algorithm>
#include <vector>

#include "JsonUtil.h"

namespace {

constexpr const char* kPrefsNamespace = "wifi";
constexpr uint32_t kConnectTimeoutMs = 10000;
constexpr uint32_t kJoinTimeoutMs = 15000;
constexpr int kMaxScanResults = 20;

enum class JoinState { Idle, Connecting, Connected, Failed };

bool apActive = false;
String savedSsid;
String savedPassword;

JoinState joinState = JoinState::Idle;
String joinSsid;
String joinPassword;
uint32_t joinStartMs = 0;

void saveCredentials(const String& ssid, const String& password) {
  Preferences prefs;
  prefs.begin(kPrefsNamespace, false);
  prefs.putString("ssid", ssid);
  prefs.putString("password", password);
  prefs.end();
  savedSsid = ssid;
  savedPassword = password;
}

void startAccessPoint() {
  WiFi.disconnect();
  // AP+STA so the web UI can still scan for networks and test a join.
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);
  apActive = true;
}

const char* joinStateName() {
  switch (joinState) {
    case JoinState::Connecting: return "connecting";
    case JoinState::Connected: return "connected";
    case JoinState::Failed: return "failed";
    default: return "idle";
  }
}

}  // namespace

void beginWifi() {
  Preferences prefs;
  prefs.begin(kPrefsNamespace, false);  // Read-write so the namespace is created on first boot instead of logging an error.
  savedSsid = prefs.isKey("ssid") ? prefs.getString("ssid") : "";
  savedPassword = prefs.isKey("password") ? prefs.getString("password") : "";
  prefs.end();

  WiFi.persistent(false);  // Credentials live in our own namespace, not in the Wi-Fi driver's.
  WiFi.setHostname(WIFI_HOSTNAME);
  if (savedSsid.length() > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(savedSsid.c_str(), savedPassword.c_str());
    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < kConnectTimeoutMs) delay(100);
  }
  if (WiFi.status() != WL_CONNECTED) startAccessPoint();
  MDNS.begin(WIFI_HOSTNAME);
  MDNS.addService("http", "tcp", 80);
}

void updateWifi() {
  if (joinState != JoinState::Connecting) return;
  if (WiFi.status() == WL_CONNECTED) {
    saveCredentials(joinSsid, joinPassword);
    joinState = JoinState::Connected;
  } else if (millis() - joinStartMs >= kJoinTimeoutMs) {
    joinState = JoinState::Failed;
    WiFi.disconnect();
    if (!apActive && savedSsid.length() > 0) {
      WiFi.begin(savedSsid.c_str(), savedPassword.c_str());
    } else if (!apActive) {
      startAccessPoint();
    }
  }
}

void joinWifi(const String& ssid, const String& password) {
  joinSsid = ssid;
  joinPassword = password;
  joinStartMs = millis();
  joinState = JoinState::Connecting;
  WiFi.disconnect();
  WiFi.begin(ssid.c_str(), password.c_str());
}

void forgetWifi() {
  Preferences prefs;
  prefs.begin(kPrefsNamespace, false);
  prefs.clear();
  prefs.end();
  savedSsid = "";
  savedPassword = "";
}

String savedWifiSsid() {
  return savedSsid;
}

String savedWifiPassword() {
  return savedPassword;
}

void saveWifi(const String& ssid, const String& password) {
  saveCredentials(ssid, password);
}

String wifiStatusJson() {
  const bool connected = WiFi.status() == WL_CONNECTED;
  String json = "{\"hostname\":" + jsonString(String(WIFI_HOSTNAME) + ".local");
  json += ",\"ap\":";
  json += apActive ? "{\"ssid\":" + jsonString(WIFI_AP_SSID) + ",\"ip\":" + jsonString(WiFi.softAPIP().toString()) + "}"
                   : "null";
  json += ",\"station\":";
  json += connected ? "{\"ssid\":" + jsonString(WiFi.SSID()) + ",\"ip\":" + jsonString(WiFi.localIP().toString()) +
                          ",\"rssi\":" + String(WiFi.RSSI()) + "}"
                    : "null";
  json += ",\"saved\":" + (savedSsid.length() > 0 ? jsonString(savedSsid) : String("null"));
  json += ",\"join\":{\"state\":\"" + String(joinStateName()) + "\",\"ssid\":" + jsonString(joinSsid) + "}}";
  return json;
}

String wifiScanJson(bool refresh) {
  int count = WiFi.scanComplete();
  if (count == WIFI_SCAN_RUNNING) return "{\"scanning\":true}";
  if (count == WIFI_SCAN_FAILED || refresh) {
    WiFi.scanDelete();
    WiFi.scanNetworks(true);
    return "{\"scanning\":true}";
  }

  // Strongest first, one entry per SSID, hidden networks skipped.
  std::vector<int> order;
  for (int i = 0; i < count; ++i) {
    if (WiFi.SSID(i).length() > 0) order.push_back(i);
  }
  std::sort(order.begin(), order.end(), [](int a, int b) { return WiFi.RSSI(a) > WiFi.RSSI(b); });

  String json = "{\"scanning\":false,\"networks\":[";
  std::vector<String> seen;
  for (int i : order) {
    const String ssid = WiFi.SSID(i);
    if (std::find(seen.begin(), seen.end(), ssid) != seen.end()) continue;
    if (int(seen.size()) == kMaxScanResults) break;
    if (!seen.empty()) json += ",";
    seen.push_back(ssid);
    json += "{\"ssid\":" + jsonString(ssid) + ",\"rssi\":" + String(WiFi.RSSI(i)) +
            ",\"secure\":" + (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "false" : "true") + "}";
  }
  return json + "]}";
}

String wifiDescription() {
  if (WiFi.status() == WL_CONNECTED) {
    return String("station ") + WiFi.SSID() + " " + WiFi.localIP().toString();
  }
  return String("access point ") + WIFI_AP_SSID + " " + WiFi.softAPIP().toString();
}
