#pragma once

#include <cstddef>
#include <cstdint>

namespace dji {

// CRC-16 over the packet header (bytes 0..9). Poly 0x8005, reflected, init 0xC55C (0x3AA3 reflected).
uint16_t crc16(const uint8_t* data, size_t len);

// CRC-32 over the whole packet minus its trailing CRC. Poly 0x04C11DB7, reflected, init 0xC55C0000 (0x3AA3 reflected).
uint32_t crc32(const uint8_t* data, size_t len);

}  // namespace dji
