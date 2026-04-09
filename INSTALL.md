# Installatie & Testing Handleiding

Omdat dit project bestaat uit enerzijds een C++ COM Library (Windows) en anderzijds een moderne C++ header-only module (`SharedValueV2`), zijn er twee respectievelijke manieren om het te compileren afhankelijk van hetgeen wat je beoogt.

## 1. De Main COM Server Bouwen (DLL)

Om de `ATLProjectcomserver.dll` succesvol the bouwen heb je de Enterprise build tools (of minstens standaard desktop dev tools) nodig.

1. Installeer **Visual Studio 2022** en selecteer in de installateur onder in ieder geval `Desktop development with C++` en `C++ ATL for latest v143 build tools`.
2. Open de root solution in Visual Studio: `ATLProjectcomserver.sln`.
3. Kies de gewenste target bovenaan in het menu (in de regel: `Debug` | `x64`).   
4. Druk op `F7` of klik op `Build` > `Build Solution`. Visual Studio compilt alle bestanden (inclusief het samenvoegen van de nieuwe SharedValueV2 headers).
5. Let op: Je applicatie zal resideren in `\x64\Debug\ATLProjectcomserver.dll`.

### COM Server Registreren
Voordat een Windows programma (zoals C# of Python) intern over deze COM objecten ("ProgIDs" zoals `ATLProjectcomserver.SharedValue`) of de interfaces kan praten, moet de DLL op het OS geregistreerd worden.

Open je Terminal als **Administrator** en voer dit uit:
```cmd
regsvr32 C:\jouwpad\csharp\cursor_com_test\x64\Debug\ATLProjectcomserver.dll
```
*Gebruik voor het weghalen (unregister) de command: `regsvr32 /u [pad_naar_dll]`*.

---

## 2. De C++ Tests Bouwen (CMake)
Als je niet te maken wilt hebben met COM en slechts de pure C++ architectuur van SharedValue wil engine-testen (eventueel in je CI/CD systeem):

1. Zorg dat CMake (`cmake`) bereikbaar is op je pad in je terminal.
2. Navigeer naar de sub-directory `SharedValueV2`:
```cmd
cd c:\csharp\cursor_com_test\SharedValueV2
```
3. Genereer de build environment en compileer in release-mode:
```cmd
cmake -B build
cmake --build build --config Release
```
4. Speel hierna succesvol de gegenereerde executable tests af. Dit is ook hoe je checkt of de machine last heeft van vastlopers:

```cmd
.\build\tests\Release\UnitTests.exe
.\build\tests\Release\StressTest.exe
```


## Gerelateerde Documentatie

- [CHANGELOG.md](CHANGELOG.md) — Overzicht van alle wijzigingen en release notes.
- [ARCHITECTURE.md](ARCHITECTURE.md) — Hoofd architectuurdocument voor het gehele COM Server project.
- [README.md](README.md) — Hoofddocumentatie en startpunt van het gehele project.
- [README.md](ATLProjectcomserverExe/README.md) — Gebruikershandleiding en overzicht van de EXE COM Server variant.
- [README.md](SharedValueV2/README.md) — Introductie en overzicht van de SharedValueV2 C++20 engine.
