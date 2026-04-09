# SharedValueV2 — C++20 Header-Only Core Library

Een volledig thread-safe, header-only C++20 library die de kern-logica levert voor gedeelde variabelen en dataset-opslag. Dit is de **engine** die door zowel de Legacy DLL als de EXE COM Server wordt ingezet.

## Rol binnen het project

`SharedValueV2` is de **enige bron van waarheid** (Single Source of Truth) voor alle business-logica. De ATL COM wrappers in zowel [`ATLProjectcomserverLegacy/`](../ATLProjectcomserverLegacy/) als [`ATLProjectcomserverExe/`](../ATLProjectcomserverExe/) delegeren intern naar deze headers. Hierdoor kan de pure C++ core onafhankelijk van COM worden getest, hergebruikt en geëvolueerd.

## Design Patterns

- **Monitor Pattern** — Data en synchronisatie zijn onlosmakelijk gekoppeld; state is alleen toegankelijk binnen locked scopes.
- **Observer Pattern (Deadlock-Free)** — Notificaties worden verzonden buiten de critical section, via een gekopieerde observer-lijst.
- **Policy-Based Design** — `SharedValue<T, Policy>` accepteert pluggable lock-policies: `LocalMutexPolicy`, `NamedSystemMutexPolicy`, `NullMutexPolicy`.

## Directorystructuur

```
SharedValueV2/
├── CMakeLists.txt          # Build configuratie voor standalone tests
├── include/                # Header-only library bronbestanden
│   ├── SharedValue.hpp     # Generieke gedeelde variabele met templated lock policy
│   ├── DatasetStore.hpp    # Thread-safe key-value store (ordered/unordered)
│   ├── StorageEngine.hpp   # Abstractie over std::map en std::unordered_map
│   ├── Errors.hpp          # Exception hiërarchie (SharedValueException subtypes)
│   ├── EventBus.hpp        # Publish/subscribe event systeem
│   ├── LockPolicies.hpp    # Lock policies: Local, Named, Null
│   ├── Observers.hpp       # Observer interface definities
│   └── DatasetObserver.hpp # Dataset-specifieke observer interface
├── tests/                  # Standalone C++ tests
│   ├── UnitTests.cpp       # Functionele unit tests
│   └── StressTest.cpp      # Multithreaded concurrency stress test
└── build/                  # CMake build output (niet in version control)
```

## Compilatie & Testen

SharedValueV2 kan volledig onafhankelijk van de COM projecten worden gebouwd en getest met CMake:

```powershell
cd SharedValueV2

# Genereer build bestanden
cmake -B build

# Compileer (Release mode aanbevolen voor stress tests)
cmake --build build --config Release

# Draai de unit tests
.\build\tests\Release\UnitTests.exe

# Draai de multithreaded stress test
.\build\tests\Release\StressTest.exe
```

## Gebruik als library

SharedValueV2 is header-only. Voeg de `include/` directory toe aan je include path:

```cpp
#include "SharedValueV2/include/SharedValue.hpp"
#include "SharedValueV2/include/DatasetStore.hpp"
#include "SharedValueV2/include/Errors.hpp"

// Voorbeeld: een thread-safe shared value met lokale mutex
SharedValueV2::SharedValue<std::string, SharedValueV2::LocalMutexPolicy> sharedVal;
sharedVal.set("Hello!");
auto val = sharedVal.get(); // "Hello!"
```

## Vereisten

- C++20 compatibele compiler (MSVC v143+, GCC 12+, Clang 14+).
- CMake ≥ 3.20 (alleen voor standalone tests).


## Gerelateerde Documentatie

- [CODE_REFERENCE.md](CODE_REFERENCE.md) — Volledige C++ API referentie en framework documentatie voor SharedValueV2.
- [README.md](../README.md) — Hoofddocumentatie en startpunt van het gehele project.
- [ARCHITECTURE.md](../ARCHITECTURE.md) — Hoofd architectuurdocument voor het gehele COM Server project.
- [README.md](../ATLProjectcomserverExe/README.md) — Gebruikershandleiding en overzicht van de EXE COM Server variant.
