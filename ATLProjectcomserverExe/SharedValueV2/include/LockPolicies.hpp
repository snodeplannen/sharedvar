#pragma once
#include <mutex>
#include <string>
#include <stdexcept>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace SharedValueV2 {

// A local, fast, in-process std::mutex
class LocalMutexPolicy {
private:
    std::mutex m_mutex;
public:
    void lock() { m_mutex.lock(); }
    void unlock() { m_mutex.unlock(); }
    bool try_lock() { return m_mutex.try_lock(); }
};

// No-op mutex for single threaded usage (zero overhead)
class NullMutexPolicy {
public:
    void lock() {}
    void unlock() {}
    bool try_lock() { return true; }
};

#ifdef _WIN32
// Cross-process named mutex for Windows
class NamedSystemMutexPolicy {
private:
    HANDLE m_hMutex;

public:
    // Ideally this name should be unique. Using a template or constructor param is better,
    // but for simplicity we bind it to a fixed name for this project or allow it to be set.
    NamedSystemMutexPolicy() {
        // We use "Local\" to avoid requiring administrator privileges, 
        // allowing it to work per-session properly without failing.
        m_hMutex = CreateMutexW(NULL, FALSE, L"Local\\SharedValueModernLock");
        if (m_hMutex == NULL) {
            throw std::runtime_error("Failed to create named mutex");
        }
    }

    ~NamedSystemMutexPolicy() {
        if (m_hMutex) {
            CloseHandle(m_hMutex);
        }
    }

    // Move semantics required for policies usually, or just keep it simple.
    NamedSystemMutexPolicy(const NamedSystemMutexPolicy&) = delete;
    NamedSystemMutexPolicy& operator=(const NamedSystemMutexPolicy&) = delete;

    void lock() {
        if (m_hMutex) {
            WaitForSingleObject(m_hMutex, INFINITE);
        }
    }

    void unlock() {
        if (m_hMutex) {
            ReleaseMutex(m_hMutex);
        }
    }

    bool try_lock() {
        if (m_hMutex) {
            return WaitForSingleObject(m_hMutex, 0) == WAIT_OBJECT_0;
        }
        return false;
    }
    
    // Allow custom timeouts natively if needed, though std::unique_lock expects try_lock/lock
    bool try_lock_for(DWORD timeoutMs) {
        if (m_hMutex) {
            return WaitForSingleObject(m_hMutex, timeoutMs) == WAIT_OBJECT_0;
        }
        return false;
    }
};
#endif

} // namespace SharedValueV2
