# Technische Architectuur: ATLProjectcomserverExe (V2)

> **Documentatie Scope:** Dit document beschrijft specifiek de architectuur van **SharedValueV2**, de COM/RPC-gebaseerde Out-of-Process Server. 
> Voor de architectuur van de nieuwere Memory-Mapped generaties, zie:
> - [SharedValueV3 MemMap Architectuur](SharedValueV3_MemMap/ARCHITECTURE.md) (Unidirectioneel FlatBuffers)
> - [SharedValueV4 Architectuur](SharedValueV4/ARCHITECTURE_NL.md) (Bidirectioneel FlatBuffers)
> - [SharedValueV5 Architectuur](SharedValueV5/ARCHITECTURE_NL.md) (Dynamic Schema IPC)
## Inhoudsopgave

1. [Systeemoverzicht](#1-systeemoverzicht)
2. [Lagenarchitectuur](#2-lagenarchitectuur)
3. [COM Server Lifecycle](#3-com-server-lifecycle)
4. [Componentenmodel](#4-componentenmodel)
5. [SharedValueV2 — De C++ Engine](#5-sharedvaluev2--de-c-engine)
6. [COM-naar-C++ Bruglaag](#6-com-naar-c-bruglaag)
7. [Cross-Process Communicatie (RPC Marshaling)](#7-cross-process-communicatie-rpc-marshaling)
8. [Observer & Event Architectuur](#8-observer--event-architectuur)
9. [Thread-Safety & Synchronisatie](#9-thread-safety--synchronisatie)
10. [Error Handling Pipeline](#10-error-handling-pipeline)
11. [.NET Interop & Late Binding](#11-net-interop--late-binding)
12. [Singleton & Lifetime Management](#12-singleton--lifetime-management)
13. [Design Patterns Overzicht](#13-design-patterns-overzicht)

---

## 1. Systeemoverzicht

Het project implementeert een **Out-of-Process COM Server** (EXE, `LocalServer32`) die als centraal singleton-proces fungeert voor cross-process data-sharing op Windows. Meerdere onafhankelijke clients communiceren gelijktijdig met dezelfde server via Windows RPC marshaling.

```mermaid
graph TB
    subgraph Clients["Client Processen"]
        VBS["VBScript<br/>(cscript.exe)"]
        CS["C# .NET 8<br/>(dotnet.exe)"]
        CPP["C++ Tool<br/>(stop_server.exe)"]
        PY["Python<br/>(python.exe)"]
    end

    subgraph RPC["Windows COM Runtime"]
        SCM["Service Control<br/>Manager (SCM)"]
        PROXY["Proxy/Stub<br/>Marshaling"]
    end

    subgraph Server["ATLProjectcomserverExe.exe"]
        ATL["ATL COM Module<br/>(CAtlExeModuleT)"]
        SV["CSharedValue<br/>(Singleton)"]
        DP["CDatasetProxy"]
        MO["CMathOperations"]
    end

    subgraph Engine["SharedValueV2 (C++20 Core)"]
        SVCore["SharedValue&lt;T, Policy&gt;"]
        DS["DatasetStore&lt;T&gt;"]
        EB["EventBus&lt;Policy&gt;"]
        LP["LockPolicies"]
    end

    VBS -->|CreateObject| SCM
    CS -->|Activator.CreateInstance| SCM
    CPP -->|CoCreateInstance| SCM
    PY -->|win32com.client| SCM

    SCM -->|Start EXE / Route RPC| PROXY
    PROXY <-->|LRPC Channel| ATL

    ATL --> SV
    ATL --> DP
    ATL --> MO

    SV -->|delegeert| SVCore
    DP -->|delegeert| DS
    SVCore --> EB
    DS --> EB
    SVCore --> LP
```

---

## 2. Lagenarchitectuur

Het systeem kent vier distinct gescheiden lagen. Elke laag heeft een duidelijke verantwoordelijkheid en communiceert uitsluitend met de direct aangrenzende laag.

```mermaid
graph LR
    subgraph L1["Laag 1: Client Applications"]
        direction TB
        C1["VBScript"]
        C2["C# .NET"]
        C3["Python"]
        C4["C++"]
    end

    subgraph L2["Laag 2: COM/RPC Transport"]
        direction TB
        IDL["IDL Interfaces<br/>(ISharedValue, IDatasetProxy)"]
        MARSH["Proxy/Stub<br/>Marshaling"]
        REG["Windows Registry<br/>(CLSID, ProgID, AppID)"]
    end

    subgraph L3["Laag 3: ATL COM Wrapper"]
        direction TB
        CSV["CSharedValue"]
        CDP["CDatasetProxy"]
        BRIDGE["EventBridge<br/>(COM ↔ C++)"]
        ERR["ComErrorHelper<br/>(Exception → HRESULT)"]
    end

    subgraph L4["Laag 4: C++20 Engine"]
        direction TB
        SV2["SharedValue&lt;T,P&gt;"]
        DST["DatasetStore&lt;T&gt;"]
        EBUS["EventBus"]
        LOCK["LockPolicies"]
    end

    L1 -->|"IDispatch / vtable calls"| L2
    L2 -->|"LRPC over Named Pipes"| L3
    L3 -->|"Direct C++ calls"| L4

    style L1 fill:#4a90d9,color:#fff
    style L2 fill:#7b68ee,color:#fff
    style L3 fill:#e67e22,color:#fff
    style L4 fill:#27ae60,color:#fff
```

| Laag | Verantwoordelijkheid | Technologie |
|---|---|---|
| **Client Applications** | Consumeren van COM interfaces | VBScript, C#, Python, C++ |
| **COM/RPC Transport** | Interface definitie, marshaling, registratie | IDL/MIDL, Windows Registry, LRPC |
| **ATL COM Wrapper** | Type-conversie, error-vertaling, lifetime management | ATL, `CComVariant`, `CComBSTR`, `SAFEARRAY` |
| **C++20 Engine** | Business-logica, thread-safety, event handling | C++20 templates, `std::mutex`, `std::atomic` |

---

## 3. COM Server Lifecycle

De EXE server doorloopt een specifiek lifecycle-model dat verschilt van de traditionele DLL-variant.

```mermaid
sequenceDiagram
    participant Client as Client (VBScript)
    participant SCM as Windows SCM
    participant EXE as ATLProjectcomserver.exe
    participant Module as CAtlExeModuleT
    participant SV as CSharedValue

    Note over Client,EXE: Fase 1: Opstarten
    Client->>SCM: CreateObject("ATLProjectcomserverExe.SharedValue")
    SCM->>SCM: Lookup CLSID in Registry
    SCM->>EXE: Start EXE proces
    EXE->>Module: _tWinMain() → _AtlModule.Lock()
    Module->>Module: RegisterClassObjects()
    Module->>Module: WinMain() message loop

    Note over Client,EXE: Fase 2: Object Creatie
    SCM->>Module: IClassFactory::CreateInstance
    Module->>SV: CSharedValue::FinalConstruct()
    SV->>SV: Maak DatasetProxy singleton
    SV->>SV: Subscribe op SharedValueV2 core
    Module-->>SCM: ISharedValue* (marshaled)
    SCM-->>Client: Proxy pointer

    Note over Client,EXE: Fase 3: Gebruik
    Client->>SCM: SetValue("data")
    SCM->>SV: ISharedValue::SetValue(VARIANT)
    SV->>SV: m_core.SetValue(CComVariant)
    SV-->>SCM: S_OK
    SCM-->>Client: S_OK

    Note over Client,EXE: Fase 4: Graceful Shutdown
    Client->>SCM: ShutdownServer()
    SCM->>SV: ISharedValue::ShutdownServer()
    SV->>SV: Clear DatasetProxy reference
    SV->>Module: _AtlModule.Unlock()
    Module->>Module: Message loop eindigt
    EXE->>EXE: Process exit
```

### Kritieke Shutdown Details

De EXE server sluit **niet** af wanneer de laatste client disconneert (ATL standaardgedrag), omdat `_AtlModule.Lock()` in `_tWinMain` een extra reference count vasthoudt. Dit voorkomt voortijdig afsluiten. Pas bij een expliciete `ShutdownServer()` aanroep wordt:

1. De globale `DatasetProxy` reference vrijgegeven (`m_core.SetValue(CComVariant())`)
2. De module lock verwijderd (`_AtlModule.Unlock()`)
3. De Win32 message loop beëindigd

---

## 4. Componentenmodel

```mermaid
classDiagram
    class ISharedValue {
        <<COM Interface>>
        +SetValue(VARIANT) HRESULT
        +GetValue(VARIANT*) HRESULT
        +GetCurrentUTCDateTime(BSTR*) HRESULT
        +GetCurrentUserLogin(BSTR*) HRESULT
        +LockSharedValue(LONG) HRESULT
        +Unlock() HRESULT
        +Subscribe(ISharedValueCallback*) HRESULT
        +Unsubscribe(ISharedValueCallback*) HRESULT
        +ShutdownServer() HRESULT
        +RegisterEventCallback(IEventCallback*) HRESULT
        +UnregisterEventCallback(IEventCallback*) HRESULT
    }

    class IDatasetProxy {
        <<COM Interface>>
        +AddRow(BSTR, BSTR) HRESULT
        +GetRowData(BSTR, BSTR*) HRESULT
        +UpdateRow(BSTR, BSTR, VARIANT_BOOL*) HRESULT
        +RemoveRow(BSTR, VARIANT_BOOL*) HRESULT
        +Clear() HRESULT
        +GetRecordCount(LONG*) HRESULT
        +HasKey(BSTR, VARIANT_BOOL*) HRESULT
        +FetchPageKeys(LONG, LONG, VARIANT*) HRESULT
        +FetchPageData(LONG, LONG, VARIANT*) HRESULT
        +SetStorageMode(LONG) HRESULT
        +GetStorageMode(LONG*) HRESULT
    }

    class ISharedValueCallback {
        <<COM Interface>>
        +OnValueChanged(VARIANT) HRESULT
        +OnDateTimeChanged(BSTR) HRESULT
    }

    class IEventCallback {
        <<COM Interface>>
        +OnMutationEvent(LONG, BSTR, BSTR, BSTR, BSTR, BSTR, LONG) HRESULT
    }

    class CSharedValue {
        -m_core : SharedValue~CComVariant, LocalMutexPolicy~
        -m_csCallbacks : CComAutoCriticalSection
        -m_callbacks : vector~CComPtr~
        -m_eventBridge : SharedValueEventBridge
        +FinalConstruct() HRESULT
        +ShutdownServer() HRESULT
    }

    class CDatasetProxy {
        -m_store : unique_ptr~DatasetStore~
        -m_bridge : ComEventBridge
        +FinalConstruct() HRESULT
    }

    ISharedValue <|.. CSharedValue : implementeert
    IDatasetProxy <|.. CDatasetProxy : implementeert
    CSharedValue --> CDatasetProxy : "bezit (server-side singleton)"
    CSharedValue --> ISharedValueCallback : "notifieert observers"
    CSharedValue --> IEventCallback : "via SharedValueEventBridge"
    CDatasetProxy --> IEventCallback : "via ComEventBridge"

    note for CSharedValue "DECLARE_CLASSFACTORY_SINGLETON\nAlle clients delen dezelfde instantie"
```

---

## 5. SharedValueV2 — De C++ Engine

De engine is een header-only C++20 library met template-gebaseerde architectuur. Ze is volledig onafhankelijk van COM en ATL.

```mermaid
classDiagram
    class SharedValue~T_MutexPolicy~ {
        -m_value : T
        -m_mutex : MutexPolicy
        -m_observers : vector~ISharedValueObserver*~
        -m_eventBus : EventBus~MutexPolicy~
        +SetValue(T) void
        +GetValue() T
        +LockSharedValue() bool
        +LockSharedValueTimeout(ulong) bool
        +Unlock() void
        +Subscribe(ISharedValueObserver*) void
        +Unsubscribe(ISharedValueObserver*) void
        +GetEventBus() EventBus&
        -notifyValueChanged(T) void
        -notifyDateTimeChanged(wstring) void
    }

    class DatasetStore~T~ {
        -m_engine : unique_ptr~IStorageEngine~
        -m_eventBus : EventBus
        +AddRow(wstring, T) void
        +GetRowData(wstring) T
        +UpdateRow(wstring, T) bool
        +RemoveRow(wstring) bool
        +FetchPageKeys(int, int) vector
        +SetStorageMode(int) void
        +GetEventBus() EventBus&
    }

    class EventBus~MutexPolicy~ {
        -m_listeners : vector~IEventListener*~
        -m_mutex : MutexPolicy
        -m_sequenceCounter : atomic~uint64_t~
        +Subscribe(IEventListener*) void
        +Unsubscribe(IEventListener*) void
        +Emit(MutationEvent) void
        +Emit(EventType, ...) void
    }

    class ISharedValueObserver~T~ {
        <<interface>>
        +OnValueChanged(T) void*
        +OnDateTimeChanged(wstring) void*
    }

    class IEventListener {
        <<interface>>
        +OnEvent(MutationEvent) void*
    }

    class LocalMutexPolicy {
        -m_mutex : std::mutex
        +lock() void
        +unlock() void
        +try_lock() bool
    }

    class NullMutexPolicy {
        +lock() void
        +unlock() void
        +try_lock() bool
    }

    class NamedSystemMutexPolicy {
        -m_hMutex : HANDLE
        +lock() void
        +unlock() void
        +try_lock_for(DWORD) bool
    }

    SharedValue --> EventBus : bevat
    SharedValue --> ISharedValueObserver : notifieert
    DatasetStore --> EventBus : bevat
    EventBus --> IEventListener : notifieert

    SharedValue ..> LocalMutexPolicy : "Policy parameter"
    SharedValue ..> NullMutexPolicy : "Policy parameter"
    SharedValue ..> NamedSystemMutexPolicy : "Policy parameter"
```

### Template Instantiaties in de COM Server

```cpp
// In CSharedValue — thread-safe met local mutex
SharedValueV2::SharedValue<CComVariant, SharedValueV2::LocalMutexPolicy> m_core;

// In CDatasetProxy — thread-safe dataset store
std::unique_ptr<SharedValueV2::DatasetStore<std::wstring>> m_store;
```

---

## 6. COM-naar-C++ Bruglaag

De bruglaag vertaalt tussen COM-typen en C++ native types. Er zijn twee bridge-klassen die als adapters fungeren.

```mermaid
graph LR
    subgraph COM["COM Wereld"]
        VARIANT["VARIANT"]
        BSTR["BSTR"]
        SAFEARRAY["SAFEARRAY"]
        HRESULT["HRESULT"]
        IErrorInfo["IErrorInfo"]
        IEC["IEventCallback"]
    end

    subgraph Bridge["Bruglaag"]
        direction TB
        CSV["CSharedValue"]
        CDP["CDatasetProxy"]
        CEH["ComErrorHelper"]
        SVB["SharedValueEventBridge"]
        CEB["ComEventBridge"]
    end

    subgraph CPP["C++ Wereld"]
        CCV["CComVariant"]
        WSTR["std::wstring"]
        VEC["std::vector"]
        EXC["SharedValueException"]
        ME["MutationEvent"]
    end

    VARIANT -->|"CComVariant::Copy()"| CCV
    BSTR -->|"CComBSTR → c_str()"| WSTR
    EXC -->|"COM_METHOD_BODY"| HRESULT
    EXC -->|"AtlReportError()"| IErrorInfo
    VEC -->|"SafeArrayCreate()"| SAFEARRAY
    ME -->|"Bridge::OnEvent()"| IEC

    style Bridge fill:#e67e22,color:#fff
```

### ComErrorHelper — Exception Vertaling

De `COM_METHOD_BODY` macro vangt C++ exceptions en vertaalt ze naar COM-compatibele `HRESULT` + `IErrorInfo`:

```mermaid
graph TD
    CPP_CODE["C++ Code in COM Method"] -->|throws| CATCH

    subgraph CATCH["COM_METHOD_BODY Exception Handler"]
        E1["SharedValueException"]
        E2["std::out_of_range"]
        E3["std::invalid_argument"]
        E4["std::exception"]
        E5["... (unknown)"]
    end

    E1 -->|"MAKE_HRESULT(SEVERITY_ERROR,<br/>FACILITY_ITF, ErrorCode)"| HR1["HRESULT + IErrorInfo<br/>(met beschrijving)"]
    E2 -->|"E_BOUNDS"| HR2["HRESULT + IErrorInfo"]
    E3 -->|"E_INVALIDARG"| HR3["HRESULT + IErrorInfo"]
    E4 -->|"E_FAIL"| HR4["HRESULT + IErrorInfo"]
    E5 -->|"E_UNEXPECTED"| HR5["HRESULT + L'Unknown error'"]

    HR1 --> CLIENT["Client ontvangt<br/>COMException (C#)<br/>Err.Description (VBS)"]
    HR2 --> CLIENT
    HR3 --> CLIENT
    HR4 --> CLIENT
    HR5 --> CLIENT
```

---

## 7. Cross-Process Communicatie (RPC Marshaling)

Als Out-of-Process server vinden alle method calls plaats via **LRPC** (Local Remote Procedure Call) over Windows Named Pipes.

```mermaid
sequenceDiagram
    participant Client as Client Proces
    participant ProxyDLL as Proxy DLL (in client)
    participant RPC as Windows RPC Runtime
    participant StubDLL as Stub DLL (in server)
    participant Server as COM Server EXE

    Note over Client,Server: IDatasetProxy::AddRow("key", "value")
    Client->>ProxyDLL: AddRow(BSTR, BSTR)
    ProxyDLL->>ProxyDLL: Serialiseer BSTR parameters
    ProxyDLL->>RPC: NDR-encoded buffer via Named Pipe
    RPC->>StubDLL: Ontvang buffer in server proces
    StubDLL->>StubDLL: Deserialiseer BSTR parameters
    StubDLL->>Server: CDatasetProxy::AddRow(BSTR, BSTR)
    Server->>Server: m_store->AddRow(key, value)
    Server-->>StubDLL: HRESULT (S_OK)
    StubDLL-->>RPC: NDR-encoded response
    RPC-->>ProxyDLL: Response via Named Pipe
    ProxyDLL-->>Client: HRESULT (S_OK)
```

### Hoe Proxy/Stub Marshaling Werkt

De MIDL compiler genereert automatisch proxy/stub code uit de `.idl` bestanden:

```mermaid
graph TD
    IDL["ATLProjectcomserver.idl<br/>(Interface definities)"] -->|MIDL Compiler| GEN

    subgraph GEN["Gegenereerde Bestanden"]
        IH["_i.h<br/>(Interface headers + CLSIDs)"]
        IC["_i.c<br/>(GUID definities)"]
        PC["_p.c<br/>(Proxy/Stub code)"]
        TLB["*.tlb<br/>(Type Library)"]
    end

    PC -->|"Ingebed in"| EXESERVER["EXE Server<br/>(self-registering)"]
    TLB -->|"Gebruikt door"| CLIENTS["Late-binding clients<br/>(IDispatch)"]
    IH -->|"Geïncludeerd door"| CPPCLIENTS["C++ Clients<br/>(early-binding)"]
```

---

## 8. Observer & Event Architectuur

Het systeem kent twee parallelle observer-mechanismen: het **legacy SharedValueCallback** systeem en het moderne **EventBus** systeem.

```mermaid
graph TB
    subgraph Legacy["Legacy Observer (ISharedValueCallback)"]
        SV_OBS["CSharedValue<br/>implementeert ISharedValueObserver"]
        V2_CORE["SharedValueV2::SharedValue<br/>→ notifyValueChanged()"]
        CB_LIST["m_callbacks vector<br/>(CComAutoCriticalSection)"]
        ISVCB["Client's ISharedValueCallback<br/>→ OnValueChanged()"]

        V2_CORE -->|"kopieer observer-lijst<br/>buiten lock"| SV_OBS
        SV_OBS --> CB_LIST
        CB_LIST -->|"itereer kopie"| ISVCB
    end

    subgraph Modern["Modern EventBus (IEventCallback)"]
        EB_CORE["EventBus::Emit()"]
        SVB_BRIDGE["SharedValueEventBridge<br/>ComEventBridge"]
        IECB["Client's IEventCallback<br/>→ OnMutationEvent()"]

        EB_CORE -->|"kopieer listener-lijst<br/>buiten lock"| SVB_BRIDGE
        SVB_BRIDGE -->|"converteer MutationEvent<br/>→ COM BSTR parameters"| IECB
    end

    style Legacy fill:#3498db,color:#fff
    style Modern fill:#2ecc71,color:#fff
```

### Deadlock-Free Notificatie (Kritiek Patroon)

Het notificatie-mechanisme is expliciet ontworpen om deadlocks te voorkomen bij trage of vastlopende clients:

```mermaid
sequenceDiagram
    participant Caller as SetValue() Caller
    participant Core as SharedValue Core
    participant Lock as m_mutex
    participant Copy as Lokale Kopie
    participant Obs1 as Observer 1 (snel)
    participant Obs2 as Observer 2 (traag)

    Caller->>Core: SetValue(newVal)
    Core->>Lock: lock_guard LOCK
    Core->>Core: m_value = newVal
    Core->>Copy: observersCopy = m_observers
    Core->>Lock: ~lock_guard UNLOCK

    Note over Core,Obs2: Mutex is VRIJ — andere threads<br/>kunnen nu SetValue/GetValue aanroepen

    Core->>Obs1: OnValueChanged(newVal)
    Obs1-->>Core: return (snel)
    Core->>Obs2: OnValueChanged(newVal)
    Note over Obs2: Langzame verwerking...<br/>Server blokkeert NIET
    Obs2-->>Core: return (traag, maar geen deadlock)
```

---

## 9. Thread-Safety & Synchronisatie

### Policy-Based Design voor Lock Strategieën

```mermaid
graph TD
    SV["SharedValue&lt;T, Policy&gt;"]

    subgraph Policies["Beschikbare Lock Policies"]
        LP["LocalMutexPolicy<br/>std::mutex<br/><i>Snel, single-process</i>"]
        NP["NullMutexPolicy<br/>No-op<br/><i>Zero overhead, single-thread</i>"]
        NSP["NamedSystemMutexPolicy<br/>Windows Named Mutex<br/><i>Cross-process synchronisatie</i>"]
    end

    SV -->|"template parameter"| LP
    SV -->|"template parameter"| NP
    SV -->|"template parameter"| NSP

    LP -->|"Gebruikt in"| PROD["ATLProjectcomserverExe<br/>(productie)"]
    NP -->|"Gebruikt in"| TEST["Unit Tests<br/>(single-threaded)"]
    NSP -->|"Beschikbaar voor"| FUTURE["Cross-process Shared Memory<br/>(toekomstig)"]
```

### Monitor Pattern — Gecombineerde Data + Lock

Data is nooit toegankelijk zonder een actieve lock. De `SharedValue<T, Policy>` klasse dwingt dit af:

```mermaid
graph TD
    subgraph Anti["Anti-Pattern (gevaarlijk)"]
        direction LR
        A1["Thread A leest value"] --> A2["Thread B schrijft value"]
        A2 --> A3["Race condition!"]
    end

    subgraph Monitor["Monitor Pattern (SharedValueV2)"]
        direction LR
        M1["lock_guard LOCK"] --> M2["Lees/schrijf m_value"]
        M2 --> M3["~lock_guard UNLOCK"]
    end

    subgraph Code["C++ Implementatie"]
        C1["void SetValue(const T& newValue) {<br/>    {<br/>        std::lock_guard lock(m_mutex);<br/>        m_value = newValue;  // alleen hier<br/>    }<br/>    notifyValueChanged(newValue);<br/>}"]
    end

    style Anti fill:#e74c3c,color:#fff
    style Monitor fill:#27ae60,color:#fff
```

### Synchronisatieoverzicht per Component

| Component | Lock Type | Scope | Beschermt |
|---|---|---|---|
| `SharedValue::m_mutex` | `LocalMutexPolicy` | `m_value`, `m_observers` | Gedeelde state en observer-lijst |
| `EventBus::m_mutex` | `LocalMutexPolicy` | `m_listeners` | Event listener registratie |
| `CSharedValue::m_csCallbacks` | `CComAutoCriticalSection` | `m_callbacks` | COM callback pointer-lijst |
| `DatasetStore` (intern) | `LocalMutexPolicy` | Store data | CRUD operaties op de dataset |
| `EventBus::m_sequenceCounter` | `std::atomic<uint64_t>` | Counter | Lock-vrije sequence nummering |

---

## 10. Error Handling Pipeline

Fouten stromen van de C++ engine door de COM-laag naar de client in hun eigen taal-specifieke formaat.

```mermaid
graph LR
    subgraph Engine["C++ Engine"]
        KNF["KeyNotFoundException<br/>code: 100"]
        DKE["DuplicateKeyException<br/>code: 101"]
        SME["StoreModeException<br/>code: 102"]
        LTE["SharedValueException<br/>code: 200 (LockTimeout)"]
    end

    subgraph ATL["ATL COM Wrapper"]
        CMB["COM_METHOD_BODY<br/>catch → AtlReportError()"]
    end

    subgraph COM["COM Runtime"]
        HR["HRESULT<br/>MAKE_HRESULT(1, 4, code)"]
        EI["IErrorInfo<br/>(beschrijving string)"]
    end

    subgraph Clients["Client Talen"]
        CSHARP["C#: COMException<br/>.Message + .HResult"]
        VBS_ERR["VBScript: Err.Number<br/>+ Err.Description"]
        CPP_HR["C++: HRESULT check<br/>+ IErrorInfo query"]
    end

    KNF --> CMB
    DKE --> CMB
    SME --> CMB
    LTE --> CMB
    CMB --> HR
    CMB --> EI
    HR --> CSHARP
    HR --> VBS_ERR
    HR --> CPP_HR
    EI --> CSHARP
    EI --> VBS_ERR
    EI --> CPP_HR
```

### Exception Hiërarchie

```mermaid
classDiagram
    class std_runtime_error {
        +what() const char*
    }

    class SharedValueException {
        +code : ErrorCode
        +SharedValueException(ErrorCode, string)
    }

    class KeyNotFoundException {
        +KeyNotFoundException(wstring key)
    }

    class DuplicateKeyException {
        +DuplicateKeyException(wstring key)
    }

    class StoreModeException {
        +StoreModeException()
    }

    class InvalidStorageModeException {
        +InvalidStorageModeException(int mode)
    }

    std_runtime_error <|-- SharedValueException
    SharedValueException <|-- KeyNotFoundException
    SharedValueException <|-- DuplicateKeyException
    SharedValueException <|-- StoreModeException
    SharedValueException <|-- InvalidStorageModeException
```

---

## 11. .NET Interop & Late Binding

C# clients gebruiken **late binding** via `IDispatch` om de COM server aan te spreken zonder compilatie-afhankelijkheid.

```mermaid
sequenceDiagram
    participant CS as C# (.NET 8)
    participant CLR as .NET CLR
    participant RCW as Runtime Callable Wrapper
    participant DISP as IDispatch (via RPC)
    participant COM as CDatasetProxy

    CS->>CLR: dynamic proxy = Activator.CreateInstance(...)
    CLR->>CLR: Type.GetTypeFromProgID("ATLProjectcomserverExe.DatasetProxy")
    CLR->>RCW: Maak RCW wrapper aan
    RCW->>DISP: CoCreateInstance(CLSID_DatasetProxy)

    CS->>CLR: proxy.AddRow("key", "value")
    CLR->>RCW: Dynamische aanroep via DLR
    RCW->>DISP: IDispatch::GetIDsOfNames("AddRow")
    DISP-->>RCW: DISPID
    RCW->>DISP: IDispatch::Invoke(DISPID, VARIANT[])
    DISP->>COM: CDatasetProxy::AddRow(BSTR, BSTR)
    COM-->>DISP: HRESULT
    DISP-->>RCW: HRESULT
    RCW-->>CLR: return / throw COMException

    Note over CS,COM: Bij HRESULT failure:<br/>RCW vertaalt automatisch<br/>HRESULT + IErrorInfo → COMException
```

### Type Mapping: COM ↔ .NET

```mermaid
graph LR
    subgraph COM_Types["COM Types"]
        VT_BSTR["BSTR"]
        VT_I4["LONG (VT_I4)"]
        VT_BOOL["VARIANT_BOOL"]
        VT_DISPATCH["IDispatch*"]
        VT_VARIANT["VARIANT"]
        VT_ARRAY["SAFEARRAY"]
    end

    subgraph NET_Types[".NET Types"]
        STR["System.String"]
        INT["System.Int32"]
        BOOL["System.Boolean"]
        OBJ["System.Object (dynamic)"]
        OBJ2["System.Object"]
        ARR["System.Array"]
    end

    VT_BSTR -->|"automatisch"| STR
    VT_I4 -->|"automatisch"| INT
    VT_BOOL -->|"automatisch"| BOOL
    VT_DISPATCH -->|"RCW wrapping"| OBJ
    VT_VARIANT -->|"boxing"| OBJ2
    VT_ARRAY -->|"SafeArray marshaling"| ARR
```

---

## 12. Singleton & Lifetime Management

`CSharedValue` is gedeclareerd als singleton via `DECLARE_CLASSFACTORY_SINGLETON`. Dit betekent dat alle clients — ongeacht proces — dezelfde instantie delen.

```mermaid
graph TB
    subgraph Server["EXE Server Proces"]
        CF["CComClassFactorySingleton"]
        SV["CSharedValue<br/>(1 instantie)"]
        DP["CDatasetProxy<br/>(eigendom van SV)"]

        CF -->|"CreateInstance() → altijd dezelfde"| SV
        SV -->|"FinalConstruct() maakt"| DP
    end

    subgraph Client1["Client Proces 1"]
        P1["Proxy → ISharedValue*"]
    end

    subgraph Client2["Client Proces 2"]
        P2["Proxy → ISharedValue*"]
    end

    subgraph Client3["Client Proces 3"]
        P3["Proxy → ISharedValue*"]
    end

    P1 -->|RPC| SV
    P2 -->|RPC| SV
    P3 -->|RPC| SV

    Note over Server: Alle proxies verwijzen naar<br/>exact dezelfde CSharedValue
```

### ATL Lifetime Referenties

```mermaid
graph TD
    WM["_tWinMain()"] -->|"Lock()"| MOD["CAtlExeModuleT<br/>m_nLockCnt = 1"]
    SV_FC["CSharedValue::FinalConstruct()"] -->|"AddRef op DatasetProxy"| MOD
    CLIENT["Client CoCreateInstance"] -->|"impliciet Lock()"| MOD

    MOD -->|"m_nLockCnt > 0"| ALIVE["Server blijft draaien"]
    MOD -->|"m_nLockCnt == 0"| EXIT["Message loop stopt → exit"]

    SD["ShutdownServer()"] -->|"Clear proxy + Unlock()"| MOD

    style ALIVE fill:#27ae60,color:#fff
    style EXIT fill:#e74c3c,color:#fff
```

---

## 13. Design Patterns Overzicht

```mermaid
mindmap
  root((ATLProjectcomserverExe<br/>Design Patterns))
    Creational
      Singleton
        CSharedValue via DECLARE_CLASSFACTORY_SINGLETON
        Eén instantie gedeeld door alle clients
      Factory Method
        ATL ClassFactory maakt COM objecten
        IClassFactory::CreateInstance
    Structural
      Adapter / Bridge
        SharedValueEventBridge
        ComEventBridge
        COM types ↔ C++ types
      Proxy
        COM Proxy/Stub marshaling
        .NET Runtime Callable Wrapper
      Facade
        CSharedValue als facade over SharedValueV2 core
    Behavioral
      Observer
        ISharedValueObserver — legacy value changes
        IEventListener — modern EventBus
        Deadlock-free notificatie
      Strategy / Policy
        LockPolicies als template parameter
        LocalMutexPolicy / NullMutexPolicy / NamedSystemMutexPolicy
      Monitor
        Data + synchronisatie onlosmakelijk gekoppeld
        lock_guard scope blocks
      Template Method
        ATL FinalConstruct / FinalRelease lifecycle
    Concurrency
      Copy-then-Notify
        Observer-lijst kopiëren vóór notificatie
        Mutex vrijgeven vóór callbacks
      Lock-free Counters
        std::atomic voor EventBus sequence IDs
```

| Pattern | Waar Toegepast | Waarom |
|---|---|---|
| **Singleton** | `CSharedValue` | Alle clients moeten dezelfde state delen |
| **Observer** | `ISharedValueObserver`, `EventBus` | Losgekoppelde notificaties bij state-wijzigingen |
| **Strategy / Policy** | `SharedValue<T, MutexPolicy>` | Verwisselbare lock-strategieën zonder code-wijziging |
| **Monitor** | `SharedValue`, `DatasetStore` | Data is ontoegankelijk buiten een locked scope |
| **Adapter** | `SharedValueEventBridge`, `ComEventBridge` | Vertaling tussen C++ events en COM callbacks |
| **Facade** | `CSharedValue`, `CDatasetProxy` | Simpele COM interface over complexe C++ engine |
| **Proxy** | COM Proxy/Stub, .NET RCW | Transparante cross-process communicatie |
| **Template Method** | ATL `FinalConstruct` / `FinalRelease` | Framework-gestuurde object lifecycle |
| **Copy-then-Notify** | `notifyValueChanged()`, `EventBus::Emit()` | Deadlock-preventie bij observer notificaties |


## Gerelateerde Documentatie

- [CHANGELOG.md](CHANGELOG.md) — Overzicht van alle wijzigingen en release notes.
- [README.md](README.md) — Hoofddocumentatie en startpunt van het gehele project.
- [INSTALL.md](INSTALL.md) — Globale bouw- en installatie-instructies.
- [README.md](ATLProjectcomserverExe/README.md) — Gebruikershandleiding en overzicht van de EXE COM Server variant.
- [README.md](SharedValueV2/README.md) — Introductie en overzicht van de SharedValueV2 C++20 engine.
