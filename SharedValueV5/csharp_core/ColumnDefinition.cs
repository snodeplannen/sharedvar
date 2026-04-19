using System;

namespace SharedValueV5
{
    /// <summary>Supported column types for SharedTable</summary>
    public enum ColumnType : byte
    {
        Int32    = 1,
        Int64    = 2,
        Float    = 3,
        Double   = 4,
        Bool     = 5,
        String   = 6,
        Blob     = 7,
        DateTime = 8
    }

    /// <summary>Definition of a single column within a SharedTable</summary>
    public class ColumnDefinition
    {
        public string Name { get; }
        public ColumnType Type { get; }
        public int Ordinal { get; internal set; }

        /// <summary>Byte size for fixed-size types, or 8 (offset+length ref) for String/Blob</summary>
        public int FixedByteSize => Type switch
        {
            ColumnType.Int32    => 4,
            ColumnType.Int64    => 8,
            ColumnType.Float    => 4,
            ColumnType.Double   => 8,
            ColumnType.Bool     => 1,
            ColumnType.String   => 8,  // (uint32 offset, uint32 length)
            ColumnType.Blob     => 8,
            ColumnType.DateTime => 8,
            _ => throw new ArgumentException($"Unknown ColumnType: {Type}")
        };

        public ColumnDefinition(string name, ColumnType type, int ordinal)
        {
            Name = name ?? throw new ArgumentNullException(nameof(name));
            Type = type;
            Ordinal = ordinal;
        }

        /// <summary>Parse a type name string (used by COM wrapper)</summary>
        public static ColumnType ParseTypeName(string typeName)
        {
            return typeName.ToLowerInvariant() switch
            {
                "int32" or "int" or "integer" => ColumnType.Int32,
                "int64" or "long"             => ColumnType.Int64,
                "float" or "single"           => ColumnType.Float,
                "double"                      => ColumnType.Double,
                "bool" or "boolean"           => ColumnType.Bool,
                "string"                      => ColumnType.String,
                "blob" or "bytes" or "binary" => ColumnType.Blob,
                "datetime" or "date"          => ColumnType.DateTime,
                _ => throw new ArgumentException($"Unknown type name: '{typeName}'. Use: Int32, Int64, Float, Double, Bool, String, Blob, DateTime")
            };
        }
    }
}
