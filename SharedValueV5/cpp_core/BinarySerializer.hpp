#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <stdexcept>
#include <memory>
#include "SharedDataSet.hpp"

namespace SharedValueV5 {

/// Serializes and deserializes SharedDataSet to/from a self-describing binary format.
/// Binary-compatible with the C# BinarySerializer.
class BinarySerializer {
    static constexpr uint8_t MAGIC[4] = { 0x53, 0x56, 0x35, 0x44 }; // "SV5D"
    static constexpr uint16_t FORMAT_VERSION = 1;

public:
    static std::vector<uint8_t> Serialize(const SharedDataSet& ds) {
        std::vector<uint8_t> out;
        Writer w(out);

        // --- GLOBAL HEADER (16 bytes) ---
        w.WriteBytes(MAGIC, 4);
        w.WriteU16(FORMAT_VERSION);
        w.WriteU16(0); // flags
        w.WriteU16(static_cast<uint16_t>(ds.TableCount()));
        w.WriteZeros(6); // reserved

        // --- Serialize each table block ---
        std::vector<std::pair<std::string, std::vector<uint8_t>>> tableBlocks;
        for (int t = 0; t < ds.TableCount(); t++) {
            auto block = SerializeTable(ds.Table(t));
            tableBlocks.push_back({ ds.Table(t).Name(), std::move(block) });
        }

        // --- TABLE DIRECTORY ---
        size_t dirSize = 0;
        for (auto& [name, _] : tableBlocks)
            dirSize += 4 + name.size() + 4 + 4;

        uint32_t dataStart = static_cast<uint32_t>(16 + dirSize);
        uint32_t currentOffset = dataStart;

        for (auto& [name, data] : tableBlocks) {
            w.WriteU32(static_cast<uint32_t>(name.size()));
            w.WriteString(name);
            w.WriteU32(currentOffset);
            w.WriteU32(static_cast<uint32_t>(data.size()));
            currentOffset += static_cast<uint32_t>(data.size());
        }

        // --- TABLE BLOCKS ---
        for (auto& [_, data] : tableBlocks)
            w.WriteBytes(data.data(), data.size());

        return out;
    }

    static SharedDataSet Deserialize(const uint8_t* data, size_t len) {
        Reader r(data, len);

        // --- GLOBAL HEADER ---
        uint8_t magic[4];
        r.ReadBytes(magic, 4);
        if (memcmp(magic, MAGIC, 4) != 0)
            throw std::runtime_error("Invalid SV5D magic bytes.");

        uint16_t version = r.ReadU16();
        if (version != FORMAT_VERSION)
            throw std::runtime_error("Unsupported format version.");

        r.ReadU16(); // flags
        uint16_t tableCount = r.ReadU16();
        r.Skip(6); // reserved

        SharedDataSet ds("Deserialized");

        // --- TABLE DIRECTORY ---
        struct Entry { std::string name; uint32_t offset; uint32_t size; };
        std::vector<Entry> entries;
        for (int i = 0; i < tableCount; i++) {
            uint32_t nameLen = r.ReadU32();
            std::string name = r.ReadString(nameLen);
            uint32_t off = r.ReadU32();
            uint32_t sz = r.ReadU32();
            entries.push_back({ name, off, sz });
        }

        // --- TABLE BLOCKS ---
        for (auto& e : entries) {
            auto table = DeserializeTable(e.name, data + e.offset, e.size);
            ds.AddTableDirect(table);
        }

        return ds;
    }

private:
    static std::vector<uint8_t> SerializeTable(const SharedTable& table) {
        std::vector<uint8_t> out;
        Writer w(out);

        // --- TABLE HEADER (8 bytes) ---
        w.WriteU16(table.SchemaVersion());
        w.WriteU8(table.IsSchemaLocked() ? 1 : 0);
        w.WriteU8(static_cast<uint8_t>(table.ColumnCount()));
        w.WriteU32(static_cast<uint32_t>(table.RowCount()));

        // --- SCHEMA ---
        for (auto& col : table.Columns()) {
            w.WriteU8(static_cast<uint8_t>(col.type));
            w.WriteU8(static_cast<uint8_t>(col.name.size()));
            w.WriteString(col.name);
        }

        // --- DATA ---
        std::vector<uint8_t> rowData;
        std::vector<uint8_t> stringPool;
        Writer rw(rowData);
        Writer pw(stringPool);

        for (int r = 0; r < table.RowCount(); r++) {
            const auto& row = table.Row(r);
            for (int c = 0; c < table.ColumnCount(); c++) {
                const auto& col = table.Column(c);
                const auto& val = row[c];

                switch (col.type) {
                case ColumnType::Int32:
                    rw.WriteI32(std::holds_alternative<int32_t>(val) ? std::get<int32_t>(val) : 0);
                    break;
                case ColumnType::Int64:
                case ColumnType::DateTime:
                    rw.WriteI64(std::holds_alternative<int64_t>(val) ? std::get<int64_t>(val) : 0);
                    break;
                case ColumnType::Float:
                    rw.WriteF32(std::holds_alternative<float>(val) ? std::get<float>(val) : 0.0f);
                    break;
                case ColumnType::Double:
                    rw.WriteF64(std::holds_alternative<double>(val) ? std::get<double>(val) : 0.0);
                    break;
                case ColumnType::Bool:
                    rw.WriteU8(std::holds_alternative<bool>(val) && std::get<bool>(val) ? 1 : 0);
                    break;
                case ColumnType::String:
                    if (std::holds_alternative<std::string>(val)) {
                        const auto& str = std::get<std::string>(val);
                        uint32_t off = static_cast<uint32_t>(stringPool.size());
                        pw.WriteString(str);
                        rw.WriteU32(off);
                        rw.WriteU32(static_cast<uint32_t>(str.size()));
                    } else {
                        rw.WriteU32(0);
                        rw.WriteU32(0);
                    }
                    break;
                case ColumnType::Blob:
                    if (std::holds_alternative<std::vector<uint8_t>>(val)) {
                        const auto& blob = std::get<std::vector<uint8_t>>(val);
                        uint32_t off = static_cast<uint32_t>(stringPool.size());
                        pw.WriteBytes(blob.data(), blob.size());
                        rw.WriteU32(off);
                        rw.WriteU32(static_cast<uint32_t>(blob.size()));
                    } else {
                        rw.WriteU32(0);
                        rw.WriteU32(0);
                    }
                    break;
                }
            }
        }

        // --- DATA HEADER ---
        int rowStride = table.RowStride();
        w.WriteU32(static_cast<uint32_t>(rowStride));
        w.WriteU32(static_cast<uint32_t>(rowData.size()));

        // --- ROW DATA + STRING POOL ---
        w.WriteBytes(rowData.data(), rowData.size());
        w.WriteBytes(stringPool.data(), stringPool.size());

        return out;
    }

    static std::shared_ptr<SharedTable> DeserializeTable(const std::string& name, const uint8_t* data, size_t len) {
        Reader r(data, len);
        auto table = std::make_shared<SharedTable>(name);

        // --- TABLE HEADER ---
        uint16_t schemaVersion = r.ReadU16();
        uint8_t locked = r.ReadU8();
        uint8_t colCount = r.ReadU8();
        uint32_t rowCount = r.ReadU32();

        // --- SCHEMA ---
        for (int i = 0; i < colCount; i++) {
            auto type = static_cast<ColumnType>(r.ReadU8());
            uint8_t nameLen = r.ReadU8();
            std::string colName = r.ReadString(nameLen);
            table->AddColumnForDeserialization(colName, type);
        }
        table->SetSchemaVersion(schemaVersion);
        table->SetSchemaLocked(locked == 1);

        // --- DATA HEADER ---
        uint32_t rowStride = r.ReadU32();
        uint32_t rowDataSize = r.ReadU32();

        // --- Read row data and pool ---
        size_t rowDataStart = r.Pos();
        const uint8_t* rowBytes = data + rowDataStart;
        const uint8_t* poolBytes = data + rowDataStart + rowDataSize;
        size_t poolSize = len - rowDataStart - rowDataSize;

        Reader rr(rowBytes, rowDataSize);
        for (uint32_t ri = 0; ri < rowCount; ri++) {
            auto row = table->NewRow();
            for (int c = 0; c < colCount; c++) {
                const auto& col = table->Column(c);
                switch (col.type) {
                case ColumnType::Int32:    row.Set(c, rr.ReadI32()); break;
                case ColumnType::Int64:
                case ColumnType::DateTime: row.Set(c, rr.ReadI64()); break;
                case ColumnType::Float:    row.Set(c, rr.ReadF32()); break;
                case ColumnType::Double:   row.Set(c, rr.ReadF64()); break;
                case ColumnType::Bool:     row.Set(c, rr.ReadU8() != 0); break;
                case ColumnType::String: {
                    uint32_t off = rr.ReadU32();
                    uint32_t slen = rr.ReadU32();
                    if (slen > 0)
                        row.Set(c, std::string(reinterpret_cast<const char*>(poolBytes + off), slen));
                    break;
                }
                case ColumnType::Blob: {
                    uint32_t off = rr.ReadU32();
                    uint32_t blen = rr.ReadU32();
                    if (blen > 0) {
                        std::vector<uint8_t> blob(poolBytes + off, poolBytes + off + blen);
                        row.Set(c, std::move(blob));
                    }
                    break;
                }
                }
            }
            table->AddRow(std::move(row));
        }

        return table;
    }

    // --- Internal Writer ---
    struct Writer {
        std::vector<uint8_t>& buf;
        Writer(std::vector<uint8_t>& b) : buf(b) {}
        void WriteU8(uint8_t v) { buf.push_back(v); }
        void WriteU16(uint16_t v) { buf.insert(buf.end(), (uint8_t*)&v, (uint8_t*)&v + 2); }
        void WriteU32(uint32_t v) { buf.insert(buf.end(), (uint8_t*)&v, (uint8_t*)&v + 4); }
        void WriteI32(int32_t v)  { buf.insert(buf.end(), (uint8_t*)&v, (uint8_t*)&v + 4); }
        void WriteI64(int64_t v)  { buf.insert(buf.end(), (uint8_t*)&v, (uint8_t*)&v + 8); }
        void WriteF32(float v)    { buf.insert(buf.end(), (uint8_t*)&v, (uint8_t*)&v + 4); }
        void WriteF64(double v)   { buf.insert(buf.end(), (uint8_t*)&v, (uint8_t*)&v + 8); }
        void WriteBytes(const uint8_t* d, size_t n) { buf.insert(buf.end(), d, d + n); }
        void WriteString(const std::string& s) { buf.insert(buf.end(), s.begin(), s.end()); }
        void WriteZeros(int n) { buf.insert(buf.end(), n, 0); }
    };

    // --- Internal Reader ---
    struct Reader {
        const uint8_t* data;
        size_t len;
        size_t pos = 0;
        Reader(const uint8_t* d, size_t l) : data(d), len(l) {}
        size_t Pos() const { return pos; }
        void Skip(int n) { pos += n; }
        void ReadBytes(uint8_t* out, int n) { memcpy(out, data + pos, n); pos += n; }
        uint8_t ReadU8()   { return data[pos++]; }
        uint16_t ReadU16() { uint16_t v; memcpy(&v, data + pos, 2); pos += 2; return v; }
        uint32_t ReadU32() { uint32_t v; memcpy(&v, data + pos, 4); pos += 4; return v; }
        int32_t ReadI32()  { int32_t v;  memcpy(&v, data + pos, 4); pos += 4; return v; }
        int64_t ReadI64()  { int64_t v;  memcpy(&v, data + pos, 8); pos += 8; return v; }
        float ReadF32()    { float v;    memcpy(&v, data + pos, 4); pos += 4; return v; }
        double ReadF64()   { double v;   memcpy(&v, data + pos, 8); pos += 8; return v; }
        std::string ReadString(size_t n) { std::string s(reinterpret_cast<const char*>(data + pos), n); pos += n; return s; }
    };
};

} // namespace SharedValueV5
