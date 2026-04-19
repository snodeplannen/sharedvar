#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <memory>
#include <iostream>
#include <string>

#include "SharedValueV5Engine.hpp"
#include "SharedDataSet.hpp"
#include "SharedTable.hpp"
#include "SharedRow.hpp"
#include "ColumnDefinition.hpp"

namespace py = pybind11;
using namespace SharedValueV5;

// ==========================================
// PySharedRow Wrapper
// ==========================================
// Represents either a loose row (being built) or a view into an existing row
class PySharedRow {
public:
    std::shared_ptr<SharedTable> m_table;
    SharedRow m_row;

    PySharedRow(std::shared_ptr<SharedTable> table, const SharedRow& row)
        : m_table(table), m_row(row) {}

    // Dynamic Python __setitem__
    void set_item(const std::string& key, const py::object& value) {
        if (!m_table) throw std::runtime_error("Row has no layout reference.");
        int ordinal = m_table->ColumnOrdinal(key);
        if (ordinal < 0) throw py::key_error("Column '" + key + "' not found in table.");

        auto type = m_table->Column(ordinal).type;
        if (value.is_none()) {
            // Nulls not strictly handled in V5 standard, we'll bypass or set defaults for now
            return; 
        }

        switch (type) {
            case ColumnType::Int32: m_row.Set(ordinal, value.cast<int32_t>()); break;
            case ColumnType::Int64: m_row.Set(ordinal, value.cast<int64_t>()); break;
            case ColumnType::Float: m_row.Set(ordinal, value.cast<float>()); break;
            case ColumnType::Double: m_row.Set(ordinal, value.cast<double>()); break;
            case ColumnType::Bool: m_row.Set(ordinal, value.cast<bool>()); break;
            case ColumnType::String: m_row.Set(ordinal, value.cast<std::string>()); break;
            case ColumnType::Blob: {
                // pybind handles bytes natively via CAST to std::string but we want vector<uint8_t>
                auto str = value.cast<std::string>();
                std::vector<uint8_t> blob(str.begin(), str.end());
                m_row.Set(ordinal, std::move(blob));
                break;
            }
            case ColumnType::DateTime: m_row.Set(ordinal, value.cast<int64_t>()); break; // Unix ticks mapping
            default: throw std::invalid_argument("Unsupported column type assignment.");
        }
    }

    // Dynamic Python __getitem__
    py::object get_item(const std::string& key) {
        if (!m_table) throw std::runtime_error("Row has no layout reference.");
        int ordinal = m_table->ColumnOrdinal(key);
        if (ordinal < 0) throw py::key_error("Column '" + key + "' not found in table.");
        
        if (m_row.IsNull(ordinal)) return py::none();

        auto type = m_table->Column(ordinal).type;
        switch (type) {
            case ColumnType::Int32: return py::cast(m_row.GetInt32(ordinal));
            case ColumnType::Int64: return py::cast(m_row.GetInt64(ordinal));
            case ColumnType::Float: return py::cast(m_row.GetFloat(ordinal));
            case ColumnType::Double: return py::cast(m_row.GetDouble(ordinal));
            case ColumnType::Bool: return py::cast(m_row.GetBool(ordinal));
            case ColumnType::String: return py::cast(m_row.GetString(ordinal));
            case ColumnType::Blob: {
                auto& b = m_row.GetBlob(ordinal);
                return py::bytes(reinterpret_cast<const char*>(b.data()), b.size());
            }
            case ColumnType::DateTime: return py::cast(m_row.GetInt64(ordinal));
            default: return py::none();
        }
    }
};

// ==========================================
// PySharedTable Wrapper
// ==========================================
class PySharedTable {
public:
    std::shared_ptr<SharedTable> m_table;

    PySharedTable(std::shared_ptr<SharedTable> table) : m_table(table) {}

    std::string get_name() const { return m_table->Name(); }
    int get_column_count() const { return m_table->ColumnCount(); }

    void add_column(const std::string& name, ColumnType type) {
        m_table->AddColumn(name, type);
    }

    PySharedRow new_row() {
        return PySharedRow(m_table, m_table->NewRow());
    }

    void add_row(const PySharedRow& row) {
        m_table->AddRow(row.m_row);
    }
    
    // Iterator representing 'rows'
    std::vector<PySharedRow> get_rows() {
        std::vector<PySharedRow> out;
        out.reserve(m_table->RowCount());
        for (int i = 0; i < m_table->RowCount(); i++) {
            out.emplace_back(m_table, m_table->Row(i));
        }
        return out;
    }
};

// ==========================================
// PySharedDataSet (Facade matching Architecture)
// ==========================================
class PySharedDataSet {
    SharedDataSet ds;
    std::unique_ptr<SharedValueV5Engine> engine;

public:
    PySharedDataSet(const std::string& name, bool is_host)
        : ds(name) 
    {
        std::wstring wname(name.begin(), name.end());
        engine = std::make_unique<SharedValueV5Engine>(wname, is_host);
    }

    PySharedTable create_table(const std::string& name) {
        if (!ds.HasTable(name)) {
            ds.AddTable(name);
        }
        // Access via index to get shared_ptr (SharedDataSet internals: we added direct shared_ptr map, 
        // but let's cheat and grab the underlying table by name)
        // Hmm, SharedDataSet::GetTable returns a reference. We need a shared_ptr for PySharedTable.
        // Wait, C++ ds doesn't expose shared_ptr. Let's modify dataset if required, or just copy?
        // Let's just create a dummy shared_ptr with custom deleter that does nothing.
        auto& t = ds.GetTable(name);
        std::shared_ptr<SharedTable> ptr(&t, [](SharedTable*){}); 
        return PySharedTable(ptr);
    }

    std::vector<PySharedTable> get_tables() {
        std::vector<PySharedTable> out;
        for (auto it = ds.begin(); it != ds.end(); ++it) {
            std::shared_ptr<SharedTable> ptr(it->get(), [](SharedTable*){});
            out.emplace_back(ptr);
        }
        return out;
    }

    void publish() {
        engine->Publish(ds);
    }

    void publish_reverse() {
        engine->PublishReverse(ds);
    }

    void start_listening() {
        engine->StartListening([this](const SharedDataSet& incomingDs) {
            // Overwrite our internal dataset with incoming tables
            // This is simple naive copy since this is a one-way listening architecture abstraction
            for(auto it = incomingDs.begin(); it != incomingDs.end(); ++it) {
                if(!this->ds.HasTable((*it)->Name())) {
                    this->ds.AddTableDirect(*it);
                } else {
                    // Replace existing table data
                    this->ds.GetTable((*it)->Name()) = *(*it); 
                }
            }
        });
    }
};

// ==========================================
// Pybind11 Module Declaration
// ==========================================
PYBIND11_MODULE(sharedvalue5, m) {
    m.doc() = "Python C-Extension for SharedValueV5 IPC";

    py::enum_<ColumnType>(m, "ColumnType")
        .value("INT32", ColumnType::Int32)
        .value("INT64", ColumnType::Int64)
        .value("FLOAT", ColumnType::Float)
        .value("DOUBLE", ColumnType::Double)
        .value("BOOL", ColumnType::Bool)
        .value("STRING", ColumnType::String)
        .value("BLOB", ColumnType::Blob)
        .value("DATETIME", ColumnType::DateTime)
        .export_values();

    // Re-export properties in module-level for clean access `sv5.DOUBLE`
    m.attr("INT32") = ColumnType::Int32;
    m.attr("DOUBLE") = ColumnType::Double;
    m.attr("STRING") = ColumnType::String;
    m.attr("BOOL") = ColumnType::Bool;

    py::class_<PySharedRow>(m, "SharedRow")
        .def("__setitem__", &PySharedRow::set_item)
        .def("__getitem__", &PySharedRow::get_item);

    py::class_<PySharedTable>(m, "SharedTable")
        .def_property_readonly("name", &PySharedTable::get_name)
        .def_property_readonly("column_count", &PySharedTable::get_column_count)
        .def_property_readonly("rows", &PySharedTable::get_rows)
        .def("add_column", &PySharedTable::add_column)
        .def("new_row", &PySharedTable::new_row)
        .def("add_row", &PySharedTable::add_row);

    py::class_<PySharedDataSet>(m, "SharedDataSet")
        .def(py::init<const std::string&, bool>(), py::arg("name"), py::arg("is_host"))
        .def_property_readonly("tables", &PySharedDataSet::get_tables)
        .def("create_table", &PySharedDataSet::create_table)
        .def("publish", &PySharedDataSet::publish)
        .def("publish_reverse", &PySharedDataSet::publish_reverse)
        .def("start_listening", &PySharedDataSet::start_listening);
}
