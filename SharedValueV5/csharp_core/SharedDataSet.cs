using System;
using System.Collections;
using System.Collections.Generic;

namespace SharedValueV5
{
    /// <summary>Collection of SharedTable instances</summary>
    public class TableCollection : IEnumerable<SharedTable>
    {
        private readonly List<SharedTable> _tables = new();
        private readonly Dictionary<string, SharedTable> _byName = new(StringComparer.OrdinalIgnoreCase);

        public SharedTable Add(string name)
        {
            if (_byName.ContainsKey(name))
                throw new ArgumentException($"Table '{name}' already exists.");
            var table = new SharedTable(name);
            _tables.Add(table);
            _byName[name] = table;
            return table;
        }

        internal void AddDirect(SharedTable table)
        {
            _tables.Add(table);
            _byName[table.Name] = table;
        }

        public SharedTable this[string name]
        {
            get
            {
                if (!_byName.TryGetValue(name, out var table))
                    throw new KeyNotFoundException($"Table '{name}' not found.");
                return table;
            }
        }

        public SharedTable this[int index] => _tables[index];
        public int Count => _tables.Count;
        public bool Contains(string name) => _byName.ContainsKey(name);

        public void Remove(string name)
        {
            if (_byName.TryGetValue(name, out var table))
            {
                _tables.Remove(table);
                _byName.Remove(name);
            }
        }

        public IEnumerator<SharedTable> GetEnumerator() => _tables.GetEnumerator();
        IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();
    }

    /// <summary>
    /// Multi-table container, analogous to System.Data.DataSet.
    /// Holds multiple SharedTable instances and serializes them as a single unit.
    /// </summary>
    public class SharedDataSet
    {
        public string Name { get; }
        public TableCollection Tables { get; }

        public SharedDataSet(string name)
        {
            Name = name ?? throw new ArgumentNullException(nameof(name));
            Tables = new TableCollection();
        }

        /// <summary>Indexer for tables by name</summary>
        public SharedTable this[string tableName] => Tables[tableName];

        /// <summary>Check if a table exists</summary>
        public bool HasTable(string tableName) => Tables.Contains(tableName);

        /// <summary>Serialize all tables and schemas to a self-describing byte array</summary>
        public byte[] Serialize() => BinarySerializer.Serialize(this);

        /// <summary>Deserialize a byte array into a SharedDataSet</summary>
        public static SharedDataSet Deserialize(byte[] data) => BinarySerializer.Deserialize(data);
    }
}
