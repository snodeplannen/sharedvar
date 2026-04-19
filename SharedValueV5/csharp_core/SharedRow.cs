using System;
using System.Collections;
using System.Collections.Generic;

namespace SharedValueV5
{
    /// <summary>
    /// A single row of data within a SharedTable.
    /// Values are accessed by column name or ordinal index.
    /// </summary>
    public class SharedRow
    {
        private readonly SharedTable _table;
        private readonly object?[] _values;

        internal SharedRow(SharedTable table)
        {
            _table = table;
            _values = new object?[table.Columns.Count];
        }

        internal SharedRow(SharedTable table, object?[] values)
        {
            _table = table;
            _values = values;
        }

        /// <summary>Get or set a value by column name</summary>
        public object? this[string columnName]
        {
            get
            {
                var col = _table.Columns[columnName];
                return _values[col.Ordinal];
            }
            set
            {
                var col = _table.Columns[columnName];
                _values[col.Ordinal] = value;
            }
        }

        /// <summary>Get or set a value by ordinal index</summary>
        public object? this[int ordinal]
        {
            get => _values[ordinal];
            set => _values[ordinal] = value;
        }

        internal object?[] RawValues => _values;

        /// <summary>Expand internal array when columns are added to the table</summary>
        internal void ExpandTo(int columnCount)
        {
            if (_values.Length < columnCount)
            {
                var newValues = new object?[columnCount];
                Array.Copy(_values, newValues, _values.Length);
                // New slots default to null
                Array.Copy(newValues, _values.Length, newValues, _values.Length, columnCount - _values.Length);
            }
        }

        // --- Typed Getters ---
        public int GetInt32(string col) => Convert.ToInt32(_values[_table.Columns[col].Ordinal]);
        public int GetInt32(int ordinal) => Convert.ToInt32(_values[ordinal]);
        public long GetInt64(string col) => Convert.ToInt64(_values[_table.Columns[col].Ordinal]);
        public long GetInt64(int ordinal) => Convert.ToInt64(_values[ordinal]);
        public float GetFloat(string col) => Convert.ToSingle(_values[_table.Columns[col].Ordinal]);
        public float GetFloat(int ordinal) => Convert.ToSingle(_values[ordinal]);
        public double GetDouble(string col) => Convert.ToDouble(_values[_table.Columns[col].Ordinal]);
        public double GetDouble(int ordinal) => Convert.ToDouble(_values[ordinal]);
        public bool GetBool(string col) => Convert.ToBoolean(_values[_table.Columns[col].Ordinal]);
        public bool GetBool(int ordinal) => Convert.ToBoolean(_values[ordinal]);
        public string? GetString(string col) => _values[_table.Columns[col].Ordinal]?.ToString();
        public string? GetString(int ordinal) => _values[ordinal]?.ToString();
        public byte[]? GetBlob(string col) => _values[_table.Columns[col].Ordinal] as byte[];
        public byte[]? GetBlob(int ordinal) => _values[ordinal] as byte[];
        public DateTime GetDateTime(string col)
        {
            var val = _values[_table.Columns[col].Ordinal];
            return val is DateTime dt ? dt : val is long ticks ? new DateTime(ticks, DateTimeKind.Utc) : Convert.ToDateTime(val);
        }
        public DateTime GetDateTime(int ordinal)
        {
            var val = _values[ordinal];
            return val is DateTime dt ? dt : val is long ticks ? new DateTime(ticks, DateTimeKind.Utc) : Convert.ToDateTime(val);
        }

        // --- Typed Setters ---
        public void SetInt32(string col, int value) => _values[_table.Columns[col].Ordinal] = value;
        public void SetInt64(string col, long value) => _values[_table.Columns[col].Ordinal] = value;
        public void SetFloat(string col, float value) => _values[_table.Columns[col].Ordinal] = value;
        public void SetDouble(string col, double value) => _values[_table.Columns[col].Ordinal] = value;
        public void SetBool(string col, bool value) => _values[_table.Columns[col].Ordinal] = value;
        public void SetString(string col, string value) => _values[_table.Columns[col].Ordinal] = value;
        public void SetBlob(string col, byte[] value) => _values[_table.Columns[col].Ordinal] = value;
        public void SetDateTime(string col, DateTime value) => _values[_table.Columns[col].Ordinal] = value;
    }

    /// <summary>Collection of rows within a SharedTable</summary>
    public class RowCollection : IEnumerable<SharedRow>
    {
        private readonly List<SharedRow> _rows = new();

        public void Add(SharedRow row) => _rows.Add(row);
        public void RemoveAt(int index) => _rows.RemoveAt(index);
        public void Clear() => _rows.Clear();
        public SharedRow this[int index] => _rows[index];
        public int Count => _rows.Count;

        public IEnumerator<SharedRow> GetEnumerator() => _rows.GetEnumerator();
        IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();
    }
}
