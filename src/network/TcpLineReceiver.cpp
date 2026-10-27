#include "TcpLineReceiver.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>

#include <array>
#include <cerrno>
#include <map>
#include <stop_token>
#include <system_error>

#include "core/Logger.h"

namespace {
constexpr int kPollTimeoutMs = 200;
constexpr int kListenBacklog = 8;
constexpr std::size_t kReadChunk = 1024;

constexpr int kKeepAliveIdleS = 15;
constexpr int kKeepAliveIntervalS = 5;
constexpr int kKeepAliveProbes = 3;

[[noreturn]] void throwErrno(const std::string& what) {
    throw std::system_error(errno, std::generic_category(), what);
}

void setOption(int fd, int level, int option, int value, const char* what) {
    if (setsockopt(fd, level, option, &value, sizeof(value)) < 0) {
        throwErrno(what);
    }
}

void enableKeepAlive(int fd) {
    setOption(fd, SOL_SOCKET, SO_KEEPALIVE, 1, "SO_KEEPALIVE");
#ifdef __APPLE__
    setOption(fd, IPPROTO_TCP, TCP_KEEPALIVE, kKeepAliveIdleS, "TCP_KEEPALIVE");
#else
    setOption(fd, IPPROTO_TCP, TCP_KEEPIDLE, kKeepAliveIdleS, "TCP_KEEPIDLE");
#endif
    setOption(fd, IPPROTO_TCP, TCP_KEEPINTVL, kKeepAliveIntervalS, "TCP_KEEPINTVL");
    setOption(fd, IPPROTO_TCP, TCP_KEEPCNT, kKeepAliveProbes, "TCP_KEEPCNT");
}
}  // namespace

TcpLineReceiver::TcpLineReceiver(std::string name, TcpReceiverOptions options)
    : name_(name), options_(options) {}

void TcpLineReceiver::run(std::stop_token stop, const LineHandler& on_line) {
    const Socket listener = this->listen();
    log("[" + name_ + "] listening on TCP " + std::to_string(options_.port));

    std::vector<pollfd> fds;
    while (!stop.stop_requested()) {
        fds.clear();
        fds.push_back({.fd = listener.get(), .events = POLLIN, .revents = 0});
        for (const Client& client : this->clients_) {
            fds.push_back({.fd = client.socket.get(), .events = POLLIN, .revents = 0});
        }

        const int ready = poll(fds.data(), static_cast<nfds_t>(fds.size()), kPollTimeoutMs);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            throwErrno("poll");
        }

        for (std::size_t i = this->clients_.size(); i-- > 0;) {
            if (fds[i + 1].revents == 0) {
                continue;
            }
            if (!this->readClient(this->clients_[i], on_line)) {
                this->clients_.erase(this->clients_.begin() + static_cast<std::ptrdiff_t>(i));
            }
        }

        if ((fds[0].revents & POLLIN) != 0) {
            acceptClient(listener);
        }

        this->closeClients();
    }

    log("[" + name_ + "] stopped, closing " + std::to_string(clients_.size()) + " client(s)");
    clients_.clear();
}

Socket TcpLineReceiver::listen() const {
    Socket listener(socket(AF_INET, SOCK_STREAM, 0));

    setOption(listener.get(), SOL_SOCKET, SO_REUSEADDR, 1, "SO_REUSEADDR");

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(this->options_.port);

    if (bind(listener.get(), reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        throwErrno("bind TCP " + std::to_string(this->options_.port));
    }

    if (::listen(listener.get(), kListenBacklog) < 0) {
        throwErrno("listen TCP " + std::to_string(this->options_.port));
    }

    return listener;
}

void TcpLineReceiver::acceptClient(const Socket& listener) {
    sockaddr_in peer{};
    socklen_t peer_len = sizeof(peer);
    const int fd = accept(listener.get(), reinterpret_cast<sockaddr*>(&peer), &peer_len);
    if (fd < 0) {
        log("[" + name_ + "] accept failed: " + std::generic_category().message(errno));
        return;
    }

    Client client{
        .socket = Socket(fd),
        .id = this->next_client_id_++,
        .peer_port = ntohs(peer.sin_port),
        .splitter = LineSplitter(this->options_.max_line_len),
        .last_rx = std::chrono::steady_clock::now(),
    };

    try {
        enableKeepAlive(client.socket.get());
    } catch (const std::system_error& e) {
        this->logClient(client, std::string("keepalive not set: ") + e.what());
    }
    this->logClient(client, "connected");
    clients_.push_back(std::move(client));
}

bool TcpLineReceiver::readClient(Client& client, const LineHandler& on_line) {
    std::array<char, kReadChunk> buffer{};
    const ssize_t received = recv(client.socket.get(), buffer.data(), buffer.size(), 0);

    if (received == 0) {
        this->logClient(client, "closed by peer");
        return false;
    }

    if (received < 0) {
        if (errno == EINTR || errno == EAGAIN) {
            return true;
        }

        this->logClient(client, "dropped: " + std::generic_category().message(errno));
        return false;
    }

    client.last_rx = std::chrono::steady_clock::now();
    const std::string_view bytes(buffer.data(), static_cast<std::size_t>(received));
    const std::size_t dropped = client.splitter.feed(bytes, on_line);
    if (dropped > 0) {
        this->logClient(client, "dropped " + std::to_string(dropped) + " too long line(s)");
    }

    return true;
}

void TcpLineReceiver::closeClients() {
    if (!this->options_.idle_timeout) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    std::erase_if(this->clients_, [&](const Client& client) {
        if (now - client.last_rx < *this->options_.idle_timeout) {
            return false;
        }
        this->logClient(
            client,
            "idle for " + std::to_string(options_.idle_timeout->count()) + " s, closing"
        );
        return true;
    });
}

void TcpLineReceiver::logClient(const Client& client, const std::string& message) const {
    log("[" + name_ + "] client #" + std::to_string(client.id) + " (port " +
        std::to_string(client.peer_port) + ") " + message);
}
