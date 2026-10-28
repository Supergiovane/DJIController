#include <unity.h>

#include <cstring>

#include "DjiAssembler.h"
#include "DjiCommands.h"
#include "DjiCrc.h"
#include "DjiPacket.h"

using namespace dji;

// Position control sample from DJI R SDK v2.3, section 3.3: yaw 3.2°, roll 4.8°, pitch 6.4°, absolute, 2 s.
static const uint8_t kSample[] = {0xAA, 0x1A, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x22, 0x11, 0xA2, 0x42, 0x0E,
                                  0x00, 0x20, 0x00, 0x30, 0x00, 0x40, 0x00, 0x01, 0x14, 0x7B, 0x40, 0x97, 0xBE};

void setUp() {}
void tearDown() {}

static void test_crc16_matches_sample() {
  TEST_ASSERT_EQUAL_HEX16(0x42A2, crc16(kSample, 10));
}

static void test_crc32_matches_sample() {
  TEST_ASSERT_EQUAL_HEX32(0xBE97407B, crc32(kSample, sizeof(kSample) - 4));
}

static void test_set_position_encodes_sample() {
  Packet p = setPosition({3.2f, 4.8f, 6.4f}, true, 2.0f);
  p.seq = 0x1122;
  uint8_t out[kMaxPacketLen];
  TEST_ASSERT_EQUAL(sizeof(kSample), encode(p, out, sizeof(out)));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(kSample, out, sizeof(kSample));
}

static void test_decode_sample() {
  Packet p;
  TEST_ASSERT_EQUAL(DecodeResult::Ok, decode(kSample, sizeof(kSample), p));
  TEST_ASSERT_EQUAL_HEX8(0x0E, p.cmdSet);
  TEST_ASSERT_EQUAL_HEX8(0x00, p.cmdId);
  TEST_ASSERT_EQUAL_HEX16(0x1122, p.seq);
  TEST_ASSERT_EQUAL(8, p.payloadLen);
  TEST_ASSERT_FALSE(p.isReply());
}

static void test_decode_rejects_corruption() {
  uint8_t buf[sizeof(kSample)];
  Packet p;

  memcpy(buf, kSample, sizeof(buf));
  buf[3] ^= 0x01;
  TEST_ASSERT_EQUAL(DecodeResult::BadHeaderCrc, decode(buf, sizeof(buf), p));

  memcpy(buf, kSample, sizeof(buf));
  buf[15] ^= 0x01;
  TEST_ASSERT_EQUAL(DecodeResult::BadCrc, decode(buf, sizeof(buf), p));

  TEST_ASSERT_EQUAL(DecodeResult::BadLength, decode(kSample, sizeof(kSample) - 1, p));
}

static void test_assembler_skips_noise_and_rebuilds_packet() {
  PacketAssembler assembler;
  const uint8_t noise[] = {0x00, 0x13, 0xAA, 0x01};  // Includes a stray SOF with an invalid length.
  for (uint8_t b : noise) TEST_ASSERT_FALSE(assembler.push(b));

  int completed = 0;
  for (size_t i = 0; i < sizeof(kSample); ++i) {
    if (assembler.push(kSample[i])) {
      ++completed;
      TEST_ASSERT_EQUAL(sizeof(kSample) - 1, i);
    }
  }
  TEST_ASSERT_EQUAL(1, completed);
  TEST_ASSERT_EQUAL_HEX16(0x1122, assembler.packet().seq);
}

static void test_assembler_drops_bad_crc_and_recovers() {
  PacketAssembler assembler;
  uint8_t bad[sizeof(kSample)];
  memcpy(bad, kSample, sizeof(bad));
  bad[sizeof(bad) - 1] ^= 0xFF;
  for (uint8_t b : bad) TEST_ASSERT_FALSE(assembler.push(b));

  bool done = false;
  for (uint8_t b : kSample) done = assembler.push(b);
  TEST_ASSERT_TRUE(done);
}

static Packet reply(uint8_t cmdId, const uint8_t* payload, size_t len) {
  Packet p;
  p.cmdType = kReplyFrameFlag;
  p.cmdSet = cmdset::kGimbal;
  p.cmdId = cmdId;
  memcpy(p.payload, payload, len);
  p.payloadLen = len;
  return p;
}

static void test_parse_angles_reply() {
  // yaw -90.0°, roll 0.5°, pitch 12.3°.
  const uint8_t payload[] = {0x00, 0x01, 0x7C, 0xFC, 0x05, 0x00, 0x7B, 0x00};
  Axes a;
  TEST_ASSERT_TRUE(parseAngles(reply(gimbal::kGetAngles, payload, sizeof(payload)), a));
  TEST_ASSERT_EQUAL_FLOAT(-90.0f, a.yaw);
  TEST_ASSERT_EQUAL_FLOAT(0.5f, a.roll);
  TEST_ASSERT_EQUAL_FLOAT(12.3f, a.pitch);

  const uint8_t notReady[] = {0x00, 0x00, 0, 0, 0, 0, 0, 0};
  TEST_ASSERT_FALSE(parseAngles(reply(gimbal::kGetAngles, notReady, sizeof(notReady)), a));
}

static void test_parse_version_reply() {
  const uint8_t payload[] = {0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x03, 0x02, 0x01};
  uint32_t version = 0;
  TEST_ASSERT_TRUE(parseVersion(reply(gimbal::kVersion, payload, sizeof(payload)), version));
  TEST_ASSERT_EQUAL_HEX32(0x01020304, version);
}

static void test_speed_and_joystick_clamp() {
  Packet s = setSpeed({1000.0f, 0.0f, -1000.0f});
  TEST_ASSERT_EQUAL_INT16(3600, int16_t(getLe16(s.payload)));
  TEST_ASSERT_EQUAL_INT16(-3600, int16_t(getLe16(s.payload + 4)));
  TEST_ASSERT_EQUAL_HEX8(0x80, s.payload[6]);

  Packet j = joystick(2.0f, 0.0f, -0.5f);
  TEST_ASSERT_EQUAL_HEX8(kReplyNone, j.cmdType);
  TEST_ASSERT_EQUAL_HEX8(0x01, j.payload[0]);
  TEST_ASSERT_EQUAL_INT16(15000, int16_t(getLe16(j.payload + 1)));
  TEST_ASSERT_EQUAL_INT16(-7500, int16_t(getLe16(j.payload + 5)));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_crc16_matches_sample);
  RUN_TEST(test_crc32_matches_sample);
  RUN_TEST(test_set_position_encodes_sample);
  RUN_TEST(test_decode_sample);
  RUN_TEST(test_decode_rejects_corruption);
  RUN_TEST(test_assembler_skips_noise_and_rebuilds_packet);
  RUN_TEST(test_assembler_drops_bad_crc_and_recovers);
  RUN_TEST(test_parse_angles_reply);
  RUN_TEST(test_parse_version_reply);
  RUN_TEST(test_speed_and_joystick_clamp);
  return UNITY_END();
}
