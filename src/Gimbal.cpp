#include "Gimbal.h"

#include <Arduino.h>

using dji::Packet;

bool Gimbal::send(Packet& packet) {
  packet.seq = ++seq_;
  uint8_t buf[dji::kMaxPacketLen];
  const size_t len = dji::encode(packet, buf, sizeof(buf));
  if (len == 0) return false;
  // Frames of one packet must go out back to back on the same ID.
  for (size_t offset = 0; offset < len; offset += 8) {
    const size_t chunk = len - offset < 8 ? len - offset : 8;
    if (!bus_.send(kTxId, buf + offset, chunk)) return false;
  }
  return true;
}

bool Gimbal::request(Packet& packet, Packet& reply, uint32_t timeoutMs) {
  if (!send(packet)) return false;
  const uint32_t start = millis();
  uint32_t elapsed = 0;
  // Replies echo CmdSet/CmdID; we don't rely on SEQ being echoed.
  while (readPacket(reply, timeoutMs - elapsed)) {
    if (reply.isReply() && reply.cmdSet == packet.cmdSet && reply.cmdId == packet.cmdId) return true;
    dispatch(reply);
    elapsed = millis() - start;
    if (elapsed >= timeoutMs) break;
  }
  return false;
}

void Gimbal::poll() {
  Packet packet;
  while (readPacket(packet, 0)) dispatch(packet);
}

bool Gimbal::readPacket(Packet& out, uint32_t timeoutMs) {
  const uint32_t start = millis();
  for (;;) {
    // Finish the current frame first: a packet can end in the middle of one.
    while (framePos_ < frameLen_) {
      if (assembler_.push(frame_[framePos_++])) {
        out = assembler_.packet();
        return true;
      }
    }
    const uint32_t elapsed = millis() - start;
    if (elapsed > timeoutMs) return false;
    framePos_ = 0;
    frameLen_ = bus_.receive(frame_, timeoutMs - elapsed);
    if (frameLen_ < 0) {
      frameLen_ = 0;
      return false;
    }
  }
}

void Gimbal::dispatch(const Packet& packet) {
  if (pushHandler_) pushHandler_(packet);
}
