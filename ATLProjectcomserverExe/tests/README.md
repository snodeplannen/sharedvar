# ATLProjectcomserverExe — Tests

> **COM Server Variant:** **Out-of-Process EXE Server** (`ATLProjectcomserver.exe`)
> ProgID prefix: `ATLProjectcomserverExe.*`

Integration tests scrutinizing the **production EXE server**. Unlike the Legacy DLL tests, the server here stages an **independent Windows process** (`ATLProjectcomserver.exe`) propelled automatically via the Windows COM subsystem as soon as a client triggers `CreateObject("ATLProjectcomserverExe.SharedValue")`.

> [!IMPORTANT]
> The EXE server acts as the **recommended architecture achieving cross-process data sharing**. All respective clients — VBScript, C#, Python — universally communicate targeting identical server instances bridging through RPC marshaling mapping an LRPC Named Pipe.

---

## Requirements

1. The EXE server mandates being **registered** executing as Administrator:
   ```cmd
   x64\Debug\ATLProjectcomserver.exe /RegServer
   ```
2. **Windows 10/11 x64** (LRPC Named Pipe transport backbone).
3. .NET 8 SDK (exclusively powering `CSharpNet8Test`).
4. `cscript.exe` present upon the host (intrinsic regarding Windows).

---

## Directory layout

```
tests/
├── Run-CrossProcessTests.ps1    # PowerShell integration test barrage (5 variations)
└── CSharpNet8Test/              # C# .NET 8 integration sequences
    ├── Program.cs
    └── CSharpNet8Test.csproj
```

---

## `Run-CrossProcessTests.ps1` — PowerShell Cross-Process Suite

**When to employ:** Comprehensive end-to-end probing testing the EXE server. Each separate scenario boots an **isolated `cscript.exe` process** hooking up acting as client — guaranteeing tangible cross-process communication.

**Executing:**
```powershell
.\tests\Run-CrossProcessTests.ps1
```

### Scenario 1 — Automatic Orchestration

**What gets verified:** Windows COM SCM springs the EXE server autonomously when discovering zero active instances running.

**Mechanics:** The inaugural `CreateObject("ATLProjectcomserverExe.SharedValue")` triggers the SCM, spinning up `ATLProjectcomserver.exe` looming in the background. The client retrieves a proxy-pointer transparently.

---

### Scenario 2 — String Producer

**What gets verified:** An abruptly terminated producer-process safely strands data secured natively inside the server.

**Mechanics:**
1. A scripted inline VBScript pushes `"Message from a terminated Producer-process!"` firing `SetValue`.
2. The `cscript.exe` execution resolves crashing/closing thoroughly.
3. The EXE server dutifully shelters the data captured tightly mapped to its singleton `CSharedValue` layer.

**Proof of cross-process logic:** The producer evaporated entirely — nevertheless data undeniably survives encapsulated internally mapping the server.

---

### Scenario 3 — String Consumer

**What gets verified:** A freshly spun consumer-process pulls data successfully previously logged via a ceased producer.

**Mechanics:**
1. A fresh `cscript.exe` iteration connects firing `CreateObject`.
2. Calls upon `GetValue()` — the server dishes out the formerly deposited string.
3. Script concludes.

**Testing threshold:** The consumer confronts the precise verbatim string mirroring everything the producer stashed away during scenario 2.

---

### Scenario 4 — DatasetProxy Producer (complex objects)

**What gets verified:** Shoveling a hefty `DatasetProxy` encasing multiple sequences over to the server driven from an isolated client endpoint.

**Mechanics:**
1. Producer drops a `DatasetProxy` (calling `sv.GetValue()` hauling the server-side proxy).
2. Tacks on 2 rows: `SERVER-01` and `SERVER-02` bundling status footprints.
3. Producer unloads closing smoothly.

> [!NOTE]
> The `DatasetProxy` inherently resides **server-side instanced** (nestled in `CSharedValue::FinalConstruct`). Clients intercept just an RPC-reference — the native data perpetually occupies the server context, zeroing outside traversing into client territories.

---

### Scenario 5 — DatasetProxy Consumer (COM marshaling proxying)

**What gets verified:** Pulling dataset records executing COM marshaling guided remotely outward a severed consumer sequence.

**Mechanics:**
1. Consumer hauls the `DatasetProxy` hooking `sv.GetValue()`.
2. Gathers `GetRecordCount()`.
3. Cycles keys pulling `FetchPageKeys(0, 100)` — churning a bulk `SAFEARRAY` translating effortlessly onto VBScript resembling ordinary Arrays.
4. Peels data open per key cycling `GetRowData(key)`.

**Why this poses serious hurdles technically:** The `SAFEARRAY` demands immaculate marshaling crossing the turbulent LRPC barriers. Hereby severely scrutinizing the proxy/stub engine assembled via MIDL.

---

### Scenario 6 — Graceful Shutdown Checks

**What gets verified:** Ensuring the overarching server collapses gracefully strictly abiding the `ShutdownServer()` mandate.

**Mechanics:**
1. Scrutinizes verifying `ATLProjectcomserver.exe` actually hums actively (`Get-Process`).
2. A generic VBScript jumps executing `sv.ShutdownServer()`.
3. The server cycles its internal routine: scrubbing DatasetProxy references → `_AtlModule.Unlock()` → arresting message looping patterns → process exiting safely.
4. Final script sweep ensuring that precise server-sequence ceased existing entirely.

**Passing:**
```
SUCCESS: EXE server seamlessly concluded.
```

**Failing:**
```
FAIL: EXE server persists actively spanning process grids.
```

---

## `CSharpNet8Test/` — C# .NET 8 Integration Sequences

Explore [`CSharpNet8Test/README_EN.md`](CSharpNet8Test/README_EN.md) targeting an encyclopedic description natively.

**Summary digest:** Broadly traversing identical 5 test branches mimicking the Legacy DLL footprint, however purely angled navigating the EXE server (`ATLProjectcomserverExe.DatasetProxy`). Invoking logic inherently rides RPC-marshaling avoiding in-process direct hops.

**Executing:**
```powershell
cd tests\CSharpNet8Test
dotnet run
```


## Related Documentation

- [README_EN.md](../../README_EN.md) — Main documentation and starting point of the entire project.
- [ARCHITECTURE_EN.md](../../ARCHITECTURE_EN.md) — Main architecture document covering the entire COM Server project tree.
- [README_EN.md](../README_EN.md) — User guide and overview of the EXE COM Server variant.
- [README_EN.md](../../SharedValueV2/README_EN.md) — Introductory overview guiding the SharedValueV2 C++20 engine.
