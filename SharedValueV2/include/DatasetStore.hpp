#pragma once
#include "StorageEngine.hpp"
#include "DatasetObserver.hpp"
#include "EventBus.hpp"
#include "Errors.hpp"
#include "LockPolicies.hpp"
#include <functional>
#include <mutex>

namespace SharedValueV2 {

template <typename TValue, typename MutexPolicy = LocalMutexPolicy>
class DatasetStore {
public:
    explicit DatasetStore(StorageMode mode = StorageMode::Ordered)
        : m_engine(CreateStorageEngine<TValue>(mode)), m_mode(mode) {}

    // --- Storage Mode (runtime switchable when empty) ---

    void SetStorageMode(StorageMode mode) {
        std::lock_guard<MutexPolicy> lock(m_mutex);
        if (m_engine->Size() > 0) {
            throw StoreModeException();
        }
        m_engine = CreateStorageEngine<TValue>(mode);
        m_mode = mode;
        // No lock held during emit
    }

    StorageMode GetStorageMode() const {
        std::lock_guard<MutexPolicy> lock(m_mutex);
        return m_mode;
    }

    // --- CRUD ---

    void AddRow(const std::wstring& key, const TValue& value) {
        {
            std::lock_guard<MutexPolicy> lock(m_mutex);
            if (m_engine->Contains(key)) {
                throw DuplicateKeyException(key);
            }
            m_engine->Insert(key, value);
        }
        // Notify outside lock
        notifyRowAdded(key, value);
        m_eventBus.Emit(EventType::RowAdded, key, L"", toWString(value));
    }

    bool UpdateRow(const std::wstring& key, const TValue& value) {
        TValue oldVal{};
        bool found = false;
        {
            std::lock_guard<MutexPolicy> lock(m_mutex);
            auto existing = m_engine->Find(key);
            if (!existing.has_value()) return false;
            oldVal = existing.value();
            found = m_engine->Update(key, value);
        }
        if (found) {
            notifyRowUpdated(key, value);
            m_eventBus.Emit(EventType::RowUpdated, key, toWString(oldVal), toWString(value));
        }
        return found;
    }

    bool RemoveRow(const std::wstring& key) {
        bool removed = false;
        {
            std::lock_guard<MutexPolicy> lock(m_mutex);
            removed = m_engine->Erase(key);
        }
        if (removed) {
            notifyRowRemoved(key);
            m_eventBus.Emit(EventType::RowRemoved, key);
        }
        return removed;
    }

    void Clear() {
        {
            std::lock_guard<MutexPolicy> lock(m_mutex);
            m_engine->Clear();
        }
        notifyDatasetCleared();
        m_eventBus.Emit(EventType::DatasetCleared);
    }

    // --- Query ---

    size_t GetRecordCount() const {
        std::lock_guard<MutexPolicy> lock(m_mutex);
        return m_engine->Size();
    }

    std::optional<TValue> GetRow(const std::wstring& key) const {
        std::lock_guard<MutexPolicy> lock(m_mutex);
        return m_engine->Find(key);
    }

    TValue GetRowOrThrow(const std::wstring& key) const {
        std::lock_guard<MutexPolicy> lock(m_mutex);
        auto result = m_engine->Find(key);
        if (!result.has_value()) {
            throw KeyNotFoundException(key);
        }
        return result.value();
    }

    bool HasKey(const std::wstring& key) const {
        std::lock_guard<MutexPolicy> lock(m_mutex);
        return m_engine->Contains(key);
    }

    // --- Paginering ---

    std::vector<std::wstring> FetchKeys(size_t startIndex, size_t limit) const {
        std::lock_guard<MutexPolicy> lock(m_mutex);
        return m_engine->GetKeys(startIndex, limit);
    }

    std::vector<std::pair<std::wstring, TValue>> FetchPage(size_t start, size_t limit) const {
        std::lock_guard<MutexPolicy> lock(m_mutex);
        return m_engine->GetPage(start, limit);
    }

    // --- In-Place Mutatie (C++ only) ---

    void AccessInPlace(const std::wstring& key, std::function<void(TValue&)> mutator) {
        TValue oldVal{}, newVal{};
        {
            std::lock_guard<MutexPolicy> lock(m_mutex);
            auto val = m_engine->Find(key);
            if (!val.has_value()) {
                throw KeyNotFoundException(key);
            }
            oldVal = val.value();
            TValue mutable_copy = val.value();
            mutator(mutable_copy);
            m_engine->Update(key, mutable_copy);
            newVal = mutable_copy;
        }
        notifyRowUpdated(key, newVal);
        m_eventBus.Emit(EventType::RowUpdated, key, toWString(oldVal), toWString(newVal));
    }

    // --- Observer Pub/Sub (backward compatible) ---

    void Subscribe(IDatasetObserver<TValue>* observer) {
        if (!observer) return;
        std::lock_guard<MutexPolicy> lock(m_mutex);
        if (std::find(m_observers.begin(), m_observers.end(), observer) == m_observers.end()) {
            m_observers.push_back(observer);
        }
    }

    void Unsubscribe(IDatasetObserver<TValue>* observer) {
        if (!observer) return;
        std::lock_guard<MutexPolicy> lock(m_mutex);
        m_observers.erase(
            std::remove(m_observers.begin(), m_observers.end(), observer),
            m_observers.end()
        );
    }

    // --- Event Bus (new, rich events) ---

    EventBus<MutexPolicy>& GetEventBus() { return m_eventBus; }

private:
    std::unique_ptr<IStorageEngine<TValue>> m_engine;
    mutable MutexPolicy m_mutex;
    StorageMode m_mode;
    std::vector<IDatasetObserver<TValue>*> m_observers;
    EventBus<MutexPolicy> m_eventBus;

    // --- String conversion helper for events ---
    static std::wstring toWString(const std::wstring& v) { return v; }

    // --- Observer notification helpers (deadlock-free) ---

    void notifyRowAdded(const std::wstring& key, const TValue& val) {
        std::vector<IDatasetObserver<TValue>*> copy;
        { std::lock_guard<MutexPolicy> lock(m_mutex); copy = m_observers; }
        for (auto* obs : copy) obs->OnRowAdded(key, val);
    }
    void notifyRowUpdated(const std::wstring& key, const TValue& val) {
        std::vector<IDatasetObserver<TValue>*> copy;
        { std::lock_guard<MutexPolicy> lock(m_mutex); copy = m_observers; }
        for (auto* obs : copy) obs->OnRowUpdated(key, val);
    }
    void notifyRowRemoved(const std::wstring& key) {
        std::vector<IDatasetObserver<TValue>*> copy;
        { std::lock_guard<MutexPolicy> lock(m_mutex); copy = m_observers; }
        for (auto* obs : copy) obs->OnRowRemoved(key);
    }
    void notifyDatasetCleared() {
        std::vector<IDatasetObserver<TValue>*> copy;
        { std::lock_guard<MutexPolicy> lock(m_mutex); copy = m_observers; }
        for (auto* obs : copy) obs->OnDatasetCleared();
    }
};

} // namespace SharedValueV2
