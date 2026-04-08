#pragma once
#include <string>
#include <map>
#include <unordered_map>
#include <vector>
#include <optional>
#include <memory>
#include <utility>
#include <algorithm>

namespace SharedValueV2 {

// --- Abstract Storage Engine Interface ---
template <typename TValue>
class IStorageEngine {
public:
    virtual ~IStorageEngine() = default;
    virtual void Insert(const std::wstring& key, const TValue& value) = 0;
    virtual bool Update(const std::wstring& key, const TValue& value) = 0;
    virtual bool Erase(const std::wstring& key) = 0;
    virtual void Clear() = 0;
    virtual size_t Size() const = 0;
    virtual std::optional<TValue> Find(const std::wstring& key) const = 0;
    virtual bool Contains(const std::wstring& key) const = 0;
    virtual std::vector<std::wstring> GetKeys(size_t start, size_t limit) const = 0;
    virtual std::vector<std::pair<std::wstring, TValue>> GetPage(size_t start, size_t limit) const = 0;
};

// --- Ordered: std::map, O(log n), sorted keys ---
template <typename TValue>
class OrderedStorageEngine : public IStorageEngine<TValue> {
private:
    std::map<std::wstring, TValue> m_data;

public:
    void Insert(const std::wstring& key, const TValue& value) override {
        m_data.emplace(key, value);
    }
    bool Update(const std::wstring& key, const TValue& value) override {
        auto it = m_data.find(key);
        if (it == m_data.end()) return false;
        it->second = value;
        return true;
    }
    bool Erase(const std::wstring& key) override {
        return m_data.erase(key) > 0;
    }
    void Clear() override { m_data.clear(); }
    size_t Size() const override { return m_data.size(); }

    std::optional<TValue> Find(const std::wstring& key) const override {
        auto it = m_data.find(key);
        if (it == m_data.end()) return std::nullopt;
        return it->second;
    }
    bool Contains(const std::wstring& key) const override {
        return m_data.count(key) > 0;
    }
    std::vector<std::wstring> GetKeys(size_t start, size_t limit) const override {
        std::vector<std::wstring> keys;
        size_t idx = 0;
        for (auto it = m_data.begin(); it != m_data.end() && keys.size() < limit; ++it, ++idx) {
            if (idx >= start) keys.push_back(it->first);
        }
        return keys;
    }
    std::vector<std::pair<std::wstring, TValue>> GetPage(size_t start, size_t limit) const override {
        std::vector<std::pair<std::wstring, TValue>> page;
        size_t idx = 0;
        for (auto it = m_data.begin(); it != m_data.end() && page.size() < limit; ++it, ++idx) {
            if (idx >= start) page.emplace_back(it->first, it->second);
        }
        return page;
    }
};

// --- Unordered: std::unordered_map, O(1) amortized ---
template <typename TValue>
class UnorderedStorageEngine : public IStorageEngine<TValue> {
private:
    std::unordered_map<std::wstring, TValue> m_data;

public:
    void Insert(const std::wstring& key, const TValue& value) override {
        m_data.emplace(key, value);
    }
    bool Update(const std::wstring& key, const TValue& value) override {
        auto it = m_data.find(key);
        if (it == m_data.end()) return false;
        it->second = value;
        return true;
    }
    bool Erase(const std::wstring& key) override {
        return m_data.erase(key) > 0;
    }
    void Clear() override { m_data.clear(); }
    size_t Size() const override { return m_data.size(); }

    std::optional<TValue> Find(const std::wstring& key) const override {
        auto it = m_data.find(key);
        if (it == m_data.end()) return std::nullopt;
        return it->second;
    }
    bool Contains(const std::wstring& key) const override {
        return m_data.count(key) > 0;
    }
    std::vector<std::wstring> GetKeys(size_t start, size_t limit) const override {
        std::vector<std::wstring> keys;
        size_t idx = 0;
        for (auto it = m_data.begin(); it != m_data.end() && keys.size() < limit; ++it, ++idx) {
            if (idx >= start) keys.push_back(it->first);
        }
        return keys;
    }
    std::vector<std::pair<std::wstring, TValue>> GetPage(size_t start, size_t limit) const override {
        std::vector<std::pair<std::wstring, TValue>> page;
        size_t idx = 0;
        for (auto it = m_data.begin(); it != m_data.end() && page.size() < limit; ++it, ++idx) {
            if (idx >= start) page.emplace_back(it->first, it->second);
        }
        return page;
    }
};

// --- Runtime selection enum ---
enum class StorageMode { Ordered = 0, Unordered = 1 };

// --- Factory ---
template <typename TValue>
std::unique_ptr<IStorageEngine<TValue>> CreateStorageEngine(StorageMode mode) {
    if (mode == StorageMode::Unordered)
        return std::make_unique<UnorderedStorageEngine<TValue>>();
    return std::make_unique<OrderedStorageEngine<TValue>>();
}

} // namespace SharedValueV2
