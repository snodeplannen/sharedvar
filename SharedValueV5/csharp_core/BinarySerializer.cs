using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace SharedValueV5
{
    /// <summary>
    /// Serializes and deserializes SharedDataSet to/from a self-describing binary format.
    /// 
    /// Format:
    ///   GLOBAL HEADER (16 bytes): Magic, Format Version, Flags, Table Count
    ///   TABLE DIRECTORY: Per table (name + offset + size)
    ///   TABLE BLOCKS: Per table (schema + data + string pool)
    /// </summary>
    public static class BinarySerializer
    {
        // Magic: "SV5D" (SharedValue V5 DataSet)
        private static readonly byte[] Magic = { 0x53, 0x56, 0x35, 0x44 };
        private const ushort FormatVersion = 1;

        public static byte[] Serialize(SharedDataSet dataSet)
        {
            using var ms = new MemoryStream();
            using var bw = new BinaryWriter(ms, Encoding.UTF8);

            // --- GLOBAL HEADER (16 bytes) ---
            bw.Write(Magic);                    // [0..3] Magic
            bw.Write(FormatVersion);            // [4..5] Format version
            bw.Write((ushort)0);                // [6..7] Flags (reserved)
            bw.Write((ushort)dataSet.Tables.Count); // [8..9] Table count
            bw.Write(new byte[6]);              // [10..15] Reserved

            // --- Serialize each table block to temp buffers ---
            var tableBlocks = new List<(string name, byte[] data)>();
            foreach (var table in dataSet.Tables)
            {
                var block = SerializeTable(table);
                tableBlocks.Add((table.Name, block));
            }

            // --- TABLE DIRECTORY ---
            // Calculate where table data starts (after header + directory)
            // Directory per entry: 4 (name len) + name bytes + 4 (offset) + 4 (size) = 12 + name
            int dirSize = 0;
            foreach (var (name, _) in tableBlocks)
            {
                dirSize += 4 + Encoding.UTF8.GetByteCount(name) + 4 + 4;
            }

            long dataStartOffset = 16 + dirSize;
            long currentOffset = dataStartOffset;

            foreach (var (name, data) in tableBlocks)
            {
                byte[] nameBytes = Encoding.UTF8.GetBytes(name);
                bw.Write((uint)nameBytes.Length);          // Name length
                bw.Write(nameBytes);                       // Name
                bw.Write((uint)currentOffset);             // Data offset in MMF
                bw.Write((uint)data.Length);                // Data size
                currentOffset += data.Length;
            }

            // --- TABLE BLOCKS ---
            foreach (var (_, data) in tableBlocks)
            {
                bw.Write(data);
            }

            bw.Flush();
            return ms.ToArray();
        }

        public static SharedDataSet Deserialize(byte[] data)
        {
            using var ms = new MemoryStream(data);
            using var br = new BinaryReader(ms, Encoding.UTF8);

            // --- GLOBAL HEADER ---
            byte[] magic = br.ReadBytes(4);
            if (magic[0] != Magic[0] || magic[1] != Magic[1] || magic[2] != Magic[2] || magic[3] != Magic[3])
                throw new InvalidDataException("Invalid SV5D magic bytes.");

            ushort version = br.ReadUInt16();
            if (version != FormatVersion)
                throw new InvalidDataException($"Unsupported format version: {version}");

            ushort flags = br.ReadUInt16();
            ushort tableCount = br.ReadUInt16();
            br.ReadBytes(6); // Reserved

            var ds = new SharedDataSet("Deserialized");

            // --- TABLE DIRECTORY ---
            var entries = new List<(string name, uint offset, uint size)>();
            for (int i = 0; i < tableCount; i++)
            {
                uint nameLen = br.ReadUInt32();
                string name = Encoding.UTF8.GetString(br.ReadBytes((int)nameLen));
                uint offset = br.ReadUInt32();
                uint size = br.ReadUInt32();
                entries.Add((name, offset, size));
            }

            // --- TABLE BLOCKS ---
            foreach (var (name, offset, size) in entries)
            {
                byte[] tableData = new byte[size];
                Array.Copy(data, offset, tableData, 0, size);
                var table = DeserializeTable(name, tableData);
                ds.Tables.AddDirect(table);
            }

            return ds;
        }

        private static byte[] SerializeTable(SharedTable table)
        {
            using var ms = new MemoryStream();
            using var bw = new BinaryWriter(ms, Encoding.UTF8);

            // --- TABLE HEADER (8 bytes) ---
            bw.Write(table.SchemaVersion);                    // [0..1] Schema version
            bw.Write((byte)(table.IsSchemaLocked ? 1 : 0));   // [2] Locked flag
            bw.Write((byte)table.Columns.Count);              // [3] Column count
            bw.Write((uint)table.Rows.Count);                 // [4..7] Row count

            // --- SCHEMA DEFINITION ---
            foreach (var col in table.Columns)
            {
                bw.Write((byte)col.Type);
                byte[] nameBytes = Encoding.UTF8.GetBytes(col.Name);
                bw.Write((byte)nameBytes.Length);
                bw.Write(nameBytes);
            }

            // --- DATA: Build fixed rows + string pool ---
            int rowStride = table.Columns.RowStride;
            using var stringPool = new MemoryStream();
            using var rowData = new MemoryStream();
            using var rowWriter = new BinaryWriter(rowData);
            using var poolWriter = new BinaryWriter(stringPool, Encoding.UTF8);

            foreach (var row in table.Rows)
            {
                for (int c = 0; c < table.Columns.Count; c++)
                {
                    var col = table.Columns[c];
                    var val = row[c];

                    switch (col.Type)
                    {
                        case ColumnType.Int32:
                            rowWriter.Write(val != null ? Convert.ToInt32(val) : 0);
                            break;
                        case ColumnType.Int64:
                            rowWriter.Write(val != null ? Convert.ToInt64(val) : 0L);
                            break;
                        case ColumnType.Float:
                            rowWriter.Write(val != null ? Convert.ToSingle(val) : 0f);
                            break;
                        case ColumnType.Double:
                            rowWriter.Write(val != null ? Convert.ToDouble(val) : 0.0);
                            break;
                        case ColumnType.Bool:
                            rowWriter.Write(val != null && Convert.ToBoolean(val) ? (byte)1 : (byte)0);
                            break;
                        case ColumnType.DateTime:
                            long ticks = val is DateTime dt ? dt.Ticks : val != null ? Convert.ToDateTime(val).Ticks : 0L;
                            rowWriter.Write(ticks);
                            break;
                        case ColumnType.String:
                        {
                            string? str = val?.ToString();
                            if (str != null)
                            {
                                byte[] strBytes = Encoding.UTF8.GetBytes(str);
                                uint poolOffset = (uint)stringPool.Position;
                                poolWriter.Write(strBytes);
                                rowWriter.Write(poolOffset);
                                rowWriter.Write((uint)strBytes.Length);
                            }
                            else
                            {
                                rowWriter.Write((uint)0);  // offset
                                rowWriter.Write((uint)0);  // length 0 = null
                            }
                            break;
                        }
                        case ColumnType.Blob:
                        {
                            byte[]? blob = val as byte[];
                            if (blob != null)
                            {
                                uint poolOffset = (uint)stringPool.Position;
                                poolWriter.Write(blob);
                                rowWriter.Write(poolOffset);
                                rowWriter.Write((uint)blob.Length);
                            }
                            else
                            {
                                rowWriter.Write((uint)0);
                                rowWriter.Write((uint)0);
                            }
                            break;
                        }
                    }
                }
            }

            rowWriter.Flush();
            poolWriter.Flush();

            // --- DATA HEADER (8 bytes) ---
            byte[] rowBytes = rowData.ToArray();
            byte[] poolBytes = stringPool.ToArray();

            bw.Write((uint)rowStride);                  // Row stride
            bw.Write((uint)rowBytes.Length);             // String pool offset (relative to row data start)

            // --- FIXED ROW DATA ---
            bw.Write(rowBytes);

            // --- STRING POOL ---
            bw.Write(poolBytes);

            bw.Flush();
            return ms.ToArray();
        }

        private static SharedTable DeserializeTable(string name, byte[] data)
        {
            using var ms = new MemoryStream(data);
            using var br = new BinaryReader(ms, Encoding.UTF8);

            var table = new SharedTable(name);

            // --- TABLE HEADER ---
            ushort schemaVersion = br.ReadUInt16();
            byte locked = br.ReadByte();
            byte colCount = br.ReadByte();
            uint rowCount = br.ReadUInt32();

            // --- SCHEMA ---
            for (int i = 0; i < colCount; i++)
            {
                var type = (ColumnType)br.ReadByte();
                byte nameLen = br.ReadByte();
                string colName = Encoding.UTF8.GetString(br.ReadBytes(nameLen));
                table.AddColumnForDeserialization(colName, type);
            }

            table.SetSchemaVersion(schemaVersion);
            table.SetSchemaLocked(locked == 1);

            // --- DATA HEADER ---
            uint rowStride = br.ReadUInt32();
            uint poolDataSize = br.ReadUInt32();

            // --- Read row data and string pool ---
            long rowDataStart = ms.Position;
            byte[] rowBytes = br.ReadBytes((int)(rowCount * rowStride));
            byte[] poolBytes = br.ReadBytes((int)(data.Length - ms.Position));

            // --- Parse rows ---
            using var rowMs = new MemoryStream(rowBytes);
            using var rowReader = new BinaryReader(rowMs, Encoding.UTF8);

            for (int r = 0; r < rowCount; r++)
            {
                var row = table.NewRow();
                for (int c = 0; c < colCount; c++)
                {
                    var col = table.Columns[c];
                    switch (col.Type)
                    {
                        case ColumnType.Int32:
                            row[c] = rowReader.ReadInt32();
                            break;
                        case ColumnType.Int64:
                            row[c] = rowReader.ReadInt64();
                            break;
                        case ColumnType.Float:
                            row[c] = rowReader.ReadSingle();
                            break;
                        case ColumnType.Double:
                            row[c] = rowReader.ReadDouble();
                            break;
                        case ColumnType.Bool:
                            row[c] = rowReader.ReadByte() != 0;
                            break;
                        case ColumnType.DateTime:
                            long ticks = rowReader.ReadInt64();
                            row[c] = new DateTime(ticks, DateTimeKind.Utc);
                            break;
                        case ColumnType.String:
                        {
                            uint offset = rowReader.ReadUInt32();
                            uint length = rowReader.ReadUInt32();
                            if (length > 0)
                                row[c] = Encoding.UTF8.GetString(poolBytes, (int)offset, (int)length);
                            else
                                row[c] = null;
                            break;
                        }
                        case ColumnType.Blob:
                        {
                            uint offset = rowReader.ReadUInt32();
                            uint length = rowReader.ReadUInt32();
                            if (length > 0)
                            {
                                byte[] blob = new byte[length];
                                Array.Copy(poolBytes, (int)offset, blob, 0, (int)length);
                                row[c] = blob;
                            }
                            else
                                row[c] = null;
                            break;
                        }
                    }
                }
                table.Rows.Add(row);
            }

            return table;
        }
    }
}
