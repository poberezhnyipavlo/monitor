#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "ILineReceiver.h"

class LineSplitter {
public:
    explicit LineSplitter(std::size_t max_line_len);

    std::size_t feed(std::string_view bytes, const LineHandler& on_line);

private:
    std::size_t max_line_len_;
    std::string line_;
    bool skipping_ = false;
};
