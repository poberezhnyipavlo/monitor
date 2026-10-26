#pragma once

#include <sys/socket.h>
#include <unistd.h>

class Socket {
public:
    explicit Socket(int fd): fd_(fd) {
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

    [[nodiscard]]int get() const { return fd_; }
private:
    int fd_;
};
