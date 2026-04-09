# SharedValueV2 — Include Headers

Header-only bronbestanden van de SharedValueV2 core library. Alle headers zijn zelfstandig bruikbaar en vereisen een C++20 compiler.

## Rol binnen het project

Deze headers bevatten de volledige implementatie van de SharedValueV2 engine. Ze worden zowel door de ATL COM wrappers (via `#include "SharedValueV2/include/..."`) als door de standalone CMake tests geïncludeerd.

## Bestandsoverzicht

| Header | Beschrijving |
|---|---|
| `SharedValue.hpp` | Generieke thread-safe gedeelde variabele. Template `SharedValue<T, LockPolicy>` met `get()`, `set()`, en observer-notificaties. |
| `DatasetStore.hpp` | Thread-safe key-value dataset store. Ondersteunt `AddRow`, `GetRowData`, `UpdateRow`, `RemoveRow`, `FetchPageKeys`, `FetchPageData`, en switchbare `StorageMode` (ordered/unordered). |
| `StorageEngine.hpp` | Abstractie over `std::map` en `std::unordered_map`. Biedt een uniforme API ongeacht de onderliggende container via het Strategy pattern. |
| `Errors.hpp` | Exception hiërarchie: `SharedValueException` als basis, met subtypes `KeyNotFound`, `DuplicateKey`, `StoreModeNotEmpty`, en error codes. |
| `EventBus.hpp` | Generiek publish/subscribe event systeem. Deadlock-vrij door observer-lijst te kopiëren vóór notificatie buiten de lock. |
| `LockPolicies.hpp` | Pluggable lock policies: `LocalMutexPolicy` (thread-local `std::mutex`), `NamedSystemMutexPolicy` (cross-process Windows named mutex), `NullMutexPolicy` (zero-overhead, geen synchronisatie). |
| `Observers.hpp` | Abstracte observer interfaces voor het observer pattern. |
| `DatasetObserver.hpp` | Dataset-specifieke observer interface voor `DatasetStore` events. |

## Dependency Graph

```
SharedValue.hpp ──► LockPolicies.hpp
                ──► Observers.hpp
                ──► EventBus.hpp

DatasetStore.hpp ──► StorageEngine.hpp
                 ──► Errors.hpp
                 ──► LockPolicies.hpp
                 ──► DatasetObserver.hpp
```


## Gerelateerde Documentatie

- [README.md](../../README.md) — Hoofddocumentatie en startpunt van het gehele project.
- [ARCHITECTURE.md](../../ARCHITECTURE.md) — Hoofd architectuurdocument voor het gehele COM Server project.
- [README.md](../../ATLProjectcomserverExe/README.md) — Gebruikershandleiding en overzicht van de EXE COM Server variant.
- [README.md](../README.md) — Introductie en overzicht van de SharedValueV2 C++20 engine.
