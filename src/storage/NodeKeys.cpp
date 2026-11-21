#include "NodeKeys.h"

#include <metric_protocol/MetricProtocol.h>

namespace {

int hexValue(char c) {
    if (c <= '9') {
        return c - '0';
    }
    return c - 'a' + 10;
}

}  // namespace

void node_keys::appendBigEndian32(std::string& out, uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) {
        unsigned char byte = (value >> shift) & 0xFF;
        out.push_back(static_cast<char>(byte));
    }
}

void node_keys::appendBigEndian64(std::string& out, uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        unsigned char byte = (value >> shift) & 0xFF;
        out.push_back(static_cast<char>(byte));
    }
}

uint64_t node_keys::readBigEndian64(std::string_view bytes) {
    uint64_t value = 0;
    for (int i = 0; i < sizeof(uint64_t); i++) {
        value = (value << 8) | static_cast<uint8_t>(bytes[i]);
    }

    return value;
}

std::optional<node_keys::DevBytes> node_keys::devFromString(std::string_view dev) {
    if (!metric::isValidDev(dev)) {
        return std::nullopt;
    }

    DevBytes out{};

    for (int i = 0; i < kDevBytes; i++) {
        const int high = hexValue(dev[i * 3]);
        const int low = hexValue(dev[i * 3 + 1]);
        out[i] = static_cast<uint8_t>(high * 16 + low);
    }

    return out;
}

std::string node_keys::recordKey(const DevBytes& dev, uint32_t boot, uint32_t seq) {
    std::string key(dev.begin(), dev.end());
    appendBigEndian32(key, boot);
    appendBigEndian32(key, seq);

    return key;
}

std::string node_keys::seriesPrefix(const DevBytes& dev, std::string_view name) {
    std::string key(dev.begin(), dev.end());
    key.append(name);
    key.push_back('\0');

    return key;
}

std::string node_keys::seriesKey(const DevBytes& dev, std::string_view name, uint64_t ts_ms) {
    std::string key = seriesPrefix(dev, name);

    appendBigEndian64(key, ts_ms);

    return key;
}

std::string node_keys::seriesEnd(const DevBytes& dev, std::string_view name) {
    std::string key = seriesPrefix(dev, name);
    key.back() = '\x01';

    return key;
}
