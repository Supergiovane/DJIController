#pragma once

#include <cstddef>
#include <cstdint>

// Thin wrapper over the ESP32 TWAI (CAN) peripheral, set up for the R SDK: 1 Mbit/s, standard 11-bit IDs.
class CanBus {
 public:
  // Starts the driver. Only frames with `acceptId` are received.
  bool begin(int txPin, int rxPin, uint32_t acceptId);
  bool send(uint32_t id, const uint8_t* data, size_t len, uint32_t timeoutMs = 10);
  // Returns the payload length of the next received frame, or -1 if none arrives within `timeoutMs`.
  int receive(uint8_t* data, uint32_t timeoutMs = 0);
  // Restarts the controller after a bus-off (e.g. the gimbal was unplugged). Call from loop().
  void service();
};
