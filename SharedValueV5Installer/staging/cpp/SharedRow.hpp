#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include "ColumnDefinition.hpp"

namespace SharedValueV5 {

/// A single row of data within a SharedTable
class SharedRow {
    std::vector<CellValue> m_values;

public:
    explicit SharedRow(int columnCount) : m_values(columnCount) {}

    CellValue& operator[](int ordinal) { return m_values[ordinal]; }
    const CellValue& operator[](int ordinal) const { return m_values[ordinal]; }

    // Typed setters
    void Set(int ordinal, int32_t v)              { m_values[ordinal] = v; }
    void Set(int ordinal, int64_t v)              { m_values[ordinal] = v; }
    void Set(int ordinal, float v)                { m_values[ordinal] = v; }
    void Set(int ordinal, double v)               { m_values[ordinal] = v; }
    void Set(int ordinal, bool v)                 { m_values[ordinal] = v; }
    void Set(int ordinal, const std::string& v)   { m_values[ordinal] = v; }
    void Set(int ordinal, std::string&& v)        { m_values[ordinal] = std::move(v); }
    void Set(int ordinal, std::vector<uint8_t> v) { m_values[ordinal] = std::move(v); }

    // Typed getters
    int32_t GetInt32(int ordinal) const             { return std::get<int32_t>(m_values[ordinal]); }
    int64_t GetInt64(int ordinal) const             { return std::get<int64_t>(m_values[ordinal]); }
    float GetFloat(int ordinal) const               { return std::get<float>(m_values[ordinal]); }
    double GetDouble(int ordinal) const             { return std::get<double>(m_values[ordinal]); }
    bool GetBool(int ordinal) const                 { return std::get<bool>(m_values[ordinal]); }
    const std::string& GetString(int ordinal) const { return std::get<std::string>(m_values[ordinal]); }
    const std::vector<uint8_t>& GetBlob(int ordinal) const { return std::get<std::vector<uint8_t>>(m_values[ordinal]); }

    bool IsNull(int ordinal) const { return std::holds_alternative<std::monostate>(m_values[ordinal]); }
    int ColumnCount() const { return static_cast<int>(m_values.size()); }
};

} // namespace SharedValueV5
