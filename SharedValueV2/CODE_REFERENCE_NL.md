# SharedValueV2 — Code Reference

Volledige technische API-referentie voor de SharedValueV2 C++20 header-only library.

**Namespace:** `SharedValueV2`
**Standaard:** C++20
**Headers:** `SharedValueV2/include/*.hpp`

---

## Inhoudsopgave

1. [SharedValue\<T, MutexPolicy\>](#1-sharedvaluet-mutexpolicy)
2. [DatasetStore\<TValue, MutexPolicy\>](#2-datasetstoretvalue-mutexpolicy)
3. [EventBus\<MutexPolicy\>](#3-eventbusmutexpolicy)
4. [StorageEngine](#4-storageengine)
5. [LockPolicies](#5-lockpolicies)
6. [Errors](#6-errors)
7. [Observer Interfaces](#7-observer-interfaces)
8. [Enums & Structs](#8-enums--structs)
9. [Dependency Graph](#9-dependency-graph)

---

## 1. SharedValue\<T, MutexPolicy\>

**Header:** [`SharedValue.hpp`](include/SharedValue.hpp)

Thread-safe generieke container voor één gedeelde waarde met observer-notificaties en een ingebouwde EventBus.

### Template Parameters

| Parameter | Default | Beschrijving |
|---|---|---|
| `T` | *(verplicht)* | Het type van de opgeslagen waarde. Moet kopieerbaar zijn (`CopyConstructible`). |
| `MutexPolicy` | `LocalMutexPolicy` | Lock-strategie. Zie [LockPolicies](#5-lockpolicies). |

### Constructor

```cpp
SharedValue();                          // Default-constructed T
explicit SharedValue(const T& initialValue);  // Met initiële waarde
```

### State Management

#### `SetValue`

```cpp
void SetValue(const T& newValue);
```

Schrijft een nieuwe waarde. Thread-safe. Notifieert alle observers en emitteert `EventType::ValueSet`.

**Gedrag:**
1. Verkrijgt lock op `m_mutex`.
2. Kopieert `newValue` naar `m_value`.
3. Geeft lock vrij.
4. Roept `OnValueChanged(newValue)` aan op alle geregistreerde observers (buiten de lock).
5. Emitteert `EventType::ValueSet` via de EventBus.

---

#### `GetValue`

```cpp
T GetValue() const;
```

Leest de huidige waarde. Thread-safe. Retourneert een **kopie** (nooit een referentie).

**Complexiteit:** O(1) + kopie-kosten van T.

---

#### `GetCurrentUTCDateTime`

```cpp
std::wstring GetCurrentUTCDateTime();
```

Retourneert de huidige UTC-tijd als `L"YYYY-MM-DD HH:MM:SS"`. Notifieert observers via `OnDateTimeChanged()`.

**Platform:** Gebruikt `gmtime_s` op Windows, `gmtime_r` op POSIX.

---

#### `GetCurrentUserLogin`

```cpp
std::wstring GetCurrentUserLogin() const;
```

Retourneert de huidige Windows-gebruikersnaam via `GetUserNameW()`. Retourneert `L"UnknownUser"` op niet-Windows platforms.

---

### Explicit Lock Management

#### `LockSharedValue`

```cpp
bool LockSharedValue();
```

Verkrijgt een exclusieve lock op de onderliggende mutex. **De caller is verantwoordelijk voor het aanroepen van `Unlock()`.** Emitteert `EventType::ValueLocked`.

**Retourneert:** Altijd `true` (blokkeert totdat lock verkregen).

---

#### `LockSharedValueTimeout`

```cpp
bool LockSharedValueTimeout(unsigned long ms);
```

Probeert de lock te verkrijgen binnen `ms` milliseconden. Emitteert `EventType::ValueLocked` bij succes.

**Retourneert:** `true` als de lock is verkregen, `false` bij timeout.

**Let op:** Timeout-functionaliteit is alleen beschikbaar met `NamedSystemMutexPolicy`. Bij `LocalMutexPolicy` wordt `try_lock()` gebruikt (non-blocking).

---

#### `Unlock`

```cpp
void Unlock();
```

Geeft de eerder verkregen lock vrij. Emitteert `EventType::ValueUnlocked`.

> [!CAUTION]
> Aanroepen zonder voorafgaande `LockSharedValue()` resulteert in undefined behavior.

---

### Observer Pub/Sub

#### `Subscribe`

```cpp
void Subscribe(ISharedValueObserver<T>* observer);
```

Registreert een observer voor `OnValueChanged` en `OnDateTimeChanged` callbacks. Thread-safe. Duplicaten worden genegeerd.

---

#### `Unsubscribe`

```cpp
void Unsubscribe(ISharedValueObserver<T>* observer);
```

Verwijdert een eerder geregistreerde observer. Thread-safe.

---

### EventBus

#### `GetEventBus`

```cpp
EventBus<MutexPolicy>& GetEventBus();
```

Retourneert een referentie naar de interne EventBus. Gebruik dit voor het registreren van `IEventListener` objecten die rijkere `MutationEvent` structs ontvangen.

---

### Private Members

| Member | Type | Beschrijving |
|---|---|---|
| `m_value` | `T` | De opgeslagen waarde. |
| `m_mutex` | `MutexPolicy` (mutable) | Lock voor thread-safety. |
| `m_observers` | `vector<ISharedValueObserver<T>*>` | Geregistreerde observers. |
| `m_eventBus` | `EventBus<MutexPolicy>` | Rich event systeem. |

### Voorbeeld

```cpp
#include "SharedValue.hpp"
#include "LockPolicies.hpp"

using namespace SharedValueV2;

// Thread-safe met lokale mutex
SharedValue<std::wstring, LocalMutexPolicy> sv(L"initieel");

sv.SetValue(L"nieuw");
auto val = sv.GetValue();  // L"nieuw"

// Zero-overhead voor single-threaded code
SharedValue<int, NullMutexPolicy> fast(42);
```

---

## 2. DatasetStore\<TValue, MutexPolicy\>

**Header:** [`DatasetStore.hpp`](include/DatasetStore.hpp)

Thread-safe pagineerbare key-value store met runtime-wisselbare storage engine, observer-notificaties en EventBus integratie.

### Template Parameters

| Parameter | Default | Beschrijving |
|---|---|---|
| `TValue` | *(verplicht)* | Type van de opgeslagen waarden. |
| `MutexPolicy` | `LocalMutexPolicy` | Lock-strategie. |

### Constructor

```cpp
explicit DatasetStore(StorageMode mode = StorageMode::Ordered);
```

Maakt een lege store aan met de opgegeven storage mode.

---

### Storage Mode

#### `SetStorageMode`

```cpp
void SetStorageMode(StorageMode mode);
```

Wisselt de onderliggende storage engine. **Alleen toegestaan als de store leeg is.**

| Mode | Container | Lookup | Iteratievolgorde |
|---|---|---|---|
| `StorageMode::Ordered` (0) | `std::map` | O(log n) | Alfabetisch gesorteerd |
| `StorageMode::Unordered` (1) | `std::unordered_map` | O(1) amortized | Ondeterministisch |

**Gooit:** `StoreModeException` als de store niet leeg is.

---

#### `GetStorageMode`

```cpp
StorageMode GetStorageMode() const;
```

Retourneert de huidige storage mode. Thread-safe.

---

### CRUD Operaties

#### `AddRow`

```cpp
void AddRow(const std::wstring& key, const TValue& value);
```

Voegt een nieuwe rij toe. Thread-safe. Notifieert observers en emitteert `EventType::RowAdded`.

**Gooit:** `DuplicateKeyException` als de key al bestaat.

---

#### `GetRow`

```cpp
std::optional<TValue> GetRow(const std::wstring& key) const;
```

Zoekt een waarde op key. Retourneert `std::nullopt` als de key niet bestaat. Thread-safe.

---

#### `GetRowOrThrow`

```cpp
TValue GetRowOrThrow(const std::wstring& key) const;
```

Zoekt een waarde op key. **Gooit** `KeyNotFoundException` als de key niet bestaat. Thread-safe.

---

#### `UpdateRow`

```cpp
bool UpdateRow(const std::wstring& key, const TValue& value);
```

Overschrijft de waarde van een bestaande key. Thread-safe. Notifieert observers en emitteert `EventType::RowUpdated` (inclusief oude en nieuwe waarde).

**Retourneert:** `true` als de key bestond en is bijgewerkt, `false` als de key niet bestaat.

---

#### `RemoveRow`

```cpp
bool RemoveRow(const std::wstring& key);
```

Verwijdert een rij. Thread-safe. Emitteert `EventType::RowRemoved`.

**Retourneert:** `true` als de key bestond en is verwijderd, `false` als de key niet bestond.

---

#### `Clear`

```cpp
void Clear();
```

Verwijdert alle rijen. Thread-safe. Emitteert `EventType::DatasetCleared`.

---

### Query Operaties

#### `GetRecordCount`

```cpp
size_t GetRecordCount() const;
```

Retourneert het aantal rijen. Thread-safe.

---

#### `HasKey`

```cpp
bool HasKey(const std::wstring& key) const;
```

Controleert of een key bestaat. Thread-safe.

---

### Paginering

#### `FetchKeys`

```cpp
std::vector<std::wstring> FetchKeys(size_t startIndex, size_t limit) const;
```

Retourneert een pagina van maximaal `limit` keys, beginnend bij index `startIndex`. Thread-safe.

**Voorbeeld:**
```cpp
auto keys = store.FetchKeys(0, 10);  // Eerste 10 keys
auto page2 = store.FetchKeys(10, 10); // Keys 10-19
```

---

#### `FetchPage`

```cpp
std::vector<std::pair<std::wstring, TValue>> FetchPage(size_t start, size_t limit) const;
```

Retourneert een pagina van key-value paren. Thread-safe.

---

### In-Place Mutatie (C++ only)

#### `AccessInPlace`

```cpp
void AccessInPlace(const std::wstring& key, std::function<void(TValue&)> mutator);
```

Voert een in-place mutatie uit op een bestaande waarde via een callback-functie. De mutator wordt aangeroepen onder de lock; notificaties worden buiten de lock verzonden.

**Gooit:** `KeyNotFoundException` als de key niet bestaat.

**Voorbeeld:**
```cpp
store.AddRow(L"counter", L"0");
store.AccessInPlace(L"counter", [](std::wstring& val) {
    int n = std::stoi(val);
    val = std::to_wstring(n + 1);
});
// counter is nu "1"
```

---

### Observer Pub/Sub

#### `Subscribe` / `Unsubscribe`

```cpp
void Subscribe(IDatasetObserver<TValue>* observer);
void Unsubscribe(IDatasetObserver<TValue>* observer);
```

Registreert/verwijdert een `IDatasetObserver` voor `OnRowAdded`, `OnRowUpdated`, `OnRowRemoved` en `OnDatasetCleared` callbacks.

---

### EventBus

#### `GetEventBus`

```cpp
EventBus<MutexPolicy>& GetEventBus();
```

Retourneert de interne EventBus voor `IEventListener` registratie.

---

### Private Members

| Member | Type | Beschrijving |
|---|---|---|
| `m_engine` | `unique_ptr<IStorageEngine<TValue>>` | Actieve storage engine. |
| `m_mutex` | `MutexPolicy` (mutable) | Lock. |
| `m_mode` | `StorageMode` | Huidige storage mode. |
| `m_observers` | `vector<IDatasetObserver<TValue>*>` | Geregistreerde observers. |
| `m_eventBus` | `EventBus<MutexPolicy>` | Rich event systeem. |

### Voorbeeld

```cpp
#include "DatasetStore.hpp"

using namespace SharedValueV2;

DatasetStore<std::wstring> store;

store.AddRow(L"server01", L"status:online|cpu:45");
store.AddRow(L"server02", L"status:offline");

auto val = store.GetRowOrThrow(L"server01");  // L"status:online|cpu:45"
store.UpdateRow(L"server02", L"status:online|cpu:12");
store.RemoveRow(L"server01");

auto keys = store.FetchKeys(0, 100);  // [L"server02"]
size_t count = store.GetRecordCount();  // 1
```

---

## 3. EventBus\<MutexPolicy\>

**Header:** [`EventBus.hpp`](include/EventBus.hpp)

Thread-safe publish/subscribe event systeem met monotoon stijgende sequence counters. Notificaties worden **deadlock-free** afgeleverd.

### Template Parameters

| Parameter | Default | Beschrijving |
|---|---|---|
| `MutexPolicy` | `LocalMutexPolicy` | Lock-strategie. |

### Methoden

#### `Subscribe`

```cpp
void Subscribe(IEventListener* listener);
```

Registreert een event listener. Duplicaten worden genegeerd. Thread-safe.

---

#### `Unsubscribe`

```cpp
void Unsubscribe(IEventListener* listener);
```

Verwijdert een listener. Thread-safe.

---

#### `Emit` (MutationEvent)

```cpp
void Emit(const MutationEvent& event);
```

Verstuurt een compleet `MutationEvent` naar alle geregistreerde listeners.

**Deadlock-free gedrag:**
1. Lock listener-lijst → kopieer naar lokale vector → unlock.
2. Itereer over de kopie om listeners aan te roepen.

---

#### `Emit` (convenience overload)

```cpp
void Emit(EventType type,
          const std::wstring& key = L"",
          const std::wstring& oldVal = L"",
          const std::wstring& newVal = L"",
          const std::wstring& source = L"");
```

Bouwt automatisch een `MutationEvent` met:
- `timestamp` = `std::chrono::system_clock::now()`
- `sequenceId` = atomisch opgehoogde teller

---

#### `GetSequenceId`

```cpp
uint64_t GetSequenceId() const;
```

Retourneert de huidige sequence counter. Atomisch (lock-free).

---

### Private Members

| Member | Type | Beschrijving |
|---|---|---|
| `m_listeners` | `vector<IEventListener*>` | Geregistreerde listeners. |
| `m_mutex` | `MutexPolicy` (mutable) | Lock voor listener-lijst. |
| `m_sequenceCounter` | `atomic<uint64_t>` | Monotoon stijgende event-teller. Lock-free. |

### Voorbeeld

```cpp
#include "EventBus.hpp"

using namespace SharedValueV2;

class MyListener : public IEventListener {
public:
    void OnEvent(const MutationEvent& event) override {
        std::wcout << L"Event: type=" << static_cast<int>(event.type)
                   << L" key=" << event.key
                   << L" seq=" << event.sequenceId << std::endl;
    }
};

EventBus<LocalMutexPolicy> bus;
MyListener listener;

bus.Subscribe(&listener);
bus.Emit(EventType::RowAdded, L"key1", L"", L"value1");
// Output: Event: type=10 key=key1 seq=0

bus.Unsubscribe(&listener);
```

---

## 4. StorageEngine

**Header:** [`StorageEngine.hpp`](include/StorageEngine.hpp)

Abstracte storage engine interface met twee concrete implementaties. Gebruikt door `DatasetStore` via het **Strategy Pattern**.

### `IStorageEngine<TValue>` (abstract interface)

```cpp
template <typename TValue>
class IStorageEngine {
public:
    virtual void Insert(const std::wstring& key, const TValue& value) = 0;
    virtual bool Update(const std::wstring& key, const TValue& value) = 0;
    virtual bool Erase(const std::wstring& key) = 0;
    virtual void Clear() = 0;
    virtual size_t Size() const = 0;
    virtual std::optional<TValue> Find(const std::wstring& key) const = 0;
    virtual bool Contains(const std::wstring& key) const = 0;
    virtual std::vector<std::wstring> GetKeys(size_t start, size_t limit) const = 0;
    virtual std::vector<std::pair<std::wstring, TValue>> GetPage(size_t start, size_t limit) const = 0;
};
```

### `OrderedStorageEngine<TValue>`

Implementatie op basis van `std::map<std::wstring, TValue>`.

| Operatie | Complexiteit | Opmerkingen |
|---|---|---|
| `Insert` | O(log n) | Via `emplace` |
| `Find` / `Contains` | O(log n) | |
| `Update` | O(log n) | `find` + overschrijf |
| `Erase` | O(log n) | |
| `GetKeys` / `GetPage` | O(n) | Lineaire iteratie met offset |
| Iteratievolgorde | Alfabetisch | `std::map` garandeert gesorteerde sleutels |

### `UnorderedStorageEngine<TValue>`

Implementatie op basis van `std::unordered_map<std::wstring, TValue>`.

| Operatie | Complexiteit | Opmerkingen |
|---|---|---|
| `Insert` | O(1) amortized | Via `emplace` |
| `Find` / `Contains` | O(1) amortized | Hash lookup |
| `Update` | O(1) amortized | |
| `Erase` | O(1) amortized | |
| `GetKeys` / `GetPage` | O(n) | Lineaire iteratie |
| Iteratievolgorde | Ondeterministisch | Geen volgorde-garantie |

### `CreateStorageEngine<TValue>` (Factory)

```cpp
template <typename TValue>
std::unique_ptr<IStorageEngine<TValue>> CreateStorageEngine(StorageMode mode);
```

Factory functie die de juiste engine instantieert op basis van `StorageMode`.

---

## 5. LockPolicies

**Header:** [`LockPolicies.hpp`](include/LockPolicies.hpp)

Drie verwisselbare lock-strategieën als template parameter. Alle policies voldoen aan de C++ `BasicLockable` / `Lockable` vereisten.

### `LocalMutexPolicy`

```cpp
class LocalMutexPolicy {
    void lock();
    void unlock();
    bool try_lock();
};
```

Wrapper rondom `std::mutex`. Geschikt voor multithreading **binnen één proces**.

| Eigenschap | Waarde |
|---|---|
| Overhead | Laag (~nanoseconden) |
| Scope | Single-process |
| Cross-process | ❌ |
| Kopieerbaarheid | Non-copyable, non-movable (vanwege `std::mutex`) |

---

### `NullMutexPolicy`

```cpp
class NullMutexPolicy {
    void lock() {}       // no-op
    void unlock() {}     // no-op
    bool try_lock() { return true; }  // altijd succes
};
```

Zero-overhead policy voor single-threaded gebruik of unit tests.

| Eigenschap | Waarde |
|---|---|
| Overhead | **Nul** (compiler elimineert de calls) |
| Scope | Single-thread |
| Cross-process | ❌ |
| Gebruik | Unit tests, embedded, single-thread applicaties |

---

### `NamedSystemMutexPolicy` (Windows only)

```cpp
class NamedSystemMutexPolicy {
    void lock();               // WaitForSingleObject(INFINITE)
    void unlock();             // ReleaseMutex
    bool try_lock();           // WaitForSingleObject(0)
    bool try_lock_for(DWORD timeoutMs);  // WaitForSingleObject(timeoutMs)
};
```

Windows Named Mutex via `CreateMutexW`. Standaardnaam: `Local\\SharedValueModernLock`.

| Eigenschap | Waarde |
|---|---|
| Overhead | Hoog (kernel-mode transition) |
| Scope | Cross-process (per-sessie met `Local\\` prefix) |
| Cross-process | ✅ |
| Privilege | Geen administrator nodig (dankzij `Local\\`) |

> [!NOTE]
> `try_lock_for(DWORD)` is een extensie bovenop de standaard C++ `Lockable` interface. Het wordt specifiek gebruikt door `SharedValue::LockSharedValueTimeout()`.

---

## 6. Errors

**Header:** [`Errors.hpp`](include/Errors.hpp)

Exception hiërarchie en error codes voor de SharedValueV2 library.

### `ErrorCode` (enum class)

```cpp
enum class ErrorCode : uint16_t {
    // Dataset errors
    KeyNotFound         = 100,
    DuplicateKey        = 101,
    StoreModeNotEmpty   = 102,
    InvalidStorageMode  = 103,

    // SharedValue errors
    LockTimeout         = 200,
    NullPointer         = 201,

    // General
    InvalidArgument     = 300,
    OutOfRange          = 301,
    InternalError       = 999
};
```

### Exception Hiërarchie

```
std::runtime_error
  └── SharedValueException          (basis, bevat ErrorCode)
        ├── KeyNotFoundException    (code: 100)
        ├── DuplicateKeyException   (code: 101)
        ├── StoreModeException      (code: 102)
        └── InvalidStorageModeException (code: 103)
```

### `SharedValueException`

```cpp
class SharedValueException : public std::runtime_error {
public:
    ErrorCode code;
    SharedValueException(ErrorCode c, const std::string& msg);
};
```

Basis-exception. `code` wordt door de COM wrapper vertaald naar een `HRESULT` via `MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, code)`.

---

### `KeyNotFoundException`

```cpp
class KeyNotFoundException : public SharedValueException {
public:
    KeyNotFoundException(const std::wstring& key);
    // Message: "Key '<key>' not found in dataset"
};
```

Gegooid door `DatasetStore::GetRowOrThrow()` en `DatasetStore::AccessInPlace()`.

---

### `DuplicateKeyException`

```cpp
class DuplicateKeyException : public SharedValueException {
public:
    DuplicateKeyException(const std::wstring& key);
    // Message: "Key '<key>' already exists"
};
```

Gegooid door `DatasetStore::AddRow()`.

---

### `StoreModeException`

```cpp
class StoreModeException : public SharedValueException {
public:
    StoreModeException();
    // Message: "Cannot change storage mode: dataset is not empty"
};
```

Gegooid door `DatasetStore::SetStorageMode()`.

---

### `InvalidStorageModeException`

```cpp
class InvalidStorageModeException : public SharedValueException {
public:
    InvalidStorageModeException(int mode);
    // Message: "Invalid storage mode: <mode>"
};
```

Gegooid door de COM wrapper wanneer een ongeldige mode (niet 0 of 1) wordt doorgegeven.

---

### Helper: `narrow`

```cpp
inline std::string narrow(const std::wstring& ws);
```

Converteert `std::wstring` naar `std::string` voor exception messages (basic ASCII conversie).

---

## 7. Observer Interfaces

### `ISharedValueObserver<T>`

**Header:** [`Observers.hpp`](include/Observers.hpp)

```cpp
template <typename T>
class ISharedValueObserver {
public:
    virtual ~ISharedValueObserver() = default;
    virtual void OnValueChanged(const T& newValue) = 0;
    virtual void OnDateTimeChanged(const std::wstring& newDateTime) = 0;
};
```

Observer voor `SharedValue`. Geregistreerd via `SharedValue::Subscribe()`.

| Callback | Trigger |
|---|---|
| `OnValueChanged` | `SharedValue::SetValue()` |
| `OnDateTimeChanged` | `SharedValue::GetCurrentUTCDateTime()` |

---

### `IDatasetObserver<TValue>`

**Header:** [`DatasetObserver.hpp`](include/DatasetObserver.hpp)

```cpp
template <typename TValue>
class IDatasetObserver {
public:
    virtual ~IDatasetObserver() = default;
    virtual void OnRowAdded(const std::wstring& key, const TValue& value) = 0;
    virtual void OnRowUpdated(const std::wstring& key, const TValue& newValue) = 0;
    virtual void OnRowRemoved(const std::wstring& key) = 0;
    virtual void OnDatasetCleared() = 0;
};
```

Observer voor `DatasetStore`. Geregistreerd via `DatasetStore::Subscribe()`.

| Callback | Trigger |
|---|---|
| `OnRowAdded` | `AddRow()` |
| `OnRowUpdated` | `UpdateRow()`, `AccessInPlace()` |
| `OnRowRemoved` | `RemoveRow()` |
| `OnDatasetCleared` | `Clear()` |

---

### `IEventListener`

**Header:** [`EventBus.hpp`](include/EventBus.hpp)

```cpp
class IEventListener {
public:
    virtual ~IEventListener() = default;
    virtual void OnEvent(const MutationEvent& event) = 0;
};
```

Generieke event listener voor de `EventBus`. Ontvangt rijke `MutationEvent` structs met type, key, oude/nieuwe waarde, timestamp en sequence ID.

---

## 8. Enums & Structs

### `EventType` (enum class)

**Header:** [`EventBus.hpp`](include/EventBus.hpp)

```cpp
enum class EventType : uint16_t {
    // SharedValue events
    ValueSet            = 1,
    ValueLocked         = 2,
    ValueUnlocked       = 3,

    // Dataset events
    RowAdded            = 10,
    RowUpdated          = 11,
    RowRemoved          = 12,
    DatasetCleared      = 13,
    StorageModeChanged  = 14,

    // Lifecycle events
    ObjectCreated       = 50,
    ObjectDestroyed     = 51
};
```

---

### `MutationEvent` (struct)

**Header:** [`EventBus.hpp`](include/EventBus.hpp)

```cpp
struct MutationEvent {
    EventType   type;           // Wat er is gebeurd
    std::wstring key;           // Betreffende key (leeg bij ValueSet)
    std::wstring oldValue;      // Vorige waarde (leeg bij AddRow)
    std::wstring newValue;      // Nieuwe waarde (leeg bij RemoveRow)
    std::wstring source;        // Bron-identificatie (optioneel)
    std::chrono::system_clock::time_point timestamp;  // Tijdstip
    uint64_t     sequenceId;    // Monotoon stijgend volgnummer
};
```

---

### `StorageMode` (enum class)

**Header:** [`StorageEngine.hpp`](include/StorageEngine.hpp)

```cpp
enum class StorageMode {
    Ordered   = 0,   // std::map (gesorteerd)
    Unordered = 1    // std::unordered_map (hash)
};
```

---

## 9. Dependency Graph

```
SharedValue.hpp
  ├── Observers.hpp          (ISharedValueObserver<T>)
  ├── LockPolicies.hpp       (LocalMutexPolicy, NullMutexPolicy, NamedSystemMutexPolicy)
  └── EventBus.hpp           (EventBus<MutexPolicy>, MutationEvent, IEventListener)
        └── LockPolicies.hpp

DatasetStore.hpp
  ├── StorageEngine.hpp      (IStorageEngine<T>, OrderedStorageEngine, UnorderedStorageEngine)
  ├── DatasetObserver.hpp    (IDatasetObserver<T>)
  ├── EventBus.hpp
  ├── Errors.hpp             (SharedValueException hierarchy, ErrorCode)
  └── LockPolicies.hpp

Errors.hpp                   (standalone, geen interne dependencies)
Observers.hpp                (standalone)
DatasetObserver.hpp          (standalone)
LockPolicies.hpp             (standalone, Windows headers op _WIN32)
```

### Include-volgorde (aanbevolen)

```cpp
// Kerntypen
#include "SharedValueV2/include/LockPolicies.hpp"
#include "SharedValueV2/include/Errors.hpp"

// Observers
#include "SharedValueV2/include/Observers.hpp"
#include "SharedValueV2/include/DatasetObserver.hpp"

// Event systeem
#include "SharedValueV2/include/EventBus.hpp"

// Storage
#include "SharedValueV2/include/StorageEngine.hpp"

// High-level API
#include "SharedValueV2/include/SharedValue.hpp"
#include "SharedValueV2/include/DatasetStore.hpp"
```

---

## Thread-Safety Contract

Alle publieke methoden in `SharedValue` en `DatasetStore` zijn thread-safe wanneer een actieve `MutexPolicy` wordt gebruikt (`LocalMutexPolicy` of `NamedSystemMutexPolicy`).

**Deadlock-free garantie:** Alle observer-notificaties en EventBus emits worden uitgevoerd **buiten** de interne lock. De techniek:

```cpp
void notifyRowAdded(const std::wstring& key, const TValue& val) {
    std::vector<IDatasetObserver<TValue>*> copy;
    { std::lock_guard<MutexPolicy> lock(m_mutex); copy = m_observers; }
    // Lock is vrijgegeven — veilig om callbacks aan te roepen
    for (auto* obs : copy) obs->OnRowAdded(key, val);
}
```

Dit voorkomt deadlocks wanneer een observer in zijn callback opnieuw de store aanroept.


## Gerelateerde Documentatie

- [README.md](README.md) — Introductie en overzicht van de SharedValueV2 C++20 engine.
- [README.md](../README.md) — Hoofddocumentatie en startpunt van het gehele project.
- [ARCHITECTURE.md](../ARCHITECTURE.md) — Hoofd architectuurdocument voor het gehele COM Server project.
- [README.md](../ATLProjectcomserverExe/README.md) — Gebruikershandleiding en overzicht van de EXE COM Server variant.
