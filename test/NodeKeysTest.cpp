#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "storage/NodeKeys.h"

namespace {

using node_keys::DevBytes;

const DevBytes kDev{0x20, 0x9b, 0xa9, 0x61, 0x15, 0x6c};

// Bytes written as text, for readable expectations: bytes({0x01, 0x00}).
std::string bytes(std::initializer_list<int> values) {
    std::string out;
    for (const int v : values) {
        out.push_back(static_cast<char>(v));
    }
    return out;
}

}  // namespace

TEST(NodeKeys, DevFromString) {
    EXPECT_EQ(node_keys::devFromString("20:9b:a9:61:15:6c"), kDev);
    EXPECT_EQ(
        node_keys::devFromString("00:ff:0a:a0:09:90"),
        (DevBytes{0x00, 0xff, 0x0a, 0xa0, 0x09, 0x90})
    );
    EXPECT_FALSE(node_keys::devFromString("20:9B:A9:61:15:6C").has_value());
    EXPECT_FALSE(node_keys::devFromString("20:9b:a9:61:15").has_value());
    EXPECT_FALSE(node_keys::devFromString("").has_value());
}

TEST(NodeKeys, BigEndian32MostSignificantByteFirst) {
    std::string out;
    node_keys::appendBigEndian32(out, 0x01020304U);
    EXPECT_EQ(out, bytes({0x01, 0x02, 0x03, 0x04}));
}

TEST(NodeKeys, BigEndian64RoundTrip) {
    std::string out;
    node_keys::appendBigEndian64(out, 0x0102030405060708ULL);
    EXPECT_EQ(out, bytes({0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}));
    EXPECT_EQ(node_keys::readBigEndian64(out), 0x0102030405060708ULL);

    std::string max;
    node_keys::appendBigEndian64(max, UINT64_MAX);
    EXPECT_EQ(node_keys::readBigEndian64(max), UINT64_MAX);
}

// The reason for big-endian: LMDB compares keys as bytes, and byte order must match number order.
TEST(NodeKeys, KeyOrderIsNumberOrder) {
    EXPECT_LT(node_keys::recordKey(kDev, 1, 255), node_keys::recordKey(kDev, 1, 256));
    EXPECT_LT(node_keys::recordKey(kDev, 1, 0xFFFFFFFFU), node_keys::recordKey(kDev, 2, 0));
    EXPECT_LT(node_keys::seriesKey(kDev, "t", 255), node_keys::seriesKey(kDev, "t", 256));
    EXPECT_LT(
        node_keys::seriesKey(kDev, "t", 1'700'000'000'000ULL),
        node_keys::seriesKey(kDev, "t", 1'700'000'000'001ULL)
    );
}

TEST(NodeKeys, RecordKeyLayout) {
    EXPECT_EQ(
        node_keys::recordKey(kDev, 0x3f2a91c4U, 42),
        bytes({0x20, 0x9b, 0xa9, 0x61, 0x15, 0x6c, 0x3f, 0x2a, 0x91, 0xc4, 0x00, 0x00, 0x00, 0x2a})
    );
}

TEST(NodeKeys, SeriesKeyLayout) {
    const std::string key = node_keys::seriesKey(kDev, "ab", 1);
    EXPECT_EQ(
        key,
        bytes({0x20, 0x9b, 0xa9, 0x61, 0x15, 0x6c, 'a', 'b', 0x00, 0, 0, 0, 0, 0, 0, 0, 1})
    );
    EXPECT_TRUE(key.starts_with(node_keys::seriesPrefix(kDev, "ab")));
}

// Without the 0x00 after the name, every "temp" key would also be a prefix of "temp_alert" keys.
TEST(NodeKeys, NameIsTerminated) {
    const std::string temp = node_keys::seriesPrefix(kDev, "temp");
    EXPECT_FALSE(node_keys::seriesKey(kDev, "temp_alert", 1).starts_with(temp));
}

// seriesEnd is right after every key of the series and before the next series of the device.
TEST(NodeKeys, SeriesEndSkipsWholeSeries) {
    const std::string end = node_keys::seriesEnd(kDev, "temp");
    EXPECT_LT(node_keys::seriesKey(kDev, "temp", UINT64_MAX), end);
    EXPECT_LT(end, node_keys::seriesKey(kDev, "temp_alert", 0));
    EXPECT_LT(end, node_keys::seriesKey(kDev, "tempa", 0));
}
