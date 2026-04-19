using System;
using System.Runtime.InteropServices;
using System.Text;

namespace SharedValueV5.Com
{
    /// <summary>
    /// COM-visible wrapper for SharedValueV5.
    /// Provides both a Cursor model and a Parameter-Array model for VBScript/VBA.
    /// 
    /// Usage from VBScript:
    ///   Set engine = CreateObject("SharedValueV5.Engine")
    ///   engine.CreateTable "Sensors"
    ///   engine.SelectTable "Sensors"
    ///   engine.DefineColumn "Temperature", "Double"
    ///   engine.Connect "ChannelName", True
    ///   engine.AddRow
    ///   engine.SetValue "Temperature", 22.5
    ///   engine.Publish
    /// </summary>
    [ComVisible(true)]
    [Guid("B7C3D5E1-4A2F-4B89-9E6D-1F3A5C7D9E0B")]
    [ProgId("SharedValueV5.Engine")]
    [ClassInterface(ClassInterfaceType.AutoDual)]
    public class SharedValueV5Com : IDisposable
    {
        private SharedDataSet _dataSet;
        private SharedValueV5Engine? _engine;
        private string? _currentTableName;
        private int _cursorRowIndex = -1;
        private SharedDataSet? _lastReceivedData;
        private SharedDataSet? _lastReceivedReverseData;
        private bool _hasNewData;
        private string? _lastError;

        /// <summary>Constructs the COM Wrapper with a default DataSet.</summary>
        public SharedValueV5Com()
        {
            _dataSet = new SharedDataSet("ComDataSet");
        }

        // =========== SCHEMA DEFINITIE ===========

        /// <summary>Create a new table</summary>
        public void CreateTable(string tableName)
        {
            try
            {
                _dataSet.Tables.Add(tableName);
                _currentTableName = tableName;
            }
            catch (Exception ex) { _lastError = ex.Message; throw; }
        }

        /// <summary>Select the active table for all subsequent operations</summary>
        public void SelectTable(string tableName)
        {
            try
            {
                _ = _dataSet[tableName]; // Validate it exists
                _currentTableName = tableName;
                _cursorRowIndex = -1;
            }
            catch (Exception ex) { _lastError = ex.Message; throw; }
        }

        /// <summary>Add a column to the active table</summary>
        public void DefineColumn(string name, string typeName)
        {
            try
            {
                var table = GetCurrentTable();
                var type = ColumnDefinition.ParseTypeName(typeName);
                table.AddColumn(name, type);
            }
            catch (Exception ex) { _lastError = ex.Message; throw; }
        }

        /// <summary>Lock the schema of the active table</summary>
        public void LockSchema()
        {
            try { GetCurrentTable().LockSchema(); }
            catch (Exception ex) { _lastError = ex.Message; throw; }
        }

        /// <summary>Is the active table's schema locked?</summary>
        public bool IsSchemaLocked => GetCurrentTableSafe()?.IsSchemaLocked ?? false;

        /// <summary>Number of columns in the active table</summary>
        public int ColumnCount => GetCurrentTableSafe()?.Columns.Count ?? 0;

        /// <summary>Get column name by index</summary>
        public string ColumnName(int index) => GetCurrentTable().Columns[index].Name;

        /// <summary>Get column type name by index</summary>
        public string ColumnTypeName(int index) => GetCurrentTable().Columns[index].Type.ToString();

        /// <summary>Number of tables</summary>
        public int TableCount => _lastReceivedData?.Tables.Count ?? _dataSet.Tables.Count;

        /// <summary>Get table name by index</summary>
        public string TableName(int index)
        {
            if (_lastReceivedData != null)
                return _lastReceivedData.Tables[index].Name;
            return _dataSet.Tables[index].Name;
        }

        // =========== VERBINDING ===========

        /// <summary>Connect to a channel</summary>
        public bool Connect(string channelName, bool isHost)
        {
            try
            {
                _engine = new SharedValueV5Engine(channelName, isHost);

                _engine.OnDataReady += (ds) =>
                {
                    _lastReceivedData = ds;
                    _hasNewData = true;
                };

                _engine.OnReverseDataReady += (ds) =>
                {
                    _lastReceivedReverseData = ds;
                };

                return true;
            }
            catch (Exception ex)
            {
                _lastError = ex.Message;
                return false;
            }
        }

        /// <summary>Disconnect and clean up</summary>
        public void Disconnect()
        {
            _engine?.Dispose();
            _engine = null;
        }

        /// <summary>Is the engine connected?</summary>
        public bool IsConnected => _engine != null;

        // =========== DATA SCHRIJVEN: MODEL 1 — Cursor ===========

        /// <summary>Start a new row (cursor points to the new row)</summary>
        public void AddRow()
        {
            try
            {
                var table = GetCurrentTable();
                var row = table.NewRow();
                table.Rows.Add(row);
                _cursorRowIndex = table.Rows.Count - 1;
            }
            catch (Exception ex) { _lastError = ex.Message; throw; }
        }

        /// <summary>Set a value in the current cursor row</summary>
        public void SetValue(string column, object value)
        {
            try
            {
                var table = GetCurrentTable();
                if (_cursorRowIndex < 0 || _cursorRowIndex >= table.Rows.Count)
                    throw new InvalidOperationException("No active row. Call AddRow() first.");
                table.Rows[_cursorRowIndex][column] = value;
            }
            catch (Exception ex) { _lastError = ex.Message; throw; }
        }

        // =========== DATA SCHRIJVEN: MODEL 2 — Parameter Array ===========

        /// <summary>Add a complete row in one call (values in column ordinal order)</summary>
        public void AddRowDirect(
            object val1,
            [Optional] object val2,
            [Optional] object val3,
            [Optional] object val4,
            [Optional] object val5,
            [Optional] object val6,
            [Optional] object val7,
            [Optional] object val8,
            [Optional] object val9,
            [Optional] object val10)
        {
            try
            {
                var table = GetCurrentTable();
                var row = table.NewRow();

                object[] vals = { val1, val2, val3, val4, val5, val6, val7, val8, val9, val10 };
                int max = Math.Min(vals.Length, table.Columns.Count);
                for (int i = 0; i < max; i++)
                {
                    if (vals[i] != null && vals[i] != Type.Missing && !(vals[i] is System.Reflection.Missing))
                        row[i] = vals[i];
                }

                table.Rows.Add(row);
                _cursorRowIndex = table.Rows.Count - 1;
            }
            catch (Exception ex) { _lastError = ex.Message; throw; }
        }

        // =========== PUBLICEREN ===========

        /// <summary>Publish all tables to shared memory (forward/P2C channel)</summary>
        public void Publish()
        {
            try
            {
                EnsureEngine();
                _engine!.Publish(_dataSet);
            }
            catch (Exception ex) { _lastError = ex.Message; throw; }
        }

        /// <summary>Send data via the reverse (C2P) channel</summary>
        public void PublishReverse()
        {
            try
            {
                EnsureEngine();
                _engine!.PublishReverse(_dataSet);
            }
            catch (Exception ex) { _lastError = ex.Message; throw; }
        }

        // =========== DATA LEZEN ===========

        /// <summary>Number of rows in the active table (from received data or local)</summary>
        public int RowCount
        {
            get
            {
                if (_lastReceivedData != null && _currentTableName != null && _lastReceivedData.HasTable(_currentTableName))
                    return _lastReceivedData[_currentTableName].Rows.Count;
                return GetCurrentTableSafe()?.Rows.Count ?? 0;
            }
        }

        /// <summary>Get a value by row index and column name</summary>
        public object? GetValue(int rowIndex, string column)
        {
            try
            {
                var table = GetReadTable();
                var val = table.Rows[rowIndex][column];
                return ConvertForCom(val);
            }
            catch (Exception ex) { _lastError = ex.Message; throw; }
        }

        /// <summary>Get a value by row index and column index</summary>
        public object? GetValueByIndex(int rowIndex, int colIndex)
        {
            try
            {
                var table = GetReadTable();
                var val = table.Rows[rowIndex][colIndex];
                return ConvertForCom(val);
            }
            catch (Exception ex) { _lastError = ex.Message; throw; }
        }

        /// <summary>Export the active table as CSV string</summary>
        public string GetAllAsCsv()
        {
            var table = GetReadTable();
            var sb = new StringBuilder();

            // Headers
            for (int c = 0; c < table.Columns.Count; c++)
            {
                if (c > 0) sb.Append(',');
                sb.Append(table.Columns[c].Name);
            }
            sb.AppendLine();

            // Data
            for (int r = 0; r < table.Rows.Count; r++)
            {
                for (int c = 0; c < table.Columns.Count; c++)
                {
                    if (c > 0) sb.Append(',');
                    var val = table.Rows[r][c];
                    sb.Append(val?.ToString() ?? "");
                }
                sb.AppendLine();
            }

            return sb.ToString();
        }

        /// <summary>Check if new data has arrived</summary>
        public bool HasNewData()
        {
            if (_hasNewData)
            {
                _hasNewData = false;
                return true;
            }
            return false;
        }

        // =========== EVENTS / LISTENING ===========

        /// <summary>Start listening for incoming data</summary>
        public void StartListening()
        {
            EnsureEngine();
            _engine!.StartListening();
        }

        /// <summary>Stop listening</summary>
        public void StopListening()
        {
            _engine?.StopListening();
        }

        /// <summary>Get the last error message</summary>
        public string GetLastError() => _lastError ?? "";

        // =========== HELPERS ===========

        private SharedTable GetCurrentTable()
        {
            if (_currentTableName == null)
                throw new InvalidOperationException("No table selected. Call CreateTable() or SelectTable() first.");
            return _dataSet[_currentTableName];
        }

        private SharedTable? GetCurrentTableSafe()
        {
            if (_currentTableName == null) return null;
            return _dataSet.HasTable(_currentTableName) ? _dataSet[_currentTableName] : null;
        }

        private SharedTable GetReadTable()
        {
            if (_currentTableName == null)
                throw new InvalidOperationException("No table selected.");

            // Prefer received data over local
            if (_lastReceivedData != null && _lastReceivedData.HasTable(_currentTableName))
                return _lastReceivedData[_currentTableName];

            return _dataSet[_currentTableName];
        }

        private void EnsureEngine()
        {
            if (_engine == null)
                throw new InvalidOperationException("Not connected. Call Connect() first.");
        }

        /// <summary>Convert .NET types to COM-safe types</summary>
        private object? ConvertForCom(object? val)
        {
            if (val == null) return "";
            if (val is DateTime dt) return dt.ToString("yyyy-MM-dd HH:mm:ss");
            if (val is byte[] blob) return $"[Blob:{blob.Length} bytes]";
            return val;
        }

        /// <summary>Clean up engine resources safely.</summary>
        public void Dispose()
        {
            Disconnect();
        }
    }
}
