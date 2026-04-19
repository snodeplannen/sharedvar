# CSharpNet8Test — C# .NET 8 DatasetProxy Integratietests (EXE Server)

> **COM Server Variant:** **Out-of-Process EXE Server** (`ATLProjectcomserver.exe`)
> ProgID prefix: `ATLProjectcomserverExe.*`

Een C# .NET 8 console-toepassing die de **EXE server** end-to-end test via **late binding** (`dynamic` / `Activator.CreateInstance`). Alle aanroepen gaan via **RPC-marshaling** over een LRPC Named Pipe naar het externe server-proces — dit is echte cross-process communicatie.

> [!NOTE]
> Dit is de **EXE server** variant. Voor de **Legacy DLL** variant (in-process), zie [`tests/CSharpNet8Test/`](../../../../tests/CSharpNet8Test/).

---

## Vereisten

1. **.NET 8 SDK** (`dotnet --version` moet `8.x` tonen).
2. De EXE server moet **geregistreerd** zijn als Administrator:
   ```cmd
   x64\Debug\ATLProjectcomserver.exe /RegServer
   ```

---

## Draaien

```powershell
cd ATLProjectcomserverExe\tests\CSharpNet8Test
dotnet run
```

De EXE server start automatisch on-demand als hij nog niet actief is.

Verwachte output bij volledig succes:
```
=== C# .NET 8 DatasetProxy Tests ===

--- Producer Flow ---
  PASS: Producer: 100 rijen toegevoegd
  PASS: Producer: Proxy geparkeerd in SharedValue

--- Consumer Flow ---
  PASS: Consumer: 100 rijen gelezen
  PASS: Consumer: Data-integriteit OK
  PASS: Consumer: Paginering (10 keys)

--- Bidirectionele Mutatie ---
  PASS: Mutatie: 2 records na update+remove
  PASS: Mutatie: UpdateRow werkt
  PASS: Mutatie: RemoveRow werkt

--- Error Handling ---
  PASS: Error: KeyNotFound met beschrijving
  PASS: Error: DuplicateKey met beschrijving
  PASS: Error: StoreModeNotEmpty met beschrijving

--- Storage Mode ---
  PASS: StorageMode: Unordered (1)
  PASS: StorageMode: CRUD werkt met Unordered

========================================
Results: 14 passed, 0 failed
```

---

## Testscenario's

### Test 1 — Producer Flow

**Wat wordt getest:** Schrijven van 100 rijen naar de server via RPC-marshaling.

**Scenario:**
1. Maakt `ATLProjectcomserverExe.DatasetProxy` aan → RPC-aanroep naar de server.
2. Voegt 100 rijen toe via `AddRow` — elke aanroep is een RPC round-trip.
3. Controleert `GetRecordCount()` = 100.
4. Parkeert de proxy in `ATLProjectcomserverExe.SharedValue` via `SetValue(proxy)`.

**Verschil met Legacy DLL:** Elke `AddRow` is een netwerk-achtige RPC call. De data leeft in een **apart Windows proces** (`ATLProjectcomserver.exe`), niet in het C# proces zelf.

---

### Test 2 — Consumer Flow

**Wat wordt getest:** Lezen van eerder opgeslagen data en validatie van `SAFEARRAY` marshaling over RPC.

**Scenario:**
1. Haalt de `DatasetProxy` reference op via `SharedValue.GetValue()`.
2. Valideert `GetRecordCount()` = 100.
3. Leest data-integriteit: `GetRowData("user:0")` moet `"name:User0"` bevatten.
4. Pagineert 10 keys via `FetchPageKeys(0, 10)` en controleert `keys.Length == 10`.

**Technisch uitdagend:** De `SAFEARRAY` (`VT_ARRAY | VT_VARIANT`) moet intact gemarshald worden van het server-proces naar het C# proces. Dit test de MIDL-gegenereerde proxy/stub code.

---

### Test 3 — Bidirectionele Mutatie

**Wat wordt getest:** `UpdateRow` en `RemoveRow` operaties over RPC.

**Scenario:**
1. Voeg 3 rijen toe: `mut_1`, `mut_2`, `mut_3`.
2. Update `mut_1` naar `"updated"`.
3. Verwijder `mut_2`.
4. Valideer: `GetRecordCount()` = 2, `GetRowData("mut_1")` = `"updated"`, `HasKey("mut_2")` = `false`.

---

### Test 4 — Error Handling

**Wat wordt getest:** Dat server-side C++ exceptions correct doorborrelen als `COMException` in C# via RPC.

**Scenario:**

| Subtest | Aanroep | Verwacht |
|---|---|---|
| KeyNotFound | `GetRowData("onbestaandeKey")` | `COMException` met `"not found"` in `.Message` |
| DuplicateKey | `AddRow("dup", ...)` twee keer | `COMException` met `"already exists"` in `.Message` |
| StoreModeNotEmpty | `SetStorageMode(1)` terwijl store niet leeg is | `COMException` met `"not empty"` in `.Message` |

**RPC error-keten:** `SharedValueException (C++)` → `HRESULT + IErrorInfo (COM server)` → `NDR marshaling` → `COMException (.NET client)`.

---

### Test 5 — Storage Mode

**Wat wordt getest:** Wisselen van `std::map` (ordered) naar `std::unordered_map` (unordered) op de server.

**Scenario:**
1. Nieuwe lege `DatasetProxy` aanmaken.
2. `SetStorageMode(1)` instellen → server schakelt intern de storage engine.
3. Controle via `GetStorageMode()` = 1.
4. CRUD-validatie: `AddRow` + `GetRowData` werken correct na de switch.

---

## Verschil met Legacy DLL Tests

| Aspect | Legacy DLL (in-process) | EXE Server (out-of-process) |
|---|---|---|
| **Server locatie** | Geladen in C# proces | Separaat Windows proces |
| **Communicatie** | Directe functie-aanroepen | RPC via LRPC Named Pipe |
| **ProgID prefix** | `ATLProjectcomserver.*` | `ATLProjectcomserverExe.*` |
| **Data persistence** | Verloren als C# afsluit | Bewaard in server-proces |
| **Prestatie** | Snel (in-process) | Langzamer (RPC round-trips) |
| **Cross-process sharing** | ❌ Niet mogelijk | ✅ Meerdere clients, één state |


## Gerelateerde Documentatie

- [README.md](../../../README.md) — Hoofddocumentatie en startpunt van het gehele project.
- [ARCHITECTURE.md](../../../ARCHITECTURE.md) — Hoofd architectuurdocument voor het gehele COM Server project.
- [README.md](../../README.md) — Gebruikershandleiding en overzicht van de EXE COM Server variant.
- [README.md](../../../SharedValueV2/README.md) — Introductie en overzicht van de SharedValueV2 C++20 engine.
