#pragma once

#include <string>
#include <filesystem>
#include <fstream>
#include <cerrno>

#include "StorageExceptions.h"

class BaseRrdStorage {
public:
    virtual ~BaseRrdStorage() = default;

protected:
    [[nodiscard]] virtual std::string getPath() const = 0;

    void ensureAccess() const {
        const std::string path = this->getPath();

        std::ofstream test(path);

        if (!test.is_open()) {
            if (errno == EACCES) {
                throw PermissionDeniedException(path);
            }
            throw StorageException("Cannot access path: " + path);
        }

        test.close();
        std::filesystem::remove(path);
    }
};