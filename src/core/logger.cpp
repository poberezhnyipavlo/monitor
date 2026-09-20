#include "logger.h"

#include <mutex>
#include <iostream>

namespace {
    std::mutex coutMutex;
}

void log(const std::string& message) {
    std::lock_guard<std::mutex> lock(coutMutex);
    std::cout << message << std::endl;
}
