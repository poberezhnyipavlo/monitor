#pragma once

#include <functional>
#include <stop_token>
#include <string_view>

using LineHandler = std::function<void(std::string_view line)>;

class ILineReceiver {
public:
    virtual ~ILineReceiver() = default;

    virtual void run(std::stop_token stop, const LineHandler& on_line) = 0;
};
