#pragma once

#include <cstddef>
#include <cstdint>

#include "DjiPacket.h"

namespace dji {

// Rebuilds R SDK packets from the byte stream carried by consecutive 8-byte CAN frames.
// Bytes before a SOF are skipped. When a candidate packet has a bad length or CRC, the assembler drops its SOF
// and rescans the buffered bytes from the next SOF, so a real packet hidden behind noise is not lost.
class PacketAssembler {
 public:
  // Feeds one byte. Returns true when this byte completes a valid packet, readable via packet().
  bool push(uint8_t byte);

  const Packet& packet() const { return packet_; }
  void reset() { len_ = 0; }

 private:
  enum class Check { NeedMore, Invalid, Complete };

  Check check();
  // Drops `count` bytes, then any bytes before the next SOF.
  void discard(size_t count);

  uint8_t buf_[kMaxPacketLen] = {};
  size_t len_ = 0;
  Packet packet_;
};

}  // namespace dji
