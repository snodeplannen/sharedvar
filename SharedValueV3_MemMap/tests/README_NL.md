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
  --linger MS     Milliseconden om actief te blijven na de laatste write (default: 5000)
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

## Waarom `--linger` nodig is bij tests

Windows kernel objects (MMF, Mutex, Event) worden vernietigd zodra alle handles gesloten zijn (refcount → 0). Bij geautomatiseerde tests stuurt de producer een beperkt aantal updates (`--count N`) en sluit dan af. Als de consumer op dat moment nog niet verbonden was (bijv. door `dotnet run` JIT-compilatie), zijn de kernel objects al vernietigd.

De `--linger MS` parameter lost dit op: de producer wacht na zijn laatste write nog N milliseconden voordat hij afsluit. Dit geeft de consumer tijd om te verbinden en alle events te ontvangen.

**Bij normaal (productie) gebruik is `--linger` niet nodig.** De producer draait dan oneindig (`--count` wordt niet opgegeven) en de kernel objects bestaan zolang het producer-proces draait. Consumers kunnen op elk moment verbinden en loskoppelen.

## Gerelateerde Documentatie

- [README.md](../README.md) — Introductie, projectstructuur en quickstart voor SharedValueV3 MemMap.
- [ARCHITECTURE.md](../ARCHITECTURE.md) — Uitgebreid architectuurdocument inclusief kernel object levenscyclus en reference counting.
- [README.md](../../README.md) — Hoofddocumentatie en startpunt van het gehele project.
- [CHANGELOG.md](../../CHANGELOG.md) — Overzicht van alle wijzigingen en release notes.

