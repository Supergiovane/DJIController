#include "DjiPacket.h"

#include <cstring>

#include "DjiCrc.h"

namespace dji {

size_t encode(const Packet& packet, uint8_t* out, size_t outCap) {
  const size_t total = kOverhead + packet.payloadLen;
  if (packet.payloadLen > kMaxPayloadLen || total > outCap) return 0;

  out[0] = kSof;
  putLe16(out + 1, total);  // Version bits [15:10] stay 0.
  out[3] = packet.cmdType;
  memset(out + 4, 0, 4);  // ENC = unencrypted, RES = 0.
  putLe16(out + 8, packet.seq);
  putLe16(out + 10, crc16(out, 10));
  out[12] = packet.cmdSet;
  out[13] = packet.cmdId;
  memcpy(out + 14, packet.payload, packet.payloadLen);
  putLe32(out + total - 4, crc32(out, total - 4));
  return total;
}

DecodeResult decode(const uint8_t* buf, size_t len, Packet& out) {
  if (len < kOverhead) return DecodeResult::TooShort;
  if (buf[0] != kSof) return DecodeResult::BadSof;
  const size_t total = packetLength(buf);
  if (total < kOverhead || total > kMaxPacketLen || total > len) return DecodeResult::BadLength;
  if (buf[4] != 0) return DecodeResult::Encrypted;
  if (crc16(buf, 10) != getLe16(buf + 10)) return DecodeResult::BadHeaderCrc;
  if (crc32(buf, total - 4) != getLe32(buf + total - 4)) return DecodeResult::BadCrc;

  out.cmdType = buf[3];
  out.seq = getLe16(buf + 8);
  out.cmdSet = buf[12];
  out.cmdId = buf[13];
  out.payloadLen = total - kOverhead;
  memcpy(out.payload, buf + 14, out.payloadLen);
  return DecodeResult::Ok;
}

}  // namespace dji
