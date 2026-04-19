#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <memory>
#include "SharedTable.hpp"

namespace SharedValueV5 {

/// Multi-table container, analogous to System.Data.DataSet
class SharedDataSet {
    std::string m_name;
    std::vector<std::shared_ptr<SharedTable>> m_tables;
    std::unordered_map<std::string, int> m_nameToIndex;

public:
    explicit SharedDataSet(const std::string& name = "DataSet") : m_name(name) {}

    const std::string& Name() const { return m_name; }
    int TableCount() const { return static_cast<int>(m_tables.size()); }

    /// Add a new table
    SharedTable& AddTable(const std::string& name) {
        if (m_nameToIndex.count(name))
            throw std::invalid_argument("Table '" + name + "' already exists.");
        auto table = std::make_shared<SharedTable>(name);
        int idx = static_cast<int>(m_tables.size());
        m_tables.push_back(table);
        m_nameToIndex[name] = idx;
        return *table;
    }

    /// Add an existing table (for deserialization)
    void AddTableDirect(std::shared_ptr<SharedTable> table) {
        m_nameToIndex[table->Name()] = static_cast<int>(m_tables.size());
        m_tables.push_back(table);
    }

    bool HasTable(const std::string& name) const {
        return m_nameToIndex.count(name) > 0;
    }

    SharedTable& GetTable(const std::string& name) {
        auto it = m_nameToIndex.find(name);
        if (it == m_nameToIndex.end())
            throw std::out_of_range("Table '" + name + "' not found.");
        return *m_tables[it->second];
    }

    const SharedTable& GetTable(const std::string& name) const {
        auto it = m_nameToIndex.find(name);
        if (it == m_nameToIndex.end())
            throw std::out_of_range("Table '" + name + "' not found.");
        return *m_tables[it->second];
    }

    SharedTable& Table(int index) { return *m_tables[index]; }
    const SharedTable& Table(int index) const { return *m_tables[index]; }

    auto begin() { return m_tables.begin(); }
    auto end() { return m_tables.end(); }
    auto begin() const { return m_tables.begin(); }
    auto end() const { return m_tables.end(); }
};

} // namespace SharedValueV5
