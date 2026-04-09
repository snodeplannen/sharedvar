# SharedValueV3 MemMap — Tests

## Overzicht

Deze directory bevat de geautomatiseerde end-to-end test suite voor de SharedValueV3 Memory-Mapped FlatBuffers engine. De tests valideren de volledige cross-process pipeline van C++ producer tot C# consumer.

## Test Suite Draaien

```powershell
# Volledig (inclusief build)
.\SharedValueV3_MemMap\tests\Run-MemMapTests.ps1

# Zonder rebuild (sneller bij herhaald testen)
.\SharedValueV3_MemMap\tests\Run-MemMapTests.ps1 -SkipBuild
```

## Testscenario's

| # | Test | Valideert |
|---|---|---|
| 0 | **Build Verification** | CMake C++ en dotnet C# compileren zonder fouten of warnings |
| 1 | **Basic Data Transfer** | Producer stuurt 3 updates → consumer ontvangt alle 3, FlatBuffer velden correct geparsed |
| 2 | **Multi-Row Dataset** | 5 rijen per update met elk 2 NestedDetails → alle rijen en nesting correct ontvangen |
| 3 | **Consumer-Before-Producer** | Consumer start vóór de producer → retry-mechanisme werkt, data komt alsnog aan |
| 4 | **Connection Timeout** | Consumer zonder producer → timeout na N seconden, exit code 2, foutmelding |
| 5 | **Rapid-Fire Updates** | 10 updates op 200ms interval → consumer ontvangt minimaal 5 events (auto-reset coalescing) |
| 6 | **Boolean Toggle** | `is_active` veld wisselt elke 3e update → consumer observeert zowel `True` als `False` |

## CLI-argumenten (Producer)

```
MemMapProducer.exe [options]
  --count N       Stuur N updates en stop (default: oneindig)
  --interval MS   Wachttijd in milliseconden tussen updates (default: 2000)
  --rows N        Aantal DataRows per update (default: 1)
  --name NAME     Naam van het gedeelde geheugen (default: MyGlobalDataset)
  --help          Toon help
```

## CLI-argumenten (Consumer)

```
dotnet run -- [options]
  --name NAME       Naam van het gedeelde geheugen (default: MyGlobalDataset)
  --count N         Stop na N ontvangen events (default: oneindig / ENTER)
  --timeout SEC     Verbindingstimeout in seconden (default: 30)
  --help            Toon help
```

## Exit Codes (Consumer)

| Code | Betekenis |
|---|---|
| 0 | Succesvol afgesloten |
| 2 | Connection timeout — producer niet gevonden |
| 3 | Event timeout — verwachte events niet ontvangen |

## Opmerking over Rapid-Fire Tests

Bij test 5 (rapid-fire) kan de consumer minder events ontvangen dan de producer verstuurt. Dit is **verwacht gedrag**: het Named Event is geconfigureerd als **auto-reset**. Als de producer twee keer `SetEvent()` aanroept voordat de consumer thread wakker wordt uit `WaitOne()`, worden die twee signalen samengevoegd tot één wake-up.

Dit is by-design: de consumer leest altijd de **meest recente** data uit het geheugenblok, ongeacht hoeveel tussenliggende updates gemist zijn.
