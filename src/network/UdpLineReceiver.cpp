#include "UdpLineReceiver.h"

#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>

#include <array>
#include <cerrno>
#include <string>
#include <string_view>
#include <system_error>

#include "Socket.h"
#include "core/Logger.h"

namespace {
constexpr int kPollTimeoutMs = 200;
}

UdpLineReceiver::UdpLineReceiver(uint16_t port) : port_(port) {}

void UdpLineReceiver::run(std::stop_token stop, const LineHandler& on_line) {
    const Socket sock(socket(AF_INET, SOCK_DGRAM, 0));

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port_);

    const int bind_result =
        bind(sock.get(), reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));

    if (bind_result < 0) {
        throw std::system_error(errno, std::generic_category(), "UPD " + std::to_string(port_));
    }

    std::array<char, 1024> buffer{};

    while (!stop.stop_requested()) {
        pollfd poll_fd{.fd = sock.get(), .events = POLLIN, .revents = 0};
        const int ready = poll(&poll_fd, 1, kPollTimeoutMs);

        if (ready <= 0) {
            continue;
        }

        const ssize_t received = recv(sock.get(), buffer.data(), buffer.size(), 0);

        if (received < 0) {
            log("[udp] recv failed: " + std::generic_category().message(errno));
            continue;
        }

        std::string_view line(buffer.data(), static_cast<std::size_t>(received));
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
            line.remove_suffix(1);
        }

        on_line(line);
    }
}