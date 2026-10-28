#pragma once

#include <DjiAssembler.h>
#include <DjiPacket.h>

#include "CanBus.h"

// R SDK client: frames packets onto the CAN bus and matches replies to requests.
class Gimbal {
 public:
  // CAN IDs seen from the controller side (R SDK v2.3, section 3.1).
  static constexpr uint32_t kTxId = 0x223;
  static constexpr uint32_t kRxId = 0x222;

  using PushHandler = void (*)(const dji::Packet&);

  explicit Gimbal(CanBus& bus) : bus_(bus) {}

  // Sends a packet without waiting for a reply. Assigns the sequence number.
  bool send(dji::Packet& packet);
  // Sends a packet and waits for the reply with the same CmdSet/CmdID.
  bool request(dji::Packet& packet, dji::Packet& reply, uint32_t timeoutMs = 200);
  // Drains received frames. Packets that are not awaited replies (e.g. parameter pushes) go to the push handler.
  void poll();

  void onPush(PushHandler handler) { pushHandler_ = handler; }

 private:
  // Reads frames for up to `timeoutMs`; returns true once a full packet has been assembled into `out`.
  bool readPacket(dji::Packet& out, uint32_t timeoutMs);
  void dispatch(const dji::Packet& packet);

  CanBus& bus_;
  dji::PacketAssembler assembler_;
  uint16_t seq_ = 0;
  PushHandler pushHandler_ = nullptr;
  uint8_t frame_[8] = {};
  int frameLen_ = 0;
  int framePos_ = 0;
};
