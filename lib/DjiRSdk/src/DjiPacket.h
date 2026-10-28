#pragma once

#include <cstddef>
#include <cstdint>

namespace dji {

// Packet layout (DJI R SDK v2.3, section 2.1):
//   SOF(1) Ver/Length(2) CmdType(1) ENC(1) RES(3) SEQ(2) CRC16(2) | CmdSet(1) CmdID(1) Data(n) | CRC32(4)
// All multi-byte fields are little-endian.
constexpr uint8_t kSof = 0xAA;
constexpr size_t kHeaderLen = 12;
constexpr size_t kOverhead = kHeaderLen + 2 + 4;
constexpr size_t kMaxPacketLen = 64;
constexpr size_t kMaxPayloadLen = kMaxPacketLen - kOverhead;

// CmdType bits [4:0]: reply type. Bit 5: set on reply frames.
constexpr uint8_t kReplyNone = 0x00;
constexpr uint8_t kReplyRequired = 0x03;
constexpr uint8_t kReplyFrameFlag = 0x20;

struct Packet {
  uint8_t cmdType = kReplyRequired;
  uint16_t seq = 0;
  uint8_t cmdSet = 0;
  uint8_t cmdId = 0;
  uint8_t payload[kMaxPayloadLen] = {};
  size_t payloadLen = 0;

  bool isReply() const { return (cmdType & kReplyFrameFlag) != 0; }
};

enum class DecodeResult { Ok, TooShort, BadSof, BadLength, Encrypted, BadHeaderCrc, BadCrc };

// Serializes `packet` into `out`. Returns the packet length, or 0 if it does not fit.
size_t encode(const Packet& packet, uint8_t* out, size_t outCap);

// Parses one complete packet from `buf`, verifying both CRCs.
DecodeResult decode(const uint8_t* buf, size_t len, Packet& out);

// Total packet length declared in a header (needs at least 3 bytes).
inline size_t packetLength(const uint8_t* header) {
  return (header[1] | (header[2] << 8)) & 0x03FF;
}

inline void putLe16(uint8_t* p, uint16_t v) {
  p[0] = v & 0xFF;
  p[1] = v >> 8;
}

inline void putLe32(uint8_t* p, uint32_t v) {
  for (int i = 0; i < 4; ++i) p[i] = (v >> (8 * i)) & 0xFF;
}

inline uint16_t getLe16(const uint8_t* p) {
  return p[0] | (p[1] << 8);
}

inline uint32_t getLe32(const uint8_t* p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

}  // namespace dji
