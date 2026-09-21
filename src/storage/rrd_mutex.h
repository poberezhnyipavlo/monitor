#pragma once

#include <mutex>

inline std::mutex& rrdMutex() {
    static std::mutex mutex;
    return mutex;
}
