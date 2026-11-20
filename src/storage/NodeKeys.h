#pragma once

// Byte layout of LMDB keys for node data. LMDB sorts keys byte by byte from the left, so numbers
// are written big-endian (most significant byte first): then key order == number order == time
// order.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace node_keys {
inline constexpr std::size_t kDevBytes = 6;
using DevBytes = std::array<uint8_t, kDevBytes>;

// "20:9b:a9:61:15:6c" -> {0x20, 0x9b, ...}. nullopt unless metric::isValidDev(dev).
[[nodiscard]] std::optional<DevBytes> devFromString(std::string_view dev);

void appendBigEndian32(std::string& out, uint32_t value);
void appendBigEndian64(std::string& out, uint64_t value);

// The first 8 bytes of `bytes` as a big-endian number. Precondition: bytes.size() >= 8.
[[nodiscard]] uint64_t readBigEndian64(std::string_view bytes);

// dev(6) | boot(4) | seq(4): identity of one metric line or one log line. 14 bytes.
[[nodiscard]] std::string recordKey(const DevBytes& dev, uint32_t boot, uint32_t seq);

// dev(6) | name | 0x00: the start of every key of one series. 0x00 ends the name, so the series
// "temp" never includes "temp_alert" (names are [a-z0-9_], 0x00 is never part of them).
[[nodiscard]] std::string seriesPrefix(const DevBytes& dev, std::string_view name);

// seriesPrefix(dev, name) | ts_ms(8).
[[nodiscard]] std::string seriesKey(const DevBytes& dev, std::string_view name, uint64_t ts_ms);
}  // namespace node_keys
