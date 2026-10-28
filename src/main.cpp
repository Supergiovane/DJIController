// DJI RS gimbal controller: drives the gimbal over the RSA/NATO CAN bus, controlled through an HTTP API over Wi-Fi
// (see WebApi) or a serial console (type `h`).

#include <Arduino.h>
#include <DjiCommands.h>

#include "CanBus.h"
#include "Gimbal.h"
#include "MotionHold.h"
#include "PresetStore.h"
#include "WebApi.h"
#include "WifiSetup.h"

namespace {

CanBus bus;
Gimbal gimbal(bus);
MotionHold motion(gimbal);
PresetStore presets;
WebApi web(gimbal, motion, presets);

String line;

const char* kHelp =
    "Commands:\n"
    "  v               module version\n"
    "  a               read attitude\n"
    "  c               recenter\n"
    "  p Y R P [sec]   go to absolute yaw/roll/pitch (deg)\n"
    "  m Y R P [sec]   move relative (deg)\n"
    "  s Y R P         hold speed (deg/s)\n"
    "  j P R Y         hold joystick (-1..1)\n"
    "  x               stop speed/joystick\n"
    "  push 1|0        enable/disable angle push\n"
    "  rec 1|0         camera record start/stop\n"
    "  ip              show network\n"
    "  wifi SSID PASS  join a network, saved once connected (wifi reset forgets it and reboots)\n";

void requestAndReport(dji::Packet packet) {
  dji::Packet reply;
  dji::ReturnCode code;
  if (!gimbal.request(packet, reply)) {
    Serial.println("no reply");
  } else if (dji::parseReturnCode(reply, code)) {
    Serial.println(dji::toString(code));
  }
}

void printAngles(const dji::Axes& a) {
  Serial.printf("yaw %.1f  roll %.1f  pitch %.1f\n", a.yaw, a.roll, a.pitch);
}

void onPush(const dji::Packet& packet) {
  dji::Axes angles;
  if (packet.cmdId == dji::gimbal::kPush && dji::parseAngles(packet, angles)) printAngles(angles);
}

void handleCommand(const String& input) {
  const char* cmd = input.c_str();
  float a = 0, b = 0, c = 0, sec = 1.0f;
  int flag = 0;
  char ssid[33] = {}, password[65] = {};

  if (input == "h") {
    Serial.print(kHelp);
  } else if (input == "v") {
    dji::Packet reply;
    dji::Packet request = dji::getVersion();
    uint32_t v;
    if (gimbal.request(request, reply) && dji::parseVersion(reply, v)) {
      Serial.printf("R SDK %u.%u.%u.%u\n", unsigned(v >> 24), unsigned((v >> 16) & 0xFF), unsigned((v >> 8) & 0xFF),
                    unsigned(v & 0xFF));
    } else {
      Serial.println("no reply");
    }
  } else if (input == "a") {
    dji::Packet reply;
    dji::Packet request = dji::getAngles();
    dji::Axes angles;
    if (gimbal.request(request, reply) && dji::parseAngles(reply, angles)) {
      printAngles(angles);
    } else {
      Serial.println("no reply");
    }
  } else if (input == "c") {
    requestAndReport(dji::recenter());
  } else if (sscanf(cmd, "p %f %f %f %f", &a, &b, &c, &sec) >= 3) {
    requestAndReport(dji::setPosition({a, b, c}, true, sec));
  } else if (sscanf(cmd, "m %f %f %f %f", &a, &b, &c, &sec) >= 3) {
    requestAndReport(dji::setPosition({a, b, c}, false, sec));
  } else if (sscanf(cmd, "s %f %f %f", &a, &b, &c) == 3) {
    motion.start(MotionHold::Mode::Speed, {a, b, c});
  } else if (sscanf(cmd, "j %f %f %f", &a, &b, &c) == 3) {
    dji::Axes value;
    value.pitch = a;
    value.roll = b;
    value.yaw = c;
    motion.start(MotionHold::Mode::Joystick, value);
  } else if (input == "x") {
    motion.stop();
  } else if (sscanf(cmd, "push %d", &flag) == 1) {
    requestAndReport(dji::setParameterPush(flag != 0));
  } else if (sscanf(cmd, "rec %d", &flag) == 1) {
    requestAndReport(dji::cameraRecord(flag != 0));
  } else if (input == "ip") {
    Serial.println(wifiDescription());
  } else if (input == "wifi reset") {
    forgetWifi();
    ESP.restart();
  } else if (sscanf(cmd, "wifi %32s %64s", ssid, password) >= 1) {
    joinWifi(ssid, password);
    Serial.println("joining, check with: ip");
  } else if (input.length() > 0) {
    Serial.println("unknown command, type h");
  }
}

void readSerial() {
  while (Serial.available()) {
    const char ch = Serial.read();
    if (ch == '\n' || ch == '\r') {
      line.trim();
      handleCommand(line);
      line = "";
    } else {
      line += ch;
    }
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  if (!bus.begin(CAN_TX_PIN, CAN_RX_PIN, Gimbal::kRxId)) {
    Serial.println("CAN init failed");
  }
  gimbal.onPush(onPush);
  presets.begin();
  beginWifi();
  web.begin();
  Serial.println("DJI RS controller ready on " + wifiDescription());
  Serial.print(kHelp);
}

void loop() {
  bus.service();
  gimbal.poll();
  web.handle();
  readSerial();
  motion.update();
  updateWifi();
}
