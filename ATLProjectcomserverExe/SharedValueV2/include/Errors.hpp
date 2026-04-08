#pragma once
#include <string>
#include <stdexcept>
#include <cstdint>

namespace SharedValueV2 {

// Helper to narrow wstring to string for exception messages
inline std::string narrow(const std::wstring& ws) {
    std::string s(ws.begin(), ws.end());
    return s;
}

enum class ErrorCode : uint16_t {
    // Dataset errors
    KeyNotFound         = 100,
    DuplicateKey        = 101,
    StoreModeNotEmpty   = 102,
    InvalidStorageMode  = 103,

    // SharedValue errors
    LockTimeout         = 200,
    NullPointer         = 201,

    // General
    InvalidArgument     = 300,
    OutOfRange          = 301,
    InternalError       = 999
};

class SharedValueException : public std::runtime_error {
public:
    ErrorCode code;
    SharedValueException(ErrorCode c, const std::string& msg)
        : std::runtime_error(msg), code(c) {}
};

class KeyNotFoundException : public SharedValueException {
public:
    KeyNotFoundException(const std::wstring& key)
        : SharedValueException(ErrorCode::KeyNotFound,
          "Key '" + narrow(key) + "' not found in dataset") {}
};

class DuplicateKeyException : public SharedValueException {
public:
    DuplicateKeyException(const std::wstring& key)
        : SharedValueException(ErrorCode::DuplicateKey,
          "Key '" + narrow(key) + "' already exists") {}
};

class StoreModeException : public SharedValueException {
public:
    StoreModeException()
        : SharedValueException(ErrorCode::StoreModeNotEmpty,
          "Cannot change storage mode: dataset is not empty") {}
};

class InvalidStorageModeException : public SharedValueException {
public:
    InvalidStorageModeException(int mode)
        : SharedValueException(ErrorCode::InvalidStorageMode,
          "Invalid storage mode: " + std::to_string(mode)) {}
};

} // namespace SharedValueV2
