#pragma once

#include <metric_protocol/Signature.h>

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

// HMAC-SHA256 with one shared key (OpenSSL). The node computes the same with mbedTLS: same
// algorithm, different library, so both sides are checked against the RFC 4231 test vectors.
class HmacSha256 {
public:
    // Any key length works for HMAC; the protocol uses 32 bytes (sig::Key). The tests use the
    // RFC 4231 keys (4 and 20 bytes).
    explicit HmacSha256(std::span<const uint8_t> key);

    // Throws std::runtime_error if OpenSSL fails (out of memory, no SHA-256).
    [[nodiscard]] sig::Mac sign(std::string_view data) const;

    // true if mac is the MAC of data. Compares in constant time.
    [[nodiscard]] bool verify(std::string_view data, const sig::Mac& mac) const;

private:
    std::vector<uint8_t> key_;
};

enum class LineCheck {
    kOk,
    kUnsigned,      // no " sig=<64 hex>" at the end
    kBadSignature,  // signed, but not with our key, or the line was changed on the way
};

struct CheckedLine {
    LineCheck result;
    std::string_view payload;  // the line without the signature; empty unless kOk
};

// What every line from a node goes through before parsing.
[[nodiscard]] CheckedLine checkLine(std::string_view line, const HmacSha256& hmac);
