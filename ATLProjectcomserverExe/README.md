# ATLProjectcomserverExe — Out-of-Process EXE COM Server

Dit is de **productie-variant** van de ATL COM Server, gebouwd als een **Out-of-Process EXE** (`LocalServer32`). De server draait als een zelfstandig Windows-proces dat via RPC-marshaling wordt aangesproken door meerdere onafhankelijke client-processen.

## Rol binnen het project

Dit is de **aanbevolen architectuur** voor cross-process data-sharing:
- Eén centraal serverproces beheert de gedeelde state (singleton).
- Meerdere clients (C#, VBScript, Python, C++) communiceren gelijktijdig via Windows COM/RPC.
- Graceful shutdown via `ISharedValue::ShutdownServer()`.

## Bestandsoverzicht

### COM Interfaces & IDL
| Bestand | Beschrijving |
|---|---|
| `ATLProjectcomserver.idl` | Interface Definition Language — COM interfaces inclusief `ShutdownServer()` methode. |
| `ATLProjectcomserver_i.h / _i.c` | Door MIDL gegenereerde headers en GUIDs. |
| `ATLProjectcomserver_p.c` | Door MIDL gegenereerde proxy/stub code. |
| `ATLProjectcomserver.tlh / .tli` | Door `#import` gegenereerde type library wrappers (voor `stop_server.cpp`). |
| `dlldata.c` | Proxy/stub DLL data. |

### COM Klasse-implementaties
| Bestand | Beschrijving |
|---|---|
| `SharedValue.h / .cpp` | `ISharedValue` implementatie met `ShutdownServer()` voor graceful EXE shutdown. Gebruikt `SharedValueV2` core via `$(SolutionDir)\SharedValueV2\include\`. |
| `DatasetProxy.h / .cpp` | `IDatasetProxy` — pagineerbare key-value store, server-side geïnstantieerd als singleton. |
| `MathOperations.h / .cpp` | `IMathOperations` — stateless rekenkundige bewerkingen. |
| `SharedValueCallback.h / .cpp` | `ISharedValueCallback` — observer proxy voor event-notificaties. |
| `ComErrorHelper.h` | Exception-naar-HRESULT vertaallaag. |

### EXE Entry Point & Lifecycle
| Bestand | Beschrijving |
|---|---|
| `dllmain.cpp` | EXE entry point (`_tWinMain`) met `CAtlExeModuleT`. Bevat `_AtlModule.Lock()` om voortijdig afsluiten te voorkomen. |
| `dllmain.h` | Declaratie van de `CATLProjectcomserverExeModule` klasse. |

### Shutdown Tools
| Bestand | Beschrijving |
|---|---|
| `stop_server.cpp` | C++ CLI tool die via COM verbindt en `ShutdownServer()` aanroept. |
| `stop_server.vbs` | VBScript equivalent van de shutdown tool. |

### ATL Infrastructure
| Bestand | Beschrijving |
|---|---|
| `pch.h / pch.cpp` | Precompiled header. |
| `targetver.h` | Windows SDK versie targeting. |
| `resource.h` | Resource ID definities. |
| `ATLProjectcomserver.rc` | Windows resource bestand. |
| `ATLProjectcomserver.def` | Module definition file. |

### Registry Scripts
| Bestand | Beschrijving |
|---|---|
| `ATLProjectcomserver.rgs` | AppID en TypeLib registratie. |
| `SharedValue.rgs` | CLSID/ProgID registratie (`ATLProjectcomserverExe.SharedValue`). |
| `DatasetProxy.rgs` | CLSID/ProgID registratie (`ATLProjectcomserverExe.DatasetProxy`). |
| `MathOperations.rgs` | CLSID/ProgID registratie. |
| `SharedValueCallback.rgs` | CLSID/ProgID registratie. |

### Build & Solution
| Bestand | Beschrijving |
|---|---|
| `ATLProjectcomserver.vcxproj` | Visual Studio C++ projectbestand (ConfigurationType=Application). |
| `ATLProjectcomserver.sln` | Standalone solution (voor onafhankelijk bouwen van alleen de EXE). |
| `.clangd` | Clangd LSP configuratie voor IntelliSense in deze subdir. |
| `.gitignore` | Lokale gitignore voor build-artefacten. |

### Documentatie
| Bestand | Beschrijving |
|---|---|
| `ARCHITECTURE.md` | Architectuurbeschrijving (gedeeld met root). |
| `INSTALL.md` | Installatie-instructies (gedeeld met root). |
| `README.md` | Dit bestand. |

### Subdirectories
| Directory | Beschrijving |
|---|---|
| [`tests/`](tests/) | Cross-process integratietests (PowerShell + C#). |

## Compilatie

```powershell
# Via de centrale solution (vanuit project root):
msbuild ATLProjectcomserver.sln /p:Configuration=Debug /p:Platform=x64 -m

# Of via de standalone EXE solution:
msbuild ATLProjectcomserverExe\ATLProjectcomserver.sln /p:Configuration=Debug /p:Platform=x64 -m
```

De EXE wordt geproduceerd in `x64\Debug\ATLProjectcomserver.exe`.

## Registratie

```cmd
:: Registreren (als Administrator)
x64\Debug\ATLProjectcomserver.exe /RegServer

:: De-registreren
x64\Debug\ATLProjectcomserver.exe /UnregServer
```

## Gebruik

```vbscript
' VBScript — de EXE start automatisch on-demand
Set sv = CreateObject("ATLProjectcomserverExe.SharedValue")
sv.SetValue "Cross-process data!"
WScript.Echo sv.GetValue()
```

```csharp
// C# — via late binding
dynamic sv = Activator.CreateInstance(
    Type.GetTypeFromProgID("ATLProjectcomserverExe.SharedValue")!);
sv.SetValue("Hallo vanuit C#!");
Console.WriteLine(sv.GetValue());
```

## Graceful Shutdown

De EXE server blijft draaien totdat expliciet afgesloten:

```powershell
# Via VBScript
cscript stop_server.vbs

# Via gecompileerde C++ tool
.\stop_server.exe
```