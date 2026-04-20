# SharedValueV4 — Architectuurdocument

Dit document beschrijft de volledige architectuur van de **SharedValueV4 Memory-Mapped Engine**: een ultra-snelle, cross-process communicatielaag die Windows Memory-Mapped Files combineert met Google FlatBuffers voor nanoseconde-latency data-uitwisseling tussen C++ en C# applicaties.

---

## Inhoudsopgave

1. [Motivatie & Architectuurkeuze](#1-motivatie--architectuurkeuze)
2. [Systeemoverzicht](#2-systeemoverzicht)
3. [Projectstructuur](#3-projectstructuur)
4. [Kerncomponenten](#4-kerncomponenten)
   - [Memory-Mapped Files (MMF)](#41-memory-mapped-files-mmf)
   - [Named Mutex (Cross-Process Locking)](#42-named-mutex-cross-process-locking)
   - [Named Event (Zero-CPU Callbacks)](#43-named-event-zero-cpu-callbacks)
   - [Google FlatBuffers (Serialisatie)](#44-google-flatbuffers-serialisatie)
5. [Data Layout in Shared Memory](#5-data-layout-in-shared-memory)
6. [Schema-Architectuur (FlatBuffers)](#6-schema-architectuur-flatbuffers)
7. [Producer-Consumer Levenscyclus](#7-producer-consumer-levenscyclus)
8. [Synchronisatiemodel](#8-synchronisatiemodel)
9. [Exception Handling Architectuur](#9-exception-handling-architectuur)
10. [Build Pipeline & Tooling](#10-build-pipeline--tooling)
11. [Vergelijking met SharedValueV3 (MemMap)](#11-vergelijking-met-sharedvaluev3-memmap)

---

## 1. Motivatie & Architectuurkeuze

De vorige generatie (`SharedValueV3`) verving COM/RPC al door snelle gedeelde-memory communicatie. Die architectuur bleef echter primair enkelrichting (producer naar consumer) voor het hoofdpad.

SharedValueV4 bouwt daarop voort met **echte bidirectionele memory-mapped kanalen** plus startup-handshake signalering zodat beide kanten veilig kunnen synchroniseren vóór data-uitwisseling.

```mermaid
flowchart LR
    subgraph "SharedValueV3 (Single-Channel)"
        A["C++ Producer"] -->|"Write<br/>(~ns)"| B["Shared Memory (P2C)"]
        B -->|"Read<br/>(~ns)"| C["C# Consumer"]
    end

    subgraph "SharedValueV4 (Dual-Channel MemMap)"
        D["C++ Host"] -->|"Write P2C"| E["P2C_Map"]
        F["C# Client"] -->|"Write C2P"| G["C2P_Map"]
        E -->|"Read P2C"| F
        G -->|"Read C2P"| D
    end

    style E fill:#2d6a4f,stroke:#1b4332,color:#fff
    style G fill:#1d3557,stroke:#0b2545,color:#fff
```

**Kernvoordelen:**

| Eigenschap | V3 (Single-Channel MemMap) | V4 (Dual-Channel MemMap) |
|---|---|---|
| Latency per read | ~10-100 ns (pointer) | ~10-100 ns (pointer) |
| Serialisatie | FlatBuffers zero-copy | FlatBuffers zero-copy |
| Richting | Alleen Producer → Consumer | Volledig Host ↔ Client |
| CPU bij idle | Event-driven sleeping threads | Event-driven sleeping threads + ready handshake |
| Dynamische data | FlatBuffer tables | FlatBuffer tables |
| Cross-language | Shared `.fbs` schema | Shared `.fbs` schema |

---

## 2. Systeemoverzicht

Het volledige systeem bestaat uit drie synchronisatieprimitieven die via de Windows-kernel gedeeld worden, en de FlatBuffers-laag die de data structureert.

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

## 3. Projectstructuur

```mermaid
graph TD
    ROOT["SharedValueV4/"]

    ROOT --> SCHEMA["schema/"]
    SCHEMA --> FBS["dataset.fbs<br/><i>FlatBuffers schema definitie</i>"]

    ROOT --> CPP["cpp_core/"]
    CPP --> ENGINE_HPP["SharedValueEngine.hpp<br/><i>MMF + Mutex + Event wrapper</i>"]
    CPP --> EXCEPT_HPP["SharedValueException.hpp<br/><i>Exception hiërarchie</i>"]
    CPP --> MAIN_CPP["main.cpp<br/><i>Producer demo applicatie</i>"]
    CPP --> GENERATED_H["dataset_generated.h<br/><i>FlatBuffers C++ header</i>"]
    CPP --> CMAKE["CMakeLists.txt<br/><i>Build configuratie</i>"]

    ROOT --> CS["csharp_core/"]
    CS --> ENGINE_CS["SharedValueEngine.cs<br/><i>MMF + Mutex + Event wrapper</i>"]
    CS --> EXCEPT_CS["SharedValueEngineExceptions.cs<br/><i>Exception hiërarchie</i>"]
    CS --> PROG_CS["Program.cs<br/><i>Consumer demo applicatie</i>"]
    CS --> GEN_CS["Generated/<br/><i>FlatBuffers C# klassen</i>"]
    CS --> CSPROJ["csharp_core.csproj"]

    ROOT --> BUILD_PS1["build_schema.ps1<br/><i>Automatisch flatc download + codegen</i>"]
    ROOT --> README["README.md"]
    ROOT --> ARCH["ARCHITECTURE.md"]

    style ROOT fill:#264653,stroke:#2a9d8f,color:#e9c46a
    style SCHEMA fill:#e76f51,stroke:#f4a261,color:#fff
    style CPP fill:#2a9d8f,stroke:#264653,color:#fff
    style CS fill:#e9c46a,stroke:#f4a261,color:#264653
```

### Bestandsoverzicht

| Bestand | Taal | Rol |
|---|---|---|
| `schema/dataset.fbs` | FlatBuffers IDL | Definieert de datastructuur (tables, nesting) |
| `cpp_core/SharedValueEngine.hpp` | C++20 | Creëert en beheert MMF, Mutex, Event; schrijft data |
| `cpp_core/SharedValueException.hpp` | C++20 | Hiërarchische exception klassen |
| `cpp_core/main.cpp` | C++20 | Producer die periodiek datasets publiceert |
| `cpp_core/dataset_generated.h` | C++ (gegenereerd) | FlatBuffers serialisatie-helpers |
| `csharp_core/SharedValueEngine.cs` | C# (.NET 9) | Opent bestaande MMF, Mutex, Event; leest data |
| `csharp_core/SharedValueEngineExceptions.cs` | C# (.NET 9) | Managed exception hiërarchie |
| `csharp_core/Program.cs` | C# (.NET 9) | Consumer die via callbacks data ontvangt |
| `csharp_core/Generated/*.cs` | C# (gegenereerd) | FlatBuffers deserialisatie-klassen |
| `build_schema.ps1` | PowerShell | Download `flatc.exe` en regenereert code |
| `cpp_core/CMakeLists.txt` | CMake | C++ build met FetchContent voor FlatBuffers |

---

## 4. Kerncomponenten

### 4.1 Memory-Mapped Files (MMF)

Een Memory-Mapped File is een Windows-kernelmechanisme waarmee een blok fysiek geheugen (vanuit de paging file) zichtbaar wordt gemaakt in de virtuele adresruimte van meerdere processen tegelijk. Het is geen bestand op disk — het leeft puur in RAM en wordt beheerd door de kernel.

```mermaid
flowchart TB
    subgraph "Windows Kernel"
        PF["Paging File<br/>(Fysiek geheugen)"]
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

**Essentiële API-aanroepen:**

| Zijde | Functie | Doel |
|---|---|---|
| C++ (Producer) | `CreateFileMappingW(INVALID_HANDLE_VALUE, ...)` | Creëert het kernel object (10 MB) |
| C++ (Producer) | `MapViewOfFile(FILE_MAP_ALL_ACCESS)` | Krijgt een raw pointer (`void*`) naar het geheugen |
| C# (Consumer) | `MemoryMappedFile.OpenExisting(...)` | Opent het bestaande kernel object by name |
| C# (Consumer) | `mmf.CreateViewAccessor(0, maxSize)` | Krijgt een `MemoryMappedViewAccessor` voor gemanaged lezen |

### 4.2 Named Mutex (Cross-Process Locking)

Zonder synchronisatie zou de C# consumer half-geschreven data kunnen lezen terwijl de C++ producer nog bezig is. Een **Named Mutex** in de `Global\`-namespace is zichtbaar voor alle processen op het systeem en garandeert exclusieve toegang.

```mermaid
sequenceDiagram
    participant P as C++ Producer
    participant M as Named Mutex<br/>(Kernel)
    participant C as C# Consumer

    P->>M: WaitForSingleObject() → LOCK
    Note over P: memcpy() data naar MMF
    P->>M: ReleaseMutex() → UNLOCK
    P->>P: SetEvent() → signal

    C->>M: _mutex.WaitOne(5000) → LOCK
    Note over C: ReadArray() data uit MMF
    C->>M: ReleaseMutex() → UNLOCK
    Note over C: FlatBuffer parse + callback
```

**Robuustheid bij crashes:** Als het producerproces crasht terwijl het de mutex vasthoudt, ontvangt de consumerthread een `WAIT_ABANDONED` (C++) of `AbandonedMutexException` (C#). In beide implementaties wordt dit expliciet opgevangen: de consumer verkrijgt eigendom van de mutex en logt een waarschuwing.

### 4.3 Named Event (Zero-CPU Callbacks)

Het Event-object fungeert als een interrupt-signaal: de consumerthread slaapt met **exact 0% CPU** totdat de producer `SetEvent()` aanroept.

```mermaid
stateDiagram-v2
    [*] --> Waiting: StartListening()

    Waiting --> Signaled: Producer calls SetEvent()
    Signaled --> Reading: Mutex acquired
    Reading --> Callback: FlatBuffer parsed
    Callback --> Waiting: OnDataReady fired

    Waiting --> Stopped: StopListening()
    Stopped --> [*]

    note right of Waiting
        Thread blocked by kernel.
        0% CPU consumption.
        WaitOne() / WaitForSingleObject()
    end note

    note right of Callback
        C# event delegate
        invoked on listener thread
    end note
```

**Auto-Reset gedrag:** Het event is geconfigureerd als **auto-reset** (`CreateEventW(..., FALSE, FALSE, ...)`). Dit betekent dat nadat één wachtende thread wakker wordt, het event automatisch terug naar non-signaled gaat — er is geen race condition mogelijk waarbij meerdere consumers hetzelfde event twee keer lezen.

### 4.4 Google FlatBuffers (Serialisatie)

FlatBuffers lost het fundamentele probleem op dat reguliere C++ datastructuren (`std::vector`, `std::string`, pointers) onbruikbaar zijn in shared memory. FlatBuffers serialiseert alles naar een **plat byte-array** dat zonder parsing direct doorgelezen kan worden.

```mermaid
flowchart LR
    subgraph "C++ Producer"
        B["FlatBufferBuilder"]
        B --> S["Serialize to<br/>uint8_t[] buffer"]
    end

    subgraph "Shared Memory (MMF)"
        MEM["[size_t len][flatbuffer bytes...]"]
    end

    subgraph "C# Consumer"
        BB["ByteBuffer(rawData)"]
        BB --> ROOT["GetRootAsSharedDataset()"]
        ROOT --> FIELD["Zero-copy field access<br/>.ApiVersion, .Rows(i)"]
    end

    S -->|"memcpy"| MEM
    MEM -->|"ReadArray"| BB

    style MEM fill:#2d6a4f,stroke:#1b4332,color:#d8f3dc
```

**Waarom geen Protocol Buffers of JSON?**

| Criterium | FlatBuffers | Protocol Buffers | JSON |
|---|---|---|---|
| Zero-copy access | ✅ Ja | ❌ Nee (decode vereist) | ❌ Nee |
| Shared memory compatible | ✅ Plat byte-array | ⚠️ Na decode, heap allocatie | ❌ String parsing |
| Schema evolutie | ✅ Forward/backward | ✅ Forward/backward | ❌ Fragiel |
| Codegen C++ + C# | ✅ Ja | ✅ Ja | ❌ Handmatig |

---

## 5. Data Layout in Shared Memory

Het geheugenblok van 10 MB heeft een eenvoudige binaire layout:

```mermaid
block-beta
    columns 4
    A["Bytes 0-7<br/><b>size_t</b><br/>Payload lengte"]:1
    B["Bytes 8 .. 8+N<br/><b>uint8_t[]</b><br/>FlatBuffer payload"]:2
    C["Ongebruikt<br/>(tot 10 MB)"]:1

    style A fill:#e76f51,stroke:#f4a261,color:#fff
    style B fill:#2a9d8f,stroke:#264653,color:#fff
    style C fill:#6c757d,stroke:#495057,color:#dee2e6
```

| Offset | Type | Beschrijving |
|---|---|---|
| `0x00` – `0x07` | `size_t` (8 bytes, x64) | Grootte van de FlatBuffer payload in bytes |
| `0x08` – `0x08 + N` | `uint8_t[N]` | De ruwe FlatBuffer binary (root: `SharedDataset`) |
| `0x08 + N` – einde | — | Ongebruikte ruimte (beschikbaar voor groei) |

**C++ schrijft:**
```cpp
size_t* pSize = static_cast<size_t*>(m_pBuf);
*pSize = size;                                    // lengte op offset 0
uint8_t* pDest = static_cast<uint8_t*>(m_pBuf) + sizeof(size_t);
memcpy(pDest, data, size);                        // payload op offset 8
```

**C# leest:**
```csharp
ulong dataSize = _accessor.ReadUInt64(0);         // lengte van offset 0
byte[] rawData = new byte[dataSize];
_accessor.ReadArray(8, rawData, 0, (int)dataSize); // payload van offset 8
```

---

## 6. Schema-Architectuur (FlatBuffers)

Het FlatBuffers-schema (`dataset.fbs`) definieert een drietal geneste tables:

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

### Schema Evolutie

FlatBuffers laat velden toe aan het einde van een table zonder bestaande data te breken. Dit maakt forward- en backward-compatibele schema-wijzigingen mogelijk:

```mermaid
flowchart TD
    V1["Schema v1<br/>temperature, humidity, status_code"]
    V2["Schema v2<br/>+ pressure (nieuw veld)"]
    V3["Schema v3<br/>+ location (nieuw veld)"]

    V1 --> V2
    V2 --> V3

    OLD_P["Oude Producer (v1)"] -->|"Schrijft v1 data"| NEW_C["Nieuwe Consumer (v3)"]
    NEW_P["Nieuwe Producer (v3)"] -->|"Schrijft v3 data"| OLD_C["Oude Consumer (v1)"]

    NEW_C -->|"pressure = default(0)"| OK1["✅ Werkt"]
    OLD_C -->|"Negeert onbekende velden"| OK2["✅ Werkt"]

    style OK1 fill:#2d6a4f,stroke:#1b4332,color:#d8f3dc
    style OK2 fill:#2d6a4f,stroke:#1b4332,color:#d8f3dc
```

> **Regel:** Verwijder nooit bestaande velden en hergebruik nooit veld-ID's. Voeg alleen velden toe aan het einde.

---

## 7. Producer-Consumer Levenscyclus

De complete opstartsequentie en dataflow van begin tot eind:

```mermaid
sequenceDiagram
    autonumber

    participant P as C++ Producer<br/>(main.cpp)
    participant K as Windows Kernel<br/>(MMF + Mutex + Event)
    participant C as C# Consumer<br/>(Program.cs)

    Note over P: Proces start
    P->>K: CreateMutexW("Global\..._Mutex")
    P->>K: CreateEventW("Global\..._Event")
    P->>K: CreateFileMappingW("Global\..._Map", 10MB)
    P->>K: MapViewOfFile() → void* m_pBuf
    Note over P: Engine ready ✅

    loop Elke 2 seconden
        P->>P: FlatBufferBuilder.Finish(dataset)
        P->>K: WaitForSingleObject(mutex) → LOCK
        P->>K: memcpy(buf, flatbuffer, size)
        P->>K: ReleaseMutex() → UNLOCK
        P->>K: SetEvent() → SIGNAL
    end

    Note over C: Proces start (later)
    C->>K: MemoryMappedFile.OpenExisting()
    Note over C: ❌ Faalt als producer nog niet draait
    C->>C: Thread.Sleep(1000) → retry

    C->>K: MemoryMappedFile.OpenExisting() ✅
    C->>K: Mutex.OpenExisting() ✅
    C->>K: EventWaitHandle.OpenExisting() ✅
    Note over C: Engine connected ✅

    C->>C: StartListening()
    Note over C: Background thread start

    loop Wacht op events
        C->>K: EventWaitHandle.WaitOne() — SLAAPT (0% CPU)
        K-->>C: Event gesignaleerd!
        C->>K: _mutex.WaitOne(5000) → LOCK
        C->>K: ReadUInt64(0) → dataSize
        C->>K: ReadArray(8, rawData) → FlatBuffer bytes
        C->>K: ReleaseMutex() → UNLOCK
        C->>C: GetRootAsSharedDataset(ByteBuffer)
        C->>C: OnDataReady?.Invoke(dataset)
        Note over C: Gebruikerscode ontvangt callback
    end
```

### Retry-Mechanisme bij Opstarten

De C# consumer kan eerder starten dan de C++ producer. Het retry-patroon vangt dit op:

```mermaid
flowchart TD
    START["Consumer start"] --> TRY["new SharedValueEngine()"]
    TRY -->|"EngineInitializationException"| SLEEP["Thread.Sleep(1000)"]
    SLEEP --> TRY
    TRY -->|"Succes"| CONNECTED["Engine connected ✅"]
    CONNECTED --> LISTEN["StartListening()"]
    LISTEN --> LOOP["ListenLoop (background thread)"]

    style CONNECTED fill:#2d6a4f,stroke:#1b4332,color:#d8f3dc
    style LOOP fill:#023e8a,stroke:#03045e,color:#caf0f8
```

### Kernel Object Levenscyclus en Reference Counting

Windows kernel objects (MMF, Mutex, Event) werken op basis van **reference counting**. Elk proces dat een handle opent verhoogt de interne teller. Zodra alle handles gesloten zijn (refcount → 0), vernietigt Windows het object. Dit heeft drie belangrijke implicaties:

#### Normaal gebruik: Producer draait oneindig, consumers komen en gaan

Dit is het primaire use-case scenario en werkt probleemloos:

```mermaid
sequenceDiagram
    participant P as C++ Producer
    participant K as Kernel Objects<br/>(refcount)
    participant CA as C# Consumer A
    participant CB as C# Consumer B

    P->>K: CreateFileMappingW → refcount = 1
    Note over K: Objects bestaan ✅

    CA->>K: OpenExisting → refcount = 2
    Note over CA: Leest data via callbacks

    CA->>K: Dispose() → refcount = 1
    Note over K: Objecten blijven bestaan ✅<br/>Producer heeft nog handles

    CB->>K: OpenExisting → refcount = 2
    Note over CB: Nieuwe consumer, geen probleem

    CB->>K: Dispose() → refcount = 1
    Note over K: Objecten blijven bestaan ✅

    Note over P: Producer draait voor altijd...
```

**Eigenschappen van dit model:**
- ✅ Producer draait oneindig lang ongeacht of er consumers zijn
- ✅ Consumers kunnen op elk moment verbinden en weer loskoppelen
- ✅ Meerdere consumers tegelijk is mogelijk (ieder opent eigen handle)
- ✅ De producer merkt niets van consumers (fire-and-forget)
- ✅ Consumer die eerder start dan de producer wacht via retry-loop

#### Problematisch scenario: Producer stopt terwijl er geen consumers zijn

```mermaid
sequenceDiagram
    participant P as C++ Producer
    participant K as Kernel Objects<br/>(refcount)
    participant C as C# Consumer

    P->>K: CreateFileMappingW → refcount = 1
    P->>K: WriteData() x3
    P->>K: ~SharedValueEngine() → refcount = 0
    Note over K: ⚠️ OBJECTS VERNIETIGD<br/>Geen handles meer open

    C->>K: OpenExisting()
    Note over C: ❌ FileNotFoundException<br/>Object bestaat niet meer
```

Dit scenario treedt **alleen** op bij geautomatiseerde tests waar de producer een beperkt aantal updates stuurt (`--count N`) en dan afsluit. Bij normaal gebruik (producer draait oneindig) is dit geen probleem.

#### Oplossing voor tests: `--linger` parameter

De producer ondersteunt een `--linger MS` parameter die hem na de laatste write nog N milliseconden in leven houdt. Dit geeft de consumer tijd om te verbinden en het laatste event te ontvangen:

```mermaid
sequenceDiagram
    participant P as C++ Producer
    participant K as Kernel Objects
    participant C as C# Consumer

    P->>K: Create → refcount = 1
    P->>K: WriteData() x3
    Note over P: --linger 8000<br/>Wacht 8 seconden...
    C->>K: OpenExisting → refcount = 2
    C->>K: WaitOne() → Event #3
    Note over C: Data ontvangen ✅
    P->>K: Exit → refcount = 1
    Note over K: Objecten blijven bestaan ✅
```

> **Samenvatting:** De `--linger` parameter is uitsluitend een testing utility. Bij productiegebruik draait de producer oneindig en is er geen linger nodig.

---


## 8. Synchronisatiemodel

Alle drie kernel-objecten werken samen om data-integriteit en efficiënte notificatie te garanderen:

```mermaid
flowchart TD
    subgraph "Write Path (C++ Producer)"
        W1["WaitForSingleObject(mutex)"]
        W2["memcpy → Shared Memory"]
        W3["ReleaseMutex()"]
        W4["SetEvent()"]
        W1 --> W2 --> W3 --> W4
    end

    subgraph "Read Path (C# Consumer)"
        R0["WaitOne(event) — blocked"]
        R1["WaitOne(mutex, 5000)"]
        R2["ReadArray ← Shared Memory"]
        R3["ReleaseMutex()"]
        R4["Parse FlatBuffer"]
        R5["OnDataReady callback"]
        R0 --> R1 --> R2 --> R3 --> R4 --> R5
        R5 -->|"loop"| R0
    end

    W4 -.->|"kernel wakes thread"| R0

    style W1 fill:#7f4f24,stroke:#582f0e,color:#ffe8d6
    style W3 fill:#7f4f24,stroke:#582f0e,color:#ffe8d6
    style R1 fill:#7f4f24,stroke:#582f0e,color:#ffe8d6
    style R3 fill:#7f4f24,stroke:#582f0e,color:#ffe8d6
    style W4 fill:#023e8a,stroke:#03045e,color:#caf0f8
    style R0 fill:#023e8a,stroke:#03045e,color:#caf0f8
```

### Garanties

| Garantie | Mechanisme |
|---|---|
| **Geen torn reads** | Mutex lockt de hele write → Mutex lockt de hele read |
| **Geen busy-wait** | `WaitOne()` en `WaitForSingleObject()` blokkeren in de kernel |
| **Geen dubbele lezing** | Auto-reset event keert automatisch terug naar non-signaled |
| **Crash-bestendig** | Abandoned mutex wordt automatisch overgenomen door wachtende thread |
| **Timeout-beveiliging** | 5 seconden timeout op mutex acquisitie voorkomt deadlocks |

---

## 9. Exception Handling Architectuur

Beide talen implementeren een parallelle exception-hiërarchie die alle faalscenario's van het systeem afdekt:

```mermaid
graph TD
    subgraph "C++ Exception Hierarchy"
        STD["std::runtime_error"]
        SVE_CPP["SharedValueException"]
        SYS["SystemException<br/><i>+ DWORD errorCode</i>"]
        MTX_CPP["MutexException"]
        MEM_CPP["MemoryMappedException"]

        STD --> SVE_CPP
        SVE_CPP --> SYS
        SVE_CPP --> MTX_CPP
        SVE_CPP --> MEM_CPP
    end

    subgraph "C# Exception Hierarchy"
        DOTNET["System.Exception"]
        SVE_CS["SharedValueException"]
        INIT["EngineInitializationException<br/><i>+ component name</i>"]
        TIMEOUT["EngineTimeoutException"]
        CORRUPT["EngineCorruptedException"]

        DOTNET --> SVE_CS
        SVE_CS --> INIT
        SVE_CS --> TIMEOUT
        SVE_CS --> CORRUPT
    end

    style SVE_CPP fill:#e76f51,stroke:#f4a261,color:#fff
    style SVE_CS fill:#e76f51,stroke:#f4a261,color:#fff
```

### Faalscenario's en Afhandeling

| Scenario | C++ Exception | C# Exception |
|---|---|---|
| Kernel object creatie faalt | `SystemException` (met `GetLastError()`) | `EngineInitializationException` (met component naam) |
| Mutex timeout (5s) | `MutexException` | `EngineTimeoutException` |
| Producer crasht (abandoned mutex) | `WAIT_ABANDONED` + warning log | `AbandonedMutexException` → catch + continue |
| Data groter dan MMF capaciteit | `MemoryMappedException` | *Niet van toepassing (read-only)* |
| Corrupte payload size | *Niet van toepassing (write-only)* | `EngineCorruptedException` |
| Generieke WinAPI fout | `SystemException` (context + error code) | Wrapped in `EngineInitializationException` |

### Mutex Safety in Exception Context

De C++ `WriteData()` methode garandeert dat de mutex altijd vrijgegeven wordt, zelfs bij onverwachte fouten:

```mermaid
flowchart TD
    ACQ["WaitForSingleObject(mutex)"] --> TRY["try { memcpy + ReleaseMutex + SetEvent }"]
    TRY -->|"succes"| DONE["✅ Klaar"]
    TRY -->|"exception"| CATCH["catch (...) { ReleaseMutex(); throw; }"]
    CATCH --> RETHROW["Exception wordt doorgegeven"]

    style ACQ fill:#7f4f24,stroke:#582f0e,color:#ffe8d6
    style CATCH fill:#e76f51,stroke:#c9302c,color:#fff
```

De C# `ReadCurrentData()` gebruikt een `try/finally` blok met dezelfde garantie:

```mermaid
flowchart TD
    ACQ2["_mutex.WaitOne(5000)"] --> TRY2["try { ReadArray + FlatBuffer parse }"]
    TRY2 --> FINALLY["finally { if (acquired) ReleaseMutex() }"]
    TRY2 -->|"exception"| FINALLY
    FINALLY --> RETURN["Return of rethrow"]

    style ACQ2 fill:#7f4f24,stroke:#582f0e,color:#ffe8d6
    style FINALLY fill:#e9c46a,stroke:#f4a261,color:#264653
```

---

## 10. Build Pipeline & Tooling

### Schema Compilatie

Het `build_schema.ps1` script automatiseert het gehele codegeneratie-proces:

```mermaid
flowchart TD
    START["build_schema.ps1"] --> CHECK{"flatc.exe<br/>aanwezig?"}

    CHECK -->|"Nee"| DL["Download flatc v24.3.25<br/>van GitHub Releases"]
    DL --> EXTRACT["Expand-Archive<br/>naar flatc_tools/"]
    EXTRACT --> COMPILE

    CHECK -->|"Ja"| COMPILE["flatc --cpp --csharp<br/>dataset.fbs"]

    COMPILE --> CPP_OUT["cpp_core/<br/>dataset_generated.h"]
    COMPILE --> CS_OUT["csharp_core/Generated/<br/>*.cs klassen"]

    CPP_OUT --> CMAKE_BUILD["CMake → MSVC<br/>MemMapProducer.exe"]
    CS_OUT --> DOTNET_BUILD["dotnet build<br/>csharp_core.dll"]

    style DL fill:#023e8a,stroke:#03045e,color:#caf0f8
    style CPP_OUT fill:#2a9d8f,stroke:#264653,color:#fff
    style CS_OUT fill:#e9c46a,stroke:#f4a261,color:#264653
```

### C++ Build (CMake + FetchContent)

```mermaid
flowchart LR
    CMAKE["CMakeLists.txt"] --> FETCH["FetchContent<br/>google/flatbuffers<br/>v24.3.25"]
    FETCH --> HEADERS["FlatBuffers C++<br/>header-only library"]
    CMAKE --> EXE["MemMapProducer.exe"]
    HEADERS --> EXE

    NOTE["NOMINMAX + WIN32_LEAN_AND_MEAN<br/>voorkomt Windows.h conflicten"]

    style FETCH fill:#023e8a,stroke:#03045e,color:#caf0f8
    style EXE fill:#2a9d8f,stroke:#264653,color:#fff
```

### C# Build (.NET 9)

```mermaid
flowchart LR
    CSPROJ["csharp_core.csproj<br/>net9.0-windows"] --> NUGET["NuGet: Google.FlatBuffers<br/>v24.3.25"]
    CSPROJ --> GEN["Generated/*.cs<br/>(van flatc)"]
    NUGET --> DLL["csharp_core.dll"]
    GEN --> DLL

    style DLL fill:#e9c46a,stroke:#f4a261,color:#264653
    style NUGET fill:#023e8a,stroke:#03045e,color:#caf0f8
```

**Versie-afstemming:** Zowel de C++ `FetchContent` als het C# NuGet-package en het `flatc`-binary zijn vastgepind op **v24.3.25** om serialisatie-incompatibiliteiten te voorkomen.

---

## 11. Vergelijking met SharedValueV3 (MemMap)

```mermaid
flowchart TB
    subgraph "V3: Single-Channel MemMap"
        direction TB
        PRODUCER_V3["C++ Producer"]
        SHARED_V3["P2C Shared Memory"]
        CONSUMER_V3["C# Consumer"]

        PRODUCER_V3 --> SHARED_V3
        SHARED_V3 --> CONSUMER_V3
    end

    subgraph "V4: Dual-Channel MemMap"
        direction TB
        HOST_V4["C++ Host"]
        P2C_V4["P2C_Map"]
        CLIENT_V4["C# Client"]
        C2P_V4["C2P_Map"]

        HOST_V4 -->|"Write P2C"| P2C_V4
        P2C_V4 -->|"Read P2C"| CLIENT_V4
        CLIENT_V4 -->|"Write C2P"| C2P_V4
        C2P_V4 -->|"Read C2P"| HOST_V4
    end

    style SHARED_V3 fill:#2d6a4f,stroke:#1b4332,color:#d8f3dc
    style P2C_V4 fill:#2d6a4f,stroke:#1b4332,color:#d8f3dc
    style C2P_V4 fill:#1d3557,stroke:#0b2545,color:#d8f3dc
```

| Aspect | SharedValueV3 (MemMap) | SharedValueV4 (MemMap) |
|---|---|---|
| **Transport** | Enkel MMF-kanaal (P2C) | Dubbel MMF-kanaal (P2C + C2P) |
| **Richting** | Producer → Consumer | Host ↔ Client bidirectioneel |
| **Startup synchronisatie** | Event-driven, zonder expliciete dual-ready handshake | Expliciete Ready Event handshake in beide richtingen |
| **Latency** | ~10-100 ns per read | ~10-100 ns per read en write in beide richtingen |
| **Schema** | FlatBuffers `.fbs` | FlatBuffers `.fbs` (zelfde compatibiliteitsmodel) |
| **Thread safety** | Named Mutex + Named Event | Per kanaal Named Mutex + Named Event + Ready Event |
| **Functionele scope** | Hoge snelheid éénrichtingsstream | Volledige request/response over shared memory |

---

## Gerelateerde Documentatie

- [README.md](README.md) — Introductie, projectstructuur en quickstart handleiding.
- [ARCHITECTURE.md](../ARCHITECTURE.md) — Hoofd architectuurdocument voor het gehele COM Server project.
- [README.md](../SharedValueV3_MemMap/README.md) — SharedValueV3 baseline memory-mapped architectuur.
