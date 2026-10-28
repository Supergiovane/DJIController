#include "DjiCrc.h"

namespace dji {
namespace {

// The R SDK document lists the polynomials and init values in normal (MSB-first) form. Both CRCs are
// reflected, so we run them LSB-first with bit-reversed constants: 0xC55C and 0xC55C0000 both reverse to 0x3AA3.
constexpr uint16_t kCrc16PolyReflected = 0xA001;
constexpr uint32_t kCrc32PolyReflected = 0xEDB88320;
constexpr uint16_t kCrc16Init = 0x3AA3;
constexpr uint32_t kCrc32Init = 0x00003AA3;

}  // namespace

uint16_t crc16(const uint8_t* data, size_t len) {
  uint16_t crc = kCrc16Init;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc & 1) ? (crc >> 1) ^ kCrc16PolyReflected : crc >> 1;
    }
  }
  return crc;
}

uint32_t crc32(const uint8_t* data, size_t len) {
  uint32_t crc = kCrc32Init;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc & 1) ? (crc >> 1) ^ kCrc32PolyReflected : crc >> 1;
    }
  }
  return crc;
}

}  // namespace dji
