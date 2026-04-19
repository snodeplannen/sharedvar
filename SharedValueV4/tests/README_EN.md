# SharedValueV3 MemMap — Tests

## Overview

This directory contains the automated end-to-end test suite for the SharedValueV3 Memory-Mapped FlatBuffers engine. The tests validate the complete cross-process pipeline from C++ producer to C# consumer.

## Running the Test Suite

```powershell
# Full run (including build)
.\SharedValueV3_MemMap\tests\Run-MemMapTests.ps1

# Without rebuild (faster for repeated testing)
.\SharedValueV3_MemMap\tests\Run-MemMapTests.ps1 -SkipBuild
```

## Test Scenarios

| # | Test | Validates |
|---|---|---|
| 0 | **Build Verification** | CMake C++ and dotnet C# compile without errors or warnings |
| 1 | **Basic Data Transfer** | Producer sends 3 updates → consumer receives all 3, FlatBuffer fields correctly parsed |
| 2 | **Multi-Row Dataset** | 5 rows per update with 2 NestedDetails each → all rows and nesting correctly received |
| 3 | **Consumer-Before-Producer** | Consumer starts before the producer → retry mechanism works, data still arrives |
| 4 | **Connection Timeout** | Consumer without producer → timeout after N seconds, exit code 2, error message |
| 5 | **Rapid-Fire Updates** | 10 updates at 200ms interval → consumer receives at least 5 events (auto-reset coalescing) |
| 6 | **Boolean Toggle** | `is_active` field toggles every 3rd update → consumer observes both `True` and `False` |

## CLI Arguments (Producer)

```
MemMapProducer.exe [options]
  --count N       Send N updates and stop (default: infinite)
  --interval MS   Wait time in milliseconds between updates (default: 2000)
  --rows N        Number of DataRows per update (default: 1)
  --name NAME     Name of the shared memory (default: MyGlobalDataset)
  --linger MS     Milliseconds to stay alive after the last write (default: 5000)
  --help          Show help
```

## CLI Arguments (Consumer)

```
dotnet run -- [options]
  --name NAME       Name of the shared memory (default: MyGlobalDataset)
  --count N         Stop after N received events (default: infinite / ENTER)
  --timeout SEC     Connection timeout in seconds (default: 30)
  --help            Show help
```

## Exit Codes (Consumer)

| Code | Meaning |
|---|---|
| 0 | Exited successfully |
| 2 | Connection timeout — producer not found |
| 3 | Event timeout — expected events not received |

## Note on Rapid-Fire Tests

In test 5 (rapid-fire), the consumer might receive fewer events than the producer sends. This is **expected behavior**: the Named Event is configured as **auto-reset**. If the producer calls `SetEvent()` twice before the consumer thread wakes up from `WaitOne()`, those two signals are coalesced into a single wake-up.

This is by design: the consumer always reads the **most recent** data from the memory block, regardless of how many intermediate updates were missed.

## Why `--linger` is needed in tests

Windows kernel objects (MMF, Mutex, Event) are destroyed as soon as all handles are closed (refcount → 0). During automated tests, the producer sends a limited number of updates (`--count N`) and then exits. If the consumer hasn't connected yet (e.g. because of `dotnet run` JIT compilation), the kernel objects are already destroyed.

The `--linger MS` parameter solves this: after its last write, the producer waits another N milliseconds before exiting. This gives the consumer time to connect and receive all events.

**In normal (production) use, `--linger` is not needed.** The producer runs continuously (`--count` is not provided) and the kernel objects remain alive as long as the producer process runs. Consumers can connect and disconnect at any time.

## Related Documentation

- [README_EN.md](../README_EN.md) — Introduction, project structure and quickstart.
- [ARCHITECTURE_EN.md](../ARCHITECTURE_EN.md) — Detailed architecture document including Windows kernel object lifecycle.
- [README_EN.md](../../README_EN.md) — Main documentation and entry point of the entire project.
- [CHANGELOG_EN.md](../../CHANGELOG_EN.md) — Overview of all changes and release notes.
