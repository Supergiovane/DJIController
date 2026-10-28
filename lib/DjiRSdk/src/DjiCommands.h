#pragma once

#include <cstdint>

#include "DjiPacket.h"

namespace dji {

namespace cmdset {
constexpr uint8_t kCamera = 0x0D;
constexpr uint8_t kGimbal = 0x0E;
}  // namespace cmdset

// CmdIDs of the gimbal command set (DJI R SDK v2.3, section 2.3.1).
namespace gimbal {
constexpr uint8_t kSetPosition = 0x00;
constexpr uint8_t kSetSpeed = 0x01;
constexpr uint8_t kGetAngles = 0x02;
constexpr uint8_t kSetLimits = 0x03;
constexpr uint8_t kGetLimits = 0x04;
constexpr uint8_t kSetStiffness = 0x05;
constexpr uint8_t kGetStiffness = 0x06;
constexpr uint8_t kSetPush = 0x07;
constexpr uint8_t kPush = 0x08;
constexpr uint8_t kVersion = 0x09;
constexpr uint8_t kExternalControl = 0x0A;
constexpr uint8_t kGetUserParams = 0x0B;
constexpr uint8_t kSetUserParams = 0x0C;
constexpr uint8_t kSetOperatingMode = 0x0D;
constexpr uint8_t kRecenterOrFollow = 0x0E;
constexpr uint8_t kAutoTune = 0x0F;
constexpr uint8_t kAutoTuneStatus = 0x10;
constexpr uint8_t kActiveTrack = 0x11;
constexpr uint8_t kFocusMotor = 0x12;
}  // namespace gimbal

namespace camera {
constexpr uint8_t kMotion = 0x00;
constexpr uint8_t kStatus = 0x01;
}  // namespace camera

// First payload byte of every reply frame.
enum class ReturnCode : uint8_t { Success = 0x00, ParseError = 0x01, ExecFailed = 0x02, Undefined = 0xFF };

const char* toString(ReturnCode code);

enum class AngleType : uint8_t { Attitude = 0x01, Joint = 0x02 };

enum class FollowMode : uint8_t { Lock = 0x00, YawFollow = 0x02, Sport = 0x03 };

// Per-axis values in degrees (angles) or degrees per second (speeds).
struct Axes {
  float yaw = 0;
  float roll = 0;
  float pitch = 0;
};

// Moves to `target` within `durationSec` (0.1..25.5 s). Axes with use* = false are left alone.
Packet setPosition(const Axes& target, bool absolute, float durationSec, bool useYaw = true, bool useRoll = true,
                   bool usePitch = true);

// Rotates at `degPerSec` (clamped to ±360). The gimbal stops after 0.5 s, so resend it periodically.
// takeover = false hands speed control back to the gimbal.
Packet setSpeed(const Axes& degPerSec, bool takeover = true);

// Joystick-style control: each axis in -1..1, scaled by the gimbal's own joystick speed and smoothing settings.
// No reply frame.
Packet joystick(float pitch, float roll, float yaw);

Packet getAngles(AngleType type = AngleType::Attitude);
Packet setParameterPush(bool enable);
Packet getVersion();
Packet recenter();
Packet selfie();
Packet setFollowMode(FollowMode mode);
Packet toggleActiveTrack();

// Third-party camera control through the camera control cable.
Packet cameraRecord(bool start);
Packet cameraShutter();

bool parseReturnCode(const Packet& reply, ReturnCode& code);
// Reply to getAngles() or a kPush frame with valid angles.
bool parseAngles(const Packet& reply, Axes& out);
// Version 0xAABBCCDD means AA.BB.CC.DD.
bool parseVersion(const Packet& reply, uint32_t& version);

}  // namespace dji
