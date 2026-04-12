# SharedValueV3 MemMap — Installatie

Dit document beschrijft hoe je de SharedValueV3 MemMap implementatie bouwt.

## Vereisten
- Windows Besturingssysteem (vereist voor Memory-Mapped Files kernel objects)
- CMake (voor C++ project)
- MSVC compiler (Visual C++ werkbelasting in Visual Studio 2022)
- .NET 8.0 SDK of nieuwer (voor C# project)
- PowerShell (voor het uitvoeren van build scripts)

## 1. FlatBuffers Schema Bouwen
Dit project maakt gebruik van FlatBuffers voor high-performance serialisatie. Het schema bevindt zich in `schema/schema.fbs`.

Voer het meegeleverde script uit om de C++ headers en C# klassen te genereren vanuit dit schema:
```powershell
.\build_schema.ps1
```
*(Dit gebruikt de lokale `flatc.exe` in `flatc_tools/` en plaatst gegenereerde code in `cpp_core/dataset_generated.h` en `csharp_core/Generated/`)*

## 2. C++ Producer Bouwen (CMake)
De C++ applicatie dient als de source of truth die data-updates via shared memory deelt.

```powershell
cd cpp_core
mkdir build
cd build
cmake ..
cmake --build . --config Release
```
*(De geresulteerde executable `MemMapProducer.exe` is nu te vinden in `cpp_core/build/Release/`)*

## 3. C# Consumer Draaien (.NET)
De C# applicatie leest razendsnel de data uit het shared memory met behulp van de FlatBuffers bibliotheek, automatisch beheerd via NuGet in het projectbestand.

```powershell
cd csharp_core
dotnet build
dotnet run
```

## Uitvoeren van Geautomatiseerde Tests
Wanneer de omgeving is geconfigureerd en gebouwd, kun je de cross-process verbindingstests lokaal draaien:

```powershell
.\tests\Run-MemMapTests.ps1
```
Zie [tests/README.md](tests/README.md) voor meer informatie over testscenario's.
