#include "DjiCommands.h"

#include <cmath>

namespace dji {
namespace {

constexpr uint32_t kDeviceIdRSdk = 0x00000001;
constexpr uint8_t kJoystickDevice = 0x01;
constexpr float kJoystickFullScale = 15000;

Packet make(uint8_t cmdSet, uint8_t cmdId, uint8_t cmdType = kReplyRequired) {
  Packet p;
  p.cmdType = cmdType;
  p.cmdSet = cmdSet;
  p.cmdId = cmdId;
  return p;
}

void append8(Packet& p, uint8_t v) {
  p.payload[p.payloadLen++] = v;
}

void append16(Packet& p, int32_t v) {
  putLe16(p.payload + p.payloadLen, uint16_t(v));
  p.payloadLen += 2;
}

void append32(Packet& p, uint32_t v) {
  putLe32(p.payload + p.payloadLen, v);
  p.payloadLen += 4;
}

int32_t scaled(float value, float scale, int32_t limit) {
  const int32_t v = int32_t(lroundf(value * scale));
  return v < -limit ? -limit : (v > limit ? limit : v);
}

float tenths(const uint8_t* p) {
  return int16_t(getLe16(p)) / 10.0f;
}

}  // namespace

const char* toString(ReturnCode code) {
  switch (code) {
    case ReturnCode::Success: return "ok";
    case ReturnCode::ParseError: return "parse error";
    case ReturnCode::ExecFailed: return "execution failed";
    default: return "undefined error";
  }
}

Packet setPosition(const Axes& target, bool absolute, float durationSec, bool useYaw, bool useRoll, bool usePitch) {
  Packet p = make(cmdset::kGimbal, gimbal::kSetPosition);
  append16(p, scaled(target.yaw, 10, 1800));
  append16(p, scaled(target.roll, 10, 1800));
  append16(p, scaled(target.pitch, 10, 1800));
  // ctrl_byte: bit 0 absolute, bits 1..3 mark yaw/roll/pitch as invalid.
  append8(p, (absolute ? 0x01 : 0) | (useYaw ? 0 : 0x02) | (useRoll ? 0 : 0x04) | (usePitch ? 0 : 0x08));
  const int32_t time = scaled(durationSec, 10, 255);
  append8(p, uint8_t(time < 1 ? 1 : time));
  return p;
}

Packet setSpeed(const Axes& degPerSec, bool takeover) {
  Packet p = make(cmdset::kGimbal, gimbal::kSetSpeed);
  append16(p, scaled(degPerSec.yaw, 10, 3600));
  append16(p, scaled(degPerSec.roll, 10, 3600));
  append16(p, scaled(degPerSec.pitch, 10, 3600));
  append8(p, takeover ? 0x80 : 0x00);
  return p;
}

Packet joystick(float pitch, float roll, float yaw) {
  Packet p = make(cmdset::kGimbal, gimbal::kExternalControl, kReplyNone);
  append8(p, kJoystickDevice);
  append16(p, scaled(pitch, kJoystickFullScale, 15000));
  append16(p, scaled(roll, kJoystickFullScale, 15000));
  append16(p, scaled(yaw, kJoystickFullScale, 15000));
  return p;
}

Packet getAngles(AngleType type) {
  Packet p = make(cmdset::kGimbal, gimbal::kGetAngles);
  append8(p, uint8_t(type));
  return p;
}

Packet setParameterPush(bool enable) {
  Packet p = make(cmdset::kGimbal, gimbal::kSetPush);
  append8(p, enable ? 0x01 : 0x02);
  return p;
}

Packet getVersion() {
  Packet p = make(cmdset::kGimbal, gimbal::kVersion);
  append32(p, kDeviceIdRSdk);
  return p;
}

Packet recenter() {
  Packet p = make(cmdset::kGimbal, gimbal::kRecenterOrFollow);
  append8(p, 0xFE);  // Keep the current follow mode.
  append8(p, 0x01);
  return p;
}

Packet selfie() {
  Packet p = make(cmdset::kGimbal, gimbal::kRecenterOrFollow);
  append8(p, 0xFE);
  append8(p, 0x02);
  return p;
}

Packet setFollowMode(FollowMode mode) {
  Packet p = make(cmdset::kGimbal, gimbal::kRecenterOrFollow);
  append8(p, uint8_t(mode));
  append8(p, 0x00);
  return p;
}

Packet toggleActiveTrack() {
  Packet p = make(cmdset::kGimbal, gimbal::kActiveTrack, kReplyNone);
  append8(p, 0x03);
  return p;
}

Packet cameraRecord(bool start) {
  Packet p = make(cmdset::kCamera, camera::kMotion);
  append16(p, start ? 0x0003 : 0x0004);
  return p;
}

Packet cameraShutter() {
  Packet p = make(cmdset::kCamera, camera::kMotion);
  append16(p, 0x0001);
  return p;
}

bool parseReturnCode(const Packet& reply, ReturnCode& code) {
  if (!reply.isReply() || reply.payloadLen < 1) return false;
  code = ReturnCode(reply.payload[0]);
  return true;
}

bool parseAngles(const Packet& reply, Axes& out) {
  const uint8_t* angles = nullptr;
  if (reply.cmdSet != cmdset::kGimbal) return false;
  if (reply.cmdId == gimbal::kGetAngles) {
    // return code, data_type (0 = not ready), yaw, roll, pitch.
    if (reply.payloadLen < 8 || reply.payload[0] != 0 || reply.payload[1] == 0) return false;
    angles = reply.payload + 2;
  } else if (reply.cmdId == gimbal::kPush) {
    // ctrl_byte (bit 0 = angles valid), yaw, roll, pitch, ...
    if (reply.payloadLen < 7 || (reply.payload[0] & 0x01) == 0) return false;
    angles = reply.payload + 1;
  } else {
    return false;
  }
  out.yaw = tenths(angles);
  out.roll = tenths(angles + 2);
  out.pitch = tenths(angles + 4);
  return true;
}

bool parseVersion(const Packet& reply, uint32_t& version) {
  if (reply.cmdId != gimbal::kVersion || reply.payloadLen < 9 || reply.payload[0] != 0) return false;
  version = getLe32(reply.payload + 5);
  return true;
}

}  // namespace dji
