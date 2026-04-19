#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <stdexcept>
#include <variant>

namespace SharedValueV5 {

/// Supported column types (must match C# ColumnType enum)
enum class ColumnType : uint8_t {
    Int32    = 1,
    Int64    = 2,
    Float    = 3,
    Double   = 4,
    Bool     = 5,
    String   = 6,
    Blob     = 7,
    DateTime = 8
};

/// Definition of a single column
struct ColumnDefinition {
    std::string name;
    ColumnType type;
    int ordinal;

    ColumnDefinition(const std::string& n, ColumnType t, int ord)
        : name(n), type(t), ordinal(ord) {}

    /// Fixed byte size for this column type in the serialized format
    int FixedByteSize() const {
        switch (type) {
            case ColumnType::Int32:    return 4;
            case ColumnType::Int64:    return 8;
            case ColumnType::Float:    return 4;
            case ColumnType::Double:   return 8;
            case ColumnType::Bool:     return 1;
            case ColumnType::String:   return 8; // (offset + length)
            case ColumnType::Blob:     return 8;
            case ColumnType::DateTime: return 8;
            default: return 0;
        }
    }
};

/// A cell value that can hold any supported type
using CellValue = std::variant<
    std::monostate,       // null
    int32_t,              // Int32
    int64_t,              // Int64 / DateTime (ticks)
    float,                // Float
    double,               // Double 
    bool,                 // Bool
    std::string,          // String
    std::vector<uint8_t>  // Blob
>;

} // namespace SharedValueV5
