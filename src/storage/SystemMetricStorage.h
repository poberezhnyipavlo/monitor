#pragma once

#include <filesystem>

#include "BaseRrdStorage.h"

class SystemMetricStorage : public BaseRrdStorage {
public:
    explicit SystemMetricStorage(std::string path) : system_metrics_path_(path) {
        if (!std::filesystem::exists(system_metrics_path_)) {
            this->createFile();
        }
    }
    void save(double cpu, double memory);

protected:
    [[nodiscard]] std::string getPath() const override;

private:
    std::string system_metrics_path_;

    void createFile() const;
};
