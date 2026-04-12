# SharedValueV2 — C++20 Header-Only Core Library

A fully thread-safe, header-only C++20 library delivering the core logic powering shared variables plus dataset-storage operations. This represents the absolute **engine** driving both the Legacy DLL alongside the EXE COM Server.

## Role within the project

`SharedValueV2` acts as the **single source of truth** dictating all business logic. The overarching ATL COM wrappers resting inside both [`ATLProjectcomserverLegacy/`](../ATLProjectcomserverLegacy/) as well as [`ATLProjectcomserverExe/`](../ATLProjectcomserverExe/) delegate internally routing towards these headers. Consequently, the pristine C++ core evaluates independently stripped of COM entanglements, proving highly testable, immensely reusable, and agile moving forward.

## Design Patterns

- **Monitor Pattern** — Information alongside synchronization acts intrinsically tethered; state permits access exclusively overlapping locked scopes.
- **Observer Pattern (Deadlock-Free)** — Notifications burst strictly outside designated critical sections, harnessing copied observer arrays.
- **Policy-Based Design** — `SharedValue<T, Policy>` digests pluggable locking directives: `LocalMutexPolicy`, `NamedSystemMutexPolicy`, `NullMutexPolicy`.

## Directory layout

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

## Compilation & Testing

SharedValueV2 comfortably compiles besides tests entirely insulated ignoring COM projects relying completely upon CMake:

```powershell
cd SharedValueV2

# Spinning up generation files
cmake -B build

# Target build sweeps (Release mode aggressively advised attacking stress tests)
cmake --build build --config Release

# Executing standard unit sweeps
.\build\tests\Release\UnitTests.exe

# Executing multithreaded heavy bombardment tests
.\build\tests\Release\StressTest.exe
```

## Usage as a library

SharedValueV2 firmly remains header-only. Simply inject the `include/` directory addressing your include paths:

```cpp
#include "SharedValueV2/include/SharedValue.hpp"
#include "SharedValueV2/include/DatasetStore.hpp"
#include "SharedValueV2/include/Errors.hpp"

// Example: provisioning a thread-safe shared instance wielding local mutex handling
SharedValueV2::SharedValue<std::string, SharedValueV2::LocalMutexPolicy> sharedVal;
sharedVal.set("Hello!");
auto val = sharedVal.get(); // "Hello!"
```

## Requirements

- C++20 strictly compliant compilers (MSVC v143+, GCC 12+, Clang 14+).
- CMake ≥ 3.20 (strictly tied towards enabling standalone testing).


## Related Documentation

- [CODE_REFERENCE_EN.md](CODE_REFERENCE_EN.md) — Comprehensive exhaustive C++ API reference plus structural documentation mapping SharedValueV2.
- [README_EN.md](../README_EN.md) — Main documentation and starting point of the entire project.
- [ARCHITECTURE_EN.md](../ARCHITECTURE_EN.md) — Main architecture document covering the entire COM Server project tree.
- [README_EN.md](../ATLProjectcomserverExe/README_EN.md) — User guide and overview of the EXE COM Server variant.
