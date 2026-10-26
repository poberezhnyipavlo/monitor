#include "Logger.h"

#include <mutex>
#include <iostream>

namespace {
    std::mutex cout_mutex;
}

void log(const std::string& message) {
    std::lock_guard<std::mutex> lock(cout_mutex);
    std::cout << message << std::endl;
}
