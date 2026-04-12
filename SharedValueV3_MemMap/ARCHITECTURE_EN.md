# SharedValueV3_MemMap — Architecture Document

This document details the complete architecture spanning the **SharedValueV3 Memory-Mapped Engine**: an ultra-high speed, cross-process communication layer blending Windows Memory-Mapped Files intertwined with Google FlatBuffers driving nanosecond-latency data exchanges flanking C++ and C# applications.

---

## Table of Contents

1. [Motivation & Architectural Choice](#1-motivation--architectural-choice)
2. [System Overview](#2-system-overview)
3. [Project Structure](#3-project-structure)
4. [Core Components](#4-core-components)
   - [Memory-Mapped Files (MMF)](#41-memory-mapped-files-mmf)
   - [Named Mutex (Cross-Process Locking)](#42-named-mutex-cross-process-locking)
   - [Named Event (Zero-CPU Callbacks)](#43-named-event-zero-cpu-callbacks)
   - [Google FlatBuffers (Serialization)](#44-google-flatbuffers-serialization)
5. [Data Layout spanning Shared Memory](#5-data-layout-spanning-shared-memory)
6. [Schema Architecture (FlatBuffers)](#6-schema-architecture-flatbuffers)
7. [Producer-Consumer Lifecycle](#7-producer-consumer-lifecycle)
8. [Synchronization Model](#8-synchronization-model)
9. [Exception Handling Architecture](#9-exception-handling-architecture)
10. [Build Pipeline & Tooling](#10-build-pipeline--tooling)
11. [Comparisons against SharedValueV2 (COM)](#11-comparisons-against-sharedvaluev2-com)

---

## 1. Motivation & Architectural Choice

The predecessor generation (`SharedValueV2`) cycles utilizing an Out-of-Process COM Server (`LocalServer32`). Any isolated property-call triggered passing through C# inflicts RPC-marshaling utilizing Named Pipes, simultaneously inducing stiff context-switches beside sharp kernel-transitions. Regarding bulk data ingestion (thousands arrays every sequence) this manifests generating intense bottlenecks.

SharedValueV3 permanently eradicates overhead instances actively **injecting directly shared memory partitions** looping parallel processes anchoring onto Windows kernel Memory-Mapped File mechanisms.

```mermaid
flowchart LR
    subgraph "SharedValueV2 (COM/RPC)"
        A["C# Client"] -->|"RPC Marshaling<br/>(~μs per call)"| B["COM Server EXE"]
        B --> C["SharedValueV2<br/>Engine (in-proc)"]
    end

    subgraph "SharedValueV3 (MemMap)"
        D["C++ Producer"] -->|"Direct Write<br/>(~ns)"| E["Shared Memory<br/>(Windows Kernel)"]
        E -->|"Direct Read<br/>(~ns)"| F["C# Consumer"]
    end

    style E fill:#2d6a4f,stroke:#1b4332,color:#fff
```

**Core Advantages:**

| Property | V2 (COM) | V3 (MemMap) |
|---|---|---|
| Latency per read | ~1-10 μs (RPC) | ~10-100 ns (pointer hop) |
| Serialization | VARIANT/BSTR marshaling | FlatBuffers zero-copy |
| CPU at idle | Polling mimicking COM Events | 0% (kernel wait locks) |
| Dynamic structures | `std::vector`, `BSTR` | FlatBuffer tables |
| Cross-language barrier | COM IDL/TLB blueprints | Shared `.fbs` schema |

---

## 2. System Overview

The entire framework encapsulates three tightly governed synchronization primitives communicating via the overarching Windows-kernel, alongside the layered FlatBuffers tier orchestrating structural logic.

```mermaid
graph TB
    subgraph "Windows Kernel Objects"
        MMF["Memory-Mapped File<br/><code>Global\MyGlobalDataset_Map</code>"]
        MTX["Named Mutex<br/><code>Global\MyGlobalDataset_Mutex</code>"]
        EVT["Named Event<br/><code>Global\MyGlobalDataset_Event</code>"]
    end

    subgraph "C++ Producer Process"
        CPP_ENGINE["SharedValueEngine<br/>(C++ / hpp)"]
        CPP_FB["FlatBufferBuilder"]
        CPP_MAIN["main.cpp<br/>(Producer Loop)"]
    end

    subgraph "C# Consumer Process"
        CS_ENGINE["SharedValueEngine<br/>(C# / .cs)"]
        CS_THREAD["ListenLoop Thread<br/>(Background)"]
        CS_EVENT["OnDataReady Event<br/>(C# Delegate)"]
        CS_MAIN["Program.cs<br/>(Consumer)"]
    end

    CPP_MAIN --> CPP_FB
    CPP_FB -->|"Serialize"| CPP_ENGINE
    CPP_ENGINE -->|"CreateFileMappingW"| MMF
    CPP_ENGINE -->|"CreateMutexW"| MTX
    CPP_ENGINE -->|"CreateEventW / SetEvent"| EVT

    CS_ENGINE -->|"OpenExisting"| MMF
    CS_ENGINE -->|"Mutex.OpenExisting"| MTX
    CS_ENGINE -->|"EventWaitHandle.OpenExisting"| EVT
    CS_THREAD -->|"WaitOne (blocks)"| EVT
    CS_THREAD -->|"ReadArray"| MMF
    CS_THREAD -->|"Invoke"| CS_EVENT
    CS_EVENT --> CS_MAIN

    style MMF fill:#1b4332,stroke:#081c15,color:#d8f3dc
    style MTX fill:#7f4f24,stroke:#582f0e,color:#ffe8d6
    style EVT fill:#023e8a,stroke:#03045e,color:#caf0f8
```

---

## 3. Project Structure

```mermaid
graph TD
    ROOT["SharedValueV3_MemMap/"]

    ROOT --> SCHEMA["schema/"]
    SCHEMA --> FBS["dataset.fbs<br/><i>FlatBuffers schema blueprint</i>"]

    ROOT --> CPP["cpp_core/"]
    CPP --> ENGINE_HPP["SharedValueEngine.hpp<br/><i>MMF + Mutex + Event wrapping</i>"]
    CPP --> EXCEPT_HPP["SharedValueException.hpp<br/><i>Exception hierarchy scaffolding</i>"]
    CPP --> MAIN_CPP["main.cpp<br/><i>Producer demonstration app</i>"]
    CPP --> GENERATED_H["dataset_generated.h<br/><i>FlatBuffers C++ logic</i>"]
    CPP --> CMAKE["CMakeLists.txt<br/><i>Build mapping constraints</i>"]

    ROOT --> CS["csharp_core/"]
    CS --> ENGINE_CS["SharedValueEngine.cs<br/><i>MMF + Mutex + Event wrapping</i>"]
    CS --> EXCEPT_CS["SharedValueEngineExceptions.cs<br/><i>Exception hierarchy scaffolding</i>"]
    CS --> PROG_CS["Program.cs<br/><i>Consumer demonstration app</i>"]
    CS --> GEN_CS["Generated/<br/><i>FlatBuffers C# classes</i>"]
    CS --> CSPROJ["csharp_core.csproj"]

    ROOT --> BUILD_PS1["build_schema.ps1<br/><i>Autonomous flatc logic driving codegen</i>"]
    ROOT --> README["README_EN.md"]
    ROOT --> ARCH["ARCHITECTURE_EN.md"]

    style ROOT fill:#264653,stroke:#2a9d8f,color:#e9c46a
    style SCHEMA fill:#e76f51,stroke:#f4a261,color:#fff
    style CPP fill:#2a9d8f,stroke:#264653,color:#fff
    style CS fill:#e9c46a,stroke:#f4a261,color:#264653
```

### Files Breakdown

| File | Language | Design Role |
|---|---|---|
| `schema/dataset.fbs` | FlatBuffers IDL | Defines foundational core shapes (tables, nesting mappings) |
| `cpp_core/SharedValueEngine.hpp` | C++20 | Scaffolds logic initializing MMF, Mutex, Event; pushes bytes |
| `cpp_core/SharedValueException.hpp` | C++20 | Hierarchical exception tracking layers |
| `cpp_core/main.cpp` | C++20 | Producer application publishing periodic clusters |
| `cpp_core/dataset_generated.h` | C++ (generated) | FlatBuffers serialization conversion logic |
| `csharp_core/SharedValueEngine.cs` | C# (.NET 9) | Re-links existing MMF, Mutex, Event layers; reads bytes |
| `csharp_core/SharedValueEngineExceptions.cs` | C# (.NET 9) | Shielded managed exception parameters |
| `csharp_core/Program.cs` | C# (.NET 9) | Consumer routing application ingesting updates heavily looping callbacks |
| `csharp_core/Generated/*.cs` | C# (generated) | FlatBuffers deserialization nodes |
| `build_schema.ps1` | PowerShell | Retrieves `flatc.exe` triggering code injections |
| `cpp_core/CMakeLists.txt` | CMake | C++ compile parameters enforcing FetchContent integrating FlatBuffers |

---

## 4. Core Components

### 4.1 Memory-Mapped Files (MMF)

A Memory-Mapped File acts representing a core Windows-kernel integration technique illuminating an actual block tracing physical memory (deriving spanning paging environments) rendering entirely visible stretching spanning dual virtual-address footprints covering overarching operations parallel. This implies nothing caches onto hard disks — it resides utterly confined occupying RAM whilst directed firmly by the kernel.

```mermaid
flowchart TB
    subgraph "Windows Kernel"
        PF["Paging File<br/>(Physical memory)"]
        KO["Kernel Object<br/><code>Global\..._Map</code>"]
        PF <--> KO
    end

    subgraph "C++ Process (Producer)"
        VA1["Virtual Address Space"]
        VIEW1["MapViewOfFile()<br/>Pointer: <code>m_pBuf</code>"]
        VA1 --> VIEW1
    end

    subgraph "C# Process (Consumer)"
        VA2["Virtual Address Space"]
        VIEW2["MemoryMappedViewAccessor<br/>ReadArray / ReadUInt64"]
        VA2 --> VIEW2
    end

    KO -->|"Mapped View"| VIEW1
    KO -->|"Mapped View"| VIEW2

    style KO fill:#2d6a4f,stroke:#1b4332,color:#d8f3dc
    style VIEW1 fill:#40916c,stroke:#2d6a4f,color:#fff
    style VIEW2 fill:#40916c,stroke:#2d6a4f,color:#fff
```

**Essential API calls:**

| Domain Constraint | Executed Function | Technical Aim |
|---|---|---|
| C++ (Producer mechanism) | `CreateFileMappingW(INVALID_HANDLE_VALUE, ...)` | Instantiates kernel component block (10 MB allocated) |
| C++ (Producer mechanism) | `MapViewOfFile(FILE_MAP_ALL_ACCESS)` | Snatches raw address pointer (`void*`) probing target space |
| C# (Consumer mechanism) | `MemoryMappedFile.OpenExisting(...)` | Extrapolates mapping aligning alongside an active tag node |
| C# (Consumer mechanism) | `mmf.CreateViewAccessor(0, maxSize)` | Generates logical `MemoryMappedViewAccessor` covering managed pulls |

### 4.2 Named Mutex (Cross-Process Locking)

Discounting flawless tight synchronization, jumping C# logic nodes might swallow fragments traversing raw byte flows exactly whereas the overlapping C++ module persistently writes inputs continuously. Securing a **Named Mutex** within the `Global\`-namespace renders an indicator perfectly transparent evaluating traversing completely segregated domains enforcing aggressive exclusivity rules.

```mermaid
sequenceDiagram
    participant P as C++ Producer
    participant M as Named Mutex<br/>(Kernel)
    participant C as C# Consumer

    P->>M: WaitForSingleObject() → LOCK
    Note over P: memcpy() stream feeding MMF
    P->>M: ReleaseMutex() → UNLOCK
    P->>P: SetEvent() → signal

    C->>M: _mutex.WaitOne(5000) → LOCK
    Note over C: ReadArray() scooping MMF payloads
    C->>M: ReleaseMutex() → UNLOCK
    Note over C: FlatBuffer parsing + delegate callback execution
```

**Rigidity concerning system failures:** Assuming producers buckle crashing completely retaining grips overlapping the mutex, consecutive consumer instances stumble trapping unique `WAIT_ABANDONED` (leveraging C++) equivalent mimicking `AbandonedMutexException` (targeting C#). Exploring mutual frameworks catches implementations explicitly: routing inherently snatches prevailing Mutex ownership while broadcasting logging flags.

### 4.3 Named Event (Zero-CPU Callbacks)

Targeting an Event object forces mimicking an isolated hardware interrupt channel: the core consumer thread drops entirely dormant registering **exactly 0% CPU consumption** pending producer interventions firing `SetEvent()`.

```mermaid
stateDiagram-v2
    [*] --> Waiting: StartListening()

    Waiting --> Signaled: Producer signals SetEvent()
    Signaled --> Reading: Mutex grip secured
    Reading --> Callback: FlatBuffer perfectly parsed
    Callback --> Waiting: OnDataReady cascades rapidly

    Waiting --> Stopped: StopListening()
    Stopped --> [*]

    note right of Waiting
        Kernel definitively blocks operations completely.
        0% CPU load remains intact.
        Leveraging WaitOne() / WaitForSingleObject()
    end note

    note right of Callback
        Native C# event sequence handling 
        unleashed natively upon designated listener threading.
    end note
```

**Auto-Reset protocol implementation:** Events are solidly bound mirroring **auto-reset** behaviors (`CreateEventW(..., FALSE, FALSE, ...)`). Demonstrating exclusively a singular dormant thread awakening instantly snapping states mirroring an un-signaled designation definitively — exterminating any race conditions allowing sequential readouts duplicating loops unintentionally.

### 4.4 Google FlatBuffers (Serialization)

FlatBuffers intercepts navigating the foundational roadblock stating universal C++ shapes (`std::vector`, `std::string`, deep pointer mappings) essentially disintegrate spanning natively mapped blocks crossing borders. FlatBuffers resolves standardizing strictly toward a **flattened byte-array template** delivering continuous read flows directly sidestepping sluggish parsing engines.

```mermaid
flowchart LR
    subgraph "C++ Producer"
        B["FlatBufferBuilder"]
        B --> S["Serialize routing towards<br/>uint8_t[] buffer block"]
    end

    subgraph "Shared Memory (MMF)"
        MEM["[size_t length mapping][flatbuffer logical bytes...]"]
    end

    subgraph "C# Consumer"
        BB["ByteBuffer(rawData wrapper)"]
        BB --> ROOT["GetRootAsSharedDataset()"]
        ROOT --> FIELD["Zero-copy localized field sweeps<br/>.ApiVersion, .Rows(i)"]
    end

    S -->|"memcpy"| MEM
    MEM -->|"ReadArray"| BB

    style MEM fill:#2d6a4f,stroke:#1b4332,color:#d8f3dc
```

**Discounting Protocol Buffers alongside JSON alternatives?**

| Requirement Criterion | FlatBuffers | Protocol Buffers | JSON Variants |
|---|---|---|---|
| Zero-copy mapping loops | ✅ Achieved perfectly | ❌ Denied (demands decode layers) | ❌ Denied entirely |
| Shared memory conformity | ✅ Shallow flattened arrays | ⚠️ Decode enforces heap jumping | ❌ Sluggish string processing hooks |
| Evolutionary schema tweaks | ✅ Flawless forward/backward | ✅ Flawless forward/backward | ❌ Highly fragile operations |
| Codegen merging C++ + C# | ✅ Embedded inherently | ✅ Embedded inherently | ❌ Hand-crafted mechanics |

---

## 5. Data Layout spanning Shared Memory

The 10 MB contiguous memory cluster utilizes extraordinarily austere binary alignments:

```mermaid
block-beta
    columns 4
    A["Bytes 0-7<br/><b>size_t</b><br/>Payload chunk size"]:1
    B["Bytes 8 .. 8+N<br/><b>uint8_t[]</b><br/>FlatBuffer operational bytes"]:2
    C["Vacant allocations<br/>(stretching up to 10 MB)"]:1

    style A fill:#e76f51,stroke:#f4a261,color:#fff
    style B fill:#2a9d8f,stroke:#264653,color:#fff
    style C fill:#6c757d,stroke:#495057,color:#dee2e6
```

| Memory Offset Segment | Variable Format | Implementation Mapping |
|---|---|---|
| `0x00` – `0x07` | `size_t` (covering 8 bytes stretching x64) | Dimensions defining raw FlatBuffer byte layouts |
| `0x08` – `0x08 + N` | `uint8_t[N]` | Raw untouched binary FlatBuffer structures (origin: `SharedDataset`) |
| `0x08 + N` – stretching towards bounds | — | Stagnant padding buffering expanded growth iterations |

**C++ injection routine:**
```cpp
size_t* pSize = static_cast<size_t*>(m_pBuf);
*pSize = size;                                    // embeds dimension metrics traversing offset 0
uint8_t* pDest = static_cast<uint8_t*>(m_pBuf) + sizeof(size_t);
memcpy(pDest, data, size);                        // embeds operational payload traversing offset 8
```

**C# probing routine:**
```csharp
ulong dataSize = _accessor.ReadUInt64(0);         // scoops dimensional logic sweeping offset 0
byte[] rawData = new byte[dataSize];
_accessor.ReadArray(8, rawData, 0, (int)dataSize); // scoops operational payload sweeping offset 8
```

---

## 6. Schema Architecture (FlatBuffers)

Core layout schemas (`dataset.fbs`) isolate mapping arrays driving three independent deeply nesting configurations:

```mermaid
erDiagram
    SharedDataset ||--|{ DataRow : "rows[]"
    DataRow ||--|{ NestedDetails : "details[]"

    SharedDataset {
        string api_version
        string last_updated_utc
    }

    DataRow {
        string row_id
        bool is_active
    }

    NestedDetails {
        double temperature
        double humidity
        int status_code
    }
```

### Navigating Schema Evolutionary Hooks

FlatBuffers inherently advocates tacking attributes flanking extreme array bounds leaving static structural frameworks functionally undisturbed. Thus manifesting rigorous forward ensuring overlapping backward-compatible iteration safety protocols:

```mermaid
flowchart TD
    V1["Schema iteration v1<br/>temperature, humidity, status_code"]
    V2["Schema iteration v2<br/>+ pressure (added node)"]
    V3["Schema iteration v3<br/>+ location (added node)"]

    V1 --> V2
    V2 --> V3

    OLD_P["Vintage Producer (v1)"] -->|"Casts v1 payload structures"| NEW_C["Modern Consumer (v3)"]
    NEW_P["Modern Producer (v3)"] -->|"Casts v3 payload structures"| OLD_C["Vintage Consumer (v1)"]

    NEW_C -->|"pressure forces heavily default(0)"| OK1["✅ Evaluates accurately"]
    OLD_C -->|"Silently dismisses disjointed unseen nodes"| OK2["✅ Evaluates accurately"]

    style OK1 fill:#2d6a4f,stroke:#1b4332,color:#d8f3dc
    style OK2 fill:#2d6a4f,stroke:#1b4332,color:#d8f3dc
```

> **Mandatory Doctrine:** Stripping established variables alongside re-allocating active Field-ID designations proves categorically forbidden. Append additions exclusively mapping external array extremities.

---

## 7. Producer-Consumer Lifecycle

Extrapolating comprehensive sequences launching parallel streams evaluating dataflow routing entirely transparently:

```mermaid
sequenceDiagram
    autonumber

    participant P as C++ Producer<br/>(main.cpp)
    participant K as Windows Kernel<br/>(MMF + Mutex + Event)
    participant C as C# Consumer<br/>(Program.cs)

    Note over P: Start routine initiates natively
    P->>K: CreateMutexW("Global\..._Mutex")
    P->>K: CreateEventW("Global\..._Event")
    P->>K: CreateFileMappingW("Global\..._Map", 10MB)
    P->>K: MapViewOfFile() → void* m_pBuf pointer hop
    Note over P: Underlying engine engages successfully ✅

    loop Firing intervals matching 2 seconds
        P->>P: FlatBufferBuilder.Finish(dataset) operation
        P->>K: WaitForSingleObject(mutex) → LOCK
        P->>K: memcpy(buf, flatbuffer, sizing metrics)
        P->>K: ReleaseMutex() → UNLOCK
        P->>K: SetEvent() → SIGNAL
    end

    Note over C: Consumer process trails (spinning up delayed sequence)
    C->>K: MemoryMappedFile.OpenExisting() evaluates probing
    Note over C: ❌ Crashes strictly assuming producers remain idle
    C->>C: Thread.Sleep(1000) → retry looping evaluates

    C->>K: MemoryMappedFile.OpenExisting() engages ✅
    C->>K: Mutex.OpenExisting() engages ✅
    C->>K: EventWaitHandle.OpenExisting() engages ✅
    Note over C: Core engine linkage bridges securely ✅

    C->>C: StartListening() fires processing loop
    Note over C: Background threaded sequence drops active

    loop Trapping periodic events continuously
        C->>K: EventWaitHandle.WaitOne() — BLOCKING (0% CPU)
        K-->>C: Logic flips event signaling parameters!
        C->>K: _mutex.WaitOne(5000) → LOCK
        C->>K: ReadUInt64(0) → dataSize mapping
        C->>K: ReadArray(8, rawData mapping) → Binary bytes routing
        C->>K: ReleaseMutex() → UNLOCK
        C->>C: GetRootAsSharedDataset(ByteBuffer overlay)
        C->>C: OnDataReady?.Invoke(dataset variable)
        Note over C: External code logic intercepts operational callbacks smoothly
    end
```

### Resilience Protocols Surrounding Startup Logic

The C# endpoint invariably manifests jumping cycles bypassing C++ processes occasionally. Catching desynchronizations demands solid retry loop scaffolding natively:

```mermaid
flowchart TD
    START["Consumer fires up sequence"] --> TRY["new SharedValueEngine() executes inherently"]
    TRY -->|"EngineInitializationException triggers sharply"| SLEEP["Thread.Sleep(1000) loop buffer"]
    SLEEP --> TRY
    TRY -->|"Connections clear successfully"| CONNECTED["Engine connects dependably ✅"]
    CONNECTED --> LISTEN["StartListening() subroutine"]
    LISTEN --> LOOP["ListenLoop (dormant background threading arrays)"]

    style CONNECTED fill:#2d6a4f,stroke:#1b4332,color:#d8f3dc
    style LOOP fill:#023e8a,stroke:#03045e,color:#caf0f8
```

### Kernel Object Lifecycle and Reference Counting Models

Windows structural components (originating MMF, Mutex, Event instances) operate solely propelled exploiting **reference-counting** arithmetic natively. Spanning external frameworks allocating bindings naturally spikes numeric increments accordingly cross-checking operations. Zeroing variables actively prompts unceremonious Windows garbage disposal sweeps instantly collapsing objects internally. Analyzing profound implications uncovers three overarching logical consequences:

#### Optimal baseline routine: Producer sustains infinite bounds, consumers transition dynamically

Serving mimicking central use-cases guarantees operational perfection flawlessly:

```mermaid
sequenceDiagram
    participant P as C++ Producer
    participant K as Kernel Variables<br/>(tracking refcounts logic)
    participant CA as C# Consumer Node A
    participant CB as C# Consumer Node B

    P->>K: Fires Native CreateFileMappingW → driving refcount = 1
    Note over K: Kernel objects anchor dynamically ✅

    CA->>K: Fires OpenExisting hooks → pushing refcount = 2
    Note over CA: Inhales incoming flows traversing callbacks

    CA->>K: Dispose() terminates → tumbling refcount = 1
    Note over K: Active arrays remain tethered securely ✅<br/>Producers grip primary nodes effectively

    CB->>K: Fires OpenExisting → leaping refcount = 2
    Note over CB: Unrelated client connects, zero logic friction observed

    CB->>K: Dispose() triggers → crashing refcount = 1
    Note over K: Kernel structures perfectly preserved ✅

    Note over P: Core Producer routine functions continuously driving indefinitely...
```

**Attribute definitions aligning this structure:**
- ✅ Producer routinely cycles completely ignoring peripheral consumer mapping counts natively
- ✅ Consumer components connect interchangeably executing un-tethered tracking bounds dynamically
- ✅ Substantial overlapping multi-consumer networks bridge securely parallel mapping unique local handlers cleanly
- ✅ Producer functions fully disconnected executing pure fire-and-forget logic loops
- ✅ Startup discrepancies isolating consumer pacing loops stall effortlessly firing continuous retry jumps smoothly

#### Severe edge-case bottleneck: Producer process disintegrates stripping consumer nodes

```mermaid
sequenceDiagram
    participant P as C++ Producer
    participant K as Kernel Variable Pools<br/>(tracking refcount routing)
    participant C as C# Consumer Base

    P->>K: Creates variables via CreateFileMappingW → pushing refcount = 1
    P->>K: Scopes WriteData() x3 clusters
    P->>K: Forces ~SharedValueEngine() terminating → dumping refcount = 0
    Note over K: ⚠️ KERNEL OBJECTS ANNIHILATED<br/>Zero anchors maintain variables concurrently

    C->>K: Attempts OpenExisting() mapping
    Note over C: ❌ Throws FileNotFoundException<br/>Core node allocations violently dissolved
```

Such permutations materialize **purely** confined covering rigidly formulated automated test sweeps whereas producers dump explicitly restrictive updates (mapping `--count N` flags) abandoning processes drastically afterwards. Normal environments (maintaining infinite cycling models) disregard anomalies.

#### Resolving test instability loops: implementing the `--linger` parameter tag

The embedded producer routine catches modifiers driving an explicit `--linger MS` argument halting shutdown mechanisms forcibly keeping node connections pulsing pushing N milliseconds overlapping concluding write actions. Providing buffer spans covering consumer initialization logic loops trapping final outstanding bursts flawlessly:

```mermaid
sequenceDiagram
    participant P as C++ Producer
    participant K as Native Kernel Core
    participant C as Active C# Consumer

    P->>K: Initial Creation → mapping refcount = 1
    P->>K: Scopes WriteData() x3 intervals
    Note over P: Parses --linger 8000 metric<br/>Delays 8 sec terminating sweeps...
    C->>K: Evaluates OpenExisting → spiking refcount = 2
    C->>K: Snatches Event WaitOne() logic → Trapping sequence #3 natively
    Note over C: Data captured accurately ✅
    P->>K: Routine exit trigger → shedding refcount = 1
    Note over K: Remaining objects persist accurately avoiding destruction ✅
```

> **Condensed observation:** Mapping explicit `--linger` extensions operates serving completely secluded testing diagnostic tools exclusively. Deployments encompassing production-level frameworks cycle producers indefinitely disregarding linger metrics completely safely.

---

## 8. Synchronization Model

Spanning three distinct kernel nodes effectively guarantees data-conformity intertwined leveraging aggressively optimal firing sequences:

```mermaid
flowchart TD
    subgraph "Write Operations Routine (C++ Producer loop)"
        W1["WaitForSingleObject(locking mutex stringently)"]
        W2["memcpy → Mapping Shared Memory buffers securely"]
        W3["ReleaseMutex() disengaging grips"]
        W4["SetEvent() launching notifications"]
        W1 --> W2 --> W3 --> W4
    end

    subgraph "Reading Traversals (C# Consumer logic)"
        R0["WaitOne(event signal) — drops blocked logic loops natively"]
        R1["WaitOne(mutex parameter, 5000)"]
        R2["ReadArray ← Scooping Shared Memory bytes"]
        R3["ReleaseMutex() peeling grips"]
        R4["Evaluate Parsing parsing FlatBuffers"]
        R5["OnDataReady triggers asynchronous overarching callbacks"]
        R0 --> R1 --> R2 --> R3 --> R4 --> R5
        R5 -->|"looping sequences"| R0
    end

    W4 -.->|"kernel rouses suspended threads perfectly"| R0

    style W1 fill:#7f4f24,stroke:#582f0e,color:#ffe8d6
    style W3 fill:#7f4f24,stroke:#582f0e,color:#ffe8d6
    style R1 fill:#7f4f24,stroke:#582f0e,color:#ffe8d6
    style R3 fill:#7f4f24,stroke:#582f0e,color:#ffe8d6
    style W4 fill:#023e8a,stroke:#03045e,color:#caf0f8
    style R0 fill:#023e8a,stroke:#03045e,color:#caf0f8
```

### Safety Operational Assurances

| Guarantee Designation | Operational Engineering Mechanic |
|---|---|
| **Eliminates torn read sequences** | Mutex rigidly sweeps spanning identical full stringing write loops → rigidly scoping reading arcs seamlessly |
| **Bypasses spinning busy-wait checks** | `WaitOne()` adjoining `WaitForSingleObject()` definitively suspends threading loops relying fully utilizing kernel states |
| **Avoids duplicate payload cycles** | Auto-reset properties inherently strip signal variables reverting silently targeting non-signaled logic bindings flawlessly |
| **Deflects hard process crashes** | Abandoned mutex strings forcibly cascade transferring allocations sweeping perfectly rescuing stuck threads |
| **Securing timeout loops efficiently** | Hard 5-second constraints sever hanging grips sweeping mutex parameters avoiding disastrous overlapping deadlocks cleanly |

---

## 9. Exception Handling Architecture

Mapping parallel environments natively implements mirrored hierarchical structures shielding system-level anomalies bridging codebases completely:

```mermaid
graph TD
    subgraph "C++ Structural Hierarchy Nodes"
        STD["std::runtime_error base abstraction"]
        SVE_CPP["SharedValueException logic module"]
        SYS["SystemException module<br/><i>wrapping native + DWORD Error constraints</i>"]
        MTX_CPP["MutexException logic block"]
        MEM_CPP["MemoryMappedException constraint tracking"]

        STD --> SVE_CPP
        SVE_CPP --> SYS
        SVE_CPP --> MTX_CPP
        SVE_CPP --> MEM_CPP
    end

    subgraph "C# Object Exception Branches"
        DOTNET["System.Exception baseline reference"]
        SVE_CS["SharedValueException logic bindings"]
        INIT["EngineInitializationException error block<br/><i>stringing component tags inherently</i>"]
        TIMEOUT["EngineTimeoutException timing flags"]
        CORRUPT["EngineCorruptedException data safety catches"]

        DOTNET --> SVE_CS
        SVE_CS --> INIT
        SVE_CS --> TIMEOUT
        SVE_CS --> CORRUPT
    end

    style SVE_CPP fill:#e76f51,stroke:#f4a261,color:#fff
    style SVE_CS fill:#e76f51,stroke:#f4a261,color:#fff
```

### Evaluating Breakdown Responses alongside Corrections

| Breakdown Trigger Scenario | C++ Embedded Component | C# Bounding Exceptions |
|---|---|---|
| Kernel component scaffolding crumbles violently | `SystemException` (husk framing `GetLastError()`) | `EngineInitializationException` (mapping specific component names recursively) |
| Mutex drops exceeding 5s timeouts | `MutexException` variables | `EngineTimeoutException` logic catches |
| Producer crashes stripping connections (abandoning mutex loops) | `WAIT_ABANDONED` sequences + logging parameters | `AbandonedMutexException` → executes catch procedures properly continuing flows |
| Payload stretches eclipsing overarching MMF limits severely | `MemoryMappedException` parameters | *Entirely isolated avoiding impacts inherently (read-only mechanics limit)* |
| Corrupted metric mapping arrays rendering structurally invalid parameters | *Untethered logic instances (write-only barriers apply natively)* | `EngineCorruptedException` triggering safely |
| Generative cascading WinAPI failure chains | `SystemException` (isolating precise failure + system bounds) | Scoped firmly utilizing `EngineInitializationException` components |

### Validating Mutex Locking Stability Handling Exception Chains

Deploying C++ `WriteData()` execution protocols guarantees mutex arrays securely disengage un-tethering natively disregarding aggressively anomalous loops inherently:

```mermaid
flowchart TD
    ACQ["WaitForSingleObject(locking mutex constraints)"] --> TRY["try { memcpy arrays + ReleaseMutex cycles + SetEvent routines }"]
    TRY -->|"success"| DONE["✅ Cleared execution correctly"]
    TRY -->|"exception cascading"| CATCH["catch (...) { ReleaseMutex() forces unlock; throw upward; }"]
    CATCH --> RETHROW["Exception safely bubbles upwards naturally"]

    style ACQ fill:#7f4f24,stroke:#582f0e,color:#ffe8d6
    style CATCH fill:#e76f51,stroke:#c9302c,color:#fff
```

Deploying C# `ReadCurrentData()` relies safely harnessing sweeping `try/finally` clusters executing comparable resilience mapping identically:

```mermaid
flowchart TD
    ACQ2["_mutex.WaitOne(5000 timeouts explicitly)"] --> TRY2["try { Execution ReadArray + Parse FlatBuffer sequences seamlessly }"]
    TRY2 --> FINALLY["finally { if (validly acquired) evaluates ReleaseMutex() cleanly }"]
    TRY2 -->|"exception mapping"| FINALLY
    FINALLY --> RETURN["Routing Return values mimicking rethrow constraints properly"]

    style ACQ2 fill:#7f4f24,stroke:#582f0e,color:#ffe8d6
    style FINALLY fill:#e9c46a,stroke:#f4a261,color:#264653
```

---

## 10. Build Pipeline & Tooling

### Schema Generation Tooling Integration

Leveraging `build_schema.ps1` sequences fundamentally automates massive underlying code-generative cycles rapidly:

```mermaid
flowchart TD
    START["build_schema.ps1 triggering routines"] --> CHECK{"Evaluates flatc.exe<br/>existence actively?"}

    CHECK -->|"Fails presence"| DL["Retrieves flatc instance v24.3.25<br/>fetching via GitHub Releases"]
    DL --> EXTRACT["Executes Expand-Archive sequences<br/>routing towards flatc_tools/ folder"]
    EXTRACT --> COMPILE

    CHECK -->|"Succeeds presence"| COMPILE["Triggers Executable flatc --cpp --csharp<br/>parsing dataset.fbs boundaries"]

    COMPILE --> CPP_OUT["cpp_core/<br/>dataset_generated.h populates cleanly"]
    COMPILE --> CS_OUT["csharp_core/Generated/<br/>*.cs classes mapped successfully"]

    CPP_OUT --> CMAKE_BUILD["CMake operations → bridging MSVC endpoints<br/>outputting MemMapProducer.exe"]
    CS_OUT --> DOTNET_BUILD["Executes dotnet build instances<br/>resolving csharp_core.dll boundaries"]

    style DL fill:#023e8a,stroke:#03045e,color:#caf0f8
    style CPP_OUT fill:#2a9d8f,stroke:#264653,color:#fff
    style CS_OUT fill:#e9c46a,stroke:#f4a261,color:#264653
```

### C++ Structural Integration (Mapping CMake alongside FetchContent routines)

```mermaid
flowchart LR
    CMAKE["CMakeLists.txt root"] --> FETCH["Resolves FetchContent logic<br/>pinning google/flatbuffers<br/>to version v24.3.25 specifically"]
    FETCH --> HEADERS["Extracts FlatBuffers Native C++<br/>isolated header-only logic matrices"]
    CMAKE --> EXE["Executable mapping MemMapProducer.exe seamlessly"]
    HEADERS --> EXE

    NOTE["Embeds NOMINMAX alongside WIN32_LEAN_AND_MEAN bounds<br/>exterminating systemic Windows.h collisions smoothly"]

    style FETCH fill:#023e8a,stroke:#03045e,color:#caf0f8
    style EXE fill:#2a9d8f,stroke:#264653,color:#fff
```

### C# Build Parameters (Deploying .NET 9 bounds)

```mermaid
flowchart LR
    CSPROJ["Project mapping csharp_core.csproj<br/>deploying net9.0-windows logic frames"] --> NUGET["Integrates Remote NuGet blocks: Google.FlatBuffers<br/>version v24.3.25 explicitly mapped"]
    CSPROJ --> GEN["Sourcing Generated/*.cs matrices<br/>(extracted spanning flatc execution bounds)"]
    NUGET --> DLL["Resolves compiling csharp_core.dll objects successfully"]
    GEN --> DLL

    style DLL fill:#e9c46a,stroke:#f4a261,color:#264653
    style NUGET fill:#023e8a,stroke:#03045e,color:#caf0f8
```

**Rigidity enveloping structural mapping variants:** Locking down specifically native C++ `FetchContent` bounds concurrently aligning C# packaged NuGet components traversing execution commands referencing `.exe` flatc binaries invariably restricts variants pinning identically spanning **v24.3.25** neutralizing systemic catastrophic serialization breakdowns continuously.

---

## 11. Comparisons against SharedValueV2 (COM)

```mermaid
flowchart TB
    subgraph "V2: Deploying COM/RPC Architecture models explicitly"
        direction TB
        CLIENT_V2["C# End-Client<br/>(Leveraging COM Interop boundaries)"]
        RPC_V2["Executing RPC strings spanning Named Pipes<br/>(Exhaustive kernel transitions)"]
        SERVER_V2["COM Server Process EXE<br/>(Mapping ATL/MFC arrays natively)"]
        ENGINE_V2["SharedValueV2 Engine blocks<br/>(Locking std::mutex, mapping std::vector)"]

        CLIENT_V2 -->|"Evaluates QueryInterface /<br/>Firing Invoke parameters"| RPC_V2
        RPC_V2 --> SERVER_V2
        SERVER_V2 --> ENGINE_V2
    end

    subgraph "V3: Advanced MemMap Framework Models"
        direction TB
        PRODUCER_V3["C++ Engine Producer Node"]
        SHARED_V3["Memory-Mapped Shared Blocks<br/>(10 MB sprawling kernel page)"]
        CONSUMER_V3["C# Engine Consumer Node"]

        PRODUCER_V3 -->|"Executes lightning fast memcpy strings<br/>(~ns execution loops)"| SHARED_V3
        SHARED_V3 -->|"Evaluating ReadArray sequences directly<br/>(~ns execution loops)"| CONSUMER_V3
    end

    style RPC_V2 fill:#e76f51,stroke:#c9302c,color:#fff
    style SHARED_V3 fill:#2d6a4f,stroke:#1b4332,color:#d8f3dc
```

| Trait Aspect Matrix | SharedValueV2 Execution (COM bounds) | SharedValueV3 Operations (MemMap metrics) |
|---|---|---|
| **Underlying transport** | Stiff RPC crossing Named Pipes | Native direct shared memory clusters |
| **Serialization strings** | Bloated VARIANT / BSTR / SAFEARRAY | Optimized FlatBuffers (zero-copy operations) |
| **Speed latency** | Dragging ~1-10 μs mapping each RPC traversal | Explosive ~10-100 ns parsing read operations |
| **Notifications cascades** | Ponderous COM Connection point handling (`IEventCallback`) | Hyper-efficient Named Event parsing adjoining C# delegate handlers |
| **Multithreading bounds** | `std::mutex` restrictions (strictly localized in-process hooks) | Expanded System Named Mutex bindings (safely mapping external cross-process operations) |
| **Structural blueprinting** | Limiting COM IDL parameters alongside TypeLib loops (`.tlb`) | Adaptable generalized FlatBuffers configurations `.fbs` (versatile crossing diverse languages safely) |
| **Dynamic allocations** | Cumbersome `std::vector<BSTR>` string mappings | Highly optimized nested FlatBuffer matrices (spanning inherently limitless logic layers) |
| **Framework dependencies** | Heaving ATL loops, overlapping COM Runtimes alongside Windows Registry | Austere minimal execution bindings relying entirely executing strict Native Windows Kernel API paths |
| **Compulsory deployment strings** | Aggressively enforced hooks navigating (`regsvr32` referencing COM mappings natively) | Extinguished cleanly bypassing registrations tracking internal isolated kernel referencing designations |

---

## Related Documentation

- [README_EN.md](README_EN.md) — Baseline introduction mechanics charting core structures beside quickstart execution guide procedures inherently.
- [ARCHITECTURE_EN.md](../ARCHITECTURE_EN.md) — Overarching architecture document defining mapping variables tracking generalized COM Server project logic trees completely.
- [README_EN.md](../SharedValueV2/README_EN.md) — Tracking foundational COM-based architecture mapping the overarching SharedValueV2 C++20 engine operations flawlessly.
