#include "LineSplitter.h"

LineSplitter::LineSplitter(std::size_t max_line_len) : max_line_len_(max_line_len) {
    line_.reserve(max_line_len_);
}

std::size_t LineSplitter::feed(std::string_view bytes, const LineHandler& on_line) {
    std::size_t dropped = 0;
    for (const char c : bytes) {
        if (c == '\n') {
            std::string_view line = this->line_;
            if (!line.empty() && line.back() == '\r') {
                line.remove_suffix(1);
            }
            on_line(line);
            this->line_.clear();
            this->skipping_ = false;
        } else if (this->skipping_) {
            continue;
        } else if (this->line_.size() == this->max_line_len_) {
            this->line_.clear();
            this->skipping_ = true;
            dropped++;
        } else {
            this->line_.push_back(c);
        }
    }

    return dropped;
}