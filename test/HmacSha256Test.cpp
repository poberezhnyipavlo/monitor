#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "security/HmacSha256.h"

namespace {

std::string hex(const sig::Mac& mac) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    for (const uint8_t byte : mac) {
        out += kHex[byte >> 4];
        out += kHex[byte & 0x0F];
    }
    return out;
}

std::vector<uint8_t> bytes(std::string_view text) {
    return {text.begin(), text.end()};
}

}  // namespace

// RFC 4231, test case 1: 20 bytes of 0x0b, "Hi There".
TEST(HmacSha256, Rfc4231Case1) {
    const HmacSha256 hmac(std::vector<uint8_t>(20, 0x0b));
    EXPECT_EQ(
        hex(hmac.sign("Hi There")),
        "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7"
    );
}

// RFC 4231, test case 2: a key shorter than the hash, "Jefe".
TEST(HmacSha256, Rfc4231Case2) {
    const HmacSha256 hmac(bytes("Jefe"));
    EXPECT_EQ(
        hex(hmac.sign("what do ya want for nothing?")),
        "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843"
    );
}

TEST(HmacSha256, VerifiesOwnSignature) {
    const HmacSha256 hmac(std::vector<uint8_t>(32, 0x42));
    const std::string_view line = "ver=2 dev=20:9b:a9:61:15:6c seq=1 name=t value=24.3";
    EXPECT_TRUE(hmac.verify(line, hmac.sign(line)));
}

TEST(HmacSha256, RejectsChangedDataMacOrKey) {
    const HmacSha256 hmac(std::vector<uint8_t>(32, 0x42));
    const HmacSha256 other(std::vector<uint8_t>(32, 0x43));
    const std::string_view line = "ver=2 dev=20:9b:a9:61:15:6c seq=1 name=t value=24.3";
    const sig::Mac mac = hmac.sign(line);

    EXPECT_FALSE(hmac.verify("ver=2 dev=20:9b:a9:61:15:6c seq=1 name=t value=99.9", mac));
    sig::Mac last_bit = mac;
    last_bit.back() ^= 0x01;
    EXPECT_FALSE(hmac.verify(line, last_bit));
    EXPECT_FALSE(other.verify(line, mac));
}

namespace {

std::string signedLine(const HmacSha256& hmac, std::string_view payload) {
    std::string buf(payload.size() + sig::kSuffixLen + 1, '\0');
    payload.copy(buf.data(), payload.size());
    const std::size_t len =
        sig::appendSignature(buf.data(), payload.size(), buf.size(), hmac.sign(payload));
    buf.resize(len);
    return buf;
}

constexpr std::string_view kPayload =
    "ver=2 dev=20:9b:a9:61:15:6c boot=3f2a91c4 seq=42 age_ms=1500 name=temperature_c value=24.3";

}  // namespace

TEST(CheckLine, AcceptsLineSignedWithOurKey) {
    const HmacSha256 hmac(std::vector<uint8_t>(32, 0x42));
    const std::string line = signedLine(hmac, kPayload);
    const CheckedLine checked = checkLine(line, hmac);
    EXPECT_EQ(checked.result, LineCheck::kOk);
    EXPECT_EQ(checked.payload, kPayload);
}

TEST(CheckLine, RejectsUnsigned) {
    const HmacSha256 hmac(std::vector<uint8_t>(32, 0x42));
    const CheckedLine checked = checkLine(kPayload, hmac);
    EXPECT_EQ(checked.result, LineCheck::kUnsigned);
    EXPECT_TRUE(checked.payload.empty());
}

TEST(CheckLine, RejectsOtherKeyAndChangedValue) {
    const HmacSha256 hmac(std::vector<uint8_t>(32, 0x42));
    const HmacSha256 attacker(std::vector<uint8_t>(32, 0x66));
    EXPECT_EQ(checkLine(signedLine(attacker, kPayload), hmac).result, LineCheck::kBadSignature);

    std::string changed = signedLine(hmac, kPayload);
    changed.replace(changed.find("24.3"), 4, "99.9");
    const CheckedLine checked = checkLine(changed, hmac);
    EXPECT_EQ(checked.result, LineCheck::kBadSignature);
    EXPECT_TRUE(checked.payload.empty());
}