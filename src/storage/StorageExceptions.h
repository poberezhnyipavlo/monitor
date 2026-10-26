#pragma once

#include <stdexcept>
#include <string>

class StorageException : public std::runtime_error {
public:
    explicit StorageException(const std::string& message)
        : std::runtime_error(message) {}
};

class PermissionDeniedException : public StorageException {
public:
    explicit PermissionDeniedException(const std::string& path)
        : StorageException("Permission denied: " + path) {}
};