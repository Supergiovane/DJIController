#pragma once

#include <DjiCommands.h>

#include "Gimbal.h"

// Streams speed or joystick commands: the gimbal drops them after 0.5 s unless they are refreshed.
class MotionHold {
 public:
  enum class Mode { None, Speed, Joystick };

  explicit MotionHold(Gimbal& gimbal) : gimbal_(gimbal) {}

  // Speed in deg/s, joystick in -1..1 per axis. Holds for `durationMs` (0 = until stop()); calling again refreshes.
  void start(Mode mode, const dji::Axes& value, uint32_t durationMs = 0);
  void stop();
  // Call from loop().
  void update();

  Mode mode() const { return mode_; }

 private:
  void sendCurrent();

  Gimbal& gimbal_;
  Mode mode_ = Mode::None;
  dji::Axes value_;
  uint32_t startMs_ = 0;
  uint32_t durationMs_ = 0;
  uint32_t lastSendMs_ = 0;
};
