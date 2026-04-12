# CSharpNet8Test — C# .NET 8 DatasetProxy Integration Tests (EXE Server)

> **COM Server Variant:** **Out-of-Process EXE Server** (`ATLProjectcomserver.exe`)
> ProgID prefix: `ATLProjectcomserverExe.*`

A heavily typed C# .NET 8 console application orchestrating **EXE server** end-to-end proving routines powered entirely spanning **late binding** (`dynamic` / `Activator.CreateInstance`). All remote instructions naturally ride **RPC-marshaling** leaping the LRPC Named Pipe penetrating the external server-process — delivering an unadulterated cross-process footprint.

> [!NOTE]
> This predominantly addresses the strictly isolated **EXE server** branch. Analyzing the contrasting **Legacy DLL** offshoot (in-process targeting), navigate towards [`tests/CSharpNet8Test/`](../../../../tests/CSharpNet8Test/).

---

## Requirements

1. **.NET 8 SDK** (verifying `dotnet --version` natively displaying `8.x`).
2. The core EXE server inherently necessitates full **registration** executed assuming Administrator rights:
   ```cmd
   x64\Debug\ATLProjectcomserver.exe /RegServer
   ```

---

## Execution Launch

```powershell
cd ATLProjectcomserverExe\tests\CSharpNet8Test
dotnet run
```

The underlying EXE server springs magically matching on-demand scenarios assuming it slumbers presently deactivated.

Anticipated diagnostics scoring an unblemished pass:
```
=== C# .NET 8 DatasetProxy Tests ===

--- Producer Flow ---
  PASS: Producer: 100 rows injected successfully
  PASS: Producer: Proxy securely parked inside SharedValue

--- Consumer Flow ---
  PASS: Consumer: 100 rows fetched
  PASS: Consumer: Data-integrity unequivocally verified
  PASS: Consumer: Pagination processing (10 keys)

--- Bidirectional Mutation ---
  PASS: Mutation: 2 records residing post update/remove sequence
  PASS: Mutation: UpdateRow acts dependably
  PASS: Mutation: RemoveRow acts dependably

--- Error Handling ---
  PASS: Error: KeyNotFound matched including descriptive payload
  PASS: Error: DuplicateKey matched including descriptive payload
  PASS: Error: StoreModeNotEmpty matched including descriptive payload

--- Storage Mode ---
  PASS: StorageMode: Unordered mode mapping (1)
  PASS: StorageMode: CRUD completely intact spanning Unordered

========================================
Results: 14 passed, 0 failed
```

---

## Testing Scenarios

### Test 1 — Producer Flow

**Core objective:** Beaming 100 entries spanning towards the server executing transparent RPC-marshaling routines.

**Timeline:**
1. Summons `ATLProjectcomserverExe.DatasetProxy` instance → translating implicitly driving RPC-pings impacting the server.
2. Wedges 100 entries driving `AddRow` arrays — every solitary hop forces grueling RPC round-trips natively.
3. Certifies tracking `GetRecordCount()` scoring exactly 100.
4. Lodges the proxy tightly inside `ATLProjectcomserverExe.SharedValue` utilizing `SetValue(proxy)`.

**Divergence against Legacy DLL logic:** Literally every `AddRow` constitutes a heavy networked RPC packet hop. The payload intrinsically lives and breathes exclusively inside a **separate shielded Windows process** (`ATLProjectcomserver.exe`), strictly evading resting within C# process boundaries.

---

### Test 2 — Consumer Flow

**Core objective:** Reading out previously injected matrices simultaneously vetting meticulous `SAFEARRAY` RPC-marshaling alignments.

**Timeline:**
1. Scoops up the primary `DatasetProxy` node mapping via `SharedValue.GetValue()`.
2. Corroborates tracking `GetRecordCount()` registering 100.
3. Tests fundamental data-integrity mappings: `GetRowData("user:0")` must categorically harbor `"name:User0"`.
4. Plucks chunks paginating 10 keys wielding `FetchPageKeys(0, 10)` securely cross-checking `keys.Length == 10`.

**Technical hurdles mapped:** The `SAFEARRAY` (`VT_ARRAY | VT_VARIANT`) structurally demands flawless marshaling retaining absolute composure transitioning from the remote server-process right onto the grasping C# app. This routinely audits the massive MIDL-generated proxy/stub architecture implicitly.

---

### Test 3 — Bidirectional Mutation Flow

**Core objective:** Exercising overlapping `UpdateRow` flanked beside `RemoveRow` routines hammering across RPC.

**Timeline:**
1. Shove 3 rows onto the queue: `mut_1`, `mut_2`, `mut_3`.
2. Heavily amend `mut_1` enforcing an `"updated"` parameter switch.
3. Purge precisely `mut_2`.
4. Solidify tests tracking: `GetRecordCount()` scores 2, `GetRowData("mut_1")` scores `"updated"`, `HasKey("mut_2")` universally rejects scoring `false`.

---

### Test 4 — Error Handling Traps

**Core objective:** Proving native server-side C++ exceptions actively pierce bounding box realities cascading downwards appearing natively as `COMException` objects within C# targeting RPC.

**Timeline:**

| Edge-Case Matrix | Trigger Command | Required Backlash Reflection |
|---|---|---|
| KeyNotFound | `GetRowData("phantomKey")` | `COMException` embedding `"not found"` enclosed inside `.Message` |
| DuplicateKey | Repeating `AddRow("dup", ...)` | `COMException` embedding `"already exists"` enclosed inside `.Message` |
| StoreModeNotEmpty | Sparking `SetStorageMode(1)` whilst payload overlaps | `COMException` embedding `"not empty"` enclosed inside `.Message` |

**Decoding RPC error cascading:** `SharedValueException (native C++)` → `HRESULT + IErrorInfo bridging (COM server)` → `NDR marshaling packet` → `COMException wrapper (.NET client)`.

---

### Test 5 — Storage Mode Tweaks

**Core objective:** Pivoting dynamically swapping out `std::map` (rigidly ordered) jumping onto `std::unordered_map` (unordered tracking) natively localized server-side.

**Timeline:**
1. Summon an intrinsically hollow untouched `DatasetProxy`.
2. Dial `SetStorageMode(1)` — the server flawlessly hotswaps the core storage engine handling silently backwards.
3. Validate metrics polling `GetStorageMode()` landing 1.
4. CRUD routine verification: `AddRow` appended by `GetRowData` act completely unharmed post-transition leaps.

---

## Direct Disparities Countering Legacy DLL Outlines

| Property | Legacy DLL (native in-process) | EXE Server (detached out-of-process) |
|---|---|---|
| **Server residency** | Nestled locally inside C# | Completely detached Windows process shell |
| **Communication flow** | Snappy immediate vtable function hops | Heavier RPC targeting LRPC Named Pipes |
| **ProgID prefixing** | `ATLProjectcomserver.*` | `ATLProjectcomserverExe.*` |
| **Data survivability** | Extinguishes matching C# termination | Preserved persistently hugging the server process |
| **Throughput speeds** | Instantaneous (zero-copy in-process) | Sluggish multipliers (costly RPC networking trips) |
| **Cross-process merging** | ❌ Categorically impossible | ✅ Universal multiple clients locking one singular state |


## Related Documentation

- [README_EN.md](../../../README_EN.md) — Main documentation and starting point of the entire project.
- [ARCHITECTURE_EN.md](../../../ARCHITECTURE_EN.md) — Main architecture document covering the entire COM Server project tree.
- [README_EN.md](../../README_EN.md) — User guide and overview of the EXE COM Server variant.
- [README_EN.md](../../../SharedValueV2/README_EN.md) — Introductory overview guiding the SharedValueV2 C++20 engine.
