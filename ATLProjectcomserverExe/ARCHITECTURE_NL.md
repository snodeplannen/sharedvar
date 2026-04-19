# Technische Architectuur: ATLProjectcomserverExe

## Inhoudsopgave

1. [Systeemoverzicht](#1-systeemoverzicht)
2. [Lagenarchitectuur](#2-lagenarchitectuur)
3. [COM Fundamentals](#3-com-fundamentals)
4. [ATL Framework Internals](#4-atl-framework-internals)
5. [EXE Server Lifecycle](#5-exe-server-lifecycle)
6. [COM Object Model](#6-com-object-model)
7. [RPC Marshaling & Proxy/Stub](#7-rpc-marshaling--proxystub)
8. [Singleton Architectuur](#8-singleton-architectuur)
9. [Observer & Event Architectuur](#9-observer--event-architectuur)
10. [Error Handling Pipeline](#10-error-handling-pipeline)
11. [Type Conversie — COM ↔ C++](#11-type-conversie--com--c)
12. [Client Interop Strategieën](#12-client-interop-strategieën)
13. [SharedValueV2 Engine Integration](#13-sharedvaluev2-engine-integration)
14. [Windows Registry Model](#14-windows-registry-model)
15. [Design Patterns Samenvatting](#15-design-patterns-samenvatting)

---

## 1. Systeemoverzicht

De ATLProjectcomserverExe is een **Out-of-Process COM Server** (LocalServer32) die als een zelfstandig Windows-proces draait. Clients communiceren via **LRPC** (Lightweight Remote Procedure Call) over Named Pipes — dezelfde techniek als DCOM, maar lokaal.

```mermaid
graph TB
    subgraph Clients["Client Processen (elk eigen geheugenruimte)"]
        VBS["cscript.exe<br/>VBScript"]
        CS["dotnet.exe<br/>C# .NET 8"]
        CPP["stop_server.exe<br/>C++ CLI"]
        PY["python.exe<br/>win32com"]
    end

    subgraph Windows["Windows COM Infrastructure"]
        SCM["Service Control Manager<br/>(SCM / rpcss.exe)"]
        OA["OLE Automation<br/>(oleaut32.dll)"]
        LRPC["LRPC Transport<br/>(Named Pipe)"]
    end

    subgraph Server["ATLProjectcomserver.exe (één proces)"]
        MODULE["CAtlExeModuleT<br/>(message loop)"]
        
        subgraph Objects["COM Objecten"]
            SV["CSharedValue<br/><i>Singleton</i>"]
            DP["CDatasetProxy<br/><i>Server-owned</i>"]
            MO["CMathOperations<br/><i>Stateless</i>"]
        end

        subgraph Engine["SharedValueV2 C++20 Core"]
            SVC["SharedValue&lt;CComVariant&gt;"]
            DSC["DatasetStore&lt;wstring&gt;"]
            EB["EventBus"]
        end
    end

    VBS -->|"CreateObject(ProgID)"| SCM
    CS -->|"Activator.CreateInstance"| SCM
    CPP -->|"CoCreateInstance(CLSID)"| SCM
    PY -->|"Dispatch(ProgID)"| SCM

    SCM -->|"Start EXE als nodig"| MODULE
    SCM <-->|"LRPC over Named Pipe"| LRPC
    LRPC <-->|"NDR marshaled calls"| MODULE

    MODULE --> SV
    MODULE --> DP
    MODULE --> MO

    SV --> SVC
    DP --> DSC
    SVC --> EB
    DSC --> EB

    style Clients fill:#3498db,color:#fff
    style Windows fill:#9b59b6,color:#fff
    style Server fill:#2c3e50,color:#fff
    style Engine fill:#27ae60,color:#fff
```

---

## 2. Lagenarchitectuur

```mermaid
graph TD
    subgraph L1["Laag 1 — Client"]
        direction LR
        L1A["VBScript<br/>CreateObject()"]
        L1B["C# .NET<br/>Activator / dynamic"]
        L1C["C++<br/>CoCreateInstance"]
    end

    subgraph L2["Laag 2 — COM Transport"]
        direction LR
        L2A["IDispatch vtable"]
        L2B["Proxy/Stub DLL"]
        L2C["LRPC Named Pipe"]
        L2D["NDR Serialisatie"]
    end

    subgraph L3["Laag 3 — ATL COM Wrapper"]
        direction LR
        L3A["CSharedValue<br/>CDatasetProxy<br/>CMathOperations"]
        L3B["ComErrorHelper<br/>Exception→HRESULT"]
        L3C["EventBridge<br/>C++→COM"]
    end

    subgraph L4["Laag 4 — C++20 Engine"]
        direction LR
        L4A["SharedValue&lt;T,P&gt;"]
        L4B["DatasetStore&lt;T,P&gt;"]
        L4C["EventBus, LockPolicies"]
    end

    L1 --> L2
    L2 --> L3
    L3 --> L4

    style L1 fill:#3498db,color:#fff
    style L2 fill:#9b59b6,color:#fff
    style L3 fill:#e67e22,color:#fff
    style L4 fill:#27ae60,color:#fff
```

| Laag | Verantwoordelijkheid | Technologie | Bestanden |
|---|---|---|---|
| **Client** | Interface consumeren | VBScript, C#, Python, C++ | *(extern)* |
| **COM Transport** | Interface definitie, marshaling, registratie, lifetime | IDL/MIDL, Windows Registry, LRPC | `*.idl`, `*_p.c`, `*.rgs` |
| **ATL COM Wrapper** | Type-conversie, error vertaling, observer bridging | ATL 14, `CComVariant`, `CComBSTR` | `SharedValue.h/cpp`, `DatasetProxy.h/cpp` |
| **C++20 Engine** | Business logica, thread-safety, event handling | C++20 templates, `std::mutex` | `SharedValueV2/include/*.hpp` |

---

## 3. COM Fundamentals

### Wat is COM?

**Component Object Model** is een binaire interface standaard van Microsoft (1993) die taal-onafhankelijke object-communicatie mogelijk maakt. Kernprincipes:

```mermaid
graph LR
    subgraph Principes["COM Kernprincipes"]
        BI["Binary Interface<br/><i>Taal-onafhankelijke vtable</i>"]
        RC["Reference Counting<br/><i>AddRef / Release</i>"]
        QI["QueryInterface<br/><i>Interface navigatie</i>"]
        LOC["Location Transparency<br/><i>In-proc / Out-of-proc / Remote</i>"]
    end
```

### IUnknown — De Basis van Alles

Elk COM object implementeert `IUnknown`:

```cpp
interface IUnknown {
    HRESULT QueryInterface(REFIID riid, void** ppvObject);  // "Ondersteun je interface X?"
    ULONG AddRef();     // Reference count +1
    ULONG Release();    // Reference count -1, delete bij 0
};
```

```mermaid
classDiagram
    class IUnknown {
        <<interface>>
        +QueryInterface(REFIID, void**) HRESULT
        +AddRef() ULONG
        +Release() ULONG
    }

    class IDispatch {
        <<interface>>
        +GetTypeInfoCount(UINT*) HRESULT
        +GetTypeInfo(UINT, LCID, ITypeInfo**) HRESULT
        +GetIDsOfNames(REFIID, LPOLESTR*, UINT, LCID, DISPID*) HRESULT
        +Invoke(DISPID, REFIID, LCID, WORD, DISPPARAMS*, VARIANT*, EXCEPINFO*, UINT*) HRESULT
    }

    class ISupportErrorInfo {
        <<interface>>
        +InterfaceSupportsErrorInfo(REFIID) HRESULT
    }

    class ISharedValue {
        <<custom interface>>
        +SetValue(VARIANT) HRESULT
        +GetValue(VARIANT*) HRESULT
        +ShutdownServer() HRESULT
        ...
    }

    IUnknown <|-- IDispatch : erft
    IUnknown <|-- ISupportErrorInfo : erft
    IDispatch <|-- ISharedValue : erft (dual interface)
```

### IDispatch — Late Binding

Alle interfaces in dit project zijn **dual interfaces**: ze ondersteunen zowel vtable calls (snel, C++) als `IDispatch` (late binding, VBScript/C#/Python).

```mermaid
sequenceDiagram
    participant VBS as VBScript Client
    participant DISP as IDispatch
    participant VTBL as vtable

    Note over VBS,VTBL: Late Binding (VBScript)
    VBS->>DISP: GetIDsOfNames("SetValue")
    DISP-->>VBS: DISPID = 1
    VBS->>DISP: Invoke(DISPID=1, args=[VARIANT])
    DISP->>VTBL: CSharedValue::SetValue(VARIANT)
    VTBL-->>DISP: HRESULT
    DISP-->>VBS: HRESULT

    Note over VBS,VTBL: Early Binding (C++)
    VBS->>VTBL: pSharedValue->SetValue(variant)
    Note right of VTBL: Directe vtable call<br/>Geen name lookup
    VTBL-->>VBS: HRESULT
```

### HRESULT — De Universele Foutcode

```
 31  30  29  28             16  15                    0
┌───┬───┬───┬────────────────┬──────────────────────────┐
│ S │ R │ C │   Facility     │        Code              │
│ e │   │   │   (4 = ITF)    │   (ErrorCode enum)       │
│ v │   │   │                │                          │
└───┴───┴───┴────────────────┴──────────────────────────┘

S = 0: Success, 1: Error
Facility ITF = Interface-specifieke fout
Code = SharedValueV2::ErrorCode waarde
```

| HRESULT | Hex | Betekenis |
|---|---|---|
| `S_OK` | `0x00000000` | Succes |
| `E_POINTER` | `0x80004003` | Null pointer argument |
| `E_FAIL` | `0x80004005` | Generieke fout |
| `E_INVALIDARG` | `0x80070057` | Ongeldig argument |
| `MAKE_HRESULT(1, 4, 100)` | `0x80040064` | `KeyNotFound` |
| `MAKE_HRESULT(1, 4, 101)` | `0x80040065` | `DuplicateKey` |
| `MAKE_HRESULT(1, 4, 102)` | `0x80040066` | `StoreModeNotEmpty` |

---

## 4. ATL Framework Internals

**ATL** (Active Template Library) is een Microsoft C++ template library die de boilerplate reduceert voor het schrijven van COM objecten.

### ATL Class Hiërarchie per COM Object

```mermaid
classDiagram
    class CComObjectRootEx {
        <<ATL base>>
        #InternalAddRef() ULONG
        #InternalRelease() ULONG
        #Lock() void
        #Unlock() void
    }

    class CComCoClass {
        <<ATL factory>>
        +GetObjectDescription() LPCTSTR
        +UpdateRegistry(BOOL) HRESULT
    }

    class IDispatchImpl {
        <<ATL helper>>
        +GetTypeInfoCount() HRESULT
        +GetTypeInfo() HRESULT
        +GetIDsOfNames() HRESULT
        +Invoke() HRESULT
    }

    class CSharedValue {
        -m_core : SharedValue
        -m_csCallbacks : CComAutoCriticalSection
        -m_eventBridge : SharedValueEventBridge
        +SetValue(VARIANT) HRESULT
        +GetValue(VARIANT*) HRESULT
        +ShutdownServer() HRESULT
    }

    CComObjectRootEx <|-- CSharedValue
    CComCoClass <|-- CSharedValue
    IDispatchImpl <|-- CSharedValue

    note for CComObjectRootEx "Template: CComMultiThreadModel\nThread-safe AddRef/Release"
    note for CSharedValue "DECLARE_CLASSFACTORY_SINGLETON\nDECLARE_REGISTRY_RESOURCEID\nBEGIN/END_COM_MAP"
```

### ATL Macro's Uitgelegd

| Macro | Wat het doet |
|---|---|
| `CComObjectRootEx<CComMultiThreadModel>` | Thread-safe reference counting via `InterlockedIncrement/Decrement`. |
| `CComCoClass<CSharedValue, &CLSID_SharedValue>` | Koppelt de class aan zijn CLSID en biedt fabrieksmethoden. |
| `IDispatchImpl<ISharedValue, &IID_ISharedValue>` | Auto-implementeert `IDispatch` op basis van de TypeLib. |
| `DECLARE_CLASSFACTORY_SINGLETON` | Vervangt de standaard class factory door `CComClassFactorySingleton`. Alle `CreateInstance()` calls retourneren hetzelfde object. |
| `DECLARE_REGISTRY_RESOURCEID(IDR_SHAREDVALUE)` | Koppelt het `.rgs` bestand aan het COM object voor automatische registratie. |
| `DECLARE_PROTECT_FINAL_CONSTRUCT` | Voorkomt `Release()` tijdens `FinalConstruct()` door tijdelijk de refcount te verhogen. |
| `BEGIN_COM_MAP / END_COM_MAP` | Bouwt de `QueryInterface` tabel — welke interfaces dit object ondersteunt. |
| `COM_INTERFACE_ENTRY(ISharedValue)` | Voegt `ISharedValue` toe aan de QI-map met correcte pointer offset. |
| `OBJECT_ENTRY_AUTO(__uuidof(SharedValue), CSharedValue)` | Registreert het COM object bij de ATL module zodat de class factory het kan instantiëren. |

### COM_MAP in Detail

```cpp
BEGIN_COM_MAP(CSharedValue)
    COM_INTERFACE_ENTRY(ISharedValue)     // QueryInterface voor ISharedValue
    COM_INTERFACE_ENTRY(IDispatch)        // QueryInterface voor IDispatch
    COM_INTERFACE_ENTRY(ISupportErrorInfo) // QueryInterface voor ISupportErrorInfo
END_COM_MAP()
```

Dit vertaalt naar een statische tabel die `QueryInterface` gebruikt:

```mermaid
graph LR
    QI["QueryInterface(IID)"] --> LOOKUP["Lineaire scan<br/>door COM_MAP"]
    LOOKUP --> IID_ISV["IID_ISharedValue<br/>→ this + offset 0"]
    LOOKUP --> IID_ID["IID_IDispatch<br/>→ this + offset IDisp"]
    LOOKUP --> IID_ISEI["IID_ISupportErrorInfo<br/>→ this + offset ISEI"]
    LOOKUP --> E_NI["Onbekend IID<br/>→ E_NOINTERFACE"]
```

---

## 5. EXE Server Lifecycle

### Volledige Lifecycle van Start tot Shutdown

```mermaid
stateDiagram-v2
    [*] --> Geregistreerd: exe /RegServer
    Geregistreerd --> Slapend: Registry entries geschreven

    Slapend --> Opstarten: Client roept CreateObject()
    note right of Opstarten: SCM start het EXE proces

    Opstarten --> Actief: RegisterClassObjects()
    note right of Actief: Message loop actief\nm_nLockCnt ≥ 1

    Actief --> Actief: Client calls via RPC\nSetValue, AddRow, etc.
    Actief --> Actief: Nieuwe client verbindt\nLock() count +1

    Actief --> Afsluiten: ShutdownServer()
    note right of Afsluiten: Clear proxy\nUnlock() → count = 0

    Afsluiten --> Gestopt: Message loop eindigt
    note right of Gestopt: RevokeClassObjects()\nProces exit

    Gestopt --> Slapend: Volgende CreateObject()
    
    Geregistreerd --> [*]: exe /UnregServer
```

### _tWinMain Entry Point — Stap voor Stap

```mermaid
sequenceDiagram
    participant OS as Windows
    participant MAIN as _tWinMain
    participant MOD as CAtlExeModuleT
    participant SCM as COM SCM
    participant MSGLOOP as Win32 Message Loop

    OS->>MAIN: Start ATLProjectcomserver.exe
    MAIN->>MOD: _AtlModule.Lock()
    Note right of MOD: m_nLockCnt = 1<br/>(voorkomt voortijdig exit)
    
    MAIN->>MOD: _AtlModule.WinMain(nShowCmd)
    MOD->>MOD: ParseCommandLine()
    
    alt /RegServer
        MOD->>MOD: RegisterAppId()
        MOD->>MOD: RegisterServer(TRUE)
        MOD-->>MAIN: return (exit)
    else /UnregServer
        MOD->>MOD: UnregisterServer(TRUE)
        MOD->>MOD: UnregisterAppId()
        MOD-->>MAIN: return (exit)
    else Normaal (gestart door SCM)
        MOD->>SCM: CoRegisterClassObject() × 4
        Note right of SCM: SharedValue, DatasetProxy,<br/>MathOperations, SharedValueCallback
        
        MOD->>MSGLOOP: GetMessage() loop
        Note right of MSGLOOP: Blokkeert totdat<br/>m_nLockCnt == 0
        
        MSGLOOP-->>MOD: WM_QUIT of lock count = 0
        MOD->>SCM: CoRevokeClassObject() × 4
        MOD-->>MAIN: return 0
    end
```

### Lock Count Mechanisme

```mermaid
graph TD
    START["_tWinMain()"] -->|"Lock()"| LC1["m_nLockCnt = 1"]
    
    LC1 -->|"Client 1 CreateInstance"| LC2["m_nLockCnt = 2"]
    LC2 -->|"Client 2 CreateInstance"| LC3["m_nLockCnt = 3"]
    LC3 -->|"Client 2 Release()"| LC2B["m_nLockCnt = 2"]
    LC2B -->|"Client 1 Release()"| LC1B["m_nLockCnt = 1"]
    
    LC1B -->|"ShutdownServer() → Unlock()"| LC0["m_nLockCnt = 0"]
    LC0 -->|"PostThreadMessage(WM_QUIT)"| EXIT["Message loop stopt<br/>Proces exit"]

    style LC0 fill:#e74c3c,color:#fff
    style EXIT fill:#c0392b,color:#fff
```

---

## 6. COM Object Model

### Interface Hiërarchie

```mermaid
classDiagram
    class IUnknown {
        <<COM basis>>
    }

    class IDispatch {
        <<Automation>>
    }

    class ISupportErrorInfo {
        <<Error info>>
    }

    class IMathOperations {
        <<dual interface>>
        +Add(DOUBLE, DOUBLE, DOUBLE*) HRESULT
        +Subtract(DOUBLE, DOUBLE, DOUBLE*) HRESULT
        +Multiply(DOUBLE, DOUBLE, DOUBLE*) HRESULT
        +Divide(DOUBLE, DOUBLE, DOUBLE*) HRESULT
    }

    class ISharedValue {
        <<dual interface>>
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
        <<dual interface>>
        +SetStorageMode(LONG) HRESULT
        +GetStorageMode(LONG*) HRESULT
        +AddRow(BSTR, BSTR) HRESULT
        +GetRowData(BSTR, BSTR*) HRESULT
        +UpdateRow(BSTR, BSTR, VARIANT_BOOL*) HRESULT
        +RemoveRow(BSTR, VARIANT_BOOL*) HRESULT
        +Clear() HRESULT
        +GetRecordCount(LONG*) HRESULT
        +HasKey(BSTR, VARIANT_BOOL*) HRESULT
        +FetchPageKeys(LONG, LONG, VARIANT*) HRESULT
        +FetchPageData(LONG, LONG, VARIANT*) HRESULT
    }

    class ISharedValueCallback {
        <<dual interface>>
        +OnValueChanged(VARIANT) HRESULT
        +OnDateTimeChanged(BSTR) HRESULT
    }

    class IEventCallback {
        <<dual interface>>
        +OnMutationEvent(LONG, BSTR, BSTR, BSTR, BSTR, BSTR, LONG) HRESULT
    }

    IUnknown <|-- IDispatch
    IUnknown <|-- ISupportErrorInfo
    IDispatch <|-- IMathOperations
    IDispatch <|-- ISharedValue
    IDispatch <|-- IDatasetProxy
    IDispatch <|-- ISharedValueCallback
    IDispatch <|-- IEventCallback
```

### GUID Tabel

| Entiteit | Type | GUID |
|---|---|---|
| **AppID / TypeLib** | Library | `{B0A0188F-59B6-42A5-AD3A-9D3CBE079253}` |
| SharedValue | CLSID | `{A5B21149-FB8C-4E50-8C52-65F3DC4AFEBF}` |
| DatasetProxy | CLSID | `{1D85075B-6ECB-4BE4-8317-9DDA91292ED8}` |
| MathOperations | CLSID | `{1CE8C512-FB0A-4C47-B3CD-44219BDC8DDF}` |
| SharedValueCallback | CLSID | `{6818DD57-F9E6-45BE-AA20-EA4B5B658AF3}` |
| ISharedValue | IID | `{8D55631F-1994-4F36-A3A3-D5B40EB0E0DB}` |
| IDatasetProxy | IID | `{50D4D0DB-6D9E-455F-8D6C-749CDBCF1949}` |
| IMathOperations | IID | `{488E9F3C-299B-4FE1-8B25-A2B9C2A23506}` |
| ISharedValueCallback | IID | `{E7639719-258C-46A3-B349-C3C96AC4B46C}` |
| IEventCallback | IID | `{DEC06BDB-7655-4E71-9937-110B78FCC576}` |

---

## 7. RPC Marshaling & Proxy/Stub

### Hoe Cross-Process Calls Werken

Omdat client en server in **aparte processen** leven, kan een interface-pointer niet simpel worden doorgegeven. Windows COM lost dit op met **proxy/stub marshaling**:

```mermaid
sequenceDiagram
    participant Client as Client Proces
    participant Proxy as Proxy Object<br/>(in client)
    participant Channel as RPC Channel<br/>(Named Pipe)
    participant Stub as Stub Object<br/>(in server)
    participant Server as COM Object<br/>(CDatasetProxy)

    Note over Client,Server: AddRow("server01", "status:online")

    Client->>Proxy: pProxy->AddRow(BSTR, BSTR)
    Note right of Proxy: Interface ziet eruit<br/>als een lokaal object

    Proxy->>Proxy: Serialiseer BSTR params<br/>naar NDR format
    Proxy->>Channel: Verstuur NDR buffer<br/>via Named Pipe IPC
    Channel->>Stub: Ontvang NDR buffer<br/>in server proces
    Stub->>Stub: Deserialiseer BSTR<br/>uit NDR buffer
    Stub->>Server: CDatasetProxy::AddRow(BSTR, BSTR)
    Server->>Server: m_store->AddRow(wstring, wstring)
    Server-->>Stub: HRESULT (S_OK)
    Stub-->>Channel: Serialiseer HRESULT
    Channel-->>Proxy: Response buffer
    Proxy-->>Client: HRESULT (S_OK)
```

### MIDL Code Generatie Pipeline

```mermaid
graph TD
    IDL["ATLProjectcomserver.idl<br/><i>Interface definities in IDL taal</i>"] -->|"midl.exe compiler"| GEN

    subgraph GEN["Gegenereerde Bestanden"]
        IH["_i.h<br/>C++ interface declaraties<br/>+ CLSID/IID constanten"]
        IC["_i.c<br/>GUID definities<br/>(linker symbols)"]
        PC["_p.c<br/>Proxy/Stub marshal code<br/>(NDR routines per methode)"]
        DC["dlldata.c<br/>DLL entry points voor<br/>proxy/stub registratie"]
        TLB[".tlb<br/>Type Library binary<br/>(voor #import / late binding)"]
    end

    IH -->|"#include"| SERVER["Server Code<br/>(SharedValue.h, DatasetProxy.h)"]
    IH -->|"#include"| CLCODE["Client Code<br/>(stop_server.cpp)"]
    PC -->|"Gelinkt in"| EXESERVER["ATLProjectcomserver.exe<br/>(self-contained proxy/stub)"]
    TLB -->|"Embedded in .rc"| EXESERVER
    TLB -->|"#import genereert"| TLHTLI[".tlh / .tli<br/>Smart pointer wrappers"]

    style IDL fill:#f39c12,color:#fff
```

### NDR (Network Data Representation)

Elke parameter wordt geserialiseerd volgens het NDR protocol:

| COM Type | NDR Wire Format | Grootte |
|---|---|---|
| `LONG` | 4 bytes, little-endian | 4 bytes |
| `DOUBLE` | IEEE 754, 8 bytes | 8 bytes |
| `VARIANT_BOOL` | 2 bytes (0x0000 of 0xFFFF) | 2 bytes |
| `BSTR` | 4-byte length prefix + UTF-16 data + null | variabel |
| `VARIANT` | 2-byte vt + padding + waarde | variabel |
| `SAFEARRAY` | dimensies + bounds + element data | variabel |

---

## 8. Singleton Architectuur

### CSharedValue als Singleton

```mermaid
graph TB
    CF["CComClassFactorySingleton<br/><i>DECLARE_CLASSFACTORY_SINGLETON</i>"]
    
    CF -->|"Eerste CreateInstance()"| CREATE["Maak CSharedValue<br/>Roep FinalConstruct()"]
    CREATE --> SV["CSharedValue instantie<br/><i>(leeft zolang server draait)</i>"]
    
    CF -->|"Tweede CreateInstance()"| SAME["Retourneer zelfde pointer"]
    SAME --> SV
    
    CF -->|"N-de CreateInstance()"| SAME2["Retourneer zelfde pointer"]
    SAME2 --> SV

    subgraph FinalConstruct["FinalConstruct() — eenmalig"]
        FC1["1. Maak CDatasetProxy via<br/>CComObject::CreateInstance()"]
        FC2["2. Wrap in CComVariant(VT_DISPATCH)"]
        FC3["3. Sla op als m_core.SetValue(variant)"]
        FC4["4. Subscribe op SharedValueV2 observers"]
        FC1 --> FC2 --> FC3 --> FC4
    end

    CREATE --> FinalConstruct
```

### Waarom Singleton?

| Eigenschap | Zonder Singleton | Met Singleton |
|---|---|---|
| Client A schrijft `SetValue("X")` | Alleen zichtbaar voor A | Zichtbaar voor A, B, C |
| Client B leest `GetValue()` | Eigen lege instantie | Ziet "X" van Client A |
| `DatasetProxy` rijen | Per-client kopie | Eén gedeelde dataset |
| Geheugengebruik | N × instantie | 1 instantie |

---

## 9. Observer & Event Architectuur

### Twee Parallelle Systemen

```mermaid
graph TB
    subgraph Legacy["Systeem 1: ISharedValueCallback (legacy)"]
        direction TB
        SV_CORE["SharedValueV2 Core<br/>notifyValueChanged()"] 
        SV_OBS["CSharedValue<br/>implementeert<br/>ISharedValueObserver&lt;CComVariant&gt;"]
        CB_LOCK["m_csCallbacks<br/>(CComAutoCriticalSection)"]
        CB_COPY["Kopieer vector<br/>buiten lock"]
        CB_FIRE["OnValueChanged(VARIANT)<br/>OnDateTimeChanged(BSTR)"]
        CLIENT_CB["Client's<br/>ISharedValueCallback"]

        SV_CORE --> SV_OBS
        SV_OBS --> CB_LOCK
        CB_LOCK --> CB_COPY
        CB_COPY --> CB_FIRE
        CB_FIRE --> CLIENT_CB
    end

    subgraph Modern["Systeem 2: IEventCallback (modern EventBus)"]
        direction TB
        EB_CORE["EventBus::Emit()<br/>MutationEvent struct"]
        EB_BRIDGE["SharedValueEventBridge<br/>ComEventBridge"]
        EB_CONV["Converteer:<br/>MutationEvent → BSTR params"]
        EB_FIRE["OnMutationEvent(<br/>type, key, old, new,<br/>source, timestamp, seqId)"]
        CLIENT_EC["Client's<br/>IEventCallback"]

        EB_CORE --> EB_BRIDGE
        EB_BRIDGE --> EB_CONV
        EB_CONV --> EB_FIRE
        EB_FIRE --> CLIENT_EC
    end

    style Legacy fill:#2980b9,color:#fff
    style Modern fill:#27ae60,color:#fff
```

### Deadlock-Free Notificatie

Het kritieke patroon dat deadlocks voorkomt bij trage of crashende clients:

```mermaid
sequenceDiagram
    participant Caller as Thread A: SetValue()
    participant Mutex as m_csCallbacks
    participant Copy as Lokale vector
    participant Slow as Client Observer (traag)
    participant Fast as Client Observer (snel)

    Caller->>Mutex: LOCK
    Caller->>Copy: callbacksCopy = m_callbacks
    Caller->>Mutex: UNLOCK
    
    Note over Caller,Fast: Andere threads kunnen nu<br/>vrij Subscribe/Unsubscribe aanroepen

    Caller->>Fast: OnValueChanged(variant)
    Fast-->>Caller: return (snel)
    
    Caller->>Slow: OnValueChanged(variant)
    Note over Slow: 500ms verwerking...<br/>Server is NIET geblokkeerd
    Slow-->>Caller: return
```

---

## 10. Error Handling Pipeline

```mermaid
graph LR
    subgraph CPP["C++ Engine"]
        EX["throw KeyNotFoundException<br/>throw DuplicateKeyException<br/>throw StoreModeException"]
    end

    subgraph MACRO["COM_METHOD_BODY Macro"]
        CATCH["catch (SharedValueException& e)"]
        HR["MAKE_HRESULT(<br/>SEVERITY_ERROR,<br/>FACILITY_ITF,<br/>e.code)"]
        EI["AtlReportError(<br/>CLSID, e.what(),<br/>IID, hresult)"]
    end

    subgraph COM["COM Transport"]
        IERR["IErrorInfo object<br/>op thread-local storage"]
        HRVAL["HRESULT<br/>retourwaarde"]
    end

    subgraph Clients["Client Talen"]
        CSHARP["C#:<br/>COMException<br/>.Message + .HResult"]
        VBS_E["VBScript:<br/>Err.Number<br/>Err.Description"]
        CPP_E["C++:<br/>FAILED(hr)<br/>+ GetErrorInfo()"]
    end

    EX --> CATCH
    CATCH --> HR
    HR --> EI
    EI --> IERR
    HR --> HRVAL
    IERR --> CSHARP
    IERR --> VBS_E
    IERR --> CPP_E
    HRVAL --> CSHARP
    HRVAL --> VBS_E
    HRVAL --> CPP_E
```

### ISupportErrorInfo

`CSharedValue` en `CDatasetProxy` implementeren `ISupportErrorInfo`. Dit vertelt de COM runtime dat deze objecten rijke foutinformatie leveren via `IErrorInfo`:

```cpp
STDMETHOD(InterfaceSupportsErrorInfo)(REFIID riid) {
    if (InlineIsEqualGUID(riid, IID_ISharedValue)) return S_OK;
    return S_FALSE;
}
```

---

## 11. Type Conversie — COM ↔ C++

### Inkomende Conversies (Client → Server)

| COM Parameter | C++ Intern | Conversie Code |
|---|---|---|
| `BSTR key` | `std::wstring` | `std::wstring(key)` |
| `VARIANT value` | `CComVariant` | `CComVariant::Copy(&value)` |
| `LONG mode` | `StorageMode` enum | `static_cast<StorageMode>(mode)` |
| `ISharedValueCallback*` | `CComPtr<ISharedValueCallback>` | `push_back(callback)` |

### Uitgaande Conversies (Server → Client)

| C++ Intern | COM Parameter | Conversie Code |
|---|---|---|
| `std::wstring` | `BSTR*` | `CComBSTR(str.c_str()).Detach()` |
| `CComVariant` | `VARIANT*` | `varValue.Detach(value)` |
| `bool` | `VARIANT_BOOL*` | `VARIANT_TRUE / VARIANT_FALSE` |
| `size_t` | `LONG*` | `static_cast<LONG>(count)` |
| `vector<wstring>` | `VARIANT* (SAFEARRAY)` | `CComSafeArray<VARIANT>` builder |
| `vector<pair<wstring,wstring>>` | `VARIANT* (2D SAFEARRAY)` | `SafeArrayCreate(VT_VARIANT, 2, bounds)` |

### SAFEARRAY Constructie (FetchPageKeys)

```mermaid
graph TD
    VEC["std::vector&lt;std::wstring&gt;<br/>[key1, key2, key3]"]
    
    VEC --> SA["CComSafeArray&lt;VARIANT&gt;<br/>grootte = vec.size()"]
    
    SA --> SET["Loop: sa.SetAt(i, CComVariant(key))"]
    
    SET --> DETACH["var.vt = VT_ARRAY | VT_VARIANT<br/>var.parray = sa.Detach()"]
    
    DETACH --> MARSHAL["NDR serialisatie<br/>over LRPC"]
    
    MARSHAL --> CLIENT["Client ontvangt:<br/>VBScript: Array()<br/>C#: System.Array<br/>Python: tuple"]
```

---

## 12. Client Interop Strategieën

### VBScript (IDispatch Late Binding)

```mermaid
graph LR
    VBS["VBScript Code"] -->|"CreateObject(ProgID)"| SCM["COM SCM"]
    SCM -->|"Lookup CLSID in Registry"| REG["HKLM\\Classes\\CLSID"]
    REG -->|"LocalServer32 = exe pad"| START["Start EXE"]
    START -->|"Retourneer IDispatch*"| PROXY["Proxy in cscript.exe"]
    PROXY -->|"GetIDsOfNames + Invoke"| SERVER["COM Server"]
```

### C# .NET (RCW + Dynamic Language Runtime)

```mermaid
graph LR
    CS["C# dynamic"] -->|"Activator.CreateInstance"| CLR[".NET CLR"]
    CLR -->|"Type.GetTypeFromProgID"| COM["COM Interop"]
    COM -->|"CoCreateInstance"| RCW["Runtime Callable<br/>Wrapper (RCW)"]
    RCW -->|"IDispatch::Invoke"| PROXY["Proxy/Stub"]
    PROXY -->|"LRPC"| SERVER["COM Server"]
```

De RCW (Runtime Callable Wrapper) is een .NET object dat de COM interface wrapt:
- `AddRef/Release` → automatisch beheerd door .NET GC + `Marshal.ReleaseComObject`
- `QueryInterface` → transparant via `dynamic` casting
- `HRESULT` failure → `COMException` met IErrorInfo beschrijving

### C++ (#import Smart Pointers)

```mermaid
graph LR
    CPP["C++ Code"] -->|"#import exe"| TLHTLI[".tlh/.tli generatie"]
    TLHTLI -->|"ISharedValuePtr"| SMART["_com_ptr_t<br/>smart pointer"]
    SMART -->|"CreateInstance(CLSID)"| COM["CoCreateInstance"]
    COM -->|"Direct vtable call"| PROXY["Proxy/Stub"]
    PROXY -->|"LRPC"| SERVER["COM Server"]
```

---

## 13. SharedValueV2 Engine Integration

Hoe de C++20 engine wordt ingezet binnen de ATL wrapper:

```mermaid
graph TB
    subgraph ATL["ATL COM Layer"]
        CSV["CSharedValue"]
        CDP["CDatasetProxy"]
        SVB["SharedValueEventBridge<br/><i>IEventListener impl</i>"]
        CEB["ComEventBridge<br/><i>IEventListener impl</i>"]
    end

    subgraph V2["SharedValueV2 Core"]
        SV["SharedValue&lt;CComVariant, LocalMutexPolicy&gt;"]
        DS["DatasetStore&lt;std::wstring, LocalMutexPolicy&gt;"]
        EB1["EventBus (SharedValue)"]
        EB2["EventBus (DatasetStore)"]
    end

    CSV -->|"m_core"| SV
    CSV -->|"m_eventBridge"| SVB
    CDP -->|"m_store (unique_ptr)"| DS

    SV -->|"GetEventBus()"| EB1
    DS -->|"GetEventBus()"| EB2

    EB1 -->|"Subscribe(&bridge)"| SVB
    EB2 -->|"Subscribe(&bridge)"| CEB

    SVB -->|"OnEvent() →<br/>converteer naar BSTR<br/>→ IEventCallback"| EC1["Client IEventCallback"]
    CEB -->|"OnEvent() →<br/>converteer naar BSTR<br/>→ IEventCallback"| EC2["Client IEventCallback"]

    CSV -.->|"implementeert<br/>ISharedValueObserver"| SV
    SV -.->|"OnValueChanged()<br/>OnDateTimeChanged()"| CSV

    style ATL fill:#e67e22,color:#fff
    style V2 fill:#27ae60,color:#fff
```

### Template Instantiaties

```cpp
// In CSharedValue — de gedeelde waarde is een VARIANT
SharedValueV2::SharedValue<CComVariant, SharedValueV2::LocalMutexPolicy> m_core;

// In CDatasetProxy — key-value pairs van wide strings
std::unique_ptr<SharedValueV2::DatasetStore<std::wstring>> m_store;
```

De keuze voor `CComVariant` als template parameter T maakt het mogelijk om **elk COM-compatibel type** op te slaan: strings, getallen, objecten (`IDispatch*`), en zelfs geneste `DatasetProxy` instanties.

---

## 14. Windows Registry Model

Na `ATLProjectcomserver.exe /RegServer` worden de volgende registry entries geschreven:

```mermaid
graph TD
    subgraph HKCR["HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes"]
        subgraph ProgIDs["ProgIDs"]
            P1["ATLProjectcomserverExe.SharedValue<br/>→ CLSID = {A5B21149...}"]
            P2["ATLProjectcomserverExe.DatasetProxy<br/>→ CLSID = {1D85075B...}"]
            P3["ATLProjectcomserverExe.MathOperations<br/>→ CLSID = {1CE8C512...}"]
        end

        subgraph CLSIDs["CLSID"]
            C1["{A5B21149...}<br/>LocalServer32 = pad\\naar\\exe"]
            C2["{1D85075B...}<br/>LocalServer32 = pad\\naar\\exe"]
            C3["{1CE8C512...}<br/>LocalServer32 = pad\\naar\\exe"]
        end

        subgraph AppID["AppID"]
            AI["{B0A0188F...}<br/>= ATLProjectcomserverExe"]
        end

        subgraph TypeLib["TypeLib"]
            TL["{B0A0188F...}\\1.0<br/>win64 = pad\\naar\\exe"]
        end

        subgraph Interfaces["Interface"]
            I1["{8D55631F...} ISharedValue<br/>ProxyStubClsid32"]
            I2["{50D4D0DB...} IDatasetProxy<br/>ProxyStubClsid32"]
        end
    end

    P1 --> C1
    P2 --> C2
    P3 --> C3
    C1 --> AI
    C2 --> AI
    C3 --> AI
```

### Registry Lookup Flow

```
Client: CreateObject("ATLProjectcomserverExe.SharedValue")
  ↓
HKCR\ATLProjectcomserverExe.SharedValue\CLSID → {A5B21149...}
  ↓
HKCR\CLSID\{A5B21149...}\LocalServer32 → "C:\...\ATLProjectcomserver.exe"
  ↓
SCM start EXE → CoRegisterClassObject → IClassFactory beschikbaar
  ↓
IClassFactory::CreateInstance() → pSharedValue (marshaled proxy)
```

---

## 15. Design Patterns Samenvatting

```mermaid
mindmap
  root((Design Patterns))
    Creational
      Singleton
        CSharedValue
        DECLARE_CLASSFACTORY_SINGLETON
      Factory Method
        ATL IClassFactory
        CreateStorageEngine()
      Abstract Factory
        CComClassFactory per coclass
    Structural
      Adapter
        SharedValueEventBridge
        ComEventBridge
        COM ↔ C++ conversie
      Proxy
        COM Proxy/Stub
        .NET RCW
        IDispatch wrapper
      Facade
        CSharedValue over SharedValueV2
        CDatasetProxy over DatasetStore
      Bridge
        IStorageEngine abstractie
        Map vs UnorderedMap
    Behavioral
      Observer
        ISharedValueObserver
        IDatasetObserver
        IEventListener
      Strategy
        LockPolicies als template
        StorageEngine runtime swap
      Monitor
        Data + lock onlosmakelijk
        lock_guard scope blocks
      Template Method
        ATL FinalConstruct / FinalRelease
      Command
        COM_METHOD_BODY macro
    Concurrency
      Copy then Notify
        Observer-lijst kopiëren
        Mutex vrij vóór callbacks
      Lock free Counter
        atomic sequence ID
```

| Pattern | Locatie | Doel |
|---|---|---|
| **Singleton** | `CSharedValue` | Gedeelde state voor alle clients |
| **Factory Method** | `CreateStorageEngine<T>()` | Runtime keuze: map vs unordered_map |
| **Adapter** | `SharedValueEventBridge`, `ComEventBridge` | C++ `MutationEvent` → COM `BSTR` parameters |
| **Proxy** | COM Proxy/Stub, .NET RCW | Transparante cross-process communicatie |
| **Facade** | `CSharedValue`, `CDatasetProxy` | Simpele COM API over complexe C++ core |
| **Bridge** | `IStorageEngine<T>` | Ontkoppeling van API en data-structuur |
| **Observer** | `ISharedValueObserver`, `IEventListener` | Losgekoppelde notificaties |
| **Strategy** | `SharedValue<T, MutexPolicy>` | Verwisselbare lock-strategie |
| **Monitor** | `SharedValue`, `DatasetStore` | Data ontoegankelijk buiten locked scope |
| **Template Method** | ATL `FinalConstruct` / `FinalRelease` | Framework-gestuurde object lifecycle |
| **Copy-then-Notify** | Alle observer notificaties | Deadlock-preventie |


## Gerelateerde Documentatie

- [README.md](README.md) — Gebruikershandleiding en overzicht van de EXE COM Server variant.
- [INSTALL.md](INSTALL.md) — Installatie-, registratie- en configuratiegids voor de EXE server.
- [README.md](../README.md) — Hoofddocumentatie en startpunt van het gehele project.
- [ARCHITECTURE.md](../ARCHITECTURE.md) — Hoofd architectuurdocument voor het gehele COM Server project.
- [README.md](../SharedValueV2/README.md) — Introductie en overzicht van de SharedValueV2 C++20 engine.
