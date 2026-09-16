#pragma once

class ISystemMetrics {
public:
    virtual double getCpuUsage() = 0;
    virtual double getMemoryUsage() = 0;
    virtual ~ISystemMetrics() = default;
};