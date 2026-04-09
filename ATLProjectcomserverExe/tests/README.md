# ATLProjectcomserverExe — Tests

Integratietests voor de Out-of-Process EXE COM Server. Deze tests valideren cross-process data-sharing, COM marshaling, en de graceful shutdown lifecycle.

## Rol binnen het project

Deze directory bevat de geautomatiseerde test suite die de kernfunctionaliteit van de EXE COM Server end-to-end valideert, inclusief scenario's met meerdere onafhankelijke client-processen.

## Bestandsoverzicht

| Bestand / Directory | Beschrijving |
|---|---|
| `Run-CrossProcessTests.ps1` | **Hoofd test-orchestrator.** PowerShell script dat 5 scenario's doorloopt: string producer/consumer, DatasetProxy CRUD via COM marshaling, en graceful shutdown. |
| [`CSharpNet8Test/`](CSharpNet8Test/) | C# .NET 8 integratietests voor DatasetProxy interop. |

## Tests Draaien

### Cross-Process Integration Suite

```powershell
# Vanuit de ATLProjectcomserverExe directory:
.\tests\Run-CrossProcessTests.ps1
```

Dit script:
1. Stopt eventuele bestaande server instanties.
2. Start een VBScript-producer die een string wegschrijft.
3. Leest de string terug vanuit een apart VBScript-consumer proces.
4. Vult de `DatasetProxy` met rijen vanuit een producer-proces.
5. Leest en valideert de rijen vanuit een apart consumer-proces.
6. Verstuurt een `ShutdownServer()` signaal en valideert dat de EXE netjes afsluit.

### C# .NET 8 Tests

```powershell
cd tests\CSharpNet8Test
dotnet run
```

## Vereisten

- De EXE COM Server moet geregistreerd zijn (`ATLProjectcomserver.exe /RegServer`).
- `cscript.exe` moet beschikbaar zijn op het systeem (standaard op Windows).
- .NET 8 SDK voor de C# tests.
