# SharedValueV2 — C++20 Header-Only Core Library

A fully thread-safe, header-only C++20 library delivering the core logic powering shared variables and dataset-storage operations. This represents the absolute **engine** driving both the Legacy In-Process DLL alongside the Out-of-Process EXE COM Server.

## Role Within the Project

`SharedValueV2` acts as the **Single Source of Truth** dictating all business logic across the COM ecosystem. The overarching ATL COM wrappers resting inside both [`ATLProjectcomserverLegacy/`](../ATLProjectcomserverLegacy/) as well as [`ATLProjectcomserverExe/`](../ATLProjectcomserverExe/) delegate internally routing their interface requests directly towards these native headers. 

Decoupling the pristine C++ core from the heavy COM-layer offers tremendous benefits:
- **Agile Testability**: The core can be completely exhausted through unit and stress tests (resolving multithreaded edge-cases) using standard C++ test frameworks, devoid of COM initialization headaches (`CoInitialize`), registry keys locking, or proxy/stub performance overheads.
- **Flawless Reusability**: The core library easily integrates universally into standalone C++ application boundaries entirely ignoring whether COM infrastructure exists or not.
- **Optimal Performance**: Functioning purely header-only enforces compilers executing maximum inlining maneuvers, yielding zero-cost abstractions escaping standard locked domains or event fires.

## Core Concepts & Architecture

The library aggressively leans on modern C++ paradigms, robustly enforcing advanced structural design patterns.

### 1. Monitor Pattern (Safety First)
Data alongside synchronization mechanisms stand inextricably bound framing a Monitor enclosure. 
State access remains tightly forbidden unless passing through designated secured C++ RAII bounds (e.g. initiating via local scopes from `.get()` or internal mutating `.set()` loops). Once modifications commence, the calling thread possesses guaranteed exclusive locking authority globally, automatically relinquishing authority upon scope destruction. 

### 2. Observer Pattern (Deadlock-Free)
Event boundaries broadcast notifications (via a publish/subscribe EventBus architecture) whenever payload data fundamentally flips locally.
**Crucially:** Callback triggers engaging external listeners execute *strictly after the mutex fully unlatches*. The event bus actively clones subscriber registries during secure locks before systematically dispatching signals resting totally uncoupled from critical path executions. This definitively annihilates notorious cyclical deadlock risks where Thread A pauses holding locks while Thread B scrambles capturing an event to sequentially mutate the same object tree.

### 3. Policy-Based Design (Pluggable Lockings)
Circumventing unnecessary heavyweight active OS Kernel dependencies, the generic templated design `SharedValue<T, LockPolicy>` enforces compile-time directive injections matching specific execution environments:
- **`LocalMutexPolicy`**: Employs an ultra-fast raw `std::mutex`. Unrivaled high-performance capabilities assuming scopes remain trapped traversing single memory-space processes strictly.
- **`NamedSystemMutexPolicy`**: Wrapping natively Windows systems functions traversing system wide `CreateMutex` boundaries. Strongly mandated framing explicit Inter-Process bounds (e.g. crossing EXE Server limits routing down towards active parallel clients).
- **`NullMutexPolicy`**: Dummy drop-off directive ensuring raw optimized zero-overhead mechanics exclusively targeting inherently safe single-threaded contexts in theory models.

## Directory Layout

```
SharedValueV2/
├── CMakeLists.txt          # Build mappings driving standalone tests
├── include/                # Header-only library footprints
│   ├── SharedValue.hpp     # Generic shared payload harboring templated lock policies
│   ├── DatasetStore.hpp    # Thread-safe overarching key-value store (ordered/unordered)
│   ├── StorageEngine.hpp   # Abstraction sweeping over std::map plus std::unordered_map
│   ├── Errors.hpp          # Exception hierarchy chains (SharedValueException subtypes)
│   ├── EventBus.hpp        # Publish/subscribe event mechanics
│   ├── LockPolicies.hpp    # Directive scopes: Local, Named, Null
│   ├── Observers.hpp       # Base observer interface structures
│   └── DatasetObserver.hpp # Dataset-specific observer scaffolding
├── tests/                  # Uncoupled standalone C++ sequences
│   ├── UnitTests.cpp       # Functional unit sweeps
│   └── StressTest.cpp      # Multithreaded concurrency bombardment
└── build/                  # CMake drop-offs (untethered from version control)
```

## Practical Implementation Examples

Operating exclusively leveraging header-only configurations completely bypasses archaic linking parameters aside from generating tests locally. Effortlessly merge the `include/` directory path steering the compiler downstream towards inclusion.

### Basic Thread-Safe Value Tracking

```cpp
#include "SharedValueV2/include/SharedValue.hpp"
#include "SharedValueV2/include/Observers.hpp"
#include <iostream>

using namespace SharedValueV2;

// 1. Establish custom listener protocols
class MyObserver : public IObserver {
public:
    void onValueChanged() override {
        std::cout << "Target value heavily altered capturing cross-thread interactions!" << std::endl;
    }
};

// 2. Initialize bound variables wielding hyper fast local locking paradigms
SharedValue<std::string, LocalMutexPolicy> username;

// 3. Register subscriptions mapping shared memory 
auto obs = std::make_shared<MyObserver>();
username.addObserver(obs);

// 4. Guaranteed thread-safe manipulations (acquire lock -> mutate -> release lock -> emit notifications to cloned array)
username.set("Admin"); 

// 5. Guaranteed thread-safe readouts
std::string currentVal = username.get(); // Safely clones snapshot routing it to current process stack
```

### Advanced: Dataset Operations in Operations

Orchestrating huge aggregates extensively relies upon `DatasetStore` scaffolding capabilities. Functioning closely paralleling isolated NoSQL in-memory map grids.

```cpp
#include "SharedValueV2/include/DatasetStore.hpp"

// Mandating explicit Named Mutex policies traversing overarching cross-process boundaries safely. 
DatasetStore<NamedSystemMutexPolicy> store;

// Targetting engine setups dynamically assigning Ordered map vs high-speed unordered Hashing
store.SetStorageMode(StorageMode::UNORDERED_MAP);

// Execute heavily synchronized operations avoiding crash barriers or torn data transfers
store.InsertValue(100, "Player Vector Offset X");
store.InsertValue(101, "Player Vector Offset Y");

auto eventBus = store.GetEventBus(); // Bind strictly harnessing dynamic notifications securely
```

## Exception Hierarchy
The framework vehemently dodges broadcasting unpredictable primitive C++ pointers relying natively upon disciplined Sub-classes inherently descending navigating `SharedValueException` base branches mapped strictly crossing `Errors.hpp`:
- `PolicyException`: Trapped encountering insurmountable circumstances orchestrating core Windows Kernel APIs (`CreateMutex` collapses).
- `StorageModeException`: Interdicting attempts alternating structures spanning arrays back passing to trees assuming current records actively populate the payload block currently.
- `EventBusException`: Explicit registry-blocks eliminating invalid listeners or dodging recursive infinite loops.

## Compilation & Testing

This purely isolated C++20 domain fully sanctions test executions eviscerating any cumbersome COM runtime needs, skyrocketing the agility backing stringent TDD verification sweeps.

```powershell
cd SharedValueV2

# Spin generating cache mappings strictly localized navigating towards \build 
cmake -B build

# Target exhaustive build compilation cycles (Release mode absolutely mandatory brutally simulating true parallel racing across stress conditions!)
cmake --build build --config Release

# 1. Execute streamlined functional mapping checks
.\build\tests\Release\UnitTests.exe

# 2. Execute severe deadlock bombardment tests crashing hundred threaded synchronization logic safely
.\build\tests\Release\StressTest.exe
```

## Requirements

- **C++ Specifications:** Impeccably compliant C++20 compilers mandating advanced RAII integration schemas alongside core language standards (MSVC v143+, GCC 12+, Clang 14+).
- **Environment Tooling:** CMake ≥ 3.20 (strictly tied routing test suites independently).
- **Operating Platforms:** Operating IPC implementations (`NamedSystemMutexPolicy`) heavily map hooking Windows OS native footprints bridging `<Windows.h>`.

## Related Documentation

- [CODE_REFERENCE_EN.md](CODE_REFERENCE_EN.md) — Comprehensive API reference plus exact structural behavior mapping SharedValueV2 details thoroughly.
- [README_EN.md](../README_EN.md) — Project starting block mapping universal integrations generally.
- [ARCHITECTURE_EN.md](../ARCHITECTURE_EN.md) — Core schematics projecting COM overarching layouts respectively.
- [README_EN.md](../ATLProjectcomserverExe/README_EN.md) — Guidance bridging Out-of-process COM implementations heavily derived around the exact SharedValue framework layouts.
