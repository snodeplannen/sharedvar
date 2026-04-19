# Cross-Process Architecture Redesign

The current `ATLProjectcomserver` is designed as an **In-Process Server (.dll)**. This signifies that memory is strictly isolated per process. When `cscript.exe` (VBScript) and `dotnet.exe` (C#) invoke the COM objects, they both independently load a copy of the DLL into their respective memory spaces (heaps).

To achieve genuine data sharing between separate processes, we must bridge the process boundary.

> [!WARNING]
> **User Review Required**
> Select one of the migration paths below. Option 2 represents the most logically sound architectural intermediate step, whereas Option 1 transforms the project type entirely and Option 3 significantly escalates the complexity of the C++ code.

---

## Option 1: Native Out-of-Process Server (ATL EXE)

We alter the output of the C++ project in Visual Studio from `.dll` to `.exe` (`LocalServer32`). 

**How it operates:**
When a program calls `CreateObject("ATLProjectcomserver.SharedValue")`, the Windows COM Service (`svchost.exe`) launches our EXE in the background. The EXE hosts the C++ Core (the *SharedValueV2* singleton). Both the VBS script and the C# application communicate with that single centralized EXE process via RPC (Remote Procedure Call) marshaling.

* **Pros:** The standard "Microsoft-recommended" mechanism to instantiate a cross-process COM singleton. Highly stable.
* **Cons:** Necessitates a refactor of the C++ COM boilerplate (from `DllMain` to `_tWinMain`, from `CAtlDllModuleT` to `CAtlExeModuleT`). Slower than in-process due to RPC marshaling.

## Option 2: Windows DLL Surrogate (`dllhost.exe`)

We retain the current C++ DLL, but we delineate a new `AppID` (Application ID) within the Windows Registry, specific to our classes. We configure this `AppID` with an empty `DllSurrogate` string.

**How it operates:**
When C# or VBS summons the COM server, Windows observes the `DllSurrogate` tag. Rather than loading the DLL directly into the calling process (e.g., `cscript.exe`), Windows initiates `dllhost.exe` (COM Surrogate) in the background. The DLL is exclusively loaded into that surrogate process. Multiple clients are then redirected by Windows to that identical `dllhost.exe`.

* **Pros:** Requires **NO** modifications to C++ code. Only the project's `.rgs` (registry scripts) must be updated, followed by a rebuild & registration cycle.
* **Cons:** `dllhost.exe` was intended by Microsoft as a "band-aid" wrapper; the debugging experience may be more challenging if crashes occur (since `dllhost.exe` will crash instead of your custom process).

## Option 3: Windows Shared Memory (File Mapping)

We keep the DLL (for blisteringly fast in-process RPC and zero COM overhead), but we rewrite the C++ `StorageEngine` within the core. Everything appended via `AddRow` is no longer placed inside a local C++ `std::map`, but written out to a shared RAM memory block reserved via `CreateFileMapping` — allowing simultaneous access across multiple processes.

* **Pros:** Unrivaled performance. Zero sluggish Windows RPC marshaling calls or IPC overhead.
* **Cons:** Exceptionally complex. C++ STL containers (like `std::string` or `std::map`) utilize default allocators unsuitable for cross-process shared memory. We would have to author custom memory allocators.

---

## Proposed Action Plan (Recommendation: Commence with Option 2)

Should you approve of **Option 2 (DllSurrogate)** to deliver swift results, I shall execute the following:

### 1. Registry Scripts Updates
I will modify `DatasetProxy.rgs` and `SharedValue.rgs`.
#### [MODIFY] `DatasetProxy.rgs`
- Insert a `val AppID = s '{new-shared-appid-guid}'` under the registry tree `NoRemove CLSID`.
- Insert a new root `NoRemove AppID { {new-shared-appid-guid} { val DllSurrogate = s '' } }` under `HKCR`.

### 2. Registry Cleanup
Initially, we run the previously crafted diagnostic script (or the unregister command) to safely wipe the *InprocServer32* configurations. 

### 3. Build & Test Run
We rebuild, register, and execute the original **Consumer/Producer tests** across distinct terminal windows. The consumer's *Proxy* call should then successfully retrieve data!

## Open Questions
- Which of the three options do you prefer for this project?
- Does performance (lower latency via `in-process` and `shared memory`) outweigh architectural cleanliness (COM EXE / RPC)?


## Related Documentation

- [README_EN.md](README_EN.md) — Starting page for general theory and project research.
- [README_EN.md](../README_EN.md) — Main documentation and starting point of the entire project.
- [ARCHITECTURE_EN.md](../ARCHITECTURE_EN.md) — Main architecture document for the entire COM Server project.
- [README_EN.md](../ATLProjectcomserverExe/README_EN.md) — User guide and overview of the EXE COM Server variant.
- [README_EN.md](../SharedValueV2/README_EN.md) — Introduction and overview of the SharedValueV2 C++20 engine.
