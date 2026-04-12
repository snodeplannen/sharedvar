# SharedValueV2 — C++20 Header-Only Core Library

Een volledig thread-safe, header-only C++20 library die de kern-logica levert voor gedeelde variabelen en dataset-opslag. Dit is de **engine** die door zowel de Legacy DLL als de EXE COM Server wordt ingezet.

## Rol binnen het project

`SharedValueV2` is de **enige bron van waarheid** (Single Source of Truth) voor alle business-logica binnen het COM-ecosysteem. De ATL COM wrappers in zowel [`ATLProjectcomserverLegacy/`](../ATLProjectcomserverLegacy/) als [`ATLProjectcomserverExe/`](../ATLProjectcomserverExe/) delegeren interne calls direct naar deze headers. 

Het scheiden van de C++ core van de COM-laag biedt enorme voordelen:
- **Testbaarheid**: De core kan exhaustief getest worden (inclusief multithreaded edge-cases) via standaard C++ test frameworks, zonder COM initialisatie (`CoInitialize`), registry keys of proxy/stub overhead.
- **Herbruikbaarheid**: Deze library kan feilloos worden geïntegreerd in andere C++ applicaties (ongeacht of deze COM gebruiken of niet).
- **Performance**: Omdat het header-only is, geniet de compiler van maximale inlining, wat resulteert in zero-cost abstractions buiten de lock-grenzen en events.

## Kernconcepten en Architectuur

De bibliotheek leunt op moderne C++ principes en maakt robuust gebruik van geavanceerde design patterns.

### 1. Monitor Pattern (Safety First)
Data en synchronisatie zijn onlosmakelijk aan elkaar gekoppeld. Variabelen worden genest in een monitor structuur. State is *uitsluitend* toegankelijk via interne C++ RAII locks (bijvoorbeeld binnen `.get()` of `.set()`). Zodra de variabele bewerkt of gelezen wordt, bezit de thread gegarandeerd de actieve lock, welke automatisch losgelaten wordt zodra de bewerking uit scope gaat.

### 2. Observer Pattern (Deadlock-Free)
Variabelen en Datasets zenden notificaties uit zodra de data wijzigt (publish/subscribe EventBus). 
**Cruciaal:** callbacks naar observers worden geresolueerd en verstuurd *onmiddellijk nadat de mutex is ontgrendeld*. De eventbus kopieert eerst de lijst van luisteraars terwijl hij de lock nog houdt deelt ze daarna asynchroon mede. Dit strakke patroon voorkomt de beruchte deadlocks waarbij thread A achter een lock nog notificaties verstuurt en wacht op thead B, terwijl thead B een event krijgt en tegelijkertijd re-entrant dezelfde block probeert in te komen.

### 3. Policy-Based Design (Flexibele Locks)
Niet elke situatie vereist een full-os OS-level Kernel Mutex. De generieke template `SharedValue<T, LockPolicy>` staat toe dat je de vergrendelingsstrategie compile-time injecteert afhankelijk van je usecase:
- **`LocalMutexPolicy`**: Maakt gebruik van de ultra-snelle `std::mutex`. Perfect voor multithreading binnen één en hetzelfde proces (high performance in-process).
- **`NamedSystemMutexPolicy`**: Wrapt de native Windows OS API call `CreateMutex` (Named Mutex). Vereist voor Inter-Process Communicatie (bijv. in de COM OutOfProcess EXE om botsingen over processor boundary heen aan te pakken).
- **`NullMutexPolicy`**: Een dummy (no-op) implementatie voor absolute zero-overhead theorie optimalisatie in een single-threaded context. 

## Directorystructuur

```
SharedValueV2/
├── CMakeLists.txt          # Build configuratie voor standalone tests
├── include/                # Header-only library bronbestanden
│   ├── SharedValue.hpp     # Generieke gedeelde variabele met templated lock policy
│   ├── DatasetStore.hpp    # Thread-safe overarching key-value store (ordered/unordered)
│   ├── StorageEngine.hpp   # Abstractie over std::map en std::unordered_map
│   ├── Errors.hpp          # Exception hiërarchie (SharedValueException subtypes)
│   ├── EventBus.hpp        # Publish/subscribe event systeem
│   ├── LockPolicies.hpp    # Lock policies: Local, Named, Null
│   ├── Observers.hpp       # Base observer interface definities
│   └── DatasetObserver.hpp # Dataset-specifieke observer interface
├── tests/                  # Standalone C++ tests
│   ├── UnitTests.cpp       # Functionele unit tests
│   └── StressTest.cpp      # Multithreaded concurrency stress test
└── build/                  # CMake build output (niet in version control)
```

## Uitgebreide Voorbeelden & Gebruik

Omdat de library header-only is, is er geen link-stap nodig (geen `.lib` of `.dll`). Je voegt enkel de `include/` directory stroomwaarts toe aan de C++ compiler flags.

### In theorie: Thread-safe Value Tracking

```cpp
#include "SharedValueV2/include/SharedValue.hpp"
#include "SharedValueV2/include/Observers.hpp"
#include <iostream>

using namespace SharedValueV2;

// 1. Zelfbouw observer klasse maken
class MyObserver : public IObserver {
public:
    void onValueChanged() override {
        std::cout << "Waarde is gewijzigd door andere thread of call!" << std::endl;
    }
};

// 2. Instantieer een veilige string met een snelle lokale std::mutex policy
SharedValue<std::string, LocalMutexPolicy> username;

// 3. Koppel en bind the listener aan 
auto obs = std::make_shared<MyObserver>();
username.addObserver(obs);

// 4. Veilige wijziging over threads (acquire -> mutate -> release -> notify all observer kopieën)
username.set("Admin"); 

// 5. Veilige readouts
std::string currentVal = username.get(); // Kopieert de inhoud volledig veilig naar memory stack
```

### Advanced: DatasetStore in Bedrijf 

Voor omvangrijke verzamelingen van variabelen is de `DatasetStore` ontwikkeld. Deze beheert data in abstracte lagen passend als een NoSQL in-memory map.

```cpp
#include "SharedValueV2/include/DatasetStore.hpp"

// Gebruik een Named Mutex policy zodat cross-process COM requests over de OS stack veilig blijven.
DatasetStore<NamedSystemMutexPolicy> store;

// Omschakelen backends: dynamisch switchen tussen std::map (geordend) of hash map via O(1) performance
store.SetStorageMode(StorageMode::UNORDERED_MAP);

// Thread-cross manipulering
store.InsertValue(100, "Speler Locatie X");
store.InsertValue(101, "Speler Locatie Y");

auto eventBus = store.GetEventBus(); // Koppel de EventBus via een pointer lock.
```

## Exception Handling
De bibliotheek gooit nooit native raw pointers naar C++ exceptions. Fouten zijn ingekaderd in gestandaardiseerde sub-classes met `SharedValueException` als vader (referentie te lezen binnen `Errors.hpp`):
- `PolicyException`: Opgeblazen bij fatale connectie bugs aan OS kant (bijv. fout bij `CreateMutex`).
- `StorageModeException`: Voorkomt memory allocaties wanneer er van runtime structuur gewisseld wordt en arrays al zware datacollectie bevatten.
- `EventBusException`: Specifieke abonnement-failures (bv: dubbel registreren, missende nulls en deadlocks).

## Compilatie & Testen

De C++20 multi-threaded concurrency en unit validatie suites kunnen in 100% isolatie gestart worden, wat de "Test-Driven Development (TDD)" cyclus extreem snel doet stempelen.

```powershell
cd SharedValueV2

# Genereer de nodige CMake backend bestanden in \build
cmake -B build

# Target de build config (Release mode is kritiek ten volle voor real-world race validation in de stress tests!)
cmake --build build --config Release

# 1. Start basistests voor functionele logica checks
.\build\tests\Release\UnitTests.exe

# 2. Start duizenddradige thread bom voor deadlock validaties
.\build\tests\Release\StressTest.exe
```

## Vereisten

- **Taal:** Een C++20 compatibele compiler wegens geavanceerde template concepts (MSVC v143+, GCC 12+, Clang 14+).
- **Tooling:** CMake ≥ 3.20 (alleen noodzakelijk voor test integraties).
- **OS Platform:** De IPC default policies zoals `NamedSystemMutexPolicy` integreren direct met Windows API (`Windows.h`).

## Gerelateerde Documentatie

- [CODE_REFERENCE.md](CODE_REFERENCE.md) — Volledige C++ API referentie en architectuur overzicht van de SharedValueV2 API's en iteraties.
- [README.md](../README.md) — Hoofddocumentatie en startpunt van het project in algemene zin.
- [ARCHITECTURE.md](../ARCHITECTURE.md) — Architectuur voor het gehele integratie COM platform.
- [README.md](../ATLProjectcomserverExe/README.md) — Lees wijzer voor de Out-of-Process implementatie COM variant op basis van dit framework.
