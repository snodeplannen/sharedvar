#pragma once
#include "LockPolicies.hpp"
#include <string>
#include <vector>
#include <chrono>
#include <atomic>
#include <algorithm>
#include <cstdint>
#include <mutex>

namespace SharedValueV2 {

enum class EventType : uint16_t {
    // SharedValue events
    ValueSet            = 1,
    ValueLocked         = 2,
    ValueUnlocked       = 3,

    // Dataset events
    RowAdded            = 10,
    RowUpdated          = 11,
    RowRemoved          = 12,
    DatasetCleared      = 13,
    StorageModeChanged  = 14,

    // Lifecycle events
    ObjectCreated       = 50,
    ObjectDestroyed     = 51
};

struct MutationEvent {
    EventType   type;
    std::wstring key;
    std::wstring oldValue;
    std::wstring newValue;
    std::wstring source;
    std::chrono::system_clock::time_point timestamp;
    uint64_t     sequenceId;
};

class IEventListener {
public:
    virtual ~IEventListener() = default;
    virtual void OnEvent(const MutationEvent& event) = 0;
};

template <typename MutexPolicy = LocalMutexPolicy>
class EventBus {
public:
    void Subscribe(IEventListener* listener) {
        if (!listener) return;
        std::lock_guard<MutexPolicy> lock(m_mutex);
        if (std::find(m_listeners.begin(), m_listeners.end(), listener) == m_listeners.end()) {
            m_listeners.push_back(listener);
        }
    }

    void Unsubscribe(IEventListener* listener) {
        if (!listener) return;
        std::lock_guard<MutexPolicy> lock(m_mutex);
        m_listeners.erase(
            std::remove(m_listeners.begin(), m_listeners.end(), listener),
            m_listeners.end()
        );
    }

    void Emit(const MutationEvent& event) {
        std::vector<IEventListener*> copy;
        {
            std::lock_guard<MutexPolicy> lock(m_mutex);
            copy = m_listeners;
        }
        for (auto* listener : copy) {
            listener->OnEvent(event);
        }
    }

    void Emit(EventType type,
              const std::wstring& key = L"",
              const std::wstring& oldVal = L"",
              const std::wstring& newVal = L"",
              const std::wstring& source = L"") {
        MutationEvent evt;
        evt.type = type;
        evt.key = key;
        evt.oldValue = oldVal;
        evt.newValue = newVal;
        evt.source = source;
        evt.timestamp = std::chrono::system_clock::now();
        evt.sequenceId = m_sequenceCounter.fetch_add(1);
        Emit(evt);
    }

    uint64_t GetSequenceId() const {
        return m_sequenceCounter.load();
    }

private:
    std::vector<IEventListener*> m_listeners;
    mutable MutexPolicy m_mutex;
    std::atomic<uint64_t> m_sequenceCounter{0};
};

} // namespace SharedValueV2
