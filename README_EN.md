# ATL COM Server & SharedValue — Monorepo

**Version:** 0.3.0

A Windows C++ project for securely exchanging and persistently maintaining shared variables between independent processes. The project provides four generations:

- **SharedValueV2** — COM/RPC-based engine with an ATL Out-of-Process EXE Server.
- **SharedValueV3 (MemMap)** — Ultra-fast Memory-Mapped Files engine with FlatBuffers, devoid of any COM dependencies.
- **SharedValueV4** — Dual-Channel (bidirectional) Memory-Mapped Engine with compile-time schemas.
- **SharedValueV5** — Dynamic Schema IPC Engine with multi-table support, runtime schema locks, and COM interoperability.

## Project Overview

### COM Server (V2)

This project provides **two variants** of the same COM Server:

| Variant | Type | Registration | Location |
|---|---|---|---|
| **Legacy DLL** (InprocServer32) | In-Process DLL | `regsvr32` | [`ATLProjectcomserverLegacy/`](ATLProjectcomserverLegacy/) |
| **EXE Server** (LocalServer32) | Out-of-Process EXE | Self-registering | [`ATLProjectcomserverExe/`](ATLProjectcomserverExe/) |

The EXE variant is the **primary production model**: it runs as an independent Windows process with which multiple independent clients (C#, VBScript, Python) can simultaneously share data via COM/RPC marshaling.

### Memory-Mapped IPC Engines (V3, V4, V5)

| Component | Language | Purpose | Location |
|---|---|---|---|
| **C++ Producer** | C++20 | Writes FlatBuffer datasets to shared memory | [`SharedValueV3_MemMap/cpp_core/`](SharedValueV3_MemMap/cpp_core/) |
| **C# Consumer** | .NET 9 | Listens via Named Events and receives callbacks | [`SharedValueV3_MemMap/csharp_core/`](SharedValueV3_MemMap/csharp_core/) |

The V3 engine bypasses COM entirely. Data is shared via a Windows Memory-Mapped File (10 MB kernel page), synchronized using Named Mutexes, and signaled via Named Events — offering **nanosecond latency** and **0% CPU when idle**.

## COM Interfaces

Both variants expose the same three interfaces:

1. **`IMathOperations`** — Stateless mathematical operations (Add, Subtract, Multiply, Divide).
2. **`ISharedValue`** — Singleton in-memory state: `GetValue`, `SetValue`, `LockSharedValue`, observer subscriptions, and `ShutdownServer` (EXE only).
3. **`IDatasetProxy`** — Pageable key-value dataset: `AddRow`, `GetRowData`, `UpdateRow`, `RemoveRow`, `FetchPageKeys`, `FetchPageData`, featuring configurable `StorageMode`.
4. **`ISharedValueCallback`** — Observer interface for live event notifications upon state changes.

## Directory Structure

```
cursor_com_test/
├── ATLProjectcomserver.sln          # Central Visual Studio Solution (all projects)
├── ATLProjectcomserverLegacy/       # Legacy In-Process DLL COM Server
├── ATLProjectcomserverExe/          # Out-of-Process EXE COM Server (production)
├── SharedValueV2/                   # C++20 header-only core library (COM engine)
├── SharedValueV3_MemMap/            # Memory-Mapped FlatBuffers engine (V3)
│   ├── schema/                      #   FlatBuffers schema definition
│   ├── cpp_core/                    #   C++ native producer
│   ├── csharp_core/                 #   C# managed consumer
│   ├── ARCHITECTURE_EN.md           #   Extensive architecture with Mermaid diagrams
│   └── build_schema.ps1             #   Automated flatc download & codegen
├── SharedValueV4/                   # SharedValue Bidirectional FlatBuffers engine (V4)
├── SharedValueV5/                   # SharedValue Dynamic Schema IPC Engine (V5)
├── scripts/                         # PowerShell diagnostic tools
├── docs/                            # Design and architecture documentation
├── tests/                           # Cross-language integration tests
├── ARCHITECTURE_EN.md               # Technical architecture & Design Patterns
├── CHANGELOG_EN.md                  # Modification history
└── INSTALL_EN.md                    # Compilation and installation instructions
```

## Comparison of the Variants

This project encompasses multiple architecturally distinct approaches for cross-process data sharing. Each has its own strengths, limitations, and ideal application domains.

### Overview Table

| Feature | EXE Server + SharedValueV2 | SharedValueV3 (MemMap) | SharedValueV4 (Bidirectional) | SharedValueV5 (Dynamic Schema) |
|---|---|---|---|---|
| **Process model** | Out-of-process (standalone EXE) | No server required | No server required | No server required |
| **Transport** | RPC over Named Pipes | Direct shared memory | Direct shared memory | Direct shared memory |
| **Serialization** | VARIANT / BSTR / SAFEARRAY | FlatBuffers (zero-copy) | FlatBuffers (zero-copy) | Custom cross-language binary format |
| **Latency per call**| ~1-10 μs (RPC marshaling) | ~10-100 ns (pointer read) | ~10-100 ns | ~10-100 ns |
| **Traffic direction**| Bidirectional | Unidirectional (P2C) | Bidirectional (Dual-Channel) | Bidirectional (Dual-Channel) |
| **Multi-client** | ✅ Yes (singleton server) | ✅ Yes (kernel object sharing) | ✅ Yes | ✅ Yes |
| **Callbacks/Events**| COM IEventCallback (RPC) | Named Events (0% CPU) | Named Events (0% CPU) | Named Events (0% CPU) |
| **Schema evolution**| COM IDL / TypeLib | FlatBuffers `.fbs` (compile) | FlatBuffers `.fbs` (compile) | Runtime (programmatic, `DataSet`) |
| **Requires reg.** | Yes (`/RegServer`) | ❌ No | ❌ No | ❌ No (except COM wrapper for VBS) |
| **COM dependency** | Full | ❌ None | ❌ None | ❌ None (optional COM wrapper provided) |

> \* The `Global\` namespace for Named Kernel Objects requires `SeCreateGlobalPrivilege` by default, which is available to Administrators and services.

---

### 1. Legacy DLL — InprocServer32

The original COM DLL is loaded directly into the address space of the calling process.

**✅ Advantages:**
- Fastest possible invocation (~1 ns): no marshaling, no context switch.
- Easiest debugging — breakpoints work immediately within the client process.
- No separate server to launch or manage.

**❌ Disadvantages:**
- **No cross-process sharing**: each client receives its own copy of the DLL inside its own memory.
- If the DLL crashes → the entire client process crashes.
- Bitness-restriction: A 64-bit client cannot load a 32-bit DLL (and vice versa).
- Requires `regsvr32` registration utilizing admin rights.

**🎯 When to use:**
- Prototyping and rapid experimentation on a single machine, within a single process.
- Legacy compatibility with existing VBScript or Office VBA macros expecting an in-process COM object.

---

### 2. EXE Server + SharedValueV2 — LocalServer32

A standalone Windows process acting as a COM Server. All communication runs via RPC marshaling.

**✅ Advantages:**
- **True cross-process sharing**: multiple clients (C#, VBScript, Python) share the same singleton state.
- Process isolation: a server crash does not directly affect the client.
- Complete COM infrastructure: interfaces, TypeLibs, Connection Points, proxy/stub registration.
- Broad language support — anything that speaks COM can participate.
- Dataset batching via `SAFEARRAY` minimizes the number of RPC calls for bulk reads.

**❌ Disadvantages:**
- RPC overhead (~1-10 μs per call) renders per-record access across large datasets slow.
- COM registration mandates admin rights and a rigorous setup.
- Complex lifecycle: the server lifecycle, reference counting, and graceful shutdown must all be managed correctly.
- Schema changes mandate IDL adaptations, proxy/stub recompilation, and re-registration.

**🎯 When to use:**
- When **multiple clients in diverse languages** (C#, VBScript, Python) must share the same live state.
- When you have existing COM infrastructure and desire to build upon it.
- When the data exchange frequency is low to moderate (< 1000 calls/sec) and you benefit from batching.
- When you demand a **rich interface** boasting methods, properties, and events within a single framework.

---

### 3. SharedValueV3 MemMap — Memory-Mapped Files + FlatBuffers

Direct memory exchange via the Windows kernel, without COM or RPC intervention.

**✅ Advantages:**
- **Nanosecond latency**: data resides in literally the same memory block across two processes.
- **Zero-copy deserialization**: FlatBuffers needn't be parsed — field access merely constitutes a pointer offset.
- **0% CPU when idle**: the consumer thread slumbers inside the kernel until a Named Event triggers.
- No COM registration, no admin rights (unless employing the `Global\` namespace), no TypeLibs.
- **Schema evolution**: FlatBuffers permits the addition of fields without breaking backward compatibility.
- Infinite nesting and dynamic arrays — unconstrained by POD/fixed-size structs.

**❌ Disadvantages:**
- No built-in method calls or RPC: the consumer merely reads data; it cannot invoke functions directly on the producer.
- The producer **must run first** to create the Memory-Mapped File. The consumer waits via a retry loop.
- Unidirectional design (producer → consumer). Bidirectionality warrants a second shared memory block.
- No automatic proxy generation for arbitrary languages — client code must manually invoke the platform APIs.
- Schema modifications require `flatc` recompilation of the `.fbs` and a rebuild of both projects.

**🎯 When to use:**
- **High-frequency data feeds**: sensor data, real-time telemetry, financial tickers — where thousands of updates per second are required.
- When you demand **maximum velocity** and are willing to relinquish COM abstractions.
- When a C++ backend continuously produces data which a C# frontend (or multiple consumers) must display.
- When you **do not wish** or cannot carry out **COM registration** (e.g., in portable or containerized deployments).

### 4. SharedValueV4 & V5 — The Modern Generations

V4 and V5 build upon the ultra-fast foundations of V3, whilst resolving specific limitations:

- **V4 (Bidirectional)**: Introduces a C2P (Consumer-to-Producer) return channel via memory-mapped files. Superbly suited for closed ecosystems harboring HFT (High Frequency Trading) latency requirements (>100,000 updates per second), where the schema is determined at compile-time (`flatc`).
- **V5 (Dynamic Schema)**: Implements an ADO.NET-esque `DataSet` + `DataTable` model *within* the shared memory. C# and VBScript clients can programmatically generate and read dynamic columns at runtime. This schema is "self-describing" and iterative, buttressed by multi-language serialization and COM integration.

---

### Decision Tree

Do you need cross-process data sharing?
├── No → Legacy DLL (fastest, simplest)
└── Yes
    ├── Do you need complex RPC in many languages with legacy Office macros?
    │   └── Yes → EXE Server + SharedValueV2
    └── Do you require zero-overhead shared memory?
        ├── Is your schema dynamic or runtime-determined?
        │   └── Yes → SharedValueV5 (Dynamic Schema)
        └── Are data types and tables determined ahead of time?
            ├── Do you solely require push-data? (Producer→Consumer)
            │   └── Yes → SharedValueV3 (MemMap)
            └── Do you require push & reply traffic? (Bidirectional)
                └── Yes → SharedValueV4 (Dual-Channel)
```


## Quick Build

### Requirements
- **Visual Studio 2022** featuring the workload `Desktop development with C++` and component `C++ ATL for latest v143 build tools`.
- **CMake ≥ 3.20** (only required for SharedValueV2 standalone tests).

### CLI Compilation
```powershell
# Load the MSVC build environment
. Invoke-BuildEnvironment.ps1 -Toolchain MSVC -Version "Enterprise 2022" -Architecture x64

# Build the entire solution (Legacy DLL + EXE Server + test tool)
msbuild ATLProjectcomserver.sln /p:Configuration=Debug /p:Platform=x64 -m
```

### Visual Studio Compilation
1. Open `ATLProjectcomserver.sln`.
2. Select `Debug | x64`.
3. Build → Build Solution (`F7`).

## Registering the COM Server

```cmd
:: Legacy DLL (run as Administrator)
regsvr32 x64\Debug\ATLProjectcomserver.dll

:: EXE Server (self-registering, run as Administrator)
x64\Debug\ATLProjectcomserver.exe /RegServer
```

## Testing

Refer to [`tests/README_EN.md`](tests/README_EN.md) for a comprehensive test overview, or execute the automated cross-process suite:

```powershell
.\ATLProjectcomserverExe\tests\Run-CrossProcessTests.ps1
```

## Documentation

- [ARCHITECTURE_EN.md](ARCHITECTURE_EN.md) — Technical architecture, Design Patterns & layers.
- [INSTALL_EN.md](INSTALL_EN.md) — Detailed build and installation instructions.
- [CHANGELOG_EN.md](CHANGELOG_EN.md) — Exhaustive modification history.
- [docs/](docs/) — Design documents and migration analyses.
- [ATLProjectcomserverExe/README_EN.md](ATLProjectcomserverExe/README_EN.md) — User guide for the EXE COM Server variant.
- [SharedValueV2/README_EN.md](SharedValueV2/README_EN.md) — Introduction and overview of the SharedValueV2 C++20 engine.
- [SharedValueV3_MemMap/README_EN.md](SharedValueV3_MemMap/README_EN.md) — Quickstart for the ultra-fast Memory-Mapped FlatBuffers engine (V3).
- [SharedValueV3_MemMap/ARCHITECTURE_EN.md](SharedValueV3_MemMap/ARCHITECTURE_EN.md) — Extensive V3 architecture document featuring Mermaid diagrams.
- [SharedValueV5/README.md](SharedValueV5/README.md) — Quickstart for the SharedValueV5 Dynamic Schema IPC engine with VBScript / C# / C++ support.
- [SharedValueV5/ARCHITECTURE_NL.md](SharedValueV5/ARCHITECTURE_NL.md) — Exhaustive design document on the SharedValueV5 DataSets, Lazy Initialization, and Binary Serialization.
