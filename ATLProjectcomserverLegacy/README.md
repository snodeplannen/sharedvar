# ATLProjectcomserverLegacy — In-Process DLL COM Server

This represents the **original** ATL COM Server implementation, structured natively as an **In-Process DLL** (`InprocServer32`). The DLL loads directly into the memory footprint mapping the calling process via `regsvr32`.

> **Note:** This specific variant prevents sharing memory spanning processes securely. Targeting robust cross-process data-sharing, the [EXE Server](../ATLProjectcomserverExe/) branch serves as the recommended overarching solution.

## Role within the project

This legacy server serves explicitly as:
- **Reference implementation** tracking the original InprocServer32 architecture identically.
- **Local testing backend** addressing scenarios prioritizing execution speed overshadowing cross-process isolation completely.
- **Foundational baseline** from which the EXE Server variant (`ATLProjectcomserverExe`) was securely migrated.

## File Overview

### COM Interfaces & IDL
| File | Description |
|---|---|
| `ATLProjectcomserver.idl` | Interface Definition Language — formally defining all sweeping COM interfaces (`IMathOperations`, `ISharedValue`, `IDatasetProxy`, `ISharedValueCallback`) tracking respective GUIDs. |
| `ATLProjectcomserver_i.h` | Generated directly via MIDL embedding interface declarations alongside corresponding CLSIDs. |
| `ATLProjectcomserver_i.c` | Generated natively via MIDL encompassing GUIDs (IIDs and CLSIDs perfectly). |
| `ATLProjectcomserver_p.c` | Generated smoothly via MIDL tracking proxy/stub codegen targeting marshaling. |
| `dlldata.c` | Proxy/stub DLL routing data securely generated via MIDL logic loops. |

### COM Class Implementations
| File | Description |
|---|---|
| `SharedValue.h / .cpp` | Implementation anchoring `ISharedValue`. Reliably wraps the underlying C++20 `SharedValueV2::SharedValue` core seamlessly within an ATL COM object securely. |
| `DatasetProxy.h / .cpp` | Implementation centering `IDatasetProxy`. Operating deeply as a pageable key-value store leveraging `AddRow`, `GetRowData`, `FetchPageKeys`, strings natively. |
| `MathOperations.h / .cpp` | Implementation defining `IMathOperations`. Providing strictly stateless core mathematical computations naturally. |
| `SharedValueCallback.h / .cpp` | Implementation targeting `ISharedValueCallback`. Operating distinctly as an observer proxy pushing event-notifications securely. |
| `ComErrorHelper.h` | Defines the `COM_METHOD_BODY` macro inherently translating `SharedValueV2` exceptions directly onto `HRESULT` + `IErrorInfo` COM cascades completely seamlessly. |

### ATL Infrastructure
| File | Description |
|---|---|
| `dllmain.cpp / .h` | Outlines the DLL entry point routing bridging `CAtlDllModuleT` ATL module configurations. |
| `pch.h / pch.cpp` | Tracks precompiled header mapping (targeting standard ATL base includes identically). |
| `targetver.h` | Enforces specific Windows SDK version targeting. |
| `Resource.h` | Defines embedded resource ID configurations distinctly. |
| `ATLProjectcomserver.rc` | Tracks Windows resource mapping file constraints (parsing version-info coupled referencing type libraries internally). |
| `ATLProjectcomserver.def` | Identifies module definition boundaries referencing explicit DLL exports natively (`DllCanUnloadNow`, `DllGetClassObject`, logic mappings stringing bounds identically). |

### Registry Scripts
| File | Description |
|---|---|
| `ATLProjectcomserver.rgs` | Resolves core registration tying AppID alongside targeted TypeLib boundaries smoothly. |
| `SharedValue.rgs` | Defines distinct CLSID tracking coupled parsing ProgID registration targeting explicitly `SharedValue` logic paths. |
| `DatasetProxy.rgs` | Enforces registration paths linking CLSID + ProgID encompassing explicitly `DatasetProxy` strings cleanly. |
| `MathOperations.rgs` | Sweeps registering mapping CLSID + ProgID referencing entirely `MathOperations`. |
| `SharedValueCallback.rgs` | Secures identifying matching arrays registering CLSID + ProgID isolating exactly `SharedValueCallback`. |

### Build Constraints
| File | Description |
|---|---|
| `ATLProjectcomserver.vcxproj` | Tracks the underlying native Visual Studio C++ project file arrays smoothly. |
| `ATLProjectcomserver.vcxproj.user` | Retains user-specific localized project tuning constraints cleanly. |

## Compilation Sequence

```powershell
# Executing natively bypassing boundaries targeting the core project root:
msbuild ATLProjectcomserver.sln /p:Configuration=Debug /p:Platform=x64 -m
```

The underlying DLL compiles gracefully outputting seamlessly into `x64\Debug\ATLProjectcomserver.dll`.

## Registration Metrics

```cmd
:: Execution routines enforcing registration hooks (Executed explicitly operating as Administrator)
regsvr32 x64\Debug\ATLProjectcomserver.dll

:: Execution routines revoking overriding hooks smoothly
regsvr32 /u x64\Debug\ATLProjectcomserver.dll
```

## Usage Implementations

```vbscript
' Operating basic legacy VBScript examples efficiently
Set sv = CreateObject("ATLProjectcomserver.SharedValue")
sv.SetValue "Operating reliably directly originating spanning VBScript interfaces natively!"
WScript.Echo sv.GetValue()
```


## Related Documentation

- [README_EN.md](../README_EN.md) — Main overarching documentation routing starting endpoints mapping the sprawling encompassing project logic.
- [ARCHITECTURE_EN.md](../ARCHITECTURE_EN.md) — Fundamental tracking tracking logic defining the overarching holistic COM Server project explicitly.
- [README_EN.md](../ATLProjectcomserverExe/README_EN.md) — Dedicated user guide bounding arrays tracking the EXE COM Server explicit variant distinctly.
- [README_EN.md](../SharedValueV2/README_EN.md) — Definitive introduction sweeps bridging explicitly mapping the comprehensive SharedValueV2 C++20 engine operations securely.
