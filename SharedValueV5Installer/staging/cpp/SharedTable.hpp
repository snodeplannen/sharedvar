#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <numeric>
#include "ColumnDefinition.hpp"
#include "SharedRow.hpp"

namespace SharedValueV5 {

class SchemaLockedException : public std::runtime_error {
public:
    explicit SchemaLockedException(const std::string& tableName)
        : std::runtime_error("Schema for table '" + tableName + "' is locked.") {}
};

/// A dynamic data table with runtime-defined schema
class SharedTable {
    std::string m_name;
    std::vector<ColumnDefinition> m_columns;
    std::unordered_map<std::string, int> m_nameToOrdinal;
    std::vector<SharedRow> m_rows;
    uint16_t m_schemaVersion = 0;
    bool m_schemaLocked = false;

public:
    explicit SharedTable(const std::string& name) : m_name(name) {}

    const std::string& Name() const { return m_name; }
    uint16_t SchemaVersion() const { return m_schemaVersion; }
    bool IsSchemaLocked() const { return m_schemaLocked; }
    int ColumnCount() const { return static_cast<int>(m_columns.size()); }
    int RowCount() const { return static_cast<int>(m_rows.size()); }

    /// Add a column (public API — bumps schema version)
    void AddColumn(const std::string& name, ColumnType type) {
        if (m_schemaLocked)
            throw SchemaLockedException(m_name);
        if (m_nameToOrdinal.count(name))
            throw std::invalid_argument("Column '" + name + "' already exists.");

        int ordinal = static_cast<int>(m_columns.size());
        m_columns.emplace_back(name, type, ordinal);
        m_nameToOrdinal[name] = ordinal;
        m_schemaVersion++;
    }

    /// Add a column during deserialization (no lock check, no version bump)
    void AddColumnForDeserialization(const std::string& name, ColumnType type) {
        int ordinal = static_cast<int>(m_columns.size());
        m_columns.emplace_back(name, type, ordinal);
        m_nameToOrdinal[name] = ordinal;
    }

    void LockSchema() { m_schemaLocked = true; }

    const ColumnDefinition& Column(int ordinal) const { return m_columns[ordinal]; }
    const ColumnDefinition& Column(const std::string& name) const {
        auto it = m_nameToOrdinal.find(name);
        if (it == m_nameToOrdinal.end())
            throw std::out_of_range("Column '" + name + "' not found.");
        return m_columns[it->second];
    }

    int ColumnOrdinal(const std::string& name) const {
        auto it = m_nameToOrdinal.find(name);
        if (it == m_nameToOrdinal.end()) return -1;
        return it->second;
    }

    /// Compute the fixed-size row stride
    int RowStride() const {
        int stride = 0;
        for (const auto& col : m_columns)
            stride += col.FixedByteSize();
        return stride;
    }

    /// Create a new empty row
    SharedRow NewRow() const {
        return SharedRow(static_cast<int>(m_columns.size()));
    }

    void AddRow(SharedRow&& row) { m_rows.push_back(std::move(row)); }
    void AddRow(const SharedRow& row) { m_rows.push_back(row); }
    void ClearRows() { m_rows.clear(); }

    SharedRow& Row(int index) { return m_rows[index]; }
    const SharedRow& Row(int index) const { return m_rows[index]; }

    // Iterator support
    auto begin() { return m_rows.begin(); }
    auto end() { return m_rows.end(); }
    auto begin() const { return m_rows.begin(); }
    auto end() const { return m_rows.end(); }

    const std::vector<ColumnDefinition>& Columns() const { return m_columns; }

    // Internal setters for deserialization
    void SetSchemaVersion(uint16_t v) { m_schemaVersion = v; }
    void SetSchemaLocked(bool locked) { m_schemaLocked = locked; }
};

} // namespace SharedValueV5
