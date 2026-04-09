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

## Vergelijking van de Drie Varianten

Dit project bevat drie architectureel verschillende benaderingen voor cross-process data sharing. Elk heeft zijn eigen sterktes, beperkingen en ideale toepassingsgebieden.

### Overzichtstabel

| Eigenschap | Legacy DLL (InprocServer32) | EXE Server + SharedValueV2 | SharedValueV3 MemMap |
|---|---|---|---|
| **Procesmodel** | In-process (geladen in client) | Out-of-process (eigen EXE) | Geen server nodig |
| **Transport** | Direct function call | RPC over Named Pipes | Direct shared memory |
| **Serialisatie** | Geen (zelfde adresruimte) | VARIANT / BSTR / SAFEARRAY | FlatBuffers (zero-copy) |
| **Latency per call** | ~1 ns (in-proc) | ~1-10 μs (RPC marshaling) | ~10-100 ns (pointer read) |
| **Cross-process** | ❌ Nee | ✅ Ja | ✅ Ja |
| **Multi-client** | ❌ Nee (per-process instantie) | ✅ Ja (singleton server) | ✅ Ja (kernel object sharing) |
| **Callbacks/Events** | COM Connection Points | COM IEventCallback (RPC) | Named Events (0% CPU) |
| **Dynamische data** | `std::vector`, `BSTR` | `VARIANT`, `SAFEARRAY` | FlatBuffer tables (onbeperkt genest) |
| **Schema evolutie** | Handmatig (C++ headers) | COM IDL / TypeLib | FlatBuffers `.fbs` (forward/backward) |
| **Registratie vereist** | Ja (`regsvr32`) | Ja (`/RegServer`) | ❌ Nee |
| **Admin-rechten nodig** | Ja (registratie) | Ja (registratie) | Nee (of `Global\` namespace*) |
| **Taalondersteuning** | Elke COM-compatibele taal | Elke COM-compatibele taal | Elke taal met FlatBuffers + OS API |
| **Thread safety** | `std::mutex` (in-process) | `std::mutex` (in-process) | Named Mutex (cross-process) |
| **COM-afhankelijkheid** | Volledig | Volledig | ❌ Geen |

> \* De `Global\` namespace voor Named Kernel Objects vereist standaard `SeCreateGlobalPrivilege`, wat beschikbaar is voor Administrators en services.

---

### 1. Legacy DLL — InprocServer32

De oorspronkelijke COM DLL wordt direct in de adresruimte van het aanroepende proces geladen.

**✅ Voordelen:**
- Snelste mogelijke aanroep (~1 ns): geen marshaling, geen context switch.
- Eenvoudigste debugging — breakpoints werken direct in het client-proces.
- Geen aparte server te starten of te beheren.

**❌ Nadelen:**
- **Geen cross-process sharing**: elke client krijgt een eigen kopie van de DLL in zijn eigen geheugen.
- Crasht de DLL → crasht het hele client-proces.
- Bitness-restrictie: een 64-bit client kan geen 32-bit DLL laden (en andersom).
- Vereist `regsvr32`-registratie met admin-rechten.

**🎯 Wanneer gebruiken:**
- Prototyping en snelle experimenten op één machine, binnen één proces.
- Legacy compatibiliteit met bestaande VBScript of Office VBA macro's die een in-process COM object verwachten.

---

### 2. EXE Server + SharedValueV2 — LocalServer32

Een zelfstandig Windows-proces dat als COM Server optreedt. Alle communicatie verloopt via RPC marshaling.

**✅ Voordelen:**
- **Echte cross-process sharing**: meerdere clients (C#, VBScript, Python) delen dezelfde singleton state.
- Proces-isolatie: crash van de server raakt de client niet direct.
- Volledige COM-infrastructuur: interfaces, TypeLibs, Connection Points, proxy/stub-registratie.
- Breed taalondersteuning — alles dat COM spreekt, kan meedoen.
- Dataset-batching via `SAFEARRAY` beperkt het aantal RPC calls bij bulk reads.

**❌ Nadelen:**
- RPC overhead (~1-10 μs per call) maakt per-record toegang tot grote datasets traag.
- COM-registratie vereist admin-rechten en strakke setup.
- Complexe levenscyclus: server lifecycle, reference counting, graceful shutdown moeten allen correct beheerd worden.
- Schema-wijzigingen vereisen IDL-aanpassingen, hercompilatie van proxy/stubs, en re-registratie.

**🎯 Wanneer gebruiken:**
- Wanneer **meerdere clients in verschillende talen** (C#, VBScript, Python) dezelfde live state moeten delen.
- Wanneer je bestaande COM-infrastructuur hebt en hierop wilt voortbouwen.
- Wanneer de data-uitwisselingsfrequentie laag tot matig is (< 1000 calls/sec) en je profiteert van batching.
- Wanneer je een **rijke interface** nodig hebt met methods, properties en events in één framework.

---

### 3. SharedValueV3 MemMap — Memory-Mapped Files + FlatBuffers

Directe geheugenuitwisseling via de Windows kernel, zonder tussenkomst van COM of RPC.

**✅ Voordelen:**
- **Nanoseconde-latency**: data is letterlijk hetzelfde geheugenblok in twee processen.
- **Zero-copy deserialisatie**: FlatBuffers hoeft niet geparsed te worden — veldtoegang is een pointer offset.
- **0% CPU bij idle**: de consumer-thread slaapt in de kernel tot een Named Event afgaat.
- Geen COM-registratie, geen admin-rechten (tenzij `Global\` namespace), geen TypeLibs.
- **Schema evolutie**: FlatBuffers ondersteunt het toevoegen van velden zonder backward-compatibility te breken.
- Onbeperkte nesting en dynamische arrays — niet beperkt tot POD/fixed-size structs.

**❌ Nadelen:**
- Geen ingebouwde method-calls of RPC: de consumer leest alleen data, kan niet direct functies aanroepen op de producer.
- De producer **moet eerst draaien** om het Memory-Mapped File aan te maken. De consumer wacht via een retry-loop.
- Unidirectioneel ontwerp (producer → consumer). Bidirectioneel vereist een tweede gedeeld geheugenblok.
- Geen automatische proxygeneratie voor willekeurige talen — client code moet handmatig de platform API's aanroepen.
- Schema-wijzigingen vereisen `flatc`-hercompilatie van de `.fbs` en rebuild van beide projecten.

**🎯 Wanneer gebruiken:**
- **High-frequency data feeds**: sensor data, real-time telemetrie, financiële tickers — waar duizenden updates per seconde nodig zijn.
- Wanneer je **maximale snelheid** nodig hebt en bereid bent COM-abstracties op te geven.
- Wanneer een C++ backend continu data produceert die een C# frontend (of meerdere consumers) moet tonen.
- Wanneer je **geen COM-registratie** wilt of kunt uitvoeren (bijv. in portable of containerized deployments).

---

### Beslisboom

```
Heb je cross-process data sharing nodig?
├── Nee → Legacy DLL (snelst, simpelst)
└── Ja
    ├── Heb je method calls / RPC nodig vanuit de client?
    │   └── Ja → EXE Server + SharedValueV2
    ├── Heb je multi-language clients (VBScript, Python, C#)?
    │   └── Ja → EXE Server + SharedValueV2
    └── Wil je maximale snelheid, unidirectionele data push?
        └── Ja → SharedValueV3 MemMap
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
