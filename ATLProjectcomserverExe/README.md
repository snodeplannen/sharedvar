# ATLProjectcomserverExe — Out-of-Process EXE COM Server

This represents the **production variant** of the ATL COM Server, constructed as an **Out-of-Process EXE** (`LocalServer32`). The server runs as an autonomous Windows process addressed via RPC marshaling by multiple independent client processes.

## Role within the project

This is the **recommended architecture** concerning cross-process data sharing:
- A single central server process manages the shared state (singleton).
- Multiple clients (C#, VBScript, Python, C++) communicate concurrently via Windows COM/RPC.
- Graceful shutdown triggered via `ISharedValue::ShutdownServer()`.

---

## Architecture Overview

```
┌────────────────────┐  ┌────────────────────┐  ┌────────────────────┐
│  VBScript Client   │  │  C# .NET Client    │  │  C++ Client        │
│  (cscript.exe)     │  │  (dotnet.exe)       │  │  (stop_server.exe) │
└────────┬───────────┘  └────────┬───────────┘  └────────┬───────────┘
         │ CreateObject          │ Activator             │ CoCreateInstance
         ▼                      ▼                       ▼
┌──────────────────────────────────────────────────────────────────────┐
│                   Windows COM Runtime (SCM)                         │
│              LRPC Named Pipe ← Proxy/Stub Marshaling                │
└───────────────────────────────┬──────────────────────────────────────┘
                                ▼
┌──────────────────────────────────────────────────────────────────────┐
│                   ATLProjectcomserver.exe                           │
│  ┌─────────────────────────────────────────────────────────┐        │
│  │  CAtlExeModuleT  (_tWinMain + Win32 message loop)      │        │
│  │  ┌───────────────┐  ┌──────────────┐  ┌──────────────┐ │        │
│  │  │ CSharedValue  │  │CDatasetProxy │  │CMathOps      │ │        │
│  │  │ (SINGLETON)   │──│(server-owned)│  │(stateless)   │ │        │
│  │  └───────┬───────┘  └──────┬───────┘  └──────────────┘ │        │
│  └──────────┼─────────────────┼───────────────────────────┘        │
│             ▼                 ▼                                     │
│  ┌─────────────────────────────────────────────────────────┐        │
│  │  SharedValueV2 (C++20 header-only engine)               │        │
│  │  SharedValue<CComVariant, LocalMutexPolicy>             │        │
│  │  DatasetStore<std::wstring> + EventBus                  │        │
│  └─────────────────────────────────────────────────────────┘        │
└──────────────────────────────────────────────────────────────────────┘
```

### Difference against the Legacy DLL

| Property | Legacy DLL (InprocServer32) | **EXE Server (LocalServer32)** |
|---|---|---|
| Process | Loaded within client process | Own Windows process |
| Communication | Direct vtable calls | RPC via LRPC Named Pipe |
| Data sharing | ❌ Per-process copy | ✅ Genuine cross-process singleton |
| Registration | `regsvr32` | `/RegServer` (self-registering) |
| Autostart | N/A | SCM launches EXE on-demand |
| Shutdown | Automatic at `DllCanUnloadNow` | Explicit via `ShutdownServer()` |

---

## Buildup, Mechanics & Design Principles

The construction of the EXE server rests upon a strict separation dividing the **COM/RPC communication layer** versus the **C++20 State Engine** (`SharedValueV2`).

### 1. Why an Out-of-Process (EXE) Server?
A standard COM DLL (In-Process) gets loaded by Windows directly into the memory footprint of the calling process. Consequently, C# and VBScript applications invariably possess their individual, isolated clones of the data. Converting the component into a **LocalServer32 EXE** causes the Windows COM subsystem and *LRPC Named Pipes* to act as an IPC (Inter-Process Communication) connection. The result entails all those diverse applications chatting via proxies to **identically the exact same physical C++ process**.

### 2. Singleton Behavior & Data Relations
The central hub is the `CSharedValue` class. Empowered by the `DECLARE_CLASSFACTORY_SINGLETON` macro, ATL generates barely one specimen of this class inside the server, disregarding whether one or a hundred clients hook up. During creation (`FinalConstruct`) of this singleton naturally a `CDatasetProxy` is instantly forged alongside. By doing so, the overarching status (both the loose shared variable and the key-value store) effectively exists as a single truth accessible by any client.

### 3. The Bridging Layer (Type Conversions)
Whereas COM stubbornly clings to archaic binary C-standards (like `BSTR` strings and `SAFEARRAY`s), the `SharedValueV2` core screams modern C++ (`std::wstring`, `std::vector`, `std::optional`). The implementation files for the ATL server (`DatasetProxy.cpp`, `SharedValue.cpp`) thus mainly serve acting as translators: inbound parameters are intercepted, converted, shoveled into the engine, and vice versa returning verdicts are staged inside a `SAFEARRAY` or `VARIANT` to successfully get marshaled via RPC returning towards the parent calling application.

### 4. Concurrency & Deadlock Prevention
Spanning cross-process event handling, a stalled or atrociously sluggish client usually bottlenecks the entire server process (simultaneously suffocating all other clients). The design preempts this since the `LocalMutexPolicy` (governing internal C++ locks) categorically copies the callback list under all circumstances, locks down, and inherently *afterwards* reverberates the callbacks triggering those clientside proxies. This guarantees that the communication bottleneck never entraps the data-integrity governing the engine.

---

## COM Interfaces — Technical Details

### `ISharedValue` — Shared State Singleton

```idl
interface ISharedValue : IDispatch
{
    HRESULT SetValue([in] VARIANT value);
    HRESULT GetValue([out, retval] VARIANT* value);
    HRESULT GetCurrentUTCDateTime([out, retval] BSTR* dateTime);
    HRESULT GetCurrentUserLogin([out, retval] BSTR* login);
    HRESULT LockSharedValue([in] LONG timeoutMs);
    HRESULT Unlock();
    HRESULT Subscribe([in] ISharedValueCallback* callback);
    HRESULT Unsubscribe([in] ISharedValueCallback* callback);
    HRESULT ShutdownServer();
    HRESULT RegisterEventCallback([in] IEventCallback* callback);
    HRESULT UnregisterEventCallback([in] IEventCallback* callback);
};
```

**Internal:** Implemented inside `CSharedValue`, mapped as a **singleton** using `DECLARE_CLASSFACTORY_SINGLETON`. All clients disregard process boundaries sharing this identical instance. The `m_core` wraps a `SharedValueV2::SharedValue<CComVariant, LocalMutexPolicy>`.

`FinalConstruct()` autonomously drops a server-side `CDatasetProxy` wrapping it as `CComVariant(VT_DISPATCH)` — clients fetch said proxy invoking `GetValue()`.

**`ShutdownServer()` operation:** Obiterates the DatasetProxy reference relying on `m_core.SetValue(CComVariant())` alongside firing `_AtlModule.Unlock()` resolving the hanging extra lock from `_tWinMain`. This breaks the Win32 message loop gracefully terminating the process.

---

### `IDatasetProxy` — Pageable Key-Value Store

```idl
interface IDatasetProxy : IDispatch
{
    // Storage Mode (0=Ordered, 1=Unordered)
    HRESULT SetStorageMode([in] LONG mode);
    HRESULT GetStorageMode([out, retval] LONG* mode);

    // CRUD
    HRESULT AddRow([in] BSTR key, [in] BSTR value);
    HRESULT GetRowData([in] BSTR key, [out, retval] BSTR* data);
    HRESULT UpdateRow([in] BSTR key, [in] BSTR value, [out, retval] VARIANT_BOOL* success);
    HRESULT RemoveRow([in] BSTR key, [out, retval] VARIANT_BOOL* success);
    HRESULT Clear();

    // Query
    HRESULT GetRecordCount([out, retval] LONG* count);
    HRESULT HasKey([in] BSTR key, [out, retval] VARIANT_BOOL* exists);

    // Paginering  
    HRESULT FetchPageKeys([in] LONG startIndex, [in] LONG limit, [out, retval] VARIANT* keys);
    HRESULT FetchPageData([in] LONG startIndex, [in] LONG limit, [out, retval] VARIANT* keysAndValues);

    // Event Callbacks
    HRESULT RegisterEventCallback([in] IEventCallback* callback);
    HRESULT UnregisterEventCallback([in] IEventCallback* callback);
};
```

**Internal:** Delegates everything downward at `SharedValueV2::DatasetStore<std::wstring>`. The store fields opposing storage modes employing the **Strategy Pattern**:
- **Mode 0 (Ordered):** `std::map<wstring, wstring>` — keys rigorously sorted.
- **Mode 1 (Unordered):** `std::unordered_map<wstring, wstring>` — faster O(1) jump lookups.

**SAFEARRAY Marshaling:** `FetchPageKeys` fires back a 1D `SAFEARRAY(VT_VARIANT)` containing every key typed `BSTR`. `FetchPageData` launches a 2D `SAFEARRAY[rows][2]` holding overarching key-value pairs. Such arrays get seamlessly marshaled piercing the RPC border courtesy of MIDL-generated proxy/stub code.

**Type Conversions inside DatasetProxy:**
| COM Type (in) | C++ Type (internal) | Conversion |
|---|---|---|
| `BSTR` key/value | `std::wstring` | `std::wstring(key)` constructor |
| `VARIANT_BOOL` return | `bool` | `VARIANT_TRUE` / `VARIANT_FALSE` |
| `LONG` mode/count | `size_t` / `StorageMode` | `static_cast` |
| `VARIANT*` array return | `std::vector<wstring>` | `CComSafeArray<VARIANT>` builder |

---

### `IMathOperations` — Stateless Math Tool

```idl
interface IMathOperations : IDispatch
{
    HRESULT Add([in] DOUBLE a, [in] DOUBLE b, [out, retval] DOUBLE* result);
    HRESULT Subtract([in] DOUBLE a, [in] DOUBLE b, [out, retval] DOUBLE* result);
    HRESULT Multiply([in] DOUBLE a, [in] DOUBLE b, [out, retval] DOUBLE* result);
    HRESULT Divide([in] DOUBLE a, [in] DOUBLE b, [out, retval] DOUBLE* result);
};
```

**Internal:** Elementary arithmetic procedures. `Divide` probes combating zero-division. Devoid of state, sans lock, sans observer. Predominantly serves displaying a baseline component alongside acting as a smoke-test probing COM infrastructure.

---

### `ISharedValueCallback` — Legacy Observer

```idl
interface ISharedValueCallback : IDispatch
{
    HRESULT OnValueChanged([in] VARIANT newValue);
    HRESULT OnDateTimeChanged([in] BSTR newDateTime);
};
```

**Internal:** Backwards compatible observer satisfying the `SharedValue` core. Client endpoints hop aboard engaging `ISharedValue::Subscribe()`. Notifying occurs **deadlock-free**: copying the observer list confined inside the lock, whereafter disabling the lock vastly precedes shooting the callbacks.

---

### `IEventCallback` — Modern Event System

```idl
interface IEventCallback : IDispatch
{
    HRESULT OnMutationEvent(
        [in] LONG eventType,     // SharedValueV2::EventType enum
        [in] BSTR key,
        [in] BSTR oldValue,
        [in] BSTR newValue,
        [in] BSTR source,
        [in] BSTR timestamp,     // UTC ISO 8601
        [in] LONG sequenceId     // Monotonically increasing (atomic)
    );
};
```

**Event types:**
| Code | Name | When |
|---|---|---|
| 1 | `ValueSet` | `SharedValue::SetValue()` |
| 2 | `ValueLocked` | `SharedValue::LockSharedValue()` |
| 3 | `ValueUnlocked` | `SharedValue::Unlock()` |
| 10 | `RowAdded` | `DatasetProxy::AddRow()` |
| 11 | `RowUpdated` | `DatasetProxy::UpdateRow()` |
| 12 | `RowRemoved` | `DatasetProxy::RemoveRow()` |
| 13 | `DatasetCleared` | `DatasetProxy::Clear()` |
| 14 | `StorageModeChanged` | `DatasetProxy::SetStorageMode()` |

**Internal:** Two bridging-classes orchestrate `SharedValueV2::MutationEvent` → COM `IEventCallback` morphs:
- `SharedValueEventBridge` — fastens the `SharedValue` EventBus tracking clients.
- `ComEventBridge` — tethers the `DatasetStore` EventBus catering clients.

---

## EXE Lifecycle — Technical Deep Dive

### Entry Point (`dllmain.cpp`)

```cpp
CATLProjectcomserverExeModule _AtlModule;

extern "C" int WINAPI _tWinMain(HINSTANCE, HINSTANCE, LPTSTR, int nShowCmd)
{
    _AtlModule.Lock();    // Prevent premature shutdown
    return _AtlModule.WinMain(nShowCmd);
}
```

`CAtlExeModuleT::WinMain()` navigates the following:
1. `RegisterClassObjects()` — logs all class factories checking into SCM.
2. **Win32 message loop** — `GetMessage()` / `DispatchMessage()` wheel spinning preserving the process lifespan.
3. The wheel halts whenever `m_nLockCnt == 0` (locks completely depleted).
4. `RevokeClassObjects()` — unlists the class factories.

### Why `Lock()` is necessary

Native ATL doctrine: assuming the final client disconnects, the lock count plummets reaching 0 crashing the EXE. Such acts as poison for a central server obligated retaining persistent state. `_AtlModule.Lock()` affixes an extraneous reference resolving purely whenever `ShutdownServer()` sparks.

### Module Declaration (`dllmain.h`)

```cpp
class CATLProjectcomserverExeModule : public ATL::CAtlExeModuleT<CATLProjectcomserverExeModule>
{
public:
    DECLARE_LIBID(LIBID_ATLProjectcomserverLib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_ATLPROJECTCOMSERVER, 
        "{B0A0188F-59B6-42A5-AD3A-9D3CBE079253}")
};
```

The `DECLARE_REGISTRY_APPID_RESOURCEID` parameter marries the AppID GUID hooking it tightly towards the `.rgs` sheet, ensuring `ATLProjectcomserver.exe /RegServer` dutifully drills away dropping all registry entries.

---

## Error Handling — `ComErrorHelper.h`

All COM methods populating this venture rely upon the `COM_METHOD_BODY` macro transforming C++ exceptions rendering them COM-compatible error markings:

```
C++ Exception                       → HRESULT                  → Client
─────────────────────────────────────────────────────────────────────────
SharedValueException (code N)       → MAKE_HRESULT(1, 4, N)    → COMException
std::out_of_range                   → E_BOUNDS                 → COMException
std::invalid_argument               → E_INVALIDARG             → COMException
std::exception                      → E_FAIL                   → COMException
... (unknown)                       → E_UNEXPECTED             → COMException
```

Every single exception consecutively pushes an `IErrorInfo` calling `AtlReportError()`, empowering clients (C#, VBScript) scooping up the fault depiction typed plainly human-readable.

---

## Windows Registry — CLSIDs & ProgIDs

Upon launching `/RegServer` the registry absorbs the subsequent entries:

| ProgID | CLSID | Type |
|---|---|---|
| `ATLProjectcomserverExe.SharedValue` | `{A5B21149-FB8C-4E50-8C52-65F3DC4AFEBF}` | Singleton |
| `ATLProjectcomserverExe.DatasetProxy` | `{1D85075B-6ECB-4BE4-8317-9DDA91292ED8}` | Multi-instance |
| `ATLProjectcomserverExe.MathOperations` | `{1CE8C512-FB0A-4C47-B3CD-44219BDC8DDF}` | Multi-instance |

**AppID:** `{B0A0188F-59B6-42A5-AD3A-9D3CBE079253}`
**TypeLib:** Identical GUID aligning AppID.

Registration is independently verifiable executing:
```powershell
.\scripts\Invoke-ComDiagnostics.ps1
```

---

## Files Outline

### COM Interfaces & IDL
| File | Description |
|---|---|
| `ATLProjectcomserver.idl` | Interface Definition Language — declaring `IMathOperations`, `ISharedValue`, `ISharedValueCallback`, `IEventCallback`, `IDatasetProxy` alongside the TypeLib. Handled by MIDL. |
| `ATLProjectcomserver_i.h / _i.c` | Interface footprints and GUID anchors generated directly via MIDL (`CLSID_SharedValue`, `IID_IDatasetProxy`, etc.). |
| `ATLProjectcomserver_p.c` | MIDL generated proxy/stub footprints — critical executing RPC marshaling. Fields the NDR serializing engines targeting interface-parameters individually. |
| `ATLProjectcomserver.tlh / .tli` | Improvised type library blankets generated calling `#import` throwing smart pointers (`ISharedValuePtr`). Solely leveraged running `stop_server.cpp`. |
| `dlldata.c` | Proxy/stub DLL linking metrics (COM necessitates this spanning custom marshaling). |

### COM Class Implementations
| File | Description |
|---|---|
| `SharedValue.h / .cpp` | `CSharedValue` — ATL wrapper sealing `SharedValueV2::SharedValue<CComVariant, LocalMutexPolicy>`. Singleton (`DECLARE_CLASSFACTORY_SINGLETON`). Carries `SharedValueEventBridge` mitigating EventBus→COM conversion. `ShutdownServer()` vaporizes the process firing `_AtlModule.Unlock()`. |
| `DatasetProxy.h / .cpp` | `CDatasetProxy` — ATL wrapper enclosing `SharedValueV2::DatasetStore<std::wstring>`. Anchors `ComEventBridge` covering dataset-events. `FetchPageKeys` stitches a `CComSafeArray<VARIANT>`, `FetchPageData` assembles a heavy 2D `SAFEARRAY`. |
| `MathOperations.h / .cpp` | `CMathOperations` — Stateless mathematical processes. Holds zero dependency relying upon SharedValueV2. Threading blueprint: `CComMultiThreadModel`. |
| `SharedValueCallback.h / .cpp` | `CSharedValueCallback` — Receiver-interface. Client nodes implement this outline, server sparks `OnValueChanged()` / `OnDateTimeChanged()`. |
| `ComErrorHelper.h` | `COM_METHOD_BODY` wrapping blanket — overarching nexus bridging exception → `HRESULT + IErrorInfo` conversion. Snares `SharedValueException`, `std::out_of_range`, `std::invalid_argument`, `std::exception`, plus fallback `catch(...)`. |

### EXE Entry Point & Lifecycle
| File | Description |
|---|---|
| `dllmain.cpp` | `_tWinMain()` boot sequence. Drops `CATLProjectcomserverExeModule` kicking off the Win32 message carousel. `_AtlModule.Lock()` clutches the process intact spanning until `ShutdownServer()` triggers. |
| `dllmain.h` | Setup detailing `CATLProjectcomserverExeModule : public CAtlExeModuleT<...>`. Houses `DECLARE_LIBID` beside `DECLARE_REGISTRY_APPID_RESOURCEID`. |

### Shutdown Tools
| File | Description |
|---|---|
| `stop_server.cpp` | C++ CLI tool deploying `#import` paired bridging smart pointers (`ISharedValuePtr`) clamping onto the EXE server firing `ShutdownServer()`. Invokes `CoInitializeEx(COINIT_APARTMENTTHREADED)`. |
| `stop_server.vbs` | VBScript one-liner identical executing: `CreateObject("ATLProjectcomserverExe.SharedValue").ShutdownServer()`. |

### ATL Infrastructure
| File | Description |
|---|---|
| `pch.h / pch.cpp` | Precompiled header — hauls in `<atlbase.h>`, `<atlcom.h>` joined alongside Windows headers. |
| `targetver.h` | Windows SDK version anchor points (`_WIN32_WINNT`). |
| `resource.h` | Distinct Resource IDs: `IDR_ATLPROJECTCOMSERVER`, `IDR_SHAREDVALUE`, `IDR_DATASETPROXY`, `IDR_MATHOPERATIONS`. |
| `ATLProjectcomserver.rc` | Windows resource sheet — bakes in the TypeLib (`.tlb`) combined flanking version tags. |
| `ATLProjectcomserver.def` | Module definition manifest — projects `DllGetClassObject`, `DllCanUnloadNow`, etc. (sought handling the proxy/stub DLL). |

### Registry Scripts (`.rgs`)
| File | Description |
|---|---|
| `ATLProjectcomserver.rgs` | Plugs the AppID `{B0A0188F-...}` alongside the footprint driving the EXE named `LocalServer32`. |
| `SharedValue.rgs` | Tethers ProgID `ATLProjectcomserverExe.SharedValue` → CLSID `{A5B21149-...}`. |
| `DatasetProxy.rgs` | Tethers ProgID `ATLProjectcomserverExe.DatasetProxy` → CLSID `{1D85075B-...}`. |
| `MathOperations.rgs` | Tethers ProgID `ATLProjectcomserverExe.MathOperations` → CLSID `{1CE8C512-...}`. |
| `SharedValueCallback.rgs` | Tethers ProgID `ATLProjectcomserverExe.SharedValueCallback` → CLSID `{6818DD57-...}`. |

### Build & Solution
| File | Description |
|---|---|
| `ATLProjectcomserver.vcxproj` | MSVC blueprint — `ConfigurationType=Application`, `SubSystem=Windows`. `AdditionalIncludeDirectories` nests `$(SolutionDir)` covering SharedValueV2 headers. |
| `ATLProjectcomserver.sln` | Dedicated isolated solution driving build constraints targeting exclusively the EXE. |
| `.clangd` | Clangd LSP mapping — extra inclusion paths reaching SharedValueV2. |
| `.gitignore` | Local cloaking: `x64/`, `*.obj`, `*.exe`, `*.pdb`, etc. |

### Subdirectories
| Directory | Description |
|---|---|
| [`tests/`](tests/) | Cross-process integration sequences — `Run-CrossProcessTests.ps1` + C# .NET 8. |

---

## Compilation

### Traversing the central solution (recommended)

```powershell
# Populate MSVC build environment variables
. Invoke-BuildEnvironment.ps1 -Toolchain MSVC -Version "Enterprise 2022" -Architecture x64

# Blanket build wrapping overarching projects (Legacy DLL + EXE Server)
msbuild ATLProjectcomserver.sln /p:Configuration=Debug /p:Platform=x64 -m
```

### Traversing the standalone EXE solution

```powershell
msbuild ATLProjectcomserverExe\ATLProjectcomserver.sln /p:Configuration=Debug /p:Platform=x64 -m
```

The resulting EXE anchors down natively at `x64\Debug\ATLProjectcomserver.exe`.

---

## Registration

```cmd
:: Executing Registration (engage as Administrator)
x64\Debug\ATLProjectcomserver.exe /RegServer

:: Scrubbing (De-registering)
x64\Debug\ATLProjectcomserver.exe /UnregServer

:: Verification checks
powershell .\scripts\Invoke-ComDiagnostics.ps1
```

---

## Usage

### VBScript — The EXE leaps automatically on-demand

```vbscript
Set sv = CreateObject("ATLProjectcomserverExe.SharedValue")
sv.SetValue "Cross-process data!"
WScript.Echo sv.GetValue()

' Grab DatasetProxy evaluating entries
Set proxy = sv.GetValue()
proxy.AddRow "key1", "value1"
WScript.Echo proxy.GetRecordCount()    ' 1
```

### C# .NET — Approaching via late binding

```csharp
dynamic sv = Activator.CreateInstance(
    Type.GetTypeFromProgID("ATLProjectcomserverExe.SharedValue")!);
sv.SetValue("Hello beamed from C#!");
Console.WriteLine(sv.GetValue());

// DatasetProxy
dynamic proxy = sv.GetValue();
proxy.AddRow("server01", "status:online|cpu:45%");
Console.WriteLine(proxy.GetRecordCount());  // 1
```

### C++ — Reaching via #import smart pointers

```cpp
#import "x64\\Release\\ATLProjectcomserver.exe" no_namespace named_guids

CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
ISharedValuePtr pSV;
pSV.CreateInstance(CLSID_SharedValue);
pSV->SetValue(_variant_t("Hello from C++!"));
pSV->ShutdownServer();
CoUninitialize();
```

---

## Graceful Shutdown

The EXE server cycles endlessly maintaining uptime awaiting an explicit termination string:

```powershell
# Spanning VBScript
cscript stop_server.vbs

# Executing compiled C++ tool
.\stop_server.exe

# Engaging PowerShell standalone snippet
$sv = New-Object -ComObject "ATLProjectcomserverExe.SharedValue"
$sv.ShutdownServer()
[System.Runtime.InteropServices.Marshal]::ReleaseComObject($sv) | Out-Null
```

---

## Testing

Visit [`tests/README_EN.md`](tests/README_EN.md) observing the expanded testing documentation.

```powershell
# Automated cross-process barrage (6 sequences)
.\tests\Run-CrossProcessTests.ps1

# C# .NET 8 integration testing barrage (14 sequence logic)
cd tests\CSharpNet8Test && dotnet run
```

## Related Documentation

- [USAGE_EXAMPLES_EN.md](USAGE_EXAMPLES_EN.md) — Clear integration examples mapping C#, Native C++, and VBScript hitting SharedValue & DatasetProxy components.
- [ARCHITECTURE_EN.md](ARCHITECTURE_EN.md) — Comprehensive technical design layouts encompassing explicit class diagrams.
- [INSTALL_EN.md](INSTALL_EN.md) — Base system thresholds, deployment steps, deep compiler instructions, and robust WiX Toolset dependencies.
- [tests/README_EN.md](tests/README_EN.md) — Aggressive Unit validations alongside Integration testing matrices.
- [../README_EN.md](../README_EN.md) — Governing overarching root structure documentation metrics.
- [ARCHITECTURE_EN.md](../ARCHITECTURE_EN.md) — Main architecture document covering the entire COM Server project tree.
- [README_EN.md](../SharedValueV2/README_EN.md) — Introductory overview guiding the SharedValueV2 C++20 engine.
