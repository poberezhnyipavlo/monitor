#pragma once

#include <cstdint>

#include "ILineReceiver.h"

class [[deprecated("stage 10 example, not used: receive with TcpLineReceiver")]] UdpLineReceiver
    : public ILineReceiver {
public:
    explicit UdpLineReceiver(uint16_t port);

    void run(std::stop_token stop, const LineHandler& on_line) override;

private:
    uint16_t port_;
};
