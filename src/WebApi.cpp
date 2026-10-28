#include "WebApi.h"

#include <DjiCommands.h>

#include "WifiSetup.h"

namespace {

constexpr uint32_t kDefaultHoldMs = 500;
constexpr uint32_t kMinHoldMs = 50;
constexpr uint32_t kMaxHoldMs = 60000;

const char* kIndex =
    "DJI RS controller - HTTP API (GET or POST, JSON replies)\n"
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
    "/api/wifi?ssid=&password=                    save network and reboot (reset=1 forgets it)\n";

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
  route("/", &WebApi::handleIndex);
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
  server_.onNotFound([this] { sendError(404, "unknown endpoint, see /"); });
  server_.begin();
}

void WebApi::route(const char* path, void (WebApi::*handler)()) {
  server_.on(path, HTTP_ANY, [this, handler] { (this->*handler)(); });
}

void WebApi::handleIndex() {
  server_.send(200, "text/plain", kIndex);
}

void WebApi::handleStatus() {
  String body = "{\"wifi\":\"" + wifiDescription() + "\",\"motion\":\"" + motionName(motion_.mode()) + "\",\"angles\":";
  dji::Packet request = dji::getAngles();
  dji::Packet reply;
  dji::Axes angles;
  const bool connected = gimbal_.request(request, reply) && dji::parseAngles(reply, angles);
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

void WebApi::handleWifi() {
  if (server_.arg("reset") == "1") {
    clearWifiCredentials();
  } else if (!saveWifiCredentials(server_.arg("ssid"), server_.arg("password"))) {
    return sendError(400, "give ssid (and password)");
  }
  sendJson(200, "{\"ok\":true,\"rebooting\":true}");
  delay(500);
  ESP.restart();
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

float WebApi::floatArg(const char* name, float fallback) {
  return server_.hasArg(name) ? server_.arg(name).toFloat() : fallback;
}

void WebApi::sendJson(int status, const String& body) {
  server_.sendHeader("Access-Control-Allow-Origin", "*");
  server_.send(status, "application/json", body);
}

void WebApi::sendError(int status, const char* message) {
  sendJson(status, String("{\"ok\":false,\"error\":\"") + message + "\"}");
}
