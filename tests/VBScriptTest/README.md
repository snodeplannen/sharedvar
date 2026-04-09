# VBScriptTest — VBScript Integratietests

VBScript test-scripts die de COM Server valideren vanuit het perspectief van een klassieke Windows scripting client.

## Rol binnen het project

VBScript is de meest directe manier om COM-objecten te testen zonder build-tooling. Deze scripts demonstreren en valideren de volledige producer/consumer lifecycle, foutafhandeling en event callbacks.

## Bestandsoverzicht

### Producer Scripts
| Bestand | Beschrijving |
|---|---|
| `TestProducerString.vbs` | Schrijft een eenvoudige string naar `SharedValue`. |
| `TestProducerSleep.vbs` | Schrijft data en wacht (`WScript.Sleep`) zodat een consumer in een apart terminal kan lezen. |
| `TestProducerVBS.vbs` | Vult de `DatasetProxy` met meerdere rijen data. |

### Consumer Scripts
| Bestand | Beschrijving |
|---|---|
| `TestConsumerString.vbs` | Leest een string terug uit `SharedValue`. |
| `TestConsumerVBS.vbs` | Leest DatasetProxy rijen uit en toont de inhoud. |

### Gecombineerde Tests
| Bestand | Beschrijving |
|---|---|
| `TestFullCycle.vbs` | Volledige producer → consumer cyclus binnen één script. |
| `TestErrorHandling.vbs` | Test COM error handling: `KeyNotFound`, `DuplicateKey`, etc. |
| `TestEventCallbacks.vbs` | Test observer event callbacks via `ISharedValueCallback`. |

## Draaien

```powershell
# Individueel script
cscript //Nologo tests\VBScriptTest\TestFullCycle.vbs

# Producer/Consumer in aparte terminals:
# Terminal 1:
cscript //Nologo tests\VBScriptTest\TestProducerSleep.vbs
# Terminal 2 (terwijl producer wacht):
cscript //Nologo tests\VBScriptTest\TestConsumerVBS.vbs
```

## Vereisten

- De COM Server moet geregistreerd zijn (DLL via `regsvr32` of EXE via `/RegServer`).
- `cscript.exe` (standaard aanwezig op Windows).
