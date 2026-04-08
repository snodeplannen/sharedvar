#pragma once

#include "Observers.hpp"
#include "LockPolicies.hpp"
#include <vector>
#include <memory>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <Lmcons.h>
#endif

namespace SharedValueV2 {

template <typename T, typename MutexPolicy = LocalMutexPolicy>
class SharedValue {
private:
    T m_value;
    mutable MutexPolicy m_mutex;
    std::vector<ISharedValueObserver<T>*> m_observers;

    void notifyValueChanged(const T& val) {
        // Safe notification: copy observers out of the lock to prevent deadlocks!
        std::vector<ISharedValueObserver<T>*> observersCopy;
        {
            std::lock_guard<MutexPolicy> lock(m_mutex);
            observersCopy = m_observers;
        }

        // Notify outside the lock
        for (auto* obs : observersCopy) {
            obs->OnValueChanged(val);
        }
    }

    void notifyDateTimeChanged(const std::wstring& dt) {
        std::vector<ISharedValueObserver<T>*> observersCopy;
        {
            std::lock_guard<MutexPolicy> lock(m_mutex);
            observersCopy = m_observers;
        }

        for (auto* obs : observersCopy) {
            obs->OnDateTimeChanged(dt);
        }
    }

public:
    SharedValue() = default;
    explicit SharedValue(const T& initialValue) : m_value(initialValue) {}

    // ---- STATE MANAGEMENT ----

    void SetValue(const T& newValue) {
        {
            std::lock_guard<MutexPolicy> lock(m_mutex);
            m_value = newValue; // copy within lock
        }
        // Notify outside lock
        notifyValueChanged(newValue);
    }

    T GetValue() const {
        std::lock_guard<MutexPolicy> lock(m_mutex);
        return m_value;
    }

    // ---- SYSTEM INFO ----

    std::wstring GetCurrentUTCDateTime() {
        auto now = std::chrono::system_clock::now();
        auto utc_time = std::chrono::system_clock::to_time_t(now);
        std::tm utc_tm;
#ifdef _WIN32
        gmtime_s(&utc_tm, &utc_time);
#else
        gmtime_r(&utc_time, &utc_tm);
#endif

        std::wstringstream wss;
        wss << std::put_time<wchar_t>(&utc_tm, L"%Y-%m-%d %H:%M:%S");
        std::wstring dt = wss.str();

        // Notify outside the lock, preventing infinite recursion!
        notifyDateTimeChanged(dt);

        return dt;
    }

    std::wstring GetCurrentUserLogin() const {
#ifdef _WIN32
        wchar_t username[UNLEN + 1];
        DWORD username_len = UNLEN + 1;
        if (GetUserNameW(username, &username_len)) {
            return std::wstring(username);
        }
#endif
        return L"UnknownUser";
    }

    // ---- EXPLICIT LOCK MANAGEMENT (If needed for atomic multi-operations) ----
    
    // Returns true if explicit lock succeeded
    bool LockSharedValue() {
        m_mutex.lock();
        return true;
    }

    bool LockSharedValueTimeout(unsigned long ms) {
        // Specialization check for NamedSystemMutexPolicy
        if constexpr (std::is_same_v<MutexPolicy, NamedSystemMutexPolicy>) {
            return m_mutex.try_lock_for((DWORD)ms);
        } else {
            // For std::mutex, we can't easily wait with timeout unless we use timed_mutex.
            // For now, simple fallback:
            return m_mutex.try_lock();
        }
    }

    void Unlock() {
        m_mutex.unlock();
    }

    // ---- PUB/SUB ----

    void Subscribe(ISharedValueObserver<T>* observer) {
        if (!observer) return;
        std::lock_guard<MutexPolicy> lock(m_mutex);
        if (std::find(m_observers.begin(), m_observers.end(), observer) == m_observers.end()) {
            m_observers.push_back(observer);
        }
    }

    void Unsubscribe(ISharedValueObserver<T>* observer) {
        if (!observer) return;
        std::lock_guard<MutexPolicy> lock(m_mutex);
        m_observers.erase(
            std::remove(m_observers.begin(), m_observers.end(), observer),
            m_observers.end()
        );
    }
};

} // namespace SharedValueV2
