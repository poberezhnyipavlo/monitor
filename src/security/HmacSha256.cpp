#include "HmacSha256.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>

#include <stdexcept>

HmacSha256::HmacSha256(std::span<const uint8_t> key) : key_(key.begin(), key.end()) {}

sig::Mac HmacSha256::sign(std::string_view data) const {
    sig::Mac mac{};
    std::size_t mac_len = 0;
    // EVP_Q_mac: one-shot MAC (OpenSSL 3+). "HMAC" with "SHA256" inside.
    const unsigned char* out = EVP_Q_mac(
        nullptr,  // default library context
        "HMAC",
        nullptr,  // no property query
        "SHA256",
        nullptr,
        key_.data(),
        key_.size(),
        reinterpret_cast<const unsigned char*>(data.data()),
        data.size(),
        mac.data(),
        mac.size(),
        &mac_len
    );

    if (out == nullptr || mac_len != mac.size()) {
        throw std::runtime_error("HMAC-SHA256 failed");
    }

    return mac;
}

bool HmacSha256::verify(std::string_view data, const sig::Mac& mac) const {
    const sig::Mac expected = sign(data);

    // Not ==: it stops at the first wrong byte, so the more leading bytes of a forged MAC are
    // right, the later the "no" comes. Averaging many tries cancels network jitter and lets an
    // attacker guess the MAC byte by byte. CRYPTO_memcmp always compares all 32 bytes: its time
    // does not depend on how much of the MAC is right.
    return CRYPTO_memcmp(expected.data(), mac.data(), mac.size()) == 0;
}

CheckedLine checkLine(std::string_view line, const HmacSha256& hmac) {
    const auto split = sig::splitSignature(line);
    if (!split) {
        return CheckedLine{
            .result = LineCheck::kUnsigned,
            .payload = {},
        };
    }

    const auto& [payload, mac] = *split;
    if (!hmac.verify(payload, mac)) {
        return CheckedLine{
            .result = LineCheck::kBadSignature,
            .payload = {},
        };
    }

    return CheckedLine{
        .result = LineCheck::kOk,
        .payload = payload,
    };
}
