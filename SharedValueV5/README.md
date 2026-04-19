# SharedValueV5 — Dynamic Schema IPC Engine

**SharedValueV5** is the next generation of the SharedValue inter-process communication engine. While V4 relies on compile-time FlatBuffers schemas, V5 offers **dynamic, programmatic schema definition at runtime** — much like `System.Data.DataSet`.

## What's new in V5?

| Feature | V4 | V5 |
|:---|:---|:---|
| Schema definition | `.fbs` + `flatc` codegen | Method calls at runtime |
| Schema modification | Recompilation required | Live, without restart |
| VBS/VBA access | Fixed COM wrapper | Full dynamic control |
| Multi-table | ❌ | ✅ (DataSet model) |
| Schema lock-out | N/A | ✅ `LockSchema()` |
| Serialization | FlatBuffers (zero-copy) | Custom binary format (self-describing) |

> **V5 does not replace V4.** V4 remains available for ultra-HFT scenarios (>100K updates/sec).

## Architecture

```
┌─────────────────────────────────────────────────┐
│  Layer 3: Language Wrappers                     │
│   C++ Native │ C# (.NET) │ Python │ VBS (COM)  │
├─────────────────────────────────────────────────┤
│  Layer 2: SharedDataSet & SharedTable (NEW)     │
│   Multi-table │ Dynamic Schema │ BinarySerializer│
├─────────────────────────────────────────────────┤
│  Layer 1: SharedValueEngine (V4 reuse)          │
│   MMF │ Named Mutex │ Named Events │ Ready Event│
└─────────────────────────────────────────────────┘
```

## Quick Start

### C# Producer
```csharp
var ds = new SharedDataSet("Demo");
var table = ds.Tables.Add("Sensors");
table.AddColumn("SensorId", ColumnType.String);
table.AddColumn("Temperature", ColumnType.Double);
table.LockSchema();

var row = table.NewRow();
row["SensorId"] = "Sensor_01";
row["Temperature"] = 22.5;
table.Rows.Add(row);

using var engine = new SharedValueV5Engine("Demo", isHost: true);
engine.Publish(ds);
```

### VBScript Producer (Cursor Model)
```vbscript
Set engine = CreateObject("SharedValueV5.Engine")
engine.CreateTable "Sensors"
engine.SelectTable "Sensors"
engine.DefineColumn "SensorId", "String"
engine.DefineColumn "Temperature", "Double"
engine.Connect "Demo", True

engine.AddRow
engine.SetValue "SensorId", "VBS_01"
engine.SetValue "Temperature", 22.5
engine.Publish
```

## Building

### C# Core + COM wrapper
```powershell
dotnet build SharedValueV5\csharp_core
dotnet build SharedValueV5\com_wrapper
```

### C++ Producer
```powershell
cmake -B SharedValueV5\cpp_core\build -S SharedValueV5\cpp_core
cmake --build SharedValueV5\cpp_core\build --config Release
```

### Testing (C++ → C#)
```powershell
powershell -File SharedValueV5\tests\Run-V5Tests.ps1
```

## Project Structure

```
SharedValueV5/
├── cpp_core/                 # C++ producer + headers
│   ├── ColumnDefinition.hpp  # ColumnType enum + definitions  
│   ├── SharedRow.hpp         # Row with typed getters/setters
│   ├── SharedTable.hpp       # Table + schema + locking
│   ├── SharedDataSet.hpp     # Multi-table container
│   ├── BinarySerializer.hpp  # Self-describing serializer
│   ├── SharedValueV5Engine.hpp # High-level Publish/Listen API
│   ├── SharedValueEngine.hpp # IPC transport (V4 reuse)
│   ├── main.cpp              # Producer example
│   └── CMakeLists.txt
├── csharp_core/              # C# consumer + core library
│   ├── ColumnDefinition.cs
│   ├── SharedRow.cs
│   ├── SharedTable.cs
│   ├── SharedDataSet.cs
│   ├── BinarySerializer.cs
│   ├── SharedValueV5Engine.cs
│   ├── SharedValueEngine.cs
│   ├── Program.cs            # Consumer example
│   └── csharp_core.csproj
├── com_wrapper/              # COM-visible wrapper for VBS/VBA
│   ├── SharedValueV5Com.cs
│   └── com_wrapper.csproj
├── examples/                 # VBScript examples
│   ├── vbs_producer_cursor.vbs
│   ├── vbs_producer_array.vbs
│   └── vbs_consumer.vbs
├── tests/
│   └── Run-V5Tests.ps1       # End-to-end cross-language test
├── ARCHITECTURE.md           # Complete architecture document
└── README.md                 # This file
```

## Documentation

- [ARCHITECTURE.md](ARCHITECTURE.md) — Complete architecture & design document
- [SharedValueV4/ARCHITECTURE.md](../SharedValueV4/ARCHITECTURE.md) — V4 architecture
