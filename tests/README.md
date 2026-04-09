# Tests — Cross-Language Integratietests

Dit project bevat integratietests voor **twee aparte COM server varianten**. Het is belangrijk te weten welke variant je test, want ze gebruiken verschillende ProgIDs en een fundamenteel ander communicatiemodel.

## De Twee Testvarianten

| | **Legacy DLL Tests** (deze directory) | **EXE Server Tests** |
|---|---|---|
| **Server** | InprocServer32 DLL | LocalServer32 EXE |
| **ProgID prefix** | `ATLProjectcomserver.*` | `ATLProjectcomserverExe.*` |
| **Server locatie** | Geladen in client-proces | Apart Windows-proces |
| **Communicatie** | Directe functie-aanroepen | RPC via LRPC Named Pipe |
| **Registreren** | `regsvr32 ATLProjectcomserver.dll` | `ATLProjectcomserver.exe /RegServer` |
| **Data gedeeld?** | ❌ Per proces een kopie | ✅ Gedeeld tussen alle clients |
| **Testlocatie** | `tests/` *(deze map)* | `ATLProjectcomserverExe/tests/` |

---

## Tests in deze directory (Legacy DLL)

> **COM Server Variant:** Legacy In-Process DLL (`ATLProjectcomserver.dll`)

De tests hier zijn primair gericht op de **Legacy InprocServer32 DLL** variant. Ze valideren de correctheid van de COM interfaces, foutafhandeling en event-mechanismen — maar ze testen **geen echte cross-process data sharing** (daarvoor is de EXE server nodig).

### Vereiste registratie

```cmd
:: Als Administrator
regsvr32 x64\Debug\ATLProjectcomserver.dll
```

### Directorystructuur

```
tests/
├── CSharpNet8Test/             # C# .NET 8 integratietests (14 tests)
│   ├── Program.cs
│   └── CSharpNet8Test.csproj
└── VBScriptTest/               # VBScript integratietests (8 scripts)
    ├── TestFullCycle.vbs       # ⭐ Aanbevolen startpunt
    ├── TestErrorHandling.vbs
    ├── TestEventCallbacks.vbs
    ├── TestProducerString.vbs
    ├── TestConsumerString.vbs
    ├── TestProducerVBS.vbs
    ├── TestProducerSleep.vbs
    └── TestConsumerVBS.vbs
```

---

## CSharpNet8Test — C# .NET 8 (Legacy DLL)

Zie [`CSharpNet8Test/README.md`](CSharpNet8Test/) voor de volledige beschrijving van alle 5 testcategorieën.

**Snel draaien:**
```powershell
cd tests\CSharpNet8Test
dotnet run
```

**Wat getest wordt:**
1. Producer Flow — 100 rijen wegschrijven en parkeren in `SharedValue`
2. Consumer Flow — rijen teruglezen, data-integriteit en paginering valideren
3. Bidirectionele Mutatie — `UpdateRow` en `RemoveRow`
4. Error Handling — `COMException` bij `KeyNotFound`, `DuplicateKey`, `StoreModeNotEmpty`
5. Storage Mode — wisselen tussen `std::map` en `std::unordered_map`

---

## VBScriptTest — VBScript (Legacy DLL)

Zie [`VBScriptTest/README.md`](VBScriptTest/) voor volledige per-script documentatie.

### Standalone tests (één terminal)

```powershell
# Aanbevolen startpunt — volledige CRUD cyclus
cscript //Nologo tests\VBScriptTest\TestFullCycle.vbs

# Error handling
cscript //Nologo tests\VBScriptTest\TestErrorHandling.vbs

# Event callbacks (register/unregister observer)
cscript //Nologo tests\VBScriptTest\TestEventCallbacks.vbs
```

### Producer/Consumer tests (twee terminals)

```powershell
# Terminal 1 — eenvoudige string
cscript //Nologo tests\VBScriptTest\TestProducerString.vbs
# Terminal 2
cscript //Nologo tests\VBScriptTest\TestConsumerString.vbs

# Terminal 1 — DatasetProxy met 3 server-status rijen
cscript //Nologo tests\VBScriptTest\TestProducerVBS.vbs
# Terminal 2
cscript //Nologo tests\VBScriptTest\TestConsumerVBS.vbs
```

---

## EXE Server Tests (cross-process)

Voor echte cross-process data sharing tests, zie [`ATLProjectcomserverExe/tests/`](../ATLProjectcomserverExe/tests/).

```powershell
# Geautomatiseerde suite: 6 cross-process scenario's inclusief graceful shutdown
.\ATLProjectcomserverExe\tests\Run-CrossProcessTests.ps1
```
