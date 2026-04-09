# ATL COM Server & SharedValue — Monorepo

**Versie:** 0.2.0

Een Windows C++ project voor het veilig uitwisselen en persistent bewaren van gedeelde variabelen tussen onafhankelijke processen. Het project biedt twee generaties:

- **SharedValueV2** — COM/RPC-gebaseerde engine met ATL Out-of-Process EXE Server.
- **SharedValueV3 (MemMap)** — Ultra-fast Memory-Mapped Files engine met FlatBuffers, zonder COM-afhankelijkheid.

## Projectoverzicht

### COM Server (V2)

Dit project levert **twee varianten** van dezelfde COM Server:

| Variant | Type | Registratie | Locatie |
|---|---|---|---|
| **Legacy DLL** (InprocServer32) | In-Process DLL | `regsvr32` | [`ATLProjectcomserverLegacy/`](ATLProjectcomserverLegacy/) |
| **EXE Server** (LocalServer32) | Out-of-Process EXE | Zelf-registrerend | [`ATLProjectcomserverExe/`](ATLProjectcomserverExe/) |

De EXE-variant is het **primaire productiemodel**: het draait als een zelfstandig Windows-proces waarmee meerdere onafhankelijke clients (C#, VBScript, Python) tegelijkertijd data delen via COM/RPC marshaling.

### Memory-Mapped Engine (V3)

| Component | Taal | Doel | Locatie |
|---|---|---|---|
| **C++ Producer** | C++20 | Schrijft FlatBuffer-datasets naar shared memory | [`SharedValueV3_MemMap/cpp_core/`](SharedValueV3_MemMap/cpp_core/) |
| **C# Consumer** | .NET 9 | Luistert via Named Events en ontvangt callbacks | [`SharedValueV3_MemMap/csharp_core/`](SharedValueV3_MemMap/csharp_core/) |

De V3-engine omzeilt COM volledig. Data wordt gedeeld via een Windows Memory-Mapped File (10 MB kernel page), gesynchroniseerd met Named Mutexes, en gesignaleerd via Named Events — met **nanoseconde-latency** en **0% CPU bij idle**.

## COM Interfaces

Beide varianten exposeren dezelfde drie interfaces:

1. **`IMathOperations`** — Stateless rekenkundige bewerkingen (Add, Subtract, Multiply, Divide).
2. **`ISharedValue`** — Singleton in-memory state: `GetValue`, `SetValue`, `LockSharedValue`, observer-subscripties, en `ShutdownServer` (alleen EXE).
3. **`IDatasetProxy`** — Pagineerbare key-value dataset: `AddRow`, `GetRowData`, `UpdateRow`, `RemoveRow`, `FetchPageKeys`, `FetchPageData`, met configureerbare `StorageMode`.
4. **`ISharedValueCallback`** — Observer-interface voor live event-notificaties bij state-wijzigingen.

## Directorystructuur

```
cursor_com_test/
├── ATLProjectcomserver.sln          # Centrale Visual Studio Solution (alle projecten)
├── ATLProjectcomserverLegacy/       # Legacy In-Process DLL COM Server
├── ATLProjectcomserverExe/          # Out-of-Process EXE COM Server (productie)
├── SharedValueV2/                   # C++20 header-only core library (COM engine)
├── SharedValueV3_MemMap/            # Memory-Mapped FlatBuffers engine (V3)
│   ├── schema/                      #   FlatBuffers schema definitie
│   ├── cpp_core/                    #   C++ native producer
│   ├── csharp_core/                 #   C# managed consumer
│   ├── ARCHITECTURE.md              #   Uitgebreide architectuur met Mermaid diagrammen
│   └── build_schema.ps1             #   Automatische flatc download & codegen
├── scripts/                         # PowerShell diagnostische tools
├── docs/                            # Ontwerp- en architectuurdocumentatie
├── tests/                           # Cross-language integratietests
├── ARCHITECTURE.md                  # Technische architectuur & Design Patterns
├── CHANGELOG.md                     # Wijzigingshistorie
└── INSTALL.md                       # Compilatie- en installatie-instructies
```

## Snel Bouwen

### Vereisten
- **Visual Studio 2022** met workload `Desktop development with C++` en component `C++ ATL for latest v143 build tools`.
- **CMake ≥ 3.20** (alleen voor SharedValueV2 standalone tests).

### Compilatie via Command Line
```powershell
# Laad de MSVC build-omgeving
. Invoke-BuildEnvironment.ps1 -Toolchain MSVC -Version "Enterprise 2022" -Architecture x64

# Bouw de gehele solution (Legacy DLL + EXE Server + testtool)
msbuild ATLProjectcomserver.sln /p:Configuration=Debug /p:Platform=x64 -m
```

### Compilatie via Visual Studio
1. Open `ATLProjectcomserver.sln`.
2. Selecteer `Debug | x64`.
3. Build → Build Solution (`F7`).

## COM Server Registreren

```cmd
:: Legacy DLL (als Administrator)
regsvr32 x64\Debug\ATLProjectcomserver.dll

:: EXE Server (zelf-registrerend, als Administrator)
x64\Debug\ATLProjectcomserver.exe /RegServer
```

## Testen

Zie [`tests/README.md`](tests/README.md) voor het volledige testoverzicht, of draai de geautomatiseerde cross-process suite:

```powershell
.\ATLProjectcomserverExe\tests\Run-CrossProcessTests.ps1
```

## Documentatie

- [ARCHITECTURE.md](ARCHITECTURE.md) — Technische architectuur, Design Patterns & lagen.
- [INSTALL.md](INSTALL.md) — Gedetailleerde bouw- en installatie-instructies.
- [CHANGELOG.md](CHANGELOG.md) — Volledig wijzigingsoverzicht.
- [docs/](docs/) — Ontwerpdocumenten en migratie-analyses.
- [ATLProjectcomserverExe/README.md](ATLProjectcomserverExe/README.md) — Gebruikershandleiding voor de EXE COM Server variant.
- [SharedValueV2/README.md](SharedValueV2/README.md) — Introductie en overzicht van de SharedValueV2 C++20 engine.
- [SharedValueV3_MemMap/README.md](SharedValueV3_MemMap/README.md) — Quickstart voor de ultra-fast Memory-Mapped FlatBuffers engine (V3).
- [SharedValueV3_MemMap/ARCHITECTURE.md](SharedValueV3_MemMap/ARCHITECTURE.md) — Uitgebreid V3-architectuurdocument met Mermaid-diagrammen.
