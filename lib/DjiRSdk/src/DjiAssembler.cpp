#include "DjiAssembler.h"

#include <cstring>

#include "DjiCrc.h"

namespace dji {

bool PacketAssembler::push(uint8_t byte) {
  if (len_ == 0 && byte != kSof) return false;
  buf_[len_++] = byte;

  bool completed = false;
  for (;;) {
    const Check result = check();
    if (result == Check::NeedMore) return completed;
    completed |= result == Check::Complete;
    discard(result == Check::Complete ? packetLength(buf_) : 1);
  }
}

PacketAssembler::Check PacketAssembler::check() {
  if (len_ < 3) return Check::NeedMore;
  const size_t expected = packetLength(buf_);
  if (expected < kOverhead || expected > kMaxPacketLen) return Check::Invalid;
  if (len_ < kHeaderLen) return Check::NeedMore;
  if (crc16(buf_, 10) != getLe16(buf_ + 10)) return Check::Invalid;
  if (len_ < expected) return Check::NeedMore;
  return decode(buf_, expected, packet_) == DecodeResult::Ok ? Check::Complete : Check::Invalid;
}

void PacketAssembler::discard(size_t count) {
  size_t start = count;
  while (start < len_ && buf_[start] != kSof) ++start;
  len_ -= start;
  memmove(buf_, buf_ + start, len_);
}

}  // namespace dji
