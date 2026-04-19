# SharedValueV2 — Core Library

Header-only bronbestanden van de SharedValueV2 core library. Deze acteren als de interne C++20 engine voor state management in het project.

## 1. Architectuur & Opbouw

De SharedValueV2 library is ontworpen als een zero-dependency, header-only C++20 bibliotheek (met uitzondering van Windows-specifieke `NamedSystemMutexPolicy` indien gebruikt onder Windows). De architectuur steunt sterk op **templates**, **strategies/policies**, en het **observer pattern** om flexibel, thread-safe, en performant te blijven.

### Kerncomponenten
- **`SharedValue<T, LockPolicy>`**: Een container voor een enkele gedeelde waarde van type `T`. Beheert zijn eigen locks op basis van de `LockPolicy`. Notificeert observers bij wijzigingen.
- **`DatasetStore<TValue, LockPolicy>`**: Een thread-safe key-value opslag (CRUD operaties). Maakt intern gebruik van een switchbare `StorageEngine` (Strategy pattern) om runtime te wisselen tussen geordende of ongeordende opslag.
- **`EventBus<MutexPolicy>`**: Een modern rich-event pub/sub systeem. Verstuurt complexe `MutationEvent` structuren inclusief timestamps, oude/nieuwe waardes, event types en sequence iterators, als vervanger/aanvulling op de traditionele observer callbacks.
- **Locking Policies (`LockPolicies.hpp`)**: Policy-based design voor concurrency. Klassen die de locking behaviour bepalen (bijv. `LocalMutexPolicy`, `NamedSystemMutexPolicy`, of een `NullMutexPolicy` voor zero-overhead zonder synchronisatie).
- **Storage Engines (`StorageEngine.hpp`)**: Abstracte interface `IStorageEngine` en de concrete implementaties `OrderedStorageEngine` (via `std::map`) en `UnorderedStorageEngine` (via `std::unordered_map`). 

## 2. Werking en Dataflow

Wanneer applicaties de core engine gebruiken (bijvoorbeeld ingebed in de ATL COM Server), is de levenscyclus doorgaans als volgt:

1. **Initialisatie:** Een `DatasetStore` (of `SharedValue`) wordt geïnstantieerd met een bepaalde `MutexPolicy` en `StorageMode`. 
2. **Mutatie:** Via methoden zoals `AddRow`, `UpdateRow`, of `RemoveRow` wordt data gemuteerd. Deze operaties zijn thread-safe doordat de mutaties binnen een `std::lock_guard` gebeuren.
3. **Notificatie:** Zodra de mutatie voltooid is, worden observers genotificeerd zónder dat de lock wordt vastgehouden (om deadlocks te voorkomen: notificatie gebeurt op een lokale kopie van de observer-lijst). De `EventBus` wordt tegelijkertijd getriggered en zendt een `MutationEvent` met rijke semantiek naar geregistreerde listeners.

### Paginering en Querying
`DatasetStore` ondersteunt paginering direct vanuit C++ (vergelijkbaar met cursors / LIMIT/OFFSET in databases). Methoden zoals `FetchKeys()` en `FetchPage()` extraheren porties data efficiënt in een thread-safe iteratie.

## 3. Ontwerpprincipes (Design Decisions)

### Header-Only en C++20
Door in C++20 header-only te werk te gaan, kan het type (`T` en `TValue`) compiler-geoptimaliseerd worden ingevuld door de gebruiker, inclusief de locking behavior. Het voorkomt DLL-hell en complexe linkingsproblemen, vooral bij integratie met oudere COM projecten.

### Policy-Based Locking
Door `MutexPolicy` als template parameter aan te bieden, kost synchronisatie exact wat het moet kosten en niet meer:
- Voor in-process multithreading: `LocalMutexPolicy` (snelle std::mutex).
- Voor cross-process synchronisatie (bijv. in een COM EXE): `NamedSystemMutexPolicy` (Windows Mutex).
- Voor single-threaded use cases: `NullMutexPolicy` (vrijwel weggesneden door compiler optimisaties).

### Deadlock-Vrije Observers
Zowel `EventBus` als de legacy Observers hanteren het patroon waarbij de lijst van subscribers binnen de lock gecopieerd wordt, en het triggeren/notificeren _buiten_ de lock gebeurt. Dit voorkomt re-entrancy deadlocks die vaak ontstaan als een listener tijdens een event-afhandeling direct de muterende methods van store weer probeert aan te roepen.

### Strategy Pattern voor Storage Engine
Omdat dataset vereisten kunnen verschillen (sommigen vereisen enumeratie in de ingevoegde/alfabetische logische datastructuur volgorde (Ordered - O(log N)), terwijl anderen pure constante snelheid O(1) nodig hebben (Unordered)), isoleert `StorageEngine.hpp` de std containers achter een `IStorageEngine` interface. Deze is runtime verwisselbaar mits de container the store op dat moment leeg is (`SetStorageMode()`).

## 4. Bestandsoverzicht

| Header | Beschrijving |
|---|---|
| `SharedValue.hpp` | Generieke thread-safe gedeelde variabele. Template `SharedValue<T, LockPolicy>` met `get()`, `set()`, en observer-notificaties. |
| `DatasetStore.hpp` | Thread-safe key-value dataset store. Ondersteunt geavanceerde CRUD, paginering en runtime StorageMode switches. |
| `StorageEngine.hpp` | Abstractie over `std::map` en `std::unordered_map` (`IStorageEngine`, `OrderedStorageEngine`, `UnorderedStorageEngine`). |
| `Errors.hpp` | Exception hiërarchie (`SharedValueException`, `KeyNotFoundException`, `DuplicateKeyException`, `StoreModeException`). |
| `EventBus.hpp` | Rich-event publish/subscribe systeem met deadlock-free emits en `MutationEvent` structuur (Sequence ID, Timestamp, Old/New). |
| `LockPolicies.hpp` | Pluggable lock policies (`LocalMutexPolicy`, `NamedSystemMutexPolicy`, `NullMutexPolicy`). |
| `Observers.hpp` | Legacy abstracte observer interfaces voor scalar notificaties. |
| `DatasetObserver.hpp` | Legacy abstracte observer interfaces specifiek voor de `DatasetStore`. |

## 5. Dependency Graph

```
SharedValue.hpp ──► LockPolicies.hpp
                ──► Observers.hpp
                ──► EventBus.hpp

DatasetStore.hpp ──► StorageEngine.hpp
                 ──► Errors.hpp
                 ──► LockPolicies.hpp
                 ──► DatasetObserver.hpp
                 ──► EventBus.hpp
```

## 6. Gerelateerde Documentatie

- [Hoofd-README](../../README.md) — Startpunt van het gehele project.
- [COM Server ARCHITECTURE](../../ARCHITECTURE.md) — Architectuurdocument voor het de ATL COM Out-of-Process Server (`SharedValueV2` integratie).
- [COM EXE Server](../../ATLProjectcomserverExe/README.md) — Gebruikeringshandleiding voor de COM host.
- [SharedValue V2 Map](../README.md) — Overzicht van de core implementaties in de parent map.
