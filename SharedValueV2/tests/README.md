# SharedValueV2 — Tests

Standalone C++ tests voor de SharedValueV2 core library. Deze tests draaien onafhankelijk van COM en valideren de pure C++ engine.

## Rol binnen het project

Deze tests vormen de eerste verdedigingslinie voor regressies in de core business-logica. Ze zijn snel uit te voeren en vereisen geen COM-registratie of Windows-specifieke setup.

## Bestandsoverzicht

| Bestand | Beschrijving |
|---|---|
| `UnitTests.cpp` | Functionele unit tests voor `SharedValue`, `DatasetStore`, `StorageEngine`, error handling, observer notificaties, en storage mode switching. |
| `StressTest.cpp` | Multithreaded concurrency stress test. Laat meerdere threads gelijktijdig lezen, schrijven en observeren om deadlocks en race conditions op te sporen. |

## Draaien

```powershell
cd SharedValueV2

# Bouw (als dat nog niet gedaan is)
cmake -B build
cmake --build build --config Release

# Unit tests
.\build\tests\Release\UnitTests.exe

# Stress test (concurrency)
.\build\tests\Release\StressTest.exe
```

## Verwachte Output

Beide executables printen `PASS`/`FAIL` per testcase en eindigen met een samenvatting. Een exitcode van `0` betekent dat alle tests geslaagd zijn.
