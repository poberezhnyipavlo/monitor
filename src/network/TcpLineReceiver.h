#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

#include "ILineReceiver.h"
#include "LineSplitter.h"
#include "Socket.h"

struct TcpReceiverOptions {
    uint16_t port;
    std::size_t max_line_len;
    std::optional<std::chrono::seconds> idle_timeout;
};

class TcpLineReceiver : public ILineReceiver {
public:
    TcpLineReceiver(std::string name, TcpReceiverOptions options);

    void run(std::stop_token token, const LineHandler& on_line) override;

private:
    struct Client {
        Socket socket;
        uint32_t id;
        uint16_t peer_port;
        LineSplitter splitter;
        std::chrono::steady_clock::time_point last_rx;
    };

    [[nodiscard]] Socket listen() const;
    void acceptClient(const Socket& listener);
    [[nodiscard]] bool readClient(Client& client, const LineHandler& on_line);
    void closeClients();
    void logClient(const Client& client, const std::string& message) const;

    std::string name_;
    TcpReceiverOptions options_;
    std::vector<Client> clients_;
    uint32_t next_client_id_ = 1;
};
