# Technical Architecture: ATLProjectcomserverExe

## Table of Contents

1. [System Overview](#1-system-overview)
2. [Layer Architecture](#2-layer-architecture)
3. [COM Server Lifecycle](#3-com-server-lifecycle)
4. [Component Model](#4-component-model)
5. [SharedValueV2 — The C++ Engine](#5-sharedvaluev2--the-c-engine)
6. [COM-to-C++ Bridge Layer](#6-com-to-c-bridge-layer)
7. [Cross-Process Communication (RPC Marshaling)](#7-cross-process-communication-rpc-marshaling)
8. [Observer & Event Architecture](#8-observer--event-architecture)
9. [Thread-Safety & Synchronization](#9-thread-safety--synchronization)
10. [Error Handling Pipeline](#10-error-handling-pipeline)
11. [.NET Interop & Late Binding](#11-net-interop--late-binding)
12. [Singleton & Lifetime Management](#12-singleton--lifetime-management)
13. [Design Patterns Overview](#13-design-patterns-overview)

---

## 1. System Overview

The project implements an **Out-of-Process COM Server** (EXE, `LocalServer32`) acting as a central singleton process for cross-process data sharing on Windows. Multiple independent clients communicate simultaneously with the same server via Windows RPC marshaling.

```mermaid
graph TB
    subgraph Clients["Client Processes"]
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

    SV -->|delegates| SVCore
    DP -->|delegates| DS
    SVCore --> EB
    DS --> EB
    SVCore --> LP
```

---

## 2. Layer Architecture

The system possesses four distinctly separated layers. Each layer has a clear responsibility and communicates exclusively with the directly adjacent layer.

```mermaid
graph LR
    subgraph L1["Layer 1: Client Applications"]
        direction TB
        C1["VBScript"]
        C2["C# .NET"]
        C3["Python"]
        C4["C++"]
    end

    subgraph L2["Layer 2: COM/RPC Transport"]
        direction TB
        IDL["IDL Interfaces<br/>(ISharedValue, IDatasetProxy)"]
        MARSH["Proxy/Stub<br/>Marshaling"]
        REG["Windows Registry<br/>(CLSID, ProgID, AppID)"]
    end

    subgraph L3["Layer 3: ATL COM Wrapper"]
        direction TB
        CSV["CSharedValue"]
        CDP["CDatasetProxy"]
        BRIDGE["EventBridge<br/>(COM ↔ C++)"]
        ERR["ComErrorHelper<br/>(Exception → HRESULT)"]
    end

    subgraph L4["Layer 4: C++20 Engine"]
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

| Layer | Responsibility | Technology |
|---|---|---|
| **Client Applications** | Consuming COM interfaces | VBScript, C#, Python, C++ |
| **COM/RPC Transport** | Interface definition, marshaling, registration | IDL/MIDL, Windows Registry, LRPC |
| **ATL COM Wrapper** | Type conversion, error translation, lifetime management | ATL, `CComVariant`, `CComBSTR`, `SAFEARRAY` |
| **C++20 Engine** | Business logic, thread safety, event handling | C++20 templates, `std::mutex`, `std::atomic` |

---

## 3. COM Server Lifecycle

The EXE server undergoes a specific lifecycle model that differs from the traditional DLL variant.

```mermaid
sequenceDiagram
    participant Client as Client (VBScript)
    participant SCM as Windows SCM
    participant EXE as ATLProjectcomserver.exe
    participant Module as CAtlExeModuleT
    participant SV as CSharedValue

    Note over Client,EXE: Phase 1: Startup
    Client->>SCM: CreateObject("ATLProjectcomserverExe.SharedValue")
    SCM->>SCM: Lookup CLSID in Registry
    SCM->>EXE: Start EXE process
    EXE->>Module: _tWinMain() → _AtlModule.Lock()
    Module->>Module: RegisterClassObjects()
    Module->>Module: WinMain() message loop

    Note over Client,EXE: Phase 2: Object Creation
    SCM->>Module: IClassFactory::CreateInstance
    Module->>SV: CSharedValue::FinalConstruct()
    SV->>SV: Create DatasetProxy singleton
    SV->>SV: Subscribe to SharedValueV2 core
    Module-->>SCM: ISharedValue* (marshaled)
    SCM-->>Client: Proxy pointer

    Note over Client,EXE: Phase 3: Utilization
    Client->>SCM: SetValue("data")
    SCM->>SV: ISharedValue::SetValue(VARIANT)
    SV->>SV: m_core.SetValue(CComVariant)
    SV-->>SCM: S_OK
    SCM-->>Client: S_OK

    Note over Client,EXE: Phase 4: Graceful Shutdown
    Client->>SCM: ShutdownServer()
    SCM->>SV: ISharedValue::ShutdownServer()
    SV->>SV: Clear DatasetProxy reference
    SV->>Module: _AtlModule.Unlock()
    Module->>Module: Message loop ends
    EXE->>EXE: Process exit
```

### Critical Shutdown Details

The EXE server does **not** terminate when the final client disconnects (ATL default behavior), because `_AtlModule.Lock()` inside `_tWinMain` retains an extra reference count. This prevents premature termination. Only upon an explicit `ShutdownServer()` call is:

1. The global `DatasetProxy` reference released (`m_core.SetValue(CComVariant())`)
2. The module lock removed (`_AtlModule.Unlock()`)
3. The Win32 message loop terminated

---

## 4. Component Model

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

    ISharedValue <|.. CSharedValue : implements
    IDatasetProxy <|.. CDatasetProxy : implements
    CSharedValue --> CDatasetProxy : "owns (server-side singleton)"
    CSharedValue --> ISharedValueCallback : "notifies observers"
    CSharedValue --> IEventCallback : "via SharedValueEventBridge"
    CDatasetProxy --> IEventCallback : "via ComEventBridge"

    note for CSharedValue "DECLARE_CLASSFACTORY_SINGLETON\nAll clients share the same instance"
```

---

## 5. SharedValueV2 — The C++ Engine

The engine is a header-only C++20 library featuring a template-based architecture. It is entirely independent of COM and ATL.

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

    SharedValue --> EventBus : contains
    SharedValue --> ISharedValueObserver : notifies
    DatasetStore --> EventBus : contains
    EventBus --> IEventListener : notifies

    SharedValue ..> LocalMutexPolicy : "Policy parameter"
    SharedValue ..> NullMutexPolicy : "Policy parameter"
    SharedValue ..> NamedSystemMutexPolicy : "Policy parameter"
```

### Template Instantiations in the COM Server

```cpp
// Within CSharedValue — thread-safe with local mutex
SharedValueV2::SharedValue<CComVariant, SharedValueV2::LocalMutexPolicy> m_core;

// Within CDatasetProxy — thread-safe dataset store
std::unique_ptr<SharedValueV2::DatasetStore<std::wstring>> m_store;
```

---

## 6. COM-to-C++ Bridge Layer

The bridge layer translates between COM types and C++ native types. There are two bridge classes acting as adapters.

```mermaid
graph LR
    subgraph COM["COM World"]
        VARIANT["VARIANT"]
        BSTR["BSTR"]
        SAFEARRAY["SAFEARRAY"]
        HRESULT["HRESULT"]
        IErrorInfo["IErrorInfo"]
        IEC["IEventCallback"]
    end

    subgraph Bridge["Bridge Layer"]
        direction TB
        CSV["CSharedValue"]
        CDP["CDatasetProxy"]
        CEH["ComErrorHelper"]
        SVB["SharedValueEventBridge"]
        CEB["ComEventBridge"]
    end

    subgraph CPP["C++ World"]
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

### ComErrorHelper — Exception Translation

The `COM_METHOD_BODY` macro catches C++ exceptions and translates them to COM-compatible `HRESULT` + `IErrorInfo`:

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

    E1 -->|"MAKE_HRESULT(SEVERITY_ERROR,<br/>FACILITY_ITF, ErrorCode)"| HR1["HRESULT + IErrorInfo<br/>(with description)"]
    E2 -->|"E_BOUNDS"| HR2["HRESULT + IErrorInfo"]
    E3 -->|"E_INVALIDARG"| HR3["HRESULT + IErrorInfo"]
    E4 -->|"E_FAIL"| HR4["HRESULT + IErrorInfo"]
    E5 -->|"E_UNEXPECTED"| HR5["HRESULT + L'Unknown error'"]

    HR1 --> CLIENT["Client receives<br/>COMException (C#)<br/>Err.Description (VBS)"]
    HR2 --> CLIENT
    HR3 --> CLIENT
    HR4 --> CLIENT
    HR5 --> CLIENT
```

---

## 7. Cross-Process Communication (RPC Marshaling)

As an Out-of-Process server, all method calls occur via **LRPC** (Local Remote Procedure Call) over Windows Named Pipes.

```mermaid
sequenceDiagram
    participant Client as Client Process
    participant ProxyDLL as Proxy DLL (in client)
    participant RPC as Windows RPC Runtime
    participant StubDLL as Stub DLL (in server)
    participant Server as COM Server EXE

    Note over Client,Server: IDatasetProxy::AddRow("key", "value")
    Client->>ProxyDLL: AddRow(BSTR, BSTR)
    ProxyDLL->>ProxyDLL: Serialize BSTR parameters
    ProxyDLL->>RPC: NDR-encoded buffer via Named Pipe
    RPC->>StubDLL: Receive buffer in server process
    StubDLL->>StubDLL: Deserialize BSTR parameters
    StubDLL->>Server: CDatasetProxy::AddRow(BSTR, BSTR)
    Server->>Server: m_store->AddRow(key, value)
    Server-->>StubDLL: HRESULT (S_OK)
    StubDLL-->>RPC: NDR-encoded response
    RPC-->>ProxyDLL: Response via Named Pipe
    ProxyDLL-->>Client: HRESULT (S_OK)
```

### How Proxy/Stub Marshaling Works

The MIDL compiler automatically generates proxy/stub code from the `.idl` files:

```mermaid
graph TD
    IDL["ATLProjectcomserver.idl<br/>(Interface definitions)"] -->|MIDL Compiler| GEN

    subgraph GEN["Generated Files"]
        IH["_i.h<br/>(Interface headers + CLSIDs)"]
        IC["_i.c<br/>(GUID definitions)"]
        PC["_p.c<br/>(Proxy/Stub code)"]
        TLB["*.tlb<br/>(Type Library)"]
    end

    PC -->|"Embedded within"| EXESERVER["EXE Server<br/>(self-registering)"]
    TLB -->|"Utilized by"| CLIENTS["Late-binding clients<br/>(IDispatch)"]
    IH -->|"Included by"| CPPCLIENTS["C++ Clients<br/>(early-binding)"]
```

---

## 8. Observer & Event Architecture

The system features two parallel observer mechanisms: the **legacy SharedValueCallback** system and the modern **EventBus** system.

```mermaid
graph TB
    subgraph Legacy["Legacy Observer (ISharedValueCallback)"]
        SV_OBS["CSharedValue<br/>implements ISharedValueObserver"]
        V2_CORE["SharedValueV2::SharedValue<br/>→ notifyValueChanged()"]
        CB_LIST["m_callbacks vector<br/>(CComAutoCriticalSection)"]
        ISVCB["Client's ISharedValueCallback<br/>→ OnValueChanged()"]

        V2_CORE -->|"copy observer list<br/>outside local lock"| SV_OBS
        SV_OBS --> CB_LIST
        CB_LIST -->|"iterate copy"| ISVCB
    end

    subgraph Modern["Modern EventBus (IEventCallback)"]
        EB_CORE["EventBus::Emit()"]
        SVB_BRIDGE["SharedValueEventBridge<br/>ComEventBridge"]
        IECB["Client's IEventCallback<br/>→ OnMutationEvent()"]

        EB_CORE -->|"copy listener list<br/>outside local lock"| SVB_BRIDGE
        SVB_BRIDGE -->|"convert MutationEvent<br/>→ COM BSTR parameters"| IECB
    end

    style Legacy fill:#3498db,color:#fff
    style Modern fill:#2ecc71,color:#fff
```

### Deadlock-Free Notification (Critical Pattern)

The notification mechanism has been explicitly designed to prevent deadlocks with slow or stalling clients:

```mermaid
sequenceDiagram
    participant Caller as SetValue() Caller
    participant Core as SharedValue Core
    participant Lock as m_mutex
    participant Copy as Local Copy
    participant Obs1 as Observer 1 (fast)
    participant Obs2 as Observer 2 (slow)

    Caller->>Core: SetValue(newVal)
    Core->>Lock: lock_guard LOCK
    Core->>Core: m_value = newVal
    Core->>Copy: observersCopy = m_observers
    Core->>Lock: ~lock_guard UNLOCK

    Note over Core,Obs2: Mutex is FREE — other threads<br/>can now call SetValue/GetValue
    Core->>Obs1: OnValueChanged(newVal)
    Obs1-->>Core: return (fast)
    Core->>Obs2: OnValueChanged(newVal)
    Note over Obs2: Slow processing...<br/>Server does NOT block
    Obs2-->>Core: return (slow, but no deadlock)
```

---

## 9. Thread-Safety & Synchronization

### Policy-Based Design for Lock Strategies

```mermaid
graph TD
    SV["SharedValue&lt;T, Policy&gt;"]

    subgraph Policies["Available Lock Policies"]
        LP["LocalMutexPolicy<br/>std::mutex<br/><i>Fast, single-process</i>"]
        NP["NullMutexPolicy<br/>No-op<br/><i>Zero overhead, single-thread</i>"]
        NSP["NamedSystemMutexPolicy<br/>Windows Named Mutex<br/><i>Cross-process synchronization</i>"]
    end

    SV -->|"template parameter"| LP
    SV -->|"template parameter"| NP
    SV -->|"template parameter"| NSP

    LP -->|"Used in"| PROD["ATLProjectcomserverExe<br/>(production)"]
    NP -->|"Used in"| TEST["Unit Tests<br/>(single-threaded)"]
    NSP -->|"Available for"| FUTURE["Cross-process Shared Memory<br/>(future)"]
```

### Monitor Pattern — Combined Data + Lock

Data is never accessible without an active lock. The `SharedValue<T, Policy>` class enforces this:

```mermaid
graph TD
    subgraph Anti["Anti-Pattern (dangerous)"]
        direction LR
        A1["Thread A reads value"] --> A2["Thread B writes value"]
        A2 --> A3["Race condition!"]
    end

    subgraph Monitor["Monitor Pattern (SharedValueV2)"]
        direction LR
        M1["lock_guard LOCK"] --> M2["Read/write m_value"]
        M2 --> M3["~lock_guard UNLOCK"]
    end

    subgraph Code["C++ Implementation"]
        C1["void SetValue(const T& newValue) {<br/>    {<br/>        std::lock_guard lock(m_mutex);<br/>        m_value = newValue;  // only here<br/>    }<br/>    notifyValueChanged(newValue);<br/>}"]
    end

    style Anti fill:#e74c3c,color:#fff
    style Monitor fill:#27ae60,color:#fff
```

### Synchronization Overview per Component

| Component | Lock Type | Scope | Protects |
|---|---|---|---|
| `SharedValue::m_mutex` | `LocalMutexPolicy` | `m_value`, `m_observers` | Shared state and observer list |
| `EventBus::m_mutex` | `LocalMutexPolicy` | `m_listeners` | Event listener registrations |
| `CSharedValue::m_csCallbacks` | `CComAutoCriticalSection` | `m_callbacks` | COM callback pointer list |
| `DatasetStore` (internal) | `LocalMutexPolicy` | Store data | CRUD operations on the dataset |
| `EventBus::m_sequenceCounter` | `std::atomic<uint64_t>` | Counter | Lock-free sequence numbering |

---

## 10. Error Handling Pipeline

Errors flow from the C++ engine through the COM layer to the client in their own language-specific formatting.

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
        EI["IErrorInfo<br/>(description string)"]
    end

    subgraph Clients["Client Languages"]
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

### Exception Hierarchy

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

C# clients leverage **late binding** via `IDispatch` to interact with the COM server without requiring compiler dependencies.

```mermaid
sequenceDiagram
    participant CS as C# (.NET 8)
    participant CLR as .NET CLR
    participant RCW as Runtime Callable Wrapper
    participant DISP as IDispatch (via RPC)
    participant COM as CDatasetProxy

    CS->>CLR: dynamic proxy = Activator.CreateInstance(...)
    CLR->>CLR: Type.GetTypeFromProgID("ATLProjectcomserverExe.DatasetProxy")
    CLR->>RCW: Create RCW wrapper
    RCW->>DISP: CoCreateInstance(CLSID_DatasetProxy)

    CS->>CLR: proxy.AddRow("key", "value")
    CLR->>RCW: Dynamic call via DLR
    RCW->>DISP: IDispatch::GetIDsOfNames("AddRow")
    DISP-->>RCW: DISPID
    RCW->>DISP: IDispatch::Invoke(DISPID, VARIANT[])
    DISP->>COM: CDatasetProxy::AddRow(BSTR, BSTR)
    COM-->>DISP: HRESULT
    DISP-->>RCW: HRESULT
    RCW-->>CLR: return / throw COMException

    Note over CS,COM: On HRESULT failure:<br/>RCW automatically translates<br/>HRESULT + IErrorInfo → COMException
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

    VT_BSTR -->|"automatic"| STR
    VT_I4 -->|"automatic"| INT
    VT_BOOL -->|"automatic"| BOOL
    VT_DISPATCH -->|"RCW wrapping"| OBJ
    VT_VARIANT -->|"boxing"| OBJ2
    VT_ARRAY -->|"SafeArray marshaling"| ARR
```

---

## 12. Singleton & Lifetime Management

`CSharedValue` is declared as a singleton via `DECLARE_CLASSFACTORY_SINGLETON`. This signifies that all clients — irrespective of their process — share the same instance.

```mermaid
graph TB
    subgraph Server["EXE Server Process"]
        CF["CComClassFactorySingleton"]
        SV["CSharedValue<br/>(1 instance)"]
        DP["CDatasetProxy<br/>(owned by SV)"]

        CF -->|"CreateInstance() → always the same"| SV
        SV -->|"FinalConstruct() makes"| DP
    end

    subgraph Client1["Client Process 1"]
        P1["Proxy → ISharedValue*"]
    end

    subgraph Client2["Client Process 2"]
        P2["Proxy → ISharedValue*"]
    end

    subgraph Client3["Client Process 3"]
        P3["Proxy → ISharedValue*"]
    end

    P1 -->|RPC| SV
    P2 -->|RPC| SV
    P3 -->|RPC| SV

    Note over Server: All proxies refer to<br/>the exact same CSharedValue
```

### ATL Lifetime References

```mermaid
graph TD
    WM["_tWinMain()"] -->|"Lock()"| MOD["CAtlExeModuleT<br/>m_nLockCnt = 1"]
    SV_FC["CSharedValue::FinalConstruct()"] -->|"AddRef on DatasetProxy"| MOD
    CLIENT["Client CoCreateInstance"] -->|"implicit Lock()"| MOD

    MOD -->|"m_nLockCnt > 0"| ALIVE["Server continues executing"]
    MOD -->|"m_nLockCnt == 0"| EXIT["Message loop halts → exit"]

    SD["ShutdownServer()"] -->|"Clear proxy + Unlock()"| MOD

    style ALIVE fill:#27ae60,color:#fff
    style EXIT fill:#e74c3c,color:#fff
```

---

## 13. Design Patterns Overview

```mermaid
mindmap
  root((ATLProjectcomserverExe<br/>Design Patterns))
    Creational
      Singleton
        CSharedValue via DECLARE_CLASSFACTORY_SINGLETON
        One instance shared by all clients
      Factory Method
        ATL ClassFactory constructs COM objects
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
        CSharedValue acting as facade over SharedValueV2 core
    Behavioral
      Observer
        ISharedValueObserver — legacy value changes
        IEventListener — modern EventBus
        Deadlock-free notification
      Strategy / Policy
        LockPolicies as template parameter
        LocalMutexPolicy / NullMutexPolicy / NamedSystemMutexPolicy
      Monitor
        Data + synchronization inextricably intertwined
        lock_guard scope blocks
      Template Method
        ATL FinalConstruct / FinalRelease lifecycle
    Concurrency
      Copy-then-Notify
        Copy observer list before notification
        Release Mutex prior to callbacks
      Lock-free Counters
        std::atomic for EventBus sequence IDs
```

| Pattern | Where Applied | Why |
|---|---|---|
| **Singleton** | `CSharedValue` | All clients must share the identical state |
| **Observer** | `ISharedValueObserver`, `EventBus` | Decoupled notifications upon state changes |
| **Strategy / Policy** | `SharedValue<T, MutexPolicy>` | Interchangeable lock strategies without code alterations |
| **Monitor** | `SharedValue`, `DatasetStore` | Data is inaccessible outside a locked scope |
| **Adapter** | `SharedValueEventBridge`, `ComEventBridge` | Translation between C++ events and COM callbacks |
| **Facade** | `CSharedValue`, `CDatasetProxy` | Streamlined COM interface atop a complex C++ engine |
| **Proxy** | COM Proxy/Stub, .NET RCW | Transparent cross-process communication |
| **Template Method** | ATL `FinalConstruct` / `FinalRelease` | Framework-driven object lifecycle |
| **Copy-then-Notify** | `notifyValueChanged()`, `EventBus::Emit()` | Deadlock prevention during observer notifications |

## Related Documentation

- [CHANGELOG_EN.md](CHANGELOG_EN.md) — Overview of all modifications and release notes.
- [README_EN.md](README_EN.md) — Primary documentation and genesis of the entire project.
- [INSTALL_EN.md](INSTALL_EN.md) — Global compilation and installation instructions.
- [README_EN.md](ATLProjectcomserverExe/README_EN.md) — User guide and overview for the EXE COM Server variant.
- [README_EN.md](SharedValueV2/README_EN.md) — Introduction and overview of the SharedValueV2 C++20 engine.
