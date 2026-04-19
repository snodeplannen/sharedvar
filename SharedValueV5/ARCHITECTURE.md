# SharedValueV5 — Dynamic Schema Design

> **Translation Note:** This document is a placeholder for the English translation of the technical architecture. The original and most up-to-date document is currently maintained in Dutch. Please refer to [ARCHITECTURE_NL.md](ARCHITECTURE_NL.md) for the exhaustive design specifications.

## Overview
SharedValueV5 introduces a dynamic schema IPC engine designed to overcome the limitations of compile-time `FlatBuffers` schemas found in V3 and V4. By employing a "self-describing" binary memory layout alongside an ADO.NET-style `DataSet` and `DataTable` model, SharedValueV5 allows any language (including VBScript via COM) to dynamically define table columns at runtime.

### Core Architecture
1. **Schema Definition at Runtime:** Instead of `.fbs` files, tables are configured via programmatic API calls (`AddColumn()`, `LockSchema()`).
2. **Backward-Compatible Evolution:** Consumers automatically detect schema version changes and re-parse column offsets without breaking existing operations.
3. **Data Serialization:** Achieved through a specialized internal BinarySerializer that segregates fixed-size variables and a dynamic string/blob pool to prevent memory fragmentation.
4. **Transport:** Relies on the same battle-tested `SharedValueEngine` from V4, utilizing unmanaged Windows Memory-Mapped Files, Named Mutexes, and Named Events.

---

## 1. System Overview

```mermaid
graph TB
    subgraph "Application Layer (Language Wrappers)"
        CPP["C++ Native"]
        CS["C# (.NET)"]
        PYEXT["Python\n(C-Extension DLL)"]
        COM["VBScript / VBA\n(via COM Wrapper)"]
    end

    subgraph "SharedValueV5 Core"
        DS["SharedDataSet\n(Multiple Tables)"]
        ST["SharedTable\n(Schema + Rows)"]
        SER["BinarySerializer\n(Self-Describing)"]
        LOCK["SchemaLock\n(Freeze Logic)"]
    end

    subgraph "IPC Transport (V4 Reuse)"
        ENG["SharedValueEngine\n(MMF + Mutex + Event)"]
        DUAL["Dual-Channel\n(P2C + C2P)"]
    end

    subgraph "Windows Kernel"
        MMF["Memory-Mapped Files"]
        MTX["Named Mutex"]
        EVT["Named Events"]
        READY["Ready Events"]
    end

    CPP --> DS
    CS --> DS
    PYEXT --> DS
    COM --> DS

    DS --> ST
    ST --> SER
    ST --> LOCK
    SER --> ENG
    ENG --> DUAL
    DUAL --> MMF
    DUAL --> MTX
    DUAL --> EVT
    ENG --> READY

    style DS fill:#264653,stroke:#2a9d8f,color:#e9c46a
    style ST fill:#e76f51,stroke:#f4a261,color:#fff
    style SER fill:#2a9d8f,stroke:#264653,color:#fff
    style ENG fill:#023e8a,stroke:#03045e,color:#caf0f8
    style MMF fill:#2d6a4f,stroke:#1b4332,color:#d8f3dc
```

---

## 2. Binary Memory Layout (Self-Describing)

```mermaid
block-beta
    columns 6

    A["Header\n16 bytes\nMagic + Version\n+ Flags"]:1
    B["Table Directory\nVariable\nTable Count\n+ offsets"]:1
    C["Table 1\nSchema + Data"]:2
    D["Table 2\nSchema + Data"]:2

    style A fill:#264653,stroke:#2a9d8f,color:#e9c46a
    style B fill:#7f4f24,stroke:#582f0e,color:#ffe8d6
    style C fill:#e76f51,stroke:#f4a261,color:#fff
    style D fill:#2a9d8f,stroke:#264653,color:#fff
```

---

## 3. Schema Evolution & Auto-Discovery

```mermaid
sequenceDiagram
    participant P as Producer
    participant MMF as Shared Memory
    participant C as Consumer

    Note over P: Initial Schema v1
    P->>MMF: Sensors: [SensorId, Temp, Humidity]
    C->>MMF: Read Schema v1 → Cache Columns
    C->>C: Parse 3 Columns ✅

    Note over P: Add Column at Runtime
    P->>P: table.Columns.Add("BatteryLevel", Double)
    Note over P: SchemaVersion: 1 → 2
    P->>MMF: Sensors: [SensorId, Temp, Humidity, BatteryLevel]
    P->>MMF: SetEvent()

    C->>MMF: Read header → SchemaVersion = 2 (was 1)
    C->>C: Reparse Schema → Discover 4th Column
    C->>C: "BatteryLevel" (Double) detected ✅
    C->>C: Parse Data with 4 Columns ✅

    Note over P: Freeze Schema
    P->>P: table.LockSchema()
    P->>MMF: Locked Flag = 1
    Note over C: Consumer sees: IsSchemaLocked = True
```
