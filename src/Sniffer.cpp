#include "Sniffer.h"

#include <Arduino.h>
#include <DjiCommands.h>

namespace {

constexpr uint32_t kSummaryIntervalMs = 3000;

const char* commandName(uint8_t cmdSet, uint8_t cmdId) {
  if (cmdSet == dji::cmdset::kCamera) return cmdId == dji::camera::kMotion ? "CameraControl" : "CameraStatus";
  if (cmdSet != dji::cmdset::kGimbal) return "?";
  static const char* const kNames[] = {
      "SetPosition", "SetSpeed",     "GetAngles",    "SetLimits",      "GetLimits",      "SetStiffness", "GetStiffness",
      "SetPush",     "Push",         "Version",      "ExternalControl", "GetUserParams", "SetUserParams", "OperatingMode",
      "RecenterFollow", "AutoTune",  "AutoTuneStatus", "ActiveTrack",  "FocusMotor"};
  return cmdId < sizeof(kNames) / sizeof(kNames[0]) ? kNames[cmdId] : "?";
}

float tenths(const uint8_t* p) {
  return int16_t(dji::getLe16(p)) / 10.0f;
}

// Human-readable fields for the commands whose layout we know.
String describe(const dji::Packet& p) {
  const uint8_t* d = p.payload;
  const size_t n = p.payloadLen;
  char buf[96] = "";
  if (p.isReply()) {
    if (p.cmdSet == dji::cmdset::kGimbal && p.cmdId == dji::gimbal::kGetAngles && n >= 8) {
      snprintf(buf, sizeof(buf), "ret %u type %u yaw %.1f roll %.1f pitch %.1f", d[0], d[1], tenths(d + 2), tenths(d + 4),
               tenths(d + 6));
    } else if (n >= 1) {
      snprintf(buf, sizeof(buf), "ret %u (%s)", d[0], dji::toString(dji::ReturnCode(d[0])));
    }
    return buf;
  }
  if (p.cmdSet == dji::cmdset::kGimbal) {
    switch (p.cmdId) {
      case dji::gimbal::kSetPosition:
        if (n >= 8)
          snprintf(buf, sizeof(buf), "yaw %.1f roll %.1f pitch %.1f ctrl 0x%02x time %.1fs", tenths(d), tenths(d + 2),
                   tenths(d + 4), d[6], d[7] / 10.0f);
        break;
      case dji::gimbal::kSetSpeed:
        if (n >= 7)
          snprintf(buf, sizeof(buf), "yaw %.1f roll %.1f pitch %.1f deg/s ctrl 0x%02x", tenths(d), tenths(d + 2),
                   tenths(d + 4), d[6]);
        break;
      case dji::gimbal::kExternalControl:
        if (n >= 7 && d[0] == 0x01) {
          snprintf(buf, sizeof(buf), "joystick pitch %d roll %d yaw %d", int16_t(dji::getLe16(d + 1)),
                   int16_t(dji::getLe16(d + 3)), int16_t(dji::getLe16(d + 5)));
        } else if (n >= 3 && d[0] == 0x02) {
          snprintf(buf, sizeof(buf), "dial %d", int16_t(dji::getLe16(d + 1)));
        }
        break;
      case dji::gimbal::kPush:
        if (n >= 7 && (d[0] & 0x01))
          snprintf(buf, sizeof(buf), "yaw %.1f roll %.1f pitch %.1f", tenths(d + 1), tenths(d + 3), tenths(d + 5));
        break;
      case dji::gimbal::kRecenterOrFollow:
        if (n >= 2) snprintf(buf, sizeof(buf), "mode 0x%02x action 0x%02x", d[0], d[1]);
        break;
    }
  } else if (p.cmdSet == dji::cmdset::kCamera && p.cmdId == dji::camera::kMotion && n >= 2) {
    snprintf(buf, sizeof(buf), "camera command %u", dji::getLe16(d));
  }
  return buf;
}

uint32_t signature(const dji::Packet& p) {
  // FNV-1a over everything but the sequence number, which changes on every packet.
  uint32_t hash = 2166136261u;
  auto mix = [&hash](uint8_t b) { hash = (hash ^ b) * 16777619u; };
  mix(p.cmdType);
  mix(p.cmdSet);
  mix(p.cmdId);
  for (size_t i = 0; i < p.payloadLen; ++i) mix(p.payload[i]);
  return hash;
}

void printHex(const uint8_t* data, size_t len) {
  for (size_t i = 0; i < len; ++i) Serial.printf("%02x ", data[i]);
}

void printTimestamp() {
  const uint32_t ms = millis();
  Serial.printf("[%6lu.%03lu] ", (unsigned long)(ms / 1000), (unsigned long)(ms % 1000));
}

}  // namespace

void Sniffer::start(bool raw) {
  raw_ = raw;
  streamCount_ = 0;
  lastSummaryMs_ = millis();
  Serial.printf("sniffer: listen-only, all IDs%s. Type 'sniff off' to stop.\n", raw ? ", raw frames" : "");
}

void Sniffer::poll() {
  uint8_t frame[8];
  uint32_t id = 0;
  int len;
  while ((len = bus_.receive(frame, 0, &id)) >= 0) {
    Stream* s = stream(id);
    if (!s) continue;
    ++s->frames;
    memcpy(s->lastFrame, frame, len);
    s->lastFrameLen = len;
    if (raw_) {
      printTimestamp();
      Serial.printf("0x%03lx [%d] ", (unsigned long)id, len);
      printHex(frame, len);
      Serial.println();
    }
    for (int i = 0; i < len; ++i) {
      if (s->assembler.push(frame[i])) onPacket(*s, s->assembler.packet());
    }
  }
  if (!raw_ && millis() - lastSummaryMs_ >= kSummaryIntervalMs) printSummary();
}

Sniffer::Stream* Sniffer::stream(uint32_t id) {
  for (int i = 0; i < streamCount_; ++i) {
    if (streams_[i].id == id) return &streams_[i];
  }
  if (streamCount_ == kMaxStreams) return nullptr;
  Stream& s = streams_[streamCount_++];
  s = Stream();
  s.id = id;
  return &s;
}

void Sniffer::onPacket(Stream& s, const dji::Packet& packet) {
  ++s.packets;
  const uint32_t sig = signature(packet);
  if (sig == s.lastSignature) {
    ++s.repeats;
    return;
  }
  flushRepeats(s);
  s.lastSignature = sig;

  printTimestamp();
  Serial.printf("0x%03lx %-5s %02X:%02X %-15s t%02x seq %5u | ", (unsigned long)s.id, packet.isReply() ? "reply" : "cmd",
                packet.cmdSet, packet.cmdId, commandName(packet.cmdSet, packet.cmdId), packet.cmdType, packet.seq);
  printHex(packet.payload, packet.payloadLen);
  const String fields = describe(packet);
  if (fields.length() > 0) Serial.print("| " + fields);
  Serial.println();
}

void Sniffer::flushRepeats(Stream& s) {
  if (s.repeats == 0) return;
  Serial.printf("             0x%03lx       ... same packet %lu more times\n", (unsigned long)s.id,
                (unsigned long)s.repeats);
  s.repeats = 0;
}

void Sniffer::printSummary() {
  lastSummaryMs_ = millis();
  for (int i = 0; i < streamCount_; ++i) {
    Stream& s = streams_[i];
    flushRepeats(s);
    if (s.frames == 0) continue;
    printTimestamp();
    Serial.printf("0x%03lx: %lu frames in %lus, %lu R SDK packets so far, last frame: ", (unsigned long)s.id,
                  (unsigned long)s.frames, (unsigned long)(kSummaryIntervalMs / 1000), (unsigned long)s.packets);
    printHex(s.lastFrame, s.lastFrameLen);
    Serial.println();
    s.frames = 0;
  }
}
