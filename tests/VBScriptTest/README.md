# VBScriptTest — VBScript Integratietests (Legacy DLL)

> **COM Server Variant:** **Legacy In-Process DLL** (`ATLProjectcomserver.dll`)
> ProgID prefix: `ATLProjectcomserver.*`

Deze scripts testen de **Legacy InprocServer32 DLL** variant via de klassieke Windows Scripting Host (`cscript.exe`). De DLL wordt **direct in het `cscript.exe` proces geladen** — er is geen apart server-proces. Dit maakt ze geschikt als snelle smoke-tests voor de in-process architectuur.

> [!IMPORTANT]
> Voor de **cross-process EXE server** tests, zie [`ATLProjectcomserverExe/tests/`](../../ATLProjectcomserverExe/tests/).

---

## Vereisten

1. De Legacy DLL moet **geregistreerd** zijn als Administrator:
   ```cmd
   regsvr32 x64\Debug\ATLProjectcomserver.dll
   ```
2. `cscript.exe` is standaard aanwezig op alle Windows installaties.

---

## Testscripts Overzicht

### Standalone Tests (draaien zelfstandig)

---

#### `TestFullCycle.vbs` ⭐ Aanbevolen startpunt

**Scenario:** Volledige CRUD-cyclus binnen één enkel script — geen tweede terminal nodig.

**Wat wordt getest:**
1. Aanmaken van een `DatasetProxy` en `SharedValue` instantie.
2. Schrijven van 3 configuratie-rijen (`cfg_timeout`, `cfg_retries`, `cfg_maxconn`).
3. Parkeren van de proxy in `SharedValue` via `SetValue`.
4. Muteren: `UpdateRow` (timeout 30→60) en `RemoveRow` (cfg_retries).
5. Validatie: `GetRecordCount()` moet 2 zijn, `GetRowData("cfg_timeout")` moet `"60"` zijn.

**Verwachte output:**
```
Records: 3
Na mutatie: 2 records, Timeout=60
PASS: Volledige CRUD-cyclus succesvol!
```

**Draaien:**
```cmd
cscript //Nologo tests\VBScriptTest\TestFullCycle.vbs
```

---

#### `TestErrorHandling.vbs`

**Scenario:** Verifieert dat de C++ `SharedValueV2` exceptions correct doorborrelen als VBScript `Err.Number`/`Err.Description`.

**Wat wordt getest:**
| Test | Actie | Verwacht |
|---|---|---|
| Test 1 | `GetRowData("onbestaandeKey")` | `Err.Number <> 0`, beschrijving bevat `not found` |
| Test 2 | `SetStorageMode(1)` terwijl de store niet leeg is | `Err.Number <> 0`, beschrijving bevat `not empty` |
| Test 3 | `AddRow("key1", ...)` terwijl `key1` al bestaat | `Err.Number <> 0`, beschrijving bevat `already exists` |

**Verwachte output:**
```
TEST 1 PASS: Key 'onbestaandeKey' not found in dataset
TEST 2 PASS: Cannot change storage mode: dataset is not empty
TEST 3 PASS: Key 'key1' already exists
Error handling tests voltooid.
```

**Draaien:**
```cmd
cscript //Nologo tests\VBScriptTest\TestErrorHandling.vbs
```

---

#### `TestEventCallbacks.vbs`

**Scenario:** Verifieert het `IEventCallback` observer-mechanisme. Een VBScript class registreert zich als event-listener en telt of de juiste events binnenkomen.

**Wat wordt getest:**
| Test | Actie | Verwacht |
|---|---|---|
| Test 1 | `AddRow`, `UpdateRow`, `RemoveRow` na `RegisterEventCallback` | 3 events ontvangen (`RowAdded`, `RowUpdated`, `RowRemoved`) |
| Test 2 | `AddRow` na `UnregisterEventCallback` | Geen nieuw event — teller blijft op 3 |

**Verwachte output:**
```
[Event #0] Type=10 Key=k1 New=v1
[Event #1] Type=11 Key=k1 New=v2
[Event #2] Type=12 Key=k1 New=
TEST 1 PASS: 3 events ontvangen
TEST 2 PASS: Unregister werkt correct
Event callback tests voltooid.
```

**Draaien:**
```cmd
cscript //Nologo tests\VBScriptTest\TestEventCallbacks.vbs
```

---

### Producer/Consumer Tests (twee terminals vereist)

Deze tests simuleren het **in-process memory sharing** scenario: producer en consumer draaien als **aparte `cscript.exe` processen**, maar delen dezelfde DLL-instantie in het geheugen.

> [!NOTE]
> Omdat dit een **InprocServer32 DLL** is, laadt elk `cscript.exe` proces een eigen kopie van de DLL. De state wordt **niet echt gedeeld** tussen processen — daarvoor is de EXE server nodig. Deze tests demonstreren de architectuur en de correctheid van de interface, niet echte cross-process sharing.

---

#### `TestProducerString.vbs` + `TestConsumerString.vbs`

**Scenario:** Simpelste producer/consumer: schrijf een string en lees hem terug.

**TestProducerString.vbs — wat doet het:**
- Maakt een `SharedValue` instantie aan.
- Schrijft de string `"HELLO FROM PRODUCER"` via `SetValue`.
- Wacht 10 seconden zodat een consumer in een tweede terminal kan worden gestart.

**TestConsumerString.vbs — wat doet het:**
- Maakt een `SharedValue` instantie aan.
- Leest de string terug met `GetValue`.
- Toont de waarde.

**Draaien (twee terminals):**
```cmd
:: Terminal 1
cscript //Nologo tests\VBScriptTest\TestProducerString.vbs

:: Terminal 2 (binnen 10 seconden starten)
cscript //Nologo tests\VBScriptTest\TestConsumerString.vbs
```

---

#### `TestProducerVBS.vbs` + `TestConsumerVBS.vbs`

**Scenario:** Uitgebreid producer/consumer scenario met een `DatasetProxy` dataset.

**TestProducerVBS.vbs — wat doet het:**
1. Maakt een `DatasetProxy` aan en voegt 3 server-status rijen toe:
   - `server01` → `status:online|cpu:45|mem:8192`
   - `server02` → `status:offline|cpu:0|mem:0`
   - `server03` → `status:online|cpu:92|mem:16384`
2. Parkeert de proxy in `SharedValue` via `SetValue`.
3. Rapporteert `PASS` bij succes.

**TestConsumerVBS.vbs — wat doet het:**
1. Haalt de `DatasetProxy` op via `SharedValue.GetValue()`.
2. Leest het record-aantal op via `GetRecordCount()`.
3. Pagineert door alle keys met `FetchPageKeys(0, 100)`.
4. Leest per key de data op met `GetRowData(key)`.
5. Rapporteert `PASS` met het aantal gelezen rijen.

**Draaien:**
```cmd
:: Terminal 1
cscript //Nologo tests\VBScriptTest\TestProducerVBS.vbs

:: Terminal 2
cscript //Nologo tests\VBScriptTest\TestConsumerVBS.vbs
```

---

#### `TestProducerSleep.vbs`

**Scenario:** Alternatieve producer die 10 seconden wacht om interactief testen of debuggen mogelijk te maken.

**Wat doet het:**
- Voegt 2 rijen toe (`server01`, `server02`) en parkeert ze in `SharedValue`.
- Slaapt 10 seconden via `WScript.Sleep 10000`.
- Ideaal in combinatie met `TestConsumerVBS.vbs` als je de timing handmatig wilt controleren.

**Draaien:**
```cmd
:: Terminal 1
cscript //Nologo tests\VBScriptTest\TestProducerSleep.vbs

:: Terminal 2 (start terwijl producer wacht)
cscript //Nologo tests\VBScriptTest\TestConsumerVBS.vbs
```

---

## Test Matrix

| Script | Standalone | Twee terminale | Server variant | Wat getest |
|---|:---:|:---:|---|---|
| `TestFullCycle.vbs` | ✅ | | Legacy DLL | CRUD cyclus end-to-end |
| `TestErrorHandling.vbs` | ✅ | | Legacy DLL | Exception → `Err` propagatie |
| `TestEventCallbacks.vbs` | ✅ | | Legacy DLL | Observer register/unregister |
| `TestProducerString.vbs` | | ✅ | Legacy DLL | Eenvoudige string producer |
| `TestConsumerString.vbs` | | ✅ | Legacy DLL | Eenvoudige string consumer |
| `TestProducerVBS.vbs` | | ✅ | Legacy DLL | DatasetProxy producer (3 rijen) |
| `TestProducerSleep.vbs` | | ✅ | Legacy DLL | Langzame producer (10s wacht) |
| `TestConsumerVBS.vbs` | | ✅ | Legacy DLL | DatasetProxy consumer + paginering |
