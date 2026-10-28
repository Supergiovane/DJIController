#include "MotionHold.h"

#include <Arduino.h>

namespace {
constexpr uint32_t kRefreshIntervalMs = 100;
}  // namespace

void MotionHold::start(Mode mode, const dji::Axes& value, uint32_t durationMs) {
  if (mode == Mode::None) return stop();
  if (mode_ != Mode::None && mode_ != mode) stop();
  mode_ = mode;
  value_ = value;
  durationMs_ = durationMs;
  startMs_ = millis();
  sendCurrent();
}

void MotionHold::stop() {
  if (mode_ == Mode::Speed) {
    dji::Packet release = dji::setSpeed({}, false);
    gimbal_.send(release);
  } else if (mode_ == Mode::Joystick) {
    dji::Packet center = dji::joystick(0, 0, 0);
    gimbal_.send(center);
  }
  mode_ = Mode::None;
}

void MotionHold::update() {
  if (mode_ == Mode::None) return;
  const uint32_t now = millis();
  if (durationMs_ != 0 && now - startMs_ >= durationMs_) {
    stop();
  } else if (now - lastSendMs_ >= kRefreshIntervalMs) {
    sendCurrent();
  }
}

void MotionHold::sendCurrent() {
  dji::Packet packet =
      mode_ == Mode::Speed ? dji::setSpeed(value_) : dji::joystick(value_.pitch, value_.roll, value_.yaw);
  packet.cmdType = dji::kReplyNone;  // Streamed: don't make the gimbal answer every 100 ms.
  gimbal_.send(packet);
  lastSendMs_ = millis();
}
