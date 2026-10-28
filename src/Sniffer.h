#pragma once

#include <DjiAssembler.h>
#include <DjiPacket.h>

#include "CanBus.h"

// Passive bus monitor for reverse engineering (e.g. what an existing RSA accessory sends). Run the bus in
// listen-only mode. R SDK packets are rebuilt on every CAN ID and printed decoded; other traffic is summarized
// every few seconds, or printed frame by frame in raw mode.
class Sniffer {
 public:
  explicit Sniffer(CanBus& bus) : bus_(bus) {}

  void start(bool raw);
  // Drains received frames and prints them. Call from loop().
  void poll();

 private:
  static constexpr int kMaxStreams = 16;

  struct Stream {
    uint32_t id = 0;
    uint32_t frames = 0;
    uint32_t packets = 0;
    uint8_t lastFrame[8] = {};
    int lastFrameLen = 0;
    dji::PacketAssembler assembler;
    // Identical consecutive packets are folded into a repeat count.
    uint32_t lastSignature = 0;
    uint32_t repeats = 0;
  };

  Stream* stream(uint32_t id);
  void onPacket(Stream& s, const dji::Packet& packet);
  void flushRepeats(Stream& s);
  void printSummary();

  CanBus& bus_;
  Stream streams_[kMaxStreams];
  int streamCount_ = 0;
  bool raw_ = false;
  uint32_t lastSummaryMs_ = 0;
};
