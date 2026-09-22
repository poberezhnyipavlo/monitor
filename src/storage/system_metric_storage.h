#pragma once

#include <filesystem>

#include "base_rrd_storage.h"

class SystemMetrixStorage : public BaseRddStorage{
public:
    explicit SystemMetrixStorage(std::string path) : systemMetricsPath_(path) {
        if (!std::filesystem::exists(systemMetricsPath_)) {
            this->createFile();
        }
    }
    void save(double cpu, double memory);
protected:
    [[nodiscard]] std::string getPath() const override;
private:
    std::string systemMetricsPath_;

    void createFile() const;
};
