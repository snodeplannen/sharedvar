# CSharpNet8Test — C# .NET 8 DatasetProxy Integratietests (Legacy DLL)

> **COM Server Variant:** **Legacy In-Process DLL** (`ATLProjectcomserver.dll`)
> ProgID prefix: `ATLProjectcomserver.*`

Een C# .NET 8 console-toepassing die de **Legacy InprocServer32 DLL** end-to-end test via **late binding** (`dynamic` / `Activator.CreateInstance`). De DLL wordt rechtstreeks in het `dotnet.exe` proces geladen — er is geen apart server-proces actief.

> [!IMPORTANT]
> Dit is de test voor de **Legacy DLL** variant. Voor de **EXE server** variant (cross-process), zie [`ATLProjectcomserverExe/tests/CSharpNet8Test/`](../../ATLProjectcomserverExe/tests/CSharpNet8Test/).

---

## Vereisten

1. **.NET 8 SDK** geïnstalleerd (`dotnet --version` moet `8.x` tonen).
2. De Legacy DLL moet **geregistreerd** zijn als Administrator:
   ```cmd
   regsvr32 x64\Debug\ATLProjectcomserver.dll
   ```

---

## Draaien

```powershell
cd tests\CSharpNet8Test
dotnet run
```

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

Een exitcode van `0` = alle tests geslaagd. Exitcode `1` = één of meer mislukt.

---

## Testscenario's

### Test 1 — Producer Flow

**Wat wordt getest:** Schrijven van 100 rijen data via `DatasetProxy` en opslaan in `SharedValue`.

**Scenario:**
1. Maak een `ATLProjectcomserver.DatasetProxy` instantie aan via late binding.
2. Voeg 100 rijen toe via `AddRow($"user:{i}", $"name:User{i}|dept:IT|level:{i % 5}")`.
3. Controleer via `GetRecordCount()` dat er exact 100 rijen zijn.
4. Parkeer de proxy in `ATLProjectcomserver.SharedValue` via `SetValue(proxy)`.

**Waarom dit relevant is:** Valideert dat de ATL COM wrapper de `IDispatch` aanroepen vanuit C# correct doorschakelt naar `DatasetStore<std::wstring>`.

---

### Test 2 — Consumer Flow

**Wat wordt getest:** Lezen van eerder opgeslagen data en validatie van paginering.

**Scenario:**
1. Haal de `DatasetProxy` op uit `SharedValue` via `GetValue()`.
2. Controleer `GetRecordCount()` = 100.
3. Valideer data-integriteit: `GetRowData("user:0")` moet `"name:User0"` bevatten.
4. Pagineer 10 keys op met `FetchPageKeys(0, 10)` en controleer dat `keys.Length == 10`.

**Waarom dit relevant is:** Valideert dat de `SAFEARRAY`-marshaling van `FetchPageKeys` correct werkt in C# (VT_ARRAY | VT_VARIANT → `System.Array`).

---

### Test 3 — Bidirectionele Mutatie

**Wat wordt getest:** `UpdateRow` en `RemoveRow` operaties en hun effect op de record count.

**Scenario:**
1. Voeg 3 rijen toe: `mut_1`, `mut_2`, `mut_3` (allemaal waarde `"original"`).
2. Update `mut_1` naar `"updated"` via `UpdateRow`.
3. Verwijder `mut_2` via `RemoveRow`.
4. Valideer: `GetRecordCount()` = 2, `GetRowData("mut_1")` = `"updated"`, `HasKey("mut_2")` = `false`.

**Waarom dit relevant is:** Valideert de volledige CRUD-doorsnede inclusief `VARIANT_BOOL` retourwaarden van update/remove.

---

### Test 4 — Error Handling

**Wat wordt getest:** Dat C++ `SharedValueV2` exceptions correct doorvertaald worden als `COMException` in C#, inclusief de beschrijvingstekst.

**Scenario:**

| Subtest | Aanroep | Verwacht |
|---|---|---|
| KeyNotFound | `GetRowData("onbestaandeKey")` | `COMException` met `"not found"` in `.Message` |
| DuplicateKey | `AddRow("dup", ...)` twee keer | `COMException` met `"already exists"` in `.Message` |
| StoreModeNotEmpty | `SetStorageMode(1)` op niet-lege store | `COMException` met `"not empty"` in `.Message` |

**Waarom dit relevant is:** Valideert de `COM_METHOD_BODY` error-vertaalpijplijn: `SharedValueException` → `HRESULT + IErrorInfo` → `COMException` in .NET.

---

### Test 5 — Storage Mode

**Wat wordt getest:** Dynamisch wisselen van de interne opslagstrategie van `std::map` (ordered) naar `std::unordered_map` (unordered).

**Scenario:**
1. Maak een nieuwe, lege `DatasetProxy` instantie aan.
2. Stel `StorageMode = 1` (Unordered) in via `SetStorageMode(1)`.
3. Controleer via `GetStorageMode()` dat de mode `1` is.
4. Voeg een rij toe en lees hem terug — CRUD moet normaal werken.

**Waarom dit relevant is:** Valideert de `StorageEngine` abstractie in `SharedValueV2`: de interface blijft identiek ongeacht de onderliggende container.

---

## Technische Achtergrond: Hoe Late Binding Werkt

```
C# dynamic  →  DLR  →  RCW  →  IDispatch::GetIDsOfNames("AddRow")
                            →  IDispatch::Invoke(DISPID, VARIANT[])
                            →  CDatasetProxy::AddRow(BSTR, BSTR)
```

De `dynamic` keyword in C# laat het .NET DLR (Dynamic Language Runtime) alle member-lookups uitstellen naar runtime. Dit werkt zonder dat er een compile-time referentie naar de COM type library nodig is.
