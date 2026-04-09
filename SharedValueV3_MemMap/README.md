# SharedValueV3 (Memory-Mapped Engine)

Dit is de V3 generatie van de SharedValue architectuur. In plaats van Out-of-Process COM (RPC over Named Pipes), draait dit model 100% op **High-Performance Memory-Mapped Files (Windows Shared Memory)**.

## Waarom deze architectuur?
De traditionele COM-methode kent een kleine delay per functieaanroep vanwege RPC marshaling (context switches). Als je 10.000 records direct stuk voor stuk in-memory wilt benaderen vanuit C#, is dat met COM niet mogelijk zonder gigantische snelheidsverliezen, *tenzij* je alles in één grote array ophaalt.

Deze **MemMap Engine** elimineert die overhead volledig:
1. **0% Overhead & Zero-Copy**: C# leest exact hetzelfde geheugenblok als waar C++ naar schrijft. Via **FlatBuffers** kunnen willekeurig geneste of dynamische datastructuren als één aaneengesloten block worden afgevuurd en *zonder parsing* in C# of C++ uitgelezen worden.
2. **0% CPU Callbacks**: Om toch push-notificaties (COM Events of Callbacks) te behouden, leunt deze engine op **Windows Named Events** (`CreateEventW`). De C# listenthread slaapt met 0% CPU footprint totdat de producer data inlaadt en de trigger overhaalt via inter-process communicatie.
3. **Multi-Process Veilig**: Data corruptie is onmogelijk gemaakt door een actieve **Windows Named Mutex**, afgedwongen in de centrale de `SharedValueEngine` wrapper.

## Project structuur
*   `schema/` bevat de `dataset.fbs`. Dit is de FlatBuffers datadefinitie. 
*   `cpp_core/` is de C++ Native Producer.
*   `csharp_core/` is de C# Managed Consumer.
*   `build_schema.ps1` downloadt automatisch de Google `flatc` compiler en regenereert de wrapper code o.b.v. je schema in beide talen.

## Hoe te draaien (Testen)
1. **Open een Terminal:** Navigeer naar `cpp_core\build\Release` (compileer eerst via CMake indien leeg) en start `MemMapProducer.exe`. Deze gooit elke 2 seconden een nieuwe lading FlatBuffer payload in het gedeelde geheugen.
2. **Open een 2e Terminal:** Navigeer naar `csharp_core` en run `dotnet run`. Je ziet C# nanoseconden later ontwaken, de array uitlezen via pointers, en de data verwerken! (Vrij van alle MSBuild of Nullable warnings).
