using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;

namespace SharedValueV5
{
    /// <summary>Thrown when attempting to modify a locked schema</summary>
    public class SchemaLockedException : InvalidOperationException
    {
        public SchemaLockedException(string tableName)
            : base($"Schema for table '{tableName}' is locked. No columns can be added or removed.") { }
    }

    /// <summary>Collection of column definitions for a SharedTable</summary>
    public class ColumnCollection : IEnumerable<ColumnDefinition>
    {
        private readonly List<ColumnDefinition> _columns = new();
        private readonly Dictionary<string, ColumnDefinition> _byName = new(StringComparer.OrdinalIgnoreCase);
        private readonly SharedTable _table;

        internal ColumnCollection(SharedTable table)
        {
            _table = table;
        }

        /// <summary>Add a column to the schema</summary>
        /// <exception cref="SchemaLockedException">Schema is locked</exception>
        /// <exception cref="ArgumentException">Column name already exists</exception>
        public ColumnDefinition Add(string name, ColumnType type)
        {
            if (_table.IsSchemaLocked)
                throw new SchemaLockedException(_table.Name);
            if (_byName.ContainsKey(name))
                throw new ArgumentException($"Column '{name}' already exists in table '{_table.Name}'.");

            var col = new ColumnDefinition(name, type, _columns.Count);
            _columns.Add(col);
            _byName[name] = col;
            return col;
        }

        public ColumnDefinition this[string name]
        {
            get
            {
                if (!_byName.TryGetValue(name, out var col))
                    throw new KeyNotFoundException($"Column '{name}' not found in table '{_table.Name}'.");
                return col;
            }
        }

        public ColumnDefinition this[int ordinal] => _columns[ordinal];
        public int Count => _columns.Count;
        public bool Contains(string name) => _byName.ContainsKey(name);

        /// <summary>Computes the fixed-size row stride (sum of all column byte sizes)</summary>
        public int RowStride => _columns.Sum(c => c.FixedByteSize);

        public IEnumerator<ColumnDefinition> GetEnumerator() => _columns.GetEnumerator();
        IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();
    }

    /// <summary>
    /// A dynamic data table with runtime-defined schema.
    /// Analogous to System.Data.DataTable.
    /// </summary>
    public class SharedTable
    {
        public string Name { get; }
        public ColumnCollection Columns { get; }
        public RowCollection Rows { get; }
        public ushort SchemaVersion { get; private set; } = 0;
        public bool IsSchemaLocked { get; private set; }

        public SharedTable(string name)
        {
            Name = name ?? throw new ArgumentNullException(nameof(name));
            Columns = new ColumnCollection(this);
            Rows = new RowCollection();
        }

        /// <summary>Create a new empty row (with correct number of cells)</summary>
        public SharedRow NewRow() => new SharedRow(this);

        /// <summary>Lock the schema — no more columns can be added or removed</summary>
        public void LockSchema()
        {
            IsSchemaLocked = true;
        }

        /// <summary>
        /// Add a column and bump schema version.
        /// This is the public-facing method used during normal operation.
        /// </summary>
        public ColumnDefinition AddColumn(string name, ColumnType type)
        {
            var col = Columns.Add(name, type);
            SchemaVersion++;
            return col;
        }

        // --- Internal helpers for deserialization (bypass lock and version logic) ---
        internal void SetSchemaVersion(ushort version) => SchemaVersion = version;
        internal void SetSchemaLocked(bool locked) => IsSchemaLocked = locked;

        /// <summary>Add a column during deserialization (no lock check, no version bump)</summary>
        internal void AddColumnForDeserialization(string name, ColumnType type)
        {
            // Temporarily unlock to allow Add
            bool wasLocked = IsSchemaLocked;
            IsSchemaLocked = false;
            Columns.Add(name, type);
            IsSchemaLocked = wasLocked;
        }
    }
}
