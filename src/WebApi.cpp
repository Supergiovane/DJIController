#include "WebApi.h"

#include <ArduinoJson.h>
#include <DjiCommands.h>

#include "JsonUtil.h"
#include "WifiSetup.h"
#include "generated/WebAssets.h"

namespace {

constexpr uint32_t kDefaultHoldMs = 500;
constexpr uint32_t kMinHoldMs = 50;
constexpr uint32_t kMaxHoldMs = 60000;
constexpr const char* kConfigApp = "DJIController";
constexpr int kConfigVersion = 1;

const char* kIndex =
    "DJI RS controller - HTTP API (GET or POST, JSON replies). Web UI at /\n"
    "\n"
    "/api/status                                  network, motion and angles\n"
    "/api/angles?type=attitude|joint              current angles (deg)\n"
    "/api/version                                 R SDK module version\n"
    "/api/position?yaw=&roll=&pitch=&time=1&mode=abs|rel\n"
    "                                             move in deg within time (s); omitted axes are left alone\n"
    "/api/speed?yaw=&roll=&pitch=&ms=500          rotate in deg/s for ms milliseconds\n"
    "/api/joystick?pitch=&roll=&yaw=&ms=500       joystick input -1..1 for ms milliseconds\n"
    "/api/stop                                    stop speed/joystick motion\n"
    "/api/recenter\n"
    "/api/follow?mode=lock|yaw|sport\n"
    "/api/record?on=1|0                           camera recording (camera control cable)\n"
    "/api/shutter\n"
    "/api/activetrack                             toggle ActiveTrack\n"
    "/api/presets                                 saved presets\n"
    "/api/presets/save?name=[&slot=][&yaw=&roll=&pitch=]\n"
    "                                             save the current position (or the given angles)\n"
    "/api/presets/recall?slot=&time=2             move to a preset within time (s)\n"
    "/api/presets/delete?slot=\n"
    "/api/wifi/status                             access point, station and join progress\n"
    "/api/wifi/scan[?refresh=1]                   nearby networks\n"
    "/api/wifi/join?ssid=&password=               try a network, saved once connected\n"
    "/api/wifi/forget                             forget the saved network and reboot\n"
    "/api/config[?secrets=1]                      export presets and saved network (password only with secrets=1)\n"
    "POST /api/config  (JSON body)                import: replaces presets; saves the network if it has a password\n";

String axesJson(const dji::Axes& a) {
  char buf[80];
  snprintf(buf, sizeof(buf), "{\"yaw\":%.1f,\"roll\":%.1f,\"pitch\":%.1f}", a.yaw, a.roll, a.pitch);
  return buf;
}

const char* motionName(MotionHold::Mode mode) {
  switch (mode) {
    case MotionHold::Mode::Speed: return "speed";
    case MotionHold::Mode::Joystick: return "joystick";
    default: return "none";
  }
}

}  // namespace

void WebApi::begin() {
  route("/", &WebApi::handleUi);
  route("/api", &WebApi::handleIndex);
  route("/api/status", &WebApi::handleStatus);
  route("/api/angles", &WebApi::handleAngles);
  route("/api/version", &WebApi::handleVersion);
  route("/api/position", &WebApi::handlePosition);
  route("/api/speed", &WebApi::handleSpeed);
  route("/api/joystick", &WebApi::handleJoystick);
  route("/api/stop", &WebApi::handleStop);
  route("/api/recenter", &WebApi::handleRecenter);
  route("/api/follow", &WebApi::handleFollow);
  route("/api/record", &WebApi::handleRecord);
  route("/api/shutter", &WebApi::handleShutter);
  route("/api/activetrack", &WebApi::handleActiveTrack);
  route("/api/wifi", &WebApi::handleWifi);
  route("/api/wifi/status", &WebApi::handleWifiStatus);
  route("/api/wifi/scan", &WebApi::handleWifiScan);
  route("/api/wifi/join", &WebApi::handleWifiJoin);
  route("/api/wifi/forget", &WebApi::handleWifiForget);
  route("/api/presets", &WebApi::handlePresets);
  route("/api/presets/save", &WebApi::handlePresetSave);
  route("/api/presets/recall", &WebApi::handlePresetRecall);
  route("/api/presets/delete", &WebApi::handlePresetDelete);
  route("/api/config", &WebApi::handleConfig);
  server_.onNotFound([this] { sendError(404, "unknown endpoint, see /api"); });
  server_.begin();
}

void WebApi::route(const char* path, void (WebApi::*handler)()) {
  server_.on(path, HTTP_ANY, [this, handler] { (this->*handler)(); });
}

void WebApi::handleUi() {
  server_.sendHeader("Content-Encoding", "gzip");
  server_.sendHeader("Cache-Control", "no-cache");
  server_.send_P(200, "text/html", reinterpret_cast<const char*>(kIndexHtmlGz), kIndexHtmlGzLen);
}

void WebApi::handleIndex() {
  server_.send(200, "text/plain", kIndex);
}

void WebApi::handleStatus() {
  String body = "{\"wifi\":" + jsonString(wifiDescription()) + ",\"motion\":\"" + motionName(motion_.mode()) +
                "\",\"angles\":";
  dji::Axes angles;
  const bool connected = readAttitude(angles);
  body += connected ? axesJson(angles) : "null";
  body += ",\"gimbal\":";
  body += connected ? "true}" : "false}";
  sendJson(200, body);
}

void WebApi::handleAngles() {
  const auto type = server_.arg("type") == "joint" ? dji::AngleType::Joint : dji::AngleType::Attitude;
  dji::Packet request = dji::getAngles(type);
  dji::Packet reply;
  dji::Axes angles;
  if (!gimbal_.request(request, reply)) return sendError(504, "no reply from gimbal");
  if (!dji::parseAngles(reply, angles)) return sendError(502, "angles not available");
  sendJson(200, axesJson(angles));
}

void WebApi::handleVersion() {
  dji::Packet request = dji::getVersion();
  dji::Packet reply;
  uint32_t v;
  if (!gimbal_.request(request, reply)) return sendError(504, "no reply from gimbal");
  if (!dji::parseVersion(reply, v)) return sendError(502, "bad reply");
  char body[48];
  snprintf(body, sizeof(body), "{\"version\":\"%u.%u.%u.%u\"}", unsigned(v >> 24), unsigned((v >> 16) & 0xFF),
           unsigned((v >> 8) & 0xFF), unsigned(v & 0xFF));
  sendJson(200, body);
}

void WebApi::handlePosition() {
  const bool useYaw = server_.hasArg("yaw");
  const bool useRoll = server_.hasArg("roll");
  const bool usePitch = server_.hasArg("pitch");
  if (!useYaw && !useRoll && !usePitch) return sendError(400, "give at least one of yaw, roll, pitch");
  const dji::Axes target = {floatArg("yaw", 0), floatArg("roll", 0), floatArg("pitch", 0)};
  const bool absolute = server_.arg("mode") != "rel";
  sendCommand(dji::setPosition(target, absolute, floatArg("time", 1), useYaw, useRoll, usePitch));
}

void WebApi::handleSpeed() {
  hold(MotionHold::Mode::Speed, {floatArg("yaw", 0), floatArg("roll", 0), floatArg("pitch", 0)});
}

void WebApi::handleJoystick() {
  hold(MotionHold::Mode::Joystick, {floatArg("yaw", 0), floatArg("roll", 0), floatArg("pitch", 0)});
}

void WebApi::handleStop() {
  motion_.stop();
  sendOk();
}

void WebApi::handleRecenter() {
  sendCommand(dji::recenter());
}

void WebApi::handleFollow() {
  const String mode = server_.arg("mode");
  if (mode == "lock") return sendCommand(dji::setFollowMode(dji::FollowMode::Lock));
  if (mode == "yaw") return sendCommand(dji::setFollowMode(dji::FollowMode::YawFollow));
  if (mode == "sport") return sendCommand(dji::setFollowMode(dji::FollowMode::Sport));
  sendError(400, "mode must be lock, yaw or sport");
}

void WebApi::handleRecord() {
  if (!server_.hasArg("on")) return sendError(400, "give on=1 or on=0");
  sendCommand(dji::cameraRecord(server_.arg("on") != "0"));
}

void WebApi::handleShutter() {
  sendCommand(dji::cameraShutter());
}

void WebApi::handleActiveTrack() {
  sendUnacknowledged(dji::toggleActiveTrack());
}

// Older single endpoint, kept for scripts written against it.
void WebApi::handleWifi() {
  if (server_.arg("reset") == "1") return handleWifiForget();
  handleWifiJoin();
}

void WebApi::handleWifiStatus() {
  sendJson(200, wifiStatusJson());
}

void WebApi::handleWifiScan() {
  sendJson(200, wifiScanJson(server_.arg("refresh") == "1"));
}

void WebApi::handleWifiJoin() {
  const String ssid = server_.arg("ssid");
  if (ssid.length() == 0) return sendError(400, "give ssid (and password)");
  // Answer first: switching networks may briefly drop this client.
  sendOk();
  joinWifi(ssid, server_.arg("password"));
}

void WebApi::handleWifiForget() {
  forgetWifi();
  sendJson(200, "{\"ok\":true,\"rebooting\":true}");
  delay(500);
  ESP.restart();
}

void WebApi::handlePresets() {
  String body = "{\"max\":" + String(PresetStore::kMaxPresets) + ",\"presets\":[";
  bool first = true;
  for (int slot = 0; slot < PresetStore::kMaxPresets; ++slot) {
    const Preset& preset = presets_.get(slot);
    if (!preset.used) continue;
    if (!first) body += ",";
    first = false;
    String angles = axesJson(preset.angles);
    angles.remove(0, 1);  // Merge the angle fields into this object.
    body += "{\"slot\":" + String(slot) + ",\"name\":" + jsonString(preset.name) + "," + angles;
  }
  sendJson(200, body + "]}");
}

void WebApi::handlePresetSave() {
  int slot = -1;
  if (server_.hasArg("slot")) {
    slot = slotArg();
    if (slot < 0) return sendError(400, "invalid slot");
  }
  String name = server_.arg("name");
  name.trim();
  if (name.length() == 0 && slot >= 0 && presets_.get(slot).used) name = presets_.get(slot).name;
  if (name.length() == 0) return sendError(400, "give a name");

  dji::Axes angles;
  if (server_.hasArg("yaw") && server_.hasArg("roll") && server_.hasArg("pitch")) {
    angles = {floatArg("yaw", 0), floatArg("roll", 0), floatArg("pitch", 0)};
  } else if (!readAttitude(angles)) {
    return sendError(504, "no reply from gimbal");
  }

  slot = presets_.save(slot, name, angles);
  if (slot < 0) return sendError(507, "no free preset slot");
  sendJson(200, "{\"ok\":true,\"slot\":" + String(slot) + "}");
}

void WebApi::handlePresetRecall() {
  const int slot = slotArg();
  if (slot < 0 || !presets_.get(slot).used) return sendError(404, "no such preset");
  sendCommand(dji::setPosition(presets_.get(slot).angles, true, floatArg("time", 2)));
}

void WebApi::handlePresetDelete() {
  if (!presets_.remove(slotArg())) return sendError(404, "no such preset");
  sendOk();
}

void WebApi::handleConfig() {
  if (server_.method() == HTTP_POST) return importConfig();

  JsonDocument doc;
  doc["app"] = kConfigApp;
  doc["version"] = kConfigVersion;
  const String ssid = savedWifiSsid();
  if (ssid.length() > 0) {
    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["ssid"] = ssid;
    // The API is unauthenticated, so the password only leaves the device when explicitly asked for.
    if (server_.arg("secrets") == "1") wifi["password"] = savedWifiPassword();
  } else {
    doc["wifi"] = nullptr;
  }
  JsonArray list = doc["presets"].to<JsonArray>();
  for (int slot = 0; slot < PresetStore::kMaxPresets; ++slot) {
    const Preset& preset = presets_.get(slot);
    if (!preset.used) continue;
    JsonObject item = list.add<JsonObject>();
    item["slot"] = slot;
    item["name"] = preset.name;
    item["yaw"] = serialized(String(preset.angles.yaw, 1));
    item["roll"] = serialized(String(preset.angles.roll, 1));
    item["pitch"] = serialized(String(preset.angles.pitch, 1));
  }
  String body;
  serializeJson(doc, body);
  sendJson(200, body);
}

void WebApi::importConfig() {
  JsonDocument doc;
  if (deserializeJson(doc, server_.arg("plain"))) return sendError(400, "invalid JSON");
  if (doc["app"] != kConfigApp) return sendError(400, "not a DJIController configuration");

  int imported = -1;  // -1: the file has no preset list, so the current one is kept.
  JsonArrayConst list = doc["presets"];
  if (!list.isNull()) {
    presets_.clear();
    imported = 0;
    for (JsonObjectConst item : list) {
      const String name = item["name"] | "";
      if (name.length() == 0) continue;
      int slot = item["slot"] | -1;
      if (!PresetStore::validSlot(slot) || presets_.get(slot).used) slot = -1;
      const dji::Axes angles = {item["yaw"] | 0.0f, item["roll"] | 0.0f, item["pitch"] | 0.0f};
      if (presets_.save(slot, name, angles) >= 0) ++imported;
    }
  }

  // Without a password we can't restore the network, and saving an empty one would break the current setup.
  const char* wifiResult = "skipped";
  bool join = false;
  JsonObjectConst wifi = doc["wifi"];
  const String ssid = wifi["ssid"] | "";
  const String password = wifi["password"] | "";
  if (ssid.length() > 0 && wifi["password"].is<const char*>()) {
    join = prepareWifiImport(ssid, password);
    wifiResult = join ? "joining" : "saved";
  }
  sendJson(200, "{\"ok\":true,\"presets\":" + String(imported) + ",\"wifi\":\"" + wifiResult + "\"}");
  // After answering: switching networks may briefly drop this client.
  if (join) joinWifi(ssid, password);
}

void WebApi::sendCommand(dji::Packet packet) {
  dji::Packet reply;
  dji::ReturnCode code;
  if (!gimbal_.request(packet, reply)) return sendError(504, "no reply from gimbal");
  if (!dji::parseReturnCode(reply, code)) return sendError(502, "bad reply");
  if (code != dji::ReturnCode::Success) return sendError(502, dji::toString(code));
  sendOk();
}

void WebApi::sendUnacknowledged(dji::Packet packet) {
  if (!gimbal_.send(packet)) return sendError(503, "CAN send failed");
  sendOk();
}

void WebApi::hold(MotionHold::Mode mode, const dji::Axes& value) {
  const long ms = server_.hasArg("ms") ? server_.arg("ms").toInt() : long(kDefaultHoldMs);
  motion_.start(mode, value, uint32_t(constrain(ms, long(kMinHoldMs), long(kMaxHoldMs))));
  sendOk();
}

bool WebApi::readAttitude(dji::Axes& out) {
  dji::Packet request = dji::getAngles();
  dji::Packet reply;
  return gimbal_.request(request, reply) && dji::parseAngles(reply, out);
}

float WebApi::floatArg(const char* name, float fallback) {
  return server_.hasArg(name) ? server_.arg(name).toFloat() : fallback;
}

int WebApi::slotArg() {
  if (!server_.hasArg("slot")) return -1;
  const int slot = server_.arg("slot").toInt();
  return PresetStore::validSlot(slot) ? slot : -1;
}

void WebApi::sendJson(int status, const String& body) {
  server_.sendHeader("Access-Control-Allow-Origin", "*");
  server_.send(status, "application/json", body);
}

void WebApi::sendError(int status, const char* message) {
  sendJson(status, "{\"ok\":false,\"error\":" + jsonString(message) + "}");
}
