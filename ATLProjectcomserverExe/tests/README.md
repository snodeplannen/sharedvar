# ATLProjectcomserverExe — Tests

> **COM Server Variant:** **Out-of-Process EXE Server** (`ATLProjectcomserver.exe`)
> ProgID prefix: `ATLProjectcomserverExe.*`

Integratietests voor de **productie EXE server**. In tegenstelling tot de Legacy DLL tests is de server hier een **zelfstandig Windows-proces** (`ATLProjectcomserver.exe`) dat automatisch door het Windows COM subsystem wordt gestart zodra een client `CreateObject("ATLProjectcomserverExe.SharedValue")` aanroept.

> [!IMPORTANT]
> De EXE server is de **aanbevolen architectuur voor cross-process data sharing**. Alle clients — VBScript, C#, Python — communiceren met dezelfde serverinstantie via RPC-marshaling over een LRPC Named Pipe.

---

## Vereisten

1. De EXE server moet **geregistreerd** zijn als Administrator:
   ```cmd
   x64\Debug\ATLProjectcomserver.exe /RegServer
   ```
2. **Windows 10/11 x64** (LRPC Named Pipe transport).
3. .NET 8 SDK (alleen voor `CSharpNet8Test`).
4. `cscript.exe` aanwezig op het systeem (standaard op Windows).

---

## Directorystructuur

```
tests/
├── Run-CrossProcessTests.ps1    # PowerShell integratietest suite (5 scenario's)
└── CSharpNet8Test/              # C# .NET 8 integratietests
    ├── Program.cs
    └── CSharpNet8Test.csproj
```

---

## `Run-CrossProcessTests.ps1` — PowerShell Cross-Process Suite

**Wanneer gebruiken:** Volledige end-to-end verificatie van de EXE server. Elke test-scenario spint een **apart `cscript.exe` proces** op dat verbindt als client — dit is échte cross-process communicatie.

**Draaien:**
```powershell
.\tests\Run-CrossProcessTests.ps1
```

### Scenario 1 — Automatisch Opstarten

**Wat wordt getest:** Windows COM SCM start de EXE server automatisch als geen instantie actief is.

**Hoe het werkt:** De eerste `CreateObject("ATLProjectcomserverExe.SharedValue")` aanroep activeert de SCM, die `ATLProjectcomserver.exe` opstart op de achtergrond. De client krijgt transparant een proxy-pointer terug.

---

### Scenario 2 — String Producer

**Wat wordt getest:** Een afgesloten producer-proces laat data achter in de server.

**Hoe het werkt:**
1. Een inline VBScript schrijft `"Boodschap vanuit afgesloten Producer-proces!"` via `SetValue`.
2. Het `cscript.exe` script eindigt en sluit volledig af.
3. De EXE server behoudt de data in zijn singleton `CSharedValue` instantie.

**Bewijs cross-process:** De producer is volledig gestopt — toch blijft de data in de server bewaard.

---

### Scenario 3 — String Consumer

**Wat wordt getest:** Een nieuw consumer-proces leest data die door een eerder gestopt proces was geschreven.

**Hoe het werkt:**
1. Een nieuw `cscript.exe` script verbindt via `CreateObject`.
2. Roept `GetValue()` aan — de server geeft de eerder opgeslagen string terug.
3. Script eindigt.

**Validatiecriterium:** De consumer ziet exact de string die de producer uit scenario 2 schreef.

---

### Scenario 4 — DatasetProxy Producer (complexe data)

**Wat wordt getest:** Schrijven van een `DatasetProxy` met meerdere rijen naar de server vanuit een geïsoleerd proces.

**Hoe het werkt:**
1. Producer maakt een `DatasetProxy` aan (`sv.GetValue()` retourneert de server-side proxy).
2. Voegt 2 rijen toe: `SERVER-01` en `SERVER-02` met statusdata.
3. Producer sluit af.

> [!NOTE]
> De `DatasetProxy` is **server-side geïnstantieerd** (in `CSharedValue::FinalConstruct`). Clients krijgen een RPC-reference — de data leeft in de server, niet in het client-proces.

---

### Scenario 5 — DatasetProxy Consumer (COM marshaling)

**Wat wordt getest:** Lezen van dataset rijen via COM marshaling vanuit een separaat consumer-proces.

**Hoe het werkt:**
1. Consumer haalt de `DatasetProxy` op via `sv.GetValue()`.
2. Leest `GetRecordCount()`.
3. Pagineert keys met `FetchPageKeys(0, 100)` — dit retourneert een `SAFEARRAY` die VBScript als Array behandelt.
4. Leest per key de data op met `GetRowData(key)`.

**Waarom dit technisch uitdagend is:** De `SAFEARRAY` moet correct gemarshald worden over de LRPC-grens. Dit test de proxy/stub code gegenereerd door MIDL.

---

### Scenario 6 — Graceful Shutdown

**Wat wordt getest:** De server sluit netjes af via het `ShutdownServer()` commando.

**Hoe het werkt:**
1. Controleert of `ATLProjectcomserver.exe` actief is (`Get-Process`).
2. Een VBScript client roept `sv.ShutdownServer()` aan.
3. De server voert intern uit: clear DatasetProxy reference → `_AtlModule.Unlock()` → message loop eindigt → proces exit.
4. Script valideert dat het server-process niet meer bestaat.

**Geslaagd:**
```
SUCCES: EXE server is naadloos gesloten.
```

**Gefaald:**
```
FAIL: EXE server draait nog.
```

---

## `CSharpNet8Test/` — C# .NET 8 Integratietests

Zie [`CSharpNet8Test/README.md`](CSharpNet8Test/) voor de volledige beschrijving.

**Samenvatting:** Dezelfde 5 testcategorieën als de Legacy DLL variant, maar dan gericht op de EXE server (`ATLProjectcomserverExe.DatasetProxy`). De aanroepen gaan via RPC-marshaling in plaats van directe in-process calls.

**Draaien:**
```powershell
cd tests\CSharpNet8Test
dotnet run
```


## Gerelateerde Documentatie

- [README.md](../../README.md) — Hoofddocumentatie en startpunt van het gehele project.
- [ARCHITECTURE.md](../../ARCHITECTURE.md) — Hoofd architectuurdocument voor het gehele COM Server project.
- [README.md](../README.md) — Gebruikershandleiding en overzicht van de EXE COM Server variant.
- [README.md](../../SharedValueV2/README.md) — Introductie en overzicht van de SharedValueV2 C++20 engine.
