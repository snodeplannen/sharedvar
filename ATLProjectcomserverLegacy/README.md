# ATLProjectcomserverLegacy — In-Process DLL COM Server

Dit is de **oorspronkelijke** ATL COM Server implementatie, gebouwd als een **In-Process DLL** (`InprocServer32`). De DLL wordt direct in het geheugen van het aanroepende proces geladen via `regsvr32`.

> **Let op:** Deze variant deelt geen geheugen tussen processen. Voor cross-process data-sharing is de [EXE Server](../ATLProjectcomserverExe/) de aanbevolen oplossing.

## Rol binnen het project

Deze legacy server dient als:
- **Referentie-implementatie** van de originele InprocServer32 architectuur.
- **Lokale test-backend** voor scenario's waar snelheid boven cross-process isolatie gaat.
- **Basis** waaruit de EXE Server variant (`ATLProjectcomserverExe`) is gemigreerd.

## Bestandsoverzicht

### COM Interfaces & IDL
| Bestand | Beschrijving |
|---|---|
| `ATLProjectcomserver.idl` | Interface Definition Language — definieert alle COM interfaces (`IMathOperations`, `ISharedValue`, `IDatasetProxy`, `ISharedValueCallback`) en hun GUIDs. |
| `ATLProjectcomserver_i.h` | Door MIDL gegenereerde header met interface declaraties en CLSIDs. |
| `ATLProjectcomserver_i.c` | Door MIDL gegenereerde GUIDs (IIDs en CLSIDs). |
| `ATLProjectcomserver_p.c` | Door MIDL gegenereerde proxy/stub code voor marshaling. |
| `dlldata.c` | Proxy/stub DLL data, gegenereerd door MIDL. |

### COM Klasse-implementaties
| Bestand | Beschrijving |
|---|---|
| `SharedValue.h / .cpp` | Implementatie van `ISharedValue`. Wikkelt de C++20 `SharedValueV2::SharedValue` core in een ATL COM object. |
| `DatasetProxy.h / .cpp` | Implementatie van `IDatasetProxy`. Een pagineerbare key-value store met `AddRow`, `GetRowData`, `FetchPageKeys`, etc. |
| `MathOperations.h / .cpp` | Implementatie van `IMathOperations`. Stateless wiskundige bewerkingen. |
| `SharedValueCallback.h / .cpp` | Implementatie van `ISharedValueCallback`. Observer proxy voor event-notificaties. |
| `ComErrorHelper.h` | `COM_METHOD_BODY` macro die `SharedValueV2` exceptions vertaalt naar `HRESULT` + `IErrorInfo`. |

### ATL Infrastructure
| Bestand | Beschrijving |
|---|---|
| `dllmain.cpp / .h` | DLL entry point met `CAtlDllModuleT` ATL module. |
| `pch.h / pch.cpp` | Precompiled header (ATL base includes). |
| `targetver.h` | Windows SDK versie targeting. |
| `Resource.h` | Resource ID definities. |
| `ATLProjectcomserver.rc` | Windows resource bestand (versie-info, type library). |
| `ATLProjectcomserver.def` | Module definition file voor DLL exports (`DllCanUnloadNow`, `DllGetClassObject`, etc.). |

### Registry Scripts
| Bestand | Beschrijving |
|---|---|
| `ATLProjectcomserver.rgs` | Registratie van de AppID en TypeLib. |
| `SharedValue.rgs` | CLSID en ProgID registratie voor `SharedValue`. |
| `DatasetProxy.rgs` | CLSID en ProgID registratie voor `DatasetProxy`. |
| `MathOperations.rgs` | CLSID en ProgID registratie voor `MathOperations`. |
| `SharedValueCallback.rgs` | CLSID en ProgID registratie voor `SharedValueCallback`. |

### Build
| Bestand | Beschrijving |
|---|---|
| `ATLProjectcomserver.vcxproj` | Visual Studio C++ projectbestand. |
| `ATLProjectcomserver.vcxproj.user` | Gebruikersspecifieke projectinstellingen. |

## Compilatie

```powershell
# Vanuit de project root:
msbuild ATLProjectcomserver.sln /p:Configuration=Debug /p:Platform=x64 -m
```

De DLL wordt geproduceerd in `x64\Debug\ATLProjectcomserver.dll`.

## Registratie

```cmd
:: Registreren (als Administrator)
regsvr32 x64\Debug\ATLProjectcomserver.dll

:: De-registreren
regsvr32 /u x64\Debug\ATLProjectcomserver.dll
```

## Gebruik

```vbscript
' VBScript voorbeeld
Set sv = CreateObject("ATLProjectcomserver.SharedValue")
sv.SetValue "Hallo vanuit VBScript!"
WScript.Echo sv.GetValue()
```
