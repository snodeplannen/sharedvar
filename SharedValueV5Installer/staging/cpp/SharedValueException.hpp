#pragma once
#include <stdexcept>
#include <string>
#include <windows.h>

namespace SharedMemMap {

class SharedValueException : public std::runtime_error {
public:
    explicit SharedValueException(const std::string& message)
        : std::runtime_error(message) {}
};

class SystemException : public SharedValueException {
public:
    DWORD errorCode;
    SystemException(const std::string& context, DWORD code)
        : SharedValueException(context + " failed with ErrorCode: " + std::to_string(code)), errorCode(code) {}
};

class MutexException : public SharedValueException {
public:
    explicit MutexException(const std::string& message) : SharedValueException(message) {}
};

class MemoryMappedException : public SharedValueException {
public:
    explicit MemoryMappedException(const std::string& message) : SharedValueException(message) {}
};

} // namespace SharedMemMap
