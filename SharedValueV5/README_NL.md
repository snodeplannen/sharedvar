# SharedValueV5 — Dynamic Schema IPC Engine

**SharedValueV5** is de volgende generatie van de SharedValue inter-process communicatie engine. Waar V4 leunt op compile-time FlatBuffers schema's, biedt V5 **dynamische, programmatische schema-definitie at runtime** — net als `System.Data.DataSet`.

## Wat is nieuw in V5?

| Feature | V4 | V5 |
|:---|:---|:---|
| Schema definitie | `.fbs` + `flatc` codegen | Method calls at runtime |
| Schema wijziging | Hercompilatie vereist | Live, zonder herstart |
| VBS/VBA toegang | Vaste COM wrapper | Volledige dynamische controle |
| Multi-table | ❌ | ✅ (DataSet model) |
| Schema lock-out | N.v.t. | ✅ `LockSchema()` |
| Serialisatie | FlatBuffers (zero-copy) | Eigen binair formaat (self-describing) |

> **V5 vervangt V4 niet.** V4 blijft bestaan voor ultra-HFT scenario's (>100K updates/sec).

## Architectuur

```
┌─────────────────────────────────────────────────┐
│  Laag 3: Taal-Wrappers                         │
│   C++ Native │ C# (.NET) │ Python │ VBS (COM)  │
├─────────────────────────────────────────────────┤
│  Laag 2: SharedDataSet & SharedTable (NIEUW)    │
│   Multi-table │ Dynamic Schema │ BinarySerializer│
├─────────────────────────────────────────────────┤
│  Laag 1: SharedValueEngine (hergebruik V4)      │
│   MMF │ Named Mutex │ Named Events │ Ready Event│
└─────────────────────────────────────────────────┘
```

## Snel Starten

### C# Producer
```csharp
var ds = new SharedDataSet("Demo");
var table = ds.Tables.Add("Sensoren");
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
engine.CreateTable "Sensoren"
engine.SelectTable "Sensoren"
engine.DefineColumn "SensorId", "String"
engine.DefineColumn "Temperature", "Double"
engine.Connect "Demo", True

engine.AddRow
engine.SetValue "SensorId", "VBS_01"
engine.SetValue "Temperature", 22.5
engine.Publish
```

## Bouwen

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

### Testen (C++ → C#)
```powershell
powershell -File SharedValueV5\tests\Run-V5Tests.ps1
```

## Projectstructuur

```
SharedValueV5/
├── cpp_core/                 # C++ producer + headers
│   ├── ColumnDefinition.hpp  # ColumnType enum + definities  
│   ├── SharedRow.hpp         # Rij met typed getters/setters
│   ├── SharedTable.hpp       # Tabel + schema + locking
│   ├── SharedDataSet.hpp     # Multi-table container
│   ├── BinarySerializer.hpp  # Self-describing serializer
│   ├── SharedValueV5Engine.hpp # High-level Publish/Listen API
│   ├── SharedValueEngine.hpp # IPC transport (hergebruik V4)
│   ├── main.cpp              # Producer voorbeeld
│   └── CMakeLists.txt
├── csharp_core/              # C# consumer + core library
│   ├── ColumnDefinition.cs
│   ├── SharedRow.cs
│   ├── SharedTable.cs
│   ├── SharedDataSet.cs
│   ├── BinarySerializer.cs
│   ├── SharedValueV5Engine.cs
│   ├── SharedValueEngine.cs
│   ├── Program.cs            # Consumer voorbeeld
│   └── csharp_core.csproj
├── com_wrapper/              # COM-visible wrapper voor VBS/VBA
│   ├── SharedValueV5Com.cs
│   └── com_wrapper.csproj
├── examples/                 # VBScript voorbeelden
│   ├── vbs_producer_cursor.vbs
│   ├── vbs_producer_array.vbs
│   └── vbs_consumer.vbs
├── tests/
│   └── Run-V5Tests.ps1       # End-to-end cross-language test
├── ARCHITECTURE_NL.md        # Volledig architectuurdocument
└── README.md                 # Dit bestand
```

## Documentatie

- [ARCHITECTURE_NL.md](ARCHITECTURE_NL.md) — Volledig architectuur & ontwerpdocument
- [SharedValueV4/ARCHITECTURE_NL.md](../SharedValueV4/ARCHITECTURE_NL.md) — V4 architectuur
