# SharedValueV3 (Memory-Mapped Engine)

This reflects the V3 generation iteration addressing the SharedValue architecture. Deviating extensively away from Out-of-Process COM (RPC leveraging Named Pipes), this model operates 100% targeting **High-Performance Memory-Mapped Files (Windows Shared Memory)**.

## Why this architecture?
The vintage COM-methodology shoulders inevitable micro-delays impacting function calls heavily tied towards RPC marshaling parameters (initiating context switches). Providing you actively attempt pulling 10,000 respective data-points piecemeal streaming in-memory reaching C# thresholds, tackling this exclusively through COM guarantees monolithic speed reductions, *unless* fetching aggregates looping single monumental array hauls.

The **MemMap Engine** spectacularly obliterates these hurdles:
1. **0% Overhead & Zero-Copy**: C# accesses the exact identical memory-partition previously manipulated passing through C++. Propelled via **FlatBuffers** infinitely layered nesting/dynamic blocks serialize blasting downwards entirely packed as solitary unbroken segments yielding flawless readouts *devoid completely of parser-mechanics* spanning both C# besides C++.
2. **0% CPU Callbacks**: Maintaining legacy push-notifications (mirroring COM occurrences alongside callbacks), this mechanism rests inherently on **Windows Named Events** (`CreateEventW`). The C# listenthread remains profoundly slumbering clocking 0% CPU consumption indefinitely observing the moment the producer loads data discharging the payload snapping the communication trigger.
3. **Multi-Process Bulletproof**: Data scrambling stands utterly eradicated enforced navigating active **Windows Named Mutex** barriers rigidly orchestrated dropping within the core `SharedValueEngine` hull.

## Lifecycle & Decoupling

The producer sitting alongside respective consumer nodes remain **completely isolated**. Windows kernel components spanning (MMF, Mutex, Event) persist autonomously assuming natively **one singular process** claims a hanging handle. Fundamentally proving:

- The producer chugs **infinitely** injecting pipelines dismissing consumer connectivity thoroughly
- Consumers remain completely free **whenever they choose** tapping the chain, inhaling readouts dropping off
- **Multiple consumers** operate seamlessly synchronously (each unlocking dedicated local handlers gripping universal kernel roots)
- Producers operate fully oblivious dismissing consumers (fire-and-forget design)

> **Important:** Provided the producer terminates leaving absolutely zero consumer counterparts chained, Windows completely incinerates the kernel framework (refcount → 0). Successive consumers booting up drastically fail establishing links. Benefiting automated test-sweeps exclusively, the producer extends fielding a `--linger` parameter suspending kernel lifecycle eradication prolonging presence slightly. Observe [ARCHITECTURE_EN.md](ARCHITECTURE_EN.md#kernel-object-lifecycle-and-reference-counting) digesting exhaustive nuances.

## Project Structure
*   `schema/` encapsulates `dataset.fbs`. Mapping out FlatBuffers data blueprints fundamentally. 
*   `cpp_core/` embodies the Native C++ Producer component.
*   `csharp_core/` embodies the Managed C# Consumer node.
*   `tests/` encases comprehensive automated end-to-end framework sweeps.
*   `build_schema.ps1` extracts silently retrieving Google `flatc` rendering automated wrap-logic derived navigating schema parameters targeting both dialects.

## How to execute

### Interactive (Manual flow)
1. **Launch a Terminal window:** Engage the producer chaining optional modifiers:
   ```powershell
   .\cpp_core\build\Release\MemMapProducer.exe
   .\cpp_core\build\Release\MemMapProducer.exe --count 10 --interval 500 --rows 3
   ```
2. **Launch a secondary Terminal:** Spark the consumer instance:
   ```powershell
   cd csharp_core
   dotnet run
   dotnet run -- --count 5    # Auto-termination triggering following 5 occurrences
   ```

### Automated (Test Suite)
Unleash the comprehensive end-to-end suite effectively checking 6 distinctive patterns:
```powershell
.\tests\Run-MemMapTests.ps1             # Enforces clean builds prior
.\tests\Run-MemMapTests.ps1 -SkipBuild  # Sidesteps re-compilations
```

Browse [`tests/README_EN.md`](tests/README_EN.md) targeting the fully exhaustive test breakdown observing all CLI flag extensions.

## Related Documentation

- [ARCHITECTURE_EN.md](ARCHITECTURE_EN.md) — Expansive architecture documentation plotting Mermaid-schematics charting Memory-Mapped Files, FlatBuffers serialization, synchronous structures (Mutex/Event), producer-consumer pacing loops, hierarchical exception mapping, flanked via the pipeline build metrics.
- [INSTALL_EN.md](INSTALL_EN.md) — Instructions for building and configuring the environment.
- [CODE_REFERENCE_EN.md](CODE_REFERENCE_EN.md) — Overview of the key components, files, and classes.
- [tests/README_EN.md](tests/README_EN.md) — Suite overview tracking, CLI modifier metrics, expanding upon scenario details.
- [ARCHITECTURE_EN.md](../ARCHITECTURE_EN.md) — Main architecture document covering the entire COM Server project tree.
- [CHANGELOG_EN.md](../CHANGELOG_EN.md) — Comprehensive log reflecting overarching amendments adjoining release notes.
