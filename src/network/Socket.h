#pragma once

#include <sys/socket.h>
#include <unistd.h>

#include <stdexcept>
#include <utility>

class Socket {
public:
    explicit Socket(int fd) : fd_(fd) {
        if (fd_ < 0) {
            throw std::runtime_error("Invalid socket file descriptor");
        }
    };
    ~Socket() {
        if (fd_ >= 0) {
            close(fd_);
        }
    };
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}

    Socket& operator=(Socket&& other) noexcept {
        if (this != &other) {
            if (fd_ >= 0) {
                close(fd_);
            }
            fd_ = std::exchange(other.fd_, -1);
        }
        return *this;
    }

    [[nodiscard]] int get() const {
        return fd_;
    }

private:
    int fd_;
};
