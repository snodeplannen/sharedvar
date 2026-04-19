# Research: Mitigations for SharedValueV3 MemMap Disadvantages

This document analyzes the five known disadvantages of the SharedValueV3 Memory-Mapped architecture and presents multiple concrete solution paths per disadvantage, ranked by feasibility and impact.

---

## Table of Contents

1. [No method calls / RPC](#1-no-method-calls--rpc)
2. [Producer must run first](#2-producer-must-run-first)
3. [Unidirectional design](#3-unidirectional-design)
4. [No automatic proxy generation](#4-no-automatic-proxy-generation)
5. [Schema changes require recompilation](#5-schema-changes-require-recompilation)
6. [Summary matrix](#6-summary-matrix)

---

## 1. No method calls / RPC

**Problem:** The consumer can only read data from the shared memory block. There is no mechanism to invoke a function executed by the C++ producer from C# (e.g., "recalculate sensor data", "change sampling rate").

### Solution A: Command Channel via Second MMF (⭐ Recommended)

Introduce a **second, diminutive Memory-Mapped block** (e.g., 4 KB) to act as a command/request buffer. The consumer writes a command, and the producer listens via a dedicated Named Event.

```
┌─────────────────┐         ┌──────────────────┐         ┌──────────────────┐
│   C# Consumer   │         │  Shared Memory   │         │  C++ Producer    │
│                 │         │                  │         │                  │
│ WriteCommand()──┼────────►│  Command MMF     │────────►│  ReadCommand()   │
│                 │         │  (4 KB)          │         │  Execute()       │
│                 │         │                  │         │                  │
│ OnDataReady() ◄─┼─────────│  Data MMF        │◄────────┼──WriteData()     │
│                 │         │  (10 MB)         │         │                  │
└─────────────────┘         └──────────────────┘         └──────────────────┘
```

**Implementation:**
- New kernel objects: `Global\..._CmdMap`, `Global\..._CmdMutex`, `Global\..._CmdEvent`
- Command buffer layout: `[uint32 cmd_id][uint32 payload_size][byte[] payload]`
- Producer runs a second listener thread waiting for `CmdEvent`
- Response is routed directly back into the same command block, using a `ResponseEvent`

**Advantages:**
- Completely contained within the existing architecture (no new dependencies)
- Nanosecond latency scaling up to commands
- Consumer and producer remain decoupled

**Disadvantages:**
- Necessitates a command-protocol definition (which cmd_id's exist?)
- Implies building custom request/response correlations for async commands

---

### Solution B: Named Pipes as Command Sidecar

Leverage the existing MMF for bulk data (unidirectional, rapid), yet affix a **Windows Named Pipe** as a bidirectional command channel.

```
DATA:     C++ ──── MMF (10 MB) ──── → C#     (high-throughput, zero-copy)
COMMANDS: C# ──── Named Pipe ────── → C++    (bidirectional, request/response)
```

**Implementation:**
- Producer instantiates `\\.\pipe\SharedValue_CommandPipe`
- Consumer connects and forwards commands as modest messages
- Named Pipes are inherently bidirectional (full-duplex) and embrace overlapped I/O

**Advantages:**
- Named Pipes feature native signaling and framing — no manual protocol
- Full-duplex: response bounces back over the identical pipe
- .NET provides native `NamedPipeClientStream` / `NamedPipeServerStream`

**Disadvantages:**
- Higher latency compared to MMF (~μs vs ~ns) for commands — however commands are typically infrequent
- Additional dependency on pipe lifecycle management

---

### Solution C: FlatBuffers RPC Framework (gRPC-Style)

FlatBuffers natively backs defining `rpc_service` interfaces inside the `.fbs` schema:

```fbs
rpc_service SensorControl {
    SetSamplingRate(SamplingRateRequest): SamplingRateResponse;
    RecalibrateAll(Empty): CalibrationResult;
}
```

`flatc` is capable of generating gRPC stubs for this. This fuses the swift FlatBuffers serialization with a comprehensive RPC framework.

**Advantages:**
- Formal interface definition residing in the schema
- Automatic codegen regarding client and server stubs
- Battle-tested framework (gRPC) acting as transport

**Disadvantages:**
- Introduces gRPC as a massive dependency (HTTP/2, Protobuf runtime)
- Commands consequently traverse TCP instead of direct shared memory — overkill for local IPC
- Complex setup targeting a purely local scenario

---

### Recommendation

**Solution A** (Command Channel via second MMF) for maximum cohesion with the prevalent architecture. Should the command frequency be minimal and you'd prefer an out-of-the-box protocol, **Solution B** (Named Pipes sidecar) constitutes the most pragmatic route.

---

## 2. Producer must run first

**Problem:** The consumer sits disconnected if the Memory-Mapped File is unborn. Currently, the consumer waits caught in a `Thread.Sleep(1000)` retry loop, which is both inelegant and tardy.

### Solution A: Named Event as "Ready" Signal (⭐ Recommended)

Instruct the consumer to create (or listen to) a **Named Event** which the producer triggers whenever the engine initializes.

**Flow:**
1. Consumer boots → creates `Global\..._Ready` event (or attempts opening it)
2. Consumer summons `WaitForSingleObject(_readyEvent)` — plunges into sleep (0% CPU)
3. Producer boots → initializes MMF, Mutex, Data Event
4. Producer unseals and signals `Global\..._Ready` → consumer awakens
5. Consumer unseals MMF, Mutex, Data Event → everything operational

**Advantages:**
- 0% CPU footprint while waiting (devoid of polling)
- Instantaneous notification the moment the producer steps up
- Painless implementation: one extraneous `CreateEventW` / `EventWaitHandle`

**Disadvantages:**
- Negligible extra kernel object administration

---

### Solution B: `CreateFileMapping` as Create-or-Open

The Windows API `CreateFileMappingW` acts exactly as a "create or open" operation. Granted the name already exists, it issues a handle to the prevailing object alongside flipping `GetLastError()` to `ERROR_ALREADY_EXISTS`.

**Implementation:**
- **Both** processes invoke `CreateFileMappingW` abandoning respectively `Create` and `Open`
- The primordial process opening the bout crafts the MMF
- The secondary process reaps a handle to the incumbent MMF

**Advantages:**
- Eradicates the startup-sequence plight — matters zero who boots first
- Eliminates any retry loop

**Disadvantages:**
- Both processes map congruent size restrictions
- Post creation a lingering wait persists until the producer effectively drops data (the block rests empty)
- C#'s `MemoryMappedFile.CreateOrOpen()` lives out there wrapping this native API favorably

---

### Solution C: Windows Service as Persistent Host

Launch the producer as an unyielding **Windows Service** booting automatically alongside system start via `sc.exe` or `services.msc`.

**Advantages:**
- Shared memory endures indefinitely available — devoid of startup-sequence dilemmas
- Service recovers dynamically on crashes (recovery tabs within Service Manager)

**Disadvantages:**
- Elevates deployment complexity drastically
- Bloats local development workflow

---

### Recommendation

**Solution A** (Ready Event) reigns supreme: zero-CPU delay, prompt notification, miniscule code. **Solution B** (CreateOrOpen) acts as an excellent alternative granted you wish to exterminate the startup-sequence plight entirely.

---

## 3. Unidirectional design

**Problem:** The prevalent architecture strictly endorses producer → consumer dataflows. The consumer possesses zero leeway to return data to the producer.

### Solution A: Symmetrical Dual-Channel (⭐ Recommended)

Construct two identical MMF channels holding individual Mutexes and Events:

```
Channel 1 (Producer → Consumer):
  MMF:   Global\Dataset_P2C_Map      (10 MB)
  Mutex: Global\Dataset_P2C_Mutex
  Event: Global\Dataset_P2C_Event

Channel 2 (Consumer → Producer):  
  MMF:   Global\Dataset_C2P_Map      (1 MB)
  Mutex: Global\Dataset_C2P_Mutex
  Event: Global\Dataset_C2P_Event
```

**Implementation:**
- Both processes mint two `SharedValueEngine` instances
- Process A behaves as producer regarding channel 1, and consumer over channel 2
- Process B acts as consumer covering channel 1, stepping up as producer spanning channel 2
- The established `SharedValueEngine` class remains seamlessly reusable

**Advantages:**
- Exhaustive reuse of existing source
- Disjointed throughput rates directionally
- Banishes deadlock hazards (split locks)

**Disadvantages:**
- Doubles kernel objects (6 instead of 3)
- Marginally inflates footprint

---

### Solution B: Partitioned Single MMF

Operate one massive MMF block yet partition it logically splitting:

```
┌────────────────────────────────────────────────────┐
│              Shared Memory (12 MB)                 │
├────────────────────────┬───────────────────────────┤
│  Zone A: P→C Data      │  Zone B: C→P Data         │
│  Offset 0 – 10 MB      │  Offset 10 MB – 12 MB     │
│  Mutex A, Event A       │  Mutex B, Event B          │
└────────────────────────┴───────────────────────────┘
```

**Advantages:**
- A solitary kernel object for the array
- Minor overhead shaving down from two mappings

**Disadvantages:**
- Escalates offset math complexity
- Rigid partitioning spells out inflexibility
- Dictates two Mutexes and Events nevertheless

---

### Solution C: Ring Buffer (Circular Buffer) within MMF

Bake a lock-free ring buffer right inside the shared memory bounds. Both processes maneuver simultaneously skimming and etching circumventing mutex-locks.

**Advantages:**
- Lock-free execution → explosive throughput handling sky-high ticks
- Intrinsically bidirectional

**Disadvantages:**
- Outstandingly dense endeavor to get flawlessly engineered (memory ordering, cache line padding)
- A nightmare to debug properly
- Exceedingly opaque paired with variable-span FlatBuffer payloads

---

### Recommendation

**Solution A** (Dual-Channel) charts the simplest course duplicating the runtime 1-on-1. Solution B reigns space-efficient although rigid. Solution C warrants attention solely navigating extreme throughput dictates (>100K msgs/sec).

---

## 4. No automatic proxy generation

**Problem:** Freshly boarded client-languages (Python, Rust, Go) must manually handshake Windows kernel APIs (`CreateFileMapping`, `WaitForSingleObject`, etc.) while replicating the FlatBuffer parsing logic.

### Solution A: C-Core featuring FFI Bindings (⭐ Recommended)

Wrench the core engine logic aside into an uncluttered **C shared library** (DLL) beaming a steady `extern "C"` ABI, subsequently auto-generating bindings addressing varied languages.

```
┌──────────────────────────────────────────────────────┐
│           SharedValueEngine.dll (C API)              │
│                                                       │
│  EXPORT sv_engine_create(name, size) → handle         │
│  EXPORT sv_engine_write(handle, data, len)            │
│  EXPORT sv_engine_read(handle, buf, maxlen) → len     │
│  EXPORT sv_engine_wait_event(handle, timeout_ms)      │
│  EXPORT sv_engine_destroy(handle)                     │
└──────────┬──────────┬───────────┬────────────────────┘
           │          │           │
     ┌─────▼──┐  ┌────▼───┐  ┌───▼────┐
     │ C#     │  │ Python │  │  Rust  │
     │P/Invoke│  │ ctypes │  │  FFI   │
     └────────┘  └────────┘  └────────┘
```

**Bindings per language:**
- **C#**: `[DllImport("SharedValueEngine.dll")]` routed via P/Invoke
- **Python**: `ctypes.CDLL("SharedValueEngine.dll")` alongside `cffi`
- **Rust**: `extern "C"` embracing FFI blocks
- **Go**: `cgo` carrying `#include` directives

**Advantages:**
- Hand-code it once → every dialect understanding FFI boards the train
- The C API boils down to the leanest interface footprint realistically achievable (5-10 methods)
- FlatBuffers independently champions built-in codegen stretching over 15+ targets

**Disadvantages:**
- Predicates spinning up an isolated DLL alongside freezing a solid ABI
- Enforces strict lifecycle handling (handles cleanup) manually mapped over diverse runtimes

---

### Solution B: FlatBuffers Codegen + Platform Wrappers

Seeing FlatBuffers already dishes out codegen aiming at C++, C#, Python, Java, Go, Rust, TypeScript alongside others, isolatedly the **MMF/Mutex/Event wrapper** demands coding bridging OS domains. It boils down to a trivial shim layer.

**Setup:**
1. `flatc` automatically mints data-classes catering uniquely
2. For each tongue you drop a bare-bones ~100 LOC shim prying open those 3 kernel objects
3. Push those wrappers onto package hubs (NuGet, PyPI, crates.io)

**Advantages:**
- The serialization tier (arguably the murkiest) bows to FlatBuffers handling it seamlessly
- The platform tether constitutes a brief, one-shot project per language

**Disadvantages:**
- Yields no evasion writing out a tailored runtime shim per syntax (Python `mmap`, Rust `winapi`, etc.)
- Scatters upkeep across language repos

---

### Solution C: SWIG paired to a gRPC Gateway

Deploy **SWIG** forcing automatic C++ → Python / C# binding minting, alternatively planting a **gRPC gateway** funneling the MMF array downstream playing a regular HTTP-based RPC provider.

**Advantages:**
- SWIG kicks out bindings straight referencing C++ blueprints
- gRPC gateways broadcast payloads atop ubiquitous HTTP/2 satisfying arbitrary consumers

**Disadvantages:**
- SWIG outfeeds frequently translate into debugging purgatory demanding exact tweaks
- gRPC gateways inject serialization dead-weight ultimately eroding the zero-copy magic

---

### Recommendation

**Solution A** (C-Core DLL) tackling uncompromising performance benchmarks. **Solution B** (FlatBuffers codegen + ultra-light platform wrapping) granted you envision quickly boarding assorted tongues escaping DLL compilation woes.

---

## 5. Schema changes require recompilation

**Problem:** Modifying `dataset.fbs` universally predicates kicking off `flatc`, tail-gating down to a comprehensive rebuild spanning neither less the C++ producer alongside the C# consumer.

### Solution A: Pre-Build Hook Integration (⭐ Recommended)

Stitch `flatc` compile-passes natively into build pipelines assuring schema modifications flow effortlessly downstream seamlessly.

**CMake (C++ side):**
```cmake
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/dataset_generated.h
    COMMAND flatc --cpp -o ${CMAKE_CURRENT_SOURCE_DIR} ${SCHEMA_DIR}/dataset.fbs
    DEPENDS ${SCHEMA_DIR}/dataset.fbs
    COMMENT "Regenerating FlatBuffers C++ header..."
)
```

**MSBuild / .csproj (C# side):**
```xml
<Target Name="GenerateFlatBuffers" BeforeTargets="BeforeBuild">
    <Exec Command="powershell -File $(SolutionDir)build_schema.ps1" />
</Target>
```

**Advantages:**
- Edit schema → hit `F5` → magic happens effortlessly without manual friction
- Exterminates the mandate pulling `build_schema.ps1` standalone ever again
- Plugs flawlessly into CI/CD pipelines

**Disadvantages:**
- Stipulates having `flatc` mapped onto PATH or auto-downloading it via script (already mitigated)

---

### Solution B: FlatBuffers Reflection (Runtime Schema)

FlatBuffers brings forward a **Full Reflection API** unleashing powers dynamically loading a binary schema (`.bfbs`) alongside transversing payloads natively without executing generated source code.

**Workflow:**
1. Blast schema towards binary: `flatc -b --schema dataset.fbs` → `dataset.bfbs`
2. Bundle the `.bfbs` file as a runtime asset (situated beside the binary)
3. The consumer ingests the `.bfbs` exploring data structurally leaning upon the reflection engine

```cpp
// C++ runtime reflection
auto schema = reflection::GetSchema(bfbs_data);
auto root_table = schema->root_table();
for (auto field : *root_table->fields()) {
    // Dynamic readout evading generated scaffolding
}
```

**Advantages:**
- Nukes rebuild mandates whenever the schema shifts — only replacing the `.bfbs` asset suffices
- Superb serving tooling, diagnostics, plus schema-agnostic viewers
- Forward-compatible capability: vintage consumers silently dump unfamiliar footprints

**Disadvantages:**
- Massively slower comparing generated logic (leveraging runtime pointer chases instead of compile-time offset hops)
- Steep to deploy right (practically authoring an interpreter)
- Strictly unsuited concerning performance-heavy hot paths

---

### Solution C: `flatc --conform` staged on CI/CD

Bolt a CI stage rigorously ensuring incoming schema tweaks align regarding backward-compatibility dictates:

```bash
flatc --conform old_schema.bfbs new_schema.fbs
```

This halts buildlines if you:
- Obliterate an existing field
- Distort a field's type
- Jumble Field IDs

**Advantages:**
- Traps destructive breaking modifications upstream shielding production
- Marvelously compliments Solution A (pre-build hooks)

**Disadvantages:**
- Abandons resolving the core recompilation hassle — strictly acts as a safeguard

---

### Solution D: FlexBuffers (Schemaless Variant)

Google parallels FlatBuffers pushing **FlexBuffers** — projecting a schemaless, fully self-describing binary envelope housed in the identical dependency.

```cpp
// Dictating schemaless writes
flexbuffers::Builder fbb;
fbb.Map([&]() {
    fbb.String("api_version", "2.0.0");
    fbb.Double("temperature", 24.5);
    fbb.Vector("sensors", [&]() {
        fbb.Double(22.1);
        fbb.Double(23.4);
    });
});
```

**Advantages:**
- Zero schema declaration anywhere
- Omits the `flatc` compiler pass universally
- Fields effortlessly tacked on lacking binary rebuilds

**Disadvantages:**
- Evaporates type-safety anchoring compile-time sanity
- Readout slower opposed to FlatBuffers (runtime type validation routines)
- Anemic dialect coverage (targets C++, Java, JavaScript, Rust — lacking official C# backing)
- Shreds any zero-copy access warranties

---

### Recommendation

**Solution A** (Pre-Build Hooks) dictates the pragmatic approach: recompilation inherently remains but stays masked away. Interlock with **Solution C** (`--conform`) across CI sniffing out breaking regressions. **Solution B** (Reflection) boasts massive utility powering developer tooling but should definitely avoid hot paths.

---

## 6. Summary matrix

| Disadvantage | Recommended Solution | Complexity | Impact |
|---|---|---|---|
| No method calls / RPC | Command Channel (second MMF) | 🟡 Moderate | High — evolves system interactivity |
| Producer must run first | Named "Ready" Event | 🟢 Low | High — bypasses polling retry loops |
| Unidirectional design | Symmetrical Dual-Channel | 🟡 Moderate | High — achieves complete bidirectional scaling |
| No auto proxy-generation | C-Core DLL + FFI bindings | 🔴 High | Medium — concerns solely >2 alien languages |
| Schema demands rebuilds | Pre-Build Hooks + `--conform` CI | 🟢 Low | Medium — catapults DX upwards |

### Prioritization (Suggested Rollout Steps)

1. **Ready Event** (mitigates #2) — lightest workload, heaviest quality uptick
2. **Pre-Build Hooks** (mitigates #5) — nominal CMake/MSBuild touch-up, stellar DX gains
3. **Command Channel** (mitigates #1) — morphs the setup into an exhaustive IPC framework
4. **Dual-Channel** (mitigates #3) — natively spawns adhering to #3, mirroring identical paradigms
5. **C-Core DLL** (mitigates #4) — solely warranted upon concrete Python/Rust onboarding demands
