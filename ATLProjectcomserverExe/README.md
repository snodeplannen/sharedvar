# ATLProjectcomserverExe — Out-of-Process EXE COM Server

Dit is de **productie-variant** van de ATL COM Server, gebouwd als een **Out-of-Process EXE** (`LocalServer32`). De server draait als een zelfstandig Windows-proces dat via RPC-marshaling wordt aangesproken door meerdere onafhankelijke client-processen.

## Rol binnen het project

Dit is de **aanbevolen architectuur** voor cross-process data-sharing:
- Eén centraal serverproces beheert de gedeelde state (singleton).
- Meerdere clients (C#, VBScript, Python, C++) communiceren gelijktijdig via Windows COM/RPC.
- Graceful shutdown via `ISharedValue::ShutdownServer()`.

---

## Architectuuroverzicht

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

### Verschil met de Legacy DLL

| Eigenschap | Legacy DLL (InprocServer32) | **EXE Server (LocalServer32)** |
|---|---|---|
| Proces | Geladen in client-proces | Eigen Windows-proces |
| Communicatie | Directe vtable calls | RPC via LRPC Named Pipe |
| Data sharing | ❌ Per-proces kopie | ✅ Echte cross-process singleton |
| Registratie | `regsvr32` | `/RegServer` (zelf-registrerend) |
| Autostart | n.v.t. | SCM start EXE on-demand |
| Shutdown | Automatisch bij `DllCanUnloadNow` | Expliciet via `ShutdownServer()` |

---

## COM Interfaces — Technische Details

### `ISharedValue` — Gedeelde State Singleton

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

**Intern:** Geïmplementeerd door `CSharedValue`, gedeclareerd als **singleton** via `DECLARE_CLASSFACTORY_SINGLETON`. Alle clients ongeacht proces delen dezelfde instantie. De `m_core` is een `SharedValueV2::SharedValue<CComVariant, LocalMutexPolicy>`.

`FinalConstruct()` maakt automatisch een server-side `CDatasetProxy` aan en slaat deze op als `CComVariant(VT_DISPATCH)` — clients kunnen de proxy ophalen met `GetValue()`.

**`ShutdownServer()` werking:** Verwijdert de DatasetProxy reference via `m_core.SetValue(CComVariant())` en roept `_AtlModule.Unlock()` aan om de extra lock uit `_tWinMain` vrij te geven. Dit beëindigt de Win32 message loop en sluit het proces.

---

### `IDatasetProxy` — Pagineerbare Key-Value Store

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

**Intern:** Delegeert naar `SharedValueV2::DatasetStore<std::wstring>`. De store ondersteunt two storage modes via het **Strategy Pattern**:
- **Mode 0 (Ordered):** `std::map<wstring, wstring>` — keys gesorteerd.
- **Mode 1 (Unordered):** `std::unordered_map<wstring, wstring>` — snellere O(1) lookups.

**SAFEARRAY Marshaling:** `FetchPageKeys` retourneert een 1D `SAFEARRAY(VT_VARIANT)` met alle keys als `BSTR`. `FetchPageData` retourneert een 2D `SAFEARRAY[rows][2]` met key-value paren. Deze arrays worden automatisch gemarshald over de RPC-grens door de MIDL-gegenereerde proxy/stub code.

**Type-conversies in DatasetProxy:**
| COM Type (in) | C++ Type (intern) | Conversie |
|---|---|---|
| `BSTR` key/value | `std::wstring` | `std::wstring(key)` constructor |
| `VARIANT_BOOL` retour | `bool` | `VARIANT_TRUE` / `VARIANT_FALSE` |
| `LONG` mode/count | `size_t` / `StorageMode` | `static_cast` |
| `VARIANT*` array retour | `std::vector<wstring>` | `CComSafeArray<VARIANT>` builder |

---

### `IMathOperations` — Stateless Rekentool

```idl
interface IMathOperations : IDispatch
{
    HRESULT Add([in] DOUBLE a, [in] DOUBLE b, [out, retval] DOUBLE* result);
    HRESULT Subtract([in] DOUBLE a, [in] DOUBLE b, [out, retval] DOUBLE* result);
    HRESULT Multiply([in] DOUBLE a, [in] DOUBLE b, [out, retval] DOUBLE* result);
    HRESULT Divide([in] DOUBLE a, [in] DOUBLE b, [out, retval] DOUBLE* result);
};
```

**Intern:** Eenvoudige rekenkundige operaties. `Divide` controleert op deling door nul. Geen state, geen lock, geen observer. Dient als demonstratie-component en smoke-test voor de COM infra.

---

### `ISharedValueCallback` — Legacy Observer

```idl
interface ISharedValueCallback : IDispatch
{
    HRESULT OnValueChanged([in] VARIANT newValue);
    HRESULT OnDateTimeChanged([in] BSTR newDateTime);
};
```

**Intern:** Backward-compatible observer voor de `SharedValue` core. Client-classes subscriben via `ISharedValue::Subscribe()`. Notificaties worden **deadlock-free** afgeleverd: de observer-lijst wordt gekopieerd onder de lock, waarna de lock wordt vrijgegeven vóór de callbacks worden aangeroepen.

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
        [in] LONG sequenceId     // Monotoon stijgend (atomic)
    );
};
```

**Event types:**
| Code | Naam | Wanneer |
|---|---|---|
| 1 | `ValueSet` | `SharedValue::SetValue()` |
| 2 | `ValueLocked` | `SharedValue::LockSharedValue()` |
| 3 | `ValueUnlocked` | `SharedValue::Unlock()` |
| 10 | `RowAdded` | `DatasetProxy::AddRow()` |
| 11 | `RowUpdated` | `DatasetProxy::UpdateRow()` |
| 12 | `RowRemoved` | `DatasetProxy::RemoveRow()` |
| 13 | `DatasetCleared` | `DatasetProxy::Clear()` |
| 14 | `StorageModeChanged` | `DatasetProxy::SetStorageMode()` |

**Intern:** Twee bridge-klassen vertalen `SharedValueV2::MutationEvent` → COM `IEventCallback` parameters:
- `SharedValueEventBridge` — koppelt de `SharedValue` EventBus aan clients.
- `ComEventBridge` — koppelt de `DatasetStore` EventBus aan clients.

---

## EXE Lifecycle — Technische Deep Dive

### Entry Point (`dllmain.cpp`)

```cpp
CATLProjectcomserverExeModule _AtlModule;

extern "C" int WINAPI _tWinMain(HINSTANCE, HINSTANCE, LPTSTR, int nShowCmd)
{
    _AtlModule.Lock();    // Voorkom voortijdig afsluiten
    return _AtlModule.WinMain(nShowCmd);
}
```

`CAtlExeModuleT::WinMain()` doet het volgende:
1. `RegisterClassObjects()` — registreert alle class factories bij de SCM.
2. **Win32 message loop** — `GetMessage()` / `DispatchMessage()` loop die het proces in leven houdt.
3. De loop eindigt wanneer `m_nLockCnt == 0` (alle locks vrijgegeven).
4. `RevokeClassObjects()` — deregistreert de class factories.

### Waarom `Lock()` nodig is

Standaard ATL-gedrag: zodra de laatste client disconneert, daalt de lock count naar 0 en stopt de EXE. Dit is ongewenst voor een server die persistent state moet bewaren. `_AtlModule.Lock()` voegt een extra referentie toe die pas door `ShutdownServer()` wordt verwijderd.

### Module Declaratie (`dllmain.h`)

```cpp
class CATLProjectcomserverExeModule : public ATL::CAtlExeModuleT<CATLProjectcomserverExeModule>
{
public:
    DECLARE_LIBID(LIBID_ATLProjectcomserverLib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_ATLPROJECTCOMSERVER, 
        "{B0A0188F-59B6-42A5-AD3A-9D3CBE079253}")
};
```

De `DECLARE_REGISTRY_APPID_RESOURCEID` macro koppelt de AppID GUID aan het `.rgs` bestand, waardoor `ATLProjectcomserver.exe /RegServer` automatisch alle registry entries schrijft.

---

## Error Handling — `ComErrorHelper.h`

Alle COM methoden in dit project gebruiken de `COM_METHOD_BODY` macro die C++ exceptions vertaalt naar COM-compatibele foutcodes:

```
C++ Exception                       → HRESULT                  → Client
─────────────────────────────────────────────────────────────────────────
SharedValueException (code N)       → MAKE_HRESULT(1, 4, N)    → COMException
std::out_of_range                   → E_BOUNDS                 → COMException
std::invalid_argument               → E_INVALIDARG             → COMException
std::exception                      → E_FAIL                   → COMException
... (unknown)                       → E_UNEXPECTED             → COMException
```

Elke exception genereert ook een `IErrorInfo` via `AtlReportError()`, zodat clients (C#, VBScript) de foutbeschrijving kunnen uitlezen als human-readable tekst.

---

## Windows Registry — CLSIDs & ProgIDs

Na `/RegServer` worden de volgende entries aangemaakt:

| ProgID | CLSID | Type |
|---|---|---|
| `ATLProjectcomserverExe.SharedValue` | `{A5B21149-FB8C-4E50-8C52-65F3DC4AFEBF}` | Singleton |
| `ATLProjectcomserverExe.DatasetProxy` | `{1D85075B-6ECB-4BE4-8317-9DDA91292ED8}` | Multi-instance |
| `ATLProjectcomserverExe.MathOperations` | `{1CE8C512-FB0A-4C47-B3CD-44219BDC8DDF}` | Multi-instance |

**AppID:** `{B0A0188F-59B6-42A5-AD3A-9D3CBE079253}`
**TypeLib:** Zelfde GUID als AppID.

De registratie kan geverifieerd worden met:
```powershell
.\scripts\Invoke-ComDiagnostics.ps1
```

---

## Bestandsoverzicht

### COM Interfaces & IDL
| Bestand | Beschrijving |
|---|---|
| `ATLProjectcomserver.idl` | Interface Definition Language — definieert `IMathOperations`, `ISharedValue`, `ISharedValueCallback`, `IEventCallback`, `IDatasetProxy` en de TypeLib. Gecompileerd door MIDL. |
| `ATLProjectcomserver_i.h / _i.c` | Door MIDL gegenereerde interface headers en GUID constanten (`CLSID_SharedValue`, `IID_IDatasetProxy`, etc.). |
| `ATLProjectcomserver_p.c` | Door MIDL gegenereerde proxy/stub code — essentieel voor RPC marshaling. Bevat de NDR serialisatie-routines voor elk interface-parameter. |
| `ATLProjectcomserver.tlh / .tli` | Door `#import` gegenereerde type library wrappers met smart pointers (`ISharedValuePtr`). Alleen gebruikt door `stop_server.cpp`. |
| `dlldata.c` | Proxy/stub DLL registratie data (COM vereist dit voor custom marshaling). |

### COM Klasse-implementaties
| Bestand | Beschrijving |
|---|---|
| `SharedValue.h / .cpp` | `CSharedValue` — ATL wrapper rondom `SharedValueV2::SharedValue<CComVariant, LocalMutexPolicy>`. Singleton (`DECLARE_CLASSFACTORY_SINGLETON`). Bevat `SharedValueEventBridge` voor EventBus→COM vertaling. `ShutdownServer()` beëindigt het proces via `_AtlModule.Unlock()`. |
| `DatasetProxy.h / .cpp` | `CDatasetProxy` — ATL wrapper rondom `SharedValueV2::DatasetStore<std::wstring>`. Bevat `ComEventBridge` voor dataset-events. `FetchPageKeys` bouwt een `CComSafeArray<VARIANT>`, `FetchPageData` bouwt een 2D `SAFEARRAY`. |
| `MathOperations.h / .cpp` | `CMathOperations` — Stateless wiskundige bewerkingen. Geen afhankelijkheid op SharedValueV2. Threading model: `CComMultiThreadModel`. |
| `SharedValueCallback.h / .cpp` | `CSharedValueCallback` — Ontvanger-interface. Clients implementeren deze interface, de server roept `OnValueChanged()` / `OnDateTimeChanged()` aan. |
| `ComErrorHelper.h` | `COM_METHOD_BODY` macro — centraal punt voor exception → `HRESULT + IErrorInfo` vertaling. Vangt `SharedValueException`, `std::out_of_range`, `std::invalid_argument`, `std::exception`, en fallback `catch(...)`. |

### EXE Entry Point & Lifecycle
| Bestand | Beschrijving |
|---|---|
| `dllmain.cpp` | `_tWinMain()` entry point. Instantieert `CATLProjectcomserverExeModule` en start de Win32 message loop. `_AtlModule.Lock()` houdt het proces in leven totdat `ShutdownServer()` wordt aangeroepen. |
| `dllmain.h` | Declaratie van `CATLProjectcomserverExeModule : public CAtlExeModuleT<...>`. Bevat `DECLARE_LIBID` en `DECLARE_REGISTRY_APPID_RESOURCEID`. |

### Shutdown Tools
| Bestand | Beschrijving |
|---|---|
| `stop_server.cpp` | C++ CLI tool die via `#import` en smart pointers (`ISharedValuePtr`) verbindt met de EXE server en `ShutdownServer()` aanroept. Gebruikt `CoInitializeEx(COINIT_APARTMENTTHREADED)`. |
| `stop_server.vbs` | VBScript one-liner equivalent: `CreateObject("ATLProjectcomserverExe.SharedValue").ShutdownServer()`. |

### ATL Infrastructure
| Bestand | Beschrijving |
|---|---|
| `pch.h / pch.cpp` | Precompiled header — trekt `<atlbase.h>`, `<atlcom.h>` en Windows headers in. |
| `targetver.h` | Windows SDK versie targeting (`_WIN32_WINNT`). |
| `resource.h` | Resource ID's: `IDR_ATLPROJECTCOMSERVER`, `IDR_SHAREDVALUE`, `IDR_DATASETPROXY`, `IDR_MATHOPERATIONS`. |
| `ATLProjectcomserver.rc` | Windows resource bestand — embedt de TypeLib (`.tlb`) en versie-informatie. |
| `ATLProjectcomserver.def` | Module definition file — exporteert `DllGetClassObject`, `DllCanUnloadNow`, etc. (gebruikt door de proxy/stub DLL). |

### Registry Scripts (`.rgs`)
| Bestand | Beschrijving |
|---|---|
| `ATLProjectcomserver.rgs` | Registreert de AppID `{B0A0188F-...}` en het pad naar de EXE als `LocalServer32`. |
| `SharedValue.rgs` | Registreert ProgID `ATLProjectcomserverExe.SharedValue` → CLSID `{A5B21149-...}`. |
| `DatasetProxy.rgs` | Registreert ProgID `ATLProjectcomserverExe.DatasetProxy` → CLSID `{1D85075B-...}`. |
| `MathOperations.rgs` | Registreert ProgID `ATLProjectcomserverExe.MathOperations` → CLSID `{1CE8C512-...}`. |
| `SharedValueCallback.rgs` | Registreert ProgID `ATLProjectcomserverExe.SharedValueCallback` → CLSID `{6818DD57-...}`. |

### Build & Solution
| Bestand | Beschrijving |
|---|---|
| `ATLProjectcomserver.vcxproj` | MSVC project — `ConfigurationType=Application`, `SubSystem=Windows`. `AdditionalIncludeDirectories` bevat `$(SolutionDir)` voor SharedValueV2 headers. |
| `ATLProjectcomserver.sln` | Standalone solution voor het bouwen van alleen de EXE. |
| `.clangd` | Clangd LSP config — extra include pad naar SharedValueV2. |
| `.gitignore` | Lokale ignores: `x64/`, `*.obj`, `*.exe`, `*.pdb`, etc. |

### Subdirectories
| Directory | Beschrijving |
|---|---|
| [`tests/`](tests/) | Cross-process integratietests — `Run-CrossProcessTests.ps1` + C# .NET 8. |

---

## Compilatie

### Via de centrale solution (aanbevolen)

```powershell
# Initialiseer MSVC build-omgeving
. Invoke-BuildEnvironment.ps1 -Toolchain MSVC -Version "Enterprise 2022" -Architecture x64

# Bouw alle projecten (Legacy DLL + EXE Server)
msbuild ATLProjectcomserver.sln /p:Configuration=Debug /p:Platform=x64 -m
```

### Via de standalone EXE solution

```powershell
msbuild ATLProjectcomserverExe\ATLProjectcomserver.sln /p:Configuration=Debug /p:Platform=x64 -m
```

De EXE wordt geproduceerd in `x64\Debug\ATLProjectcomserver.exe`.

---

## Registratie

```cmd
:: Registreren (als Administrator)
x64\Debug\ATLProjectcomserver.exe /RegServer

:: De-registreren
x64\Debug\ATLProjectcomserver.exe /UnregServer

:: Verifiëren
powershell .\scripts\Invoke-ComDiagnostics.ps1
```

---

## Gebruik

### VBScript — De EXE start automatisch on-demand

```vbscript
Set sv = CreateObject("ATLProjectcomserverExe.SharedValue")
sv.SetValue "Cross-process data!"
WScript.Echo sv.GetValue()

' DatasetProxy ophalen en bevragen
Set proxy = sv.GetValue()
proxy.AddRow "key1", "waarde1"
WScript.Echo proxy.GetRecordCount()    ' 1
```

### C# .NET — Via late binding

```csharp
dynamic sv = Activator.CreateInstance(
    Type.GetTypeFromProgID("ATLProjectcomserverExe.SharedValue")!);
sv.SetValue("Hallo vanuit C#!");
Console.WriteLine(sv.GetValue());

// DatasetProxy
dynamic proxy = sv.GetValue();
proxy.AddRow("server01", "status:online|cpu:45%");
Console.WriteLine(proxy.GetRecordCount());  // 1
```

### C++ — Via #import smart pointers

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

De EXE server blijft draaien totdat expliciet afgesloten:

```powershell
# Via VBScript
cscript stop_server.vbs

# Via gecompileerde C++ tool
.\stop_server.exe

# Via PowerShell one-liner
$sv = New-Object -ComObject "ATLProjectcomserverExe.SharedValue"
$sv.ShutdownServer()
[System.Runtime.InteropServices.Marshal]::ReleaseComObject($sv) | Out-Null
```

---

## Testen

Zie [`tests/README.md`](tests/) voor de volledige testdocumentatie.

```powershell
# Geautomatiseerde cross-process suite (6 scenario's)
.\tests\Run-CrossProcessTests.ps1

# C# .NET 8 integratietests (14 tests)
cd tests\CSharpNet8Test && dotnet run
```

## Gerelateerde Documentatie

- [ARCHITECTURE.md](ARCHITECTURE.md) — Diepgaande technische architectuur van de Out-of-Process EXE COM Server, iteratie met RPC marshaling en de singleton lifecycle.
- [INSTALL.md](INSTALL.md) — Installatie-, registratie- en configuratiegids voor de EXE server.
- [README.md](../README.md) — Hoofddocumentatie en startpunt van het gehele project.
- [ARCHITECTURE.md](../ARCHITECTURE.md) — Hoofd architectuurdocument voor het gehele COM Server project.
- [README.md](../SharedValueV2/README.md) — Introductie en overzicht van de SharedValueV2 C++20 engine.
