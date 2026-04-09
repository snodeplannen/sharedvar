# ATL COM Server & SharedValueV2 — Monorepo

Een Windows C++ ATL/COM project voor het veilig uitwisselen en persistent bewaren van gedeelde variabelen (`VARIANT`) tussen onafhankelijke processen. Intern aangedreven door een moderne, thread-safe, **C++20 header-only core** (`SharedValueV2`).

## Projectoverzicht

Dit project levert **twee varianten** van dezelfde COM Server:

| Variant | Type | Registratie | Locatie |
|---|---|---|---|
| **Legacy DLL** (InprocServer32) | In-Process DLL | `regsvr32` | [`ATLProjectcomserverLegacy/`](ATLProjectcomserverLegacy/) |
| **EXE Server** (LocalServer32) | Out-of-Process EXE | Zelf-registrerend | [`ATLProjectcomserverExe/`](ATLProjectcomserverExe/) |

De EXE-variant is het **primaire productiemodel**: het draait als een zelfstandig Windows-proces waarmee meerdere onafhankelijke clients (C#, VBScript, Python) tegelijkertijd data delen via COM/RPC marshaling.

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
├── SharedValueV2/                   # C++20 header-only core library
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

## Verdere Documentatie

- [ARCHITECTURE.md](ARCHITECTURE.md) — Technische architectuur, Design Patterns & lagen.
- [INSTALL.md](INSTALL.md) — Gedetailleerde bouw- en installatie-instructies.
- [CHANGELOG.md](CHANGELOG.md) — Volledig wijzigingsoverzicht.
- [docs/](docs/) — Ontwerpdocumenten en migratie-analyses.