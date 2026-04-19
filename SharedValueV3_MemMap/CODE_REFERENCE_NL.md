# SharedValueV3 MemMap — Code Reference

Dit document biedt een overzicht van de belangrijkste bestanden, componenten en klassen in deze implementatie.

## 1. Schema (`schema.fbs`)
Centraal in de V3 architectuur staat de **vooraf afgesproken structuur**. In tegenstelling tot V2, waar de structuur dynamisch op runtime werd ontdekt via type-flags, verwacht V3 een strak schema.

Belangrijkste tabellen en structen in FlatBuffers:
- **`NestedDetail`** (Table): Bevat een `id` (int) en een string `description`.
- **`DataRow`** (Table): Datastructuur per rij met o.a. `id`, `timestamp`, `score`, een boolean `is_active` én een array met `NestedDetail` objecten.
- **`SharedDataset`** (Table): De root-node die een vector of lijst van `DataRow` objecten omvat.

## 2. C++ Core (Producer)

De producer bevindt zich in `cpp_core/`.

### `SharedValueEngine.hpp`
De C++ implementatie van de inter-proces transportlaag.
- **`SharedValueEngine(name, maxSize)`**: Beheert en opent drie kern Windows Kernel Objects gebaseerd op de ingestelde `name` (zoals `Global\MyGlobalDataset_Map`):
  1. Een **Mutex** ter voorkoming van tear-reads tijdens overdracht.
  2. Een **Auto-Reset Event** voor asynchrone client wakeups na writes.
  3. De daadwerkelijke **Memory Mapped File** handle (+ MapViewOfFile) voor het aanroepen van data.
- **`WriteData(const uint8_t* data, size_t size)`**: Zorgt voor veilige exclusieve toegang dmv een mutex lock (`WaitForSingleObject`), checkt of de data in de mapping past, kopieert éérst een `size_t` metadata veld en daarna direct de raw serialisatie byte buffer heen via een memory-copy, ontgrendelt de mutex en seint naar eventuele consumers via het Event (`SetEvent()`).

### `main.cpp`
Toont hoe C++ code dynamisch via de `FlatBufferBuilder` boomstructuren (o.a. sub-vectors met `NestedDetail` en `SharedDataset`) optuigt, finaliseert met `.Finish()`, extraheert in byte blokken en verstuurt met behulp van de engine.

## 3. C# Core (Consumer)

De consumer implementatie bevindt zich in `csharp_core/`.

### `SharedValueEngine.cs`
De .NET wrappering via `System.IO.MemoryMappedFiles` en WaitHandles voor IPC overdracht.
- **OnDataReady Event (`Action<SharedDataset>`)**: Native C# event waaraan clientcode zich kan registreren.
- **`SharedValueEngine(name, maxSize)`**: Opent de corresponderende Mutex, EventWaitHandle en MemoryMappedFile. Verwacht dat deze reeds door de Producer zijn opgesteld (`OpenExisting` overloads, welke throwen als de producer niet leefbaar is of er onvoldoende rechten zijn voor de prefix).
- **`StartListening()`**: Gebruikt een low-impact background listner Thread die blockeert met event `.WaitOne()`. Vraagt extreem weinig CPU als er geen writes plaatsvinden.
- **`ReadCurrentData()`**: Regelt via timeout based `Mutex.WaitOne()` exclusieve toegang en decodeert de 8 bytes headers. Geeft deze bytes in the .NET ByteBuffer mee aan de flatbuffer gegenereerde accessors (`SharedDataset.GetRootAsSharedDataset`). 

### `Program.cs`
Console startpunt voor C#; hecht een parser event function aan `OnDataReady` en haalt moeiteloos data eruit als ware het standaard C# POCO objecten via zero-copy abstractie van de geneste detail arrays en properties.
