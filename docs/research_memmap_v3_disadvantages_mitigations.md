# Onderzoek: Mitigaties voor SharedValueV3 MemMap Nadelen

Dit document analyseert de vijf bekende nadelen van de SharedValueV3 Memory-Mapped architectuur en presenteert per nadeel meerdere concrete oplossingsrichtingen, gerankschikt op haalbaarheid en impact.

---

## Inhoudsopgave

1. [Geen method-calls / RPC](#1-geen-method-calls--rpc)
2. [Producer moet eerst draaien](#2-producer-moet-eerst-draaien)
3. [Unidirectioneel ontwerp](#3-unidirectioneel-ontwerp)
4. [Geen automatische proxy-generatie](#4-geen-automatische-proxy-generatie)
5. [Schema-wijzigingen vereisen hercompilatie](#5-schema-wijzigingen-vereisen-hercompilatie)
6. [Samenvattingsmatrix](#6-samenvattingsmatrix)

---

## 1. Geen method-calls / RPC

**Probleem:** De consumer kan alleen data lezen uit het shared memory blok. Er is geen mechanisme om vanuit C# een functie aan te roepen die de C++ producer uitvoert (bijv. "herbereken sensordata", "wijzig samplingrate").

### Oplossing A: Command Channel via Tweede MMF (⭐ Aanbevolen)

Introduceer een **tweede, klein Memory-Mapped blok** (bijv. 4 KB) dat dienst doet als command/request buffer. De consumer schrijft een commando, de producer luistert via een eigen Named Event.

```
┌─────────────────┐         ┌──────────────────┐         ┌──────────────────┐
│   C# Consumer   │         │  Shared Memory   │         │  C++ Producer    │
│                 │         │                  │         │                  │
│ WriteCommand()──┼────────►│  Command MMF     │────────►│  ReadCommand()   │
│                 │         │  (4 KB)          │         │  Execute()       │
│                 │         │                  │         │                  │
│ OnDataReady() ◄─┼─────────│  Data MMF        │◄────────┼──WriteData()     │
│                 │         │  (10 MB)         │         │                  │
└─────────────────┘         └──────────────────┘         └──────────────────┘
```

**Implementatie:**
- Nieuwe kernel objecten: `Global\..._CmdMap`, `Global\..._CmdMutex`, `Global\..._CmdEvent`
- Command buffer layout: `[uint32 cmd_id][uint32 payload_size][byte[] payload]`
- Producer draait een tweede listener thread die wacht op `CmdEvent`
- Response kan direct teruggeschreven worden in hetzelfde command blok, met een `ResponseEvent`

**Voordelen:**
- Volledig binnen de bestaande architectuur (geen nieuwe dependencies)
- Nanoseconde-latency ook voor commands
- Consumer en producer blijven losgekoppeld

**Nadelen:**
- Vereist een command-protocol definitie (welke cmd_id's bestaan er?)
- Zelf request/response correlatie bouwen bij asynchrone commands

---

### Oplossing B: Named Pipes als Command Sidecar

Gebruik de bestaande MMF voor bulk data (unidirectioneel, snel), maar voeg een **Windows Named Pipe** toe als bidirectioneel command-kanaal.

```
DATA:     C++ ──── MMF (10 MB) ──── → C#     (high-throughput, zero-copy)
COMMANDS: C# ──── Named Pipe ────── → C++    (bidirectioneel, request/response)
```

**Implementatie:**
- Producer creëert `\\.\pipe\SharedValue_CommandPipe`
- Consumer verbindt en stuurt commands als kleine berichten
- Named Pipes zijn inherent bidirectioneel (full-duplex) en ondersteunen overlapped I/O

**Voordelen:**
- Named Pipes hebben ingebouwde signalering en framing — geen handmatig protocol
- Full-duplex: response komt terug over dezelfde pipe
- .NET heeft native `NamedPipeClientStream` / `NamedPipeServerStream`

**Nadelen:**
- Hogere latency dan MMF (~μs i.p.v. ~ns) voor commands — maar commands zijn typisch infrequent
- Extra dependency op pipe lifecycle management

---

### Oplossing C: FlatBuffers RPC Framework (gRPC-Style)

FlatBuffers heeft native ondersteuning voor het definiëren van `rpc_service` interfaces in het `.fbs` schema:

```fbs
rpc_service SensorControl {
    SetSamplingRate(SamplingRateRequest): SamplingRateResponse;
    RecalibrateAll(Empty): CalibrationResult;
}
```

`flatc` kan hiervoor gRPC-stubs genereren. Dit combineert de snelle FlatBuffers serialisatie met een volledig RPC framework.

**Voordelen:**
- Formele interface definitie in het schema
- Automatische codegen voor client en server stubs
- Bewezen framework (gRPC) voor transport

**Nadelen:**
- Introduceert gRPC als zware dependency (HTTP/2, Protobuf runtime)
- De commands gaan dan via TCP i.p.v. direct shared memory — overkill voor lokale IPC
- Complexe setup voor een puur lokaal scenario

---

### Aanbeveling

**Oplossing A** (Command Channel via tweede MMF) voor maximale consistentie met de bestaande architectuur. Als de command-frequentie laag is en je liever een kant-en-klaar protocol hebt, is **Oplossing B** (Named Pipes sidecar) het pragmatischst.

---

## 2. Producer moet eerst draaien

**Probleem:** De consumer kan niet verbinden als het Memory-Mapped File nog niet bestaat. Momenteel wacht de consumer in een `Thread.Sleep(1000)` retry-loop, wat onelegant en traag is.

### Oplossing A: Named Event als "Ready" Signaal (⭐ Aanbevolen)

Laat de consumer een **Named Event** aanmaken (of ernaar luisteren) dat de producer aanvuurt zodra de engine geïnitialiseerd is.

**Flow:**
1. Consumer start → creëert `Global\..._Ready` event (of probeert te openen)
2. Consumer roept `WaitForSingleObject(_readyEvent)` aan — slaapt (0% CPU)
3. Producer start → initialiseert MMF, Mutex, Data Event
4. Producer opent en signaleert `Global\..._Ready` → consumer ontwaakt
5. Consumer opent MMF, Mutex, Data Event → alles operationeel

**Voordelen:**
- 0% CPU tijdens het wachten (geen polling)
- Instantane notificatie zodra producer klaar is
- Eenvoudige implementatie: één extra `CreateEventW` / `EventWaitHandle`

**Nadelen:**
- Minimaal extra kernel object beheer

---

### Oplossing B: `CreateFileMapping` als Create-or-Open

De Windows API `CreateFileMappingW` functioneert als een "create or open" operatie. Als de naam al bestaat, retourneert het een handle naar het bestaande object en zet `GetLastError()` op `ERROR_ALREADY_EXISTS`.

**Implementatie:**
- **Beide** processen gebruiken `CreateFileMappingW` i.p.v. respectievelijk `Create` en `Open`
- Het eerste proces dat start creëert het MMF
- Het tweede proces krijgt een handle naar het bestaande MMF

**Voordelen:**
- Eliminatie van de startup-volgorde problematiek — het maakt niet meer uit wie eerst start
- Geen retry-loop nodig

**Nadelen:**
- Beide processen moeten dezelfde grootte specificeren
- Na create moet er nog gewacht worden tot de producer daadwerkelijk data heeft geschreven (het blok is leeg)
- C# `MemoryMappedFile.CreateOrOpen()` bestaat als native API en vereenvoudigt dit

---

### Oplossing C: Windows Service als Persistente Host

Draai de producer als een **Windows Service** die automatisch start bij systeemboot via `sc.exe` of `services.msc`.

**Voordelen:**
- De shared memory is altijd beschikbaar — geen startup-orde probleem
- Service herstart automatisch bij crashes (recovery opties in Service Manager)

**Nadelen:**
- Verhoogde deployment-complexiteit
- Lokale ontwikkeling wordt zwaarder

---

### Aanbeveling

**Oplossing A** (Ready Event) is het schoonst: zero-CPU wait, directe notificatie, minimale code. **Oplossing B** (CreateOrOpen) is een goed alternatief als je de volgorde-afhankelijkheid volledig wilt elimineren.

---

## 3. Unidirectioneel ontwerp

**Probleem:** De huidige architectuur ondersteunt alleen producer → consumer dataflow. De consumer kan geen data terugsturen naar de producer.

### Oplossing A: Symmetrische Dual-Channel (⭐ Aanbevolen)

Maak twee identieke MMF-kanalen met elk hun eigen Mutex en Event:

```
Kanaal 1 (Producer → Consumer):
  MMF:   Global\Dataset_P2C_Map      (10 MB)
  Mutex: Global\Dataset_P2C_Mutex
  Event: Global\Dataset_P2C_Event

Kanaal 2 (Consumer → Producer):  
  MMF:   Global\Dataset_C2P_Map      (1 MB)
  Mutex: Global\Dataset_C2P_Mutex
  Event: Global\Dataset_C2P_Event
```

**Implementatie:**
- Beide processen instantiëren twee `SharedValueEngine` objecten
- Proces A is producer op kanaal 1, consumer op kanaal 2
- Proces B is consumer op kanaal 1, producer op kanaal 2
- De bestaande `SharedValueEngine` klasse is herbruikbaar zonder wijziging

**Voordelen:**
- Volledige hergebruik van bestaande code
- Onafhankelijke throughput per richting
- Geen deadlock-risico (gescheiden locks)

**Nadelen:**
- Dubbele kernel objects (6 i.p.v. 3)
- Iets hogere geheugengebruik

---

### Oplossing B: Gepartitioneerd Single MMF

Gebruik één groot MMF-blok maar verdeel het in twee logische zones:

```
┌────────────────────────────────────────────────────┐
│              Shared Memory (12 MB)                 │
├────────────────────────┬───────────────────────────┤
│  Zone A: P→C Data      │  Zone B: C→P Data         │
│  Offset 0 – 10 MB      │  Offset 10 MB – 12 MB     │
│  Mutex A, Event A       │  Mutex B, Event B          │
└────────────────────────┴───────────────────────────┘
```

**Voordelen:**
- Eén kernel object voor het MMF
- Lagere overhead dan twee aparte mappings

**Nadelen:**
- Complexere offset-berekeningen
- Vaste partitionering kan inflexibel zijn
- Twee Mutexes en Events zijn nog steeds nodig

---

### Oplossing C: Ring Buffer (Circular Buffer) in MMF

Implementeer een lock-free ring buffer in het shared memory blok. Beide processen kunnen gelijktijdig lezen en schrijven zonder mutex-locks.

**Voordelen:**
- Lock-free design → hogere throughput bij hoge frequenties
- Intrinsiek bidirectioneel

**Nadelen:**
- Significant complexer om correct te implementeren (memory ordering, cache line padding)
- Moeilijker te debuggen
- Niet triviaal met variabele-grootte FlatBuffer payloads

---

### Aanbeveling

**Oplossing A** (Dual-Channel) is het eenvoudigst en hergebruikt de bestaande engine 1-op-1. Oplossing B is ruimte-efficiënter maar fragiel. Oplossing C is alleen relevant bij extreem hoge throughput-eisen (>100K msgs/sec).

---

## 4. Geen automatische proxy-generatie

**Probleem:** Nieuwe client-talen (Python, Rust, Go) moeten handmatig de Windows kernel API's aanroepen (`CreateFileMapping`, `WaitForSingleObject`, etc.) en de FlatBuffer parselogica implementeren.

### Oplossing A: C-Core met FFI Bindings (⭐ Aanbevolen)

Extraheer de engine-logica naar een **pure C shared library** (DLL) met een stabiele `extern "C"` ABI, en genereer bindings per taal.

```
┌──────────────────────────────────────────────────────┐
│           SharedValueEngine.dll (C API)              │
│                                                       │
│  EXPORT sv_engine_create(name, size) → handle         │
│  EXPORT sv_engine_write(handle, data, len)            │
│  EXPORT sv_engine_read(handle, buf, maxlen) → len     │
│  EXPORT sv_engine_wait_event(handle, timeout_ms)      │
│  EXPORT sv_engine_destroy(handle)                     │
└──────────┬──────────┬───────────┬────────────────────┘
           │          │           │
     ┌─────▼──┐  ┌────▼───┐  ┌───▼────┐
     │ C#     │  │ Python │  │  Rust  │
     │P/Invoke│  │ ctypes │  │  FFI   │
     └────────┘  └────────┘  └────────┘
```

**Bindingen per taal:**
- **C#**: `[DllImport("SharedValueEngine.dll")]` via P/Invoke
- **Python**: `ctypes.CDLL("SharedValueEngine.dll")` of `cffi`
- **Rust**: `extern "C"` FFI block
- **Go**: `cgo` met `#include` header

**Voordelen:**
- Eenmaal geschreven → elke taal met FFI support kan verbinden
- De C API is de kleinst mogelijke interface (5-10 functies)
- FlatBuffers heeft al native codegen voor 15+ talen

**Nadelen:**
- Vereist het bouwen van een aparte DLL en een stabiele ABI-definitie
- Lifecycle management (handles, cleanup) moet per taal correct geïmplementeerd worden

---

### Oplossing B: FlatBuffers Codegen + Platform Wrappers

Aangezien FlatBuffers al codegen levert voor C++, C#, Python, Java, Go, Rust, TypeScript en meer, hoeft alleen de **MMF/Mutex/Event wrapper** per taal geschreven te worden. Dit is een relatief kleine laag.

**Aanpak:**
1. `flatc` genereert automatisch de data-klassen voor elke doeltaal
2. Per taal schrijf je een dunne ~100 LOC wrapper die de 3 kernel objecten opent
3. Publiceer deze wrappers als packages (NuGet, PyPI, crates.io)

**Voordelen:**
- De serialisatielaag (het complexe deel) is al geregeld door FlatBuffers
- De platformlaag is per taal een klein, eenmalig project

**Nadelen:**
- Je moet toch per taal een wrapper schrijven (Python `mmap`, Rust `winapi`, etc.)
- Onderhoud over meerdere talen

---

### Oplossing C: SWIG of gRPC Gateway

Gebruik **SWIG** om automatisch C++ → Python / C# bindings te genereren, of zet een **gRPC gateway** neer die de MMF data proxiet als een standaard RPC service.

**Voordelen:**
- SWIG genereert bindings automatisch vanuit C++ headers
- gRPC gateway maakt de data beschikbaar via standaard HTTP/2 voor elke taal

**Nadelen:**
- SWIG-output is vaak moeilijk te debuggen en vereist specifieke configuratie
- gRPC gateway introduceert serialisatie-overhead en vernietigt het zero-copy voordeel

---

### Aanbeveling

**Oplossing A** (C-Core DLL) voor strikte performance-eisen. **Oplossing B** (FlatBuffers codegen + dunne platform wrapper) als je snel meerdere talen wilt ondersteunen zonder een nieuwe DLL te bouwen.

---

## 5. Schema-wijzigingen vereisen hercompilatie

**Probleem:** Elke wijziging aan `dataset.fbs` vereist het opnieuw draaien van `flatc`, gevolgd door een rebuild van zowel de C++ producer als de C# consumer.

### Oplossing A: Pre-Build Hook Integratie (⭐ Aanbevolen)

Integreer de `flatc`-compilatie direct in de build-pipelines zodat schema-wijzigingen automatisch meegenomen worden.

**CMake (C++ kant):**
```cmake
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/dataset_generated.h
    COMMAND flatc --cpp -o ${CMAKE_CURRENT_SOURCE_DIR} ${SCHEMA_DIR}/dataset.fbs
    DEPENDS ${SCHEMA_DIR}/dataset.fbs
    COMMENT "Regenerating FlatBuffers C++ header..."
)
```

**MSBuild / .csproj (C# kant):**
```xml
<Target Name="GenerateFlatBuffers" BeforeTargets="BeforeBuild">
    <Exec Command="powershell -File $(SolutionDir)build_schema.ps1" />
</Target>
```

**Voordelen:**
- Schema-wijziging → `F5` → alles is bijgewerkt, geen handmatige stappen
- Ontwikkelaar hoeft nooit meer handmatig `build_schema.ps1` te draaien
- Werkt ook in CI/CD pipelines

**Nadelen:**
- Vereist dat `flatc` beschikbaar is in het PATH of via het script gedownload wordt (al geregeld)

---

### Oplossing B: FlatBuffers Reflection (Runtime Schema)

FlatBuffers biedt een **Full Reflection API** waarmee je een binary schema (`.bfbs`) runtime kunt laden en data dynamisch kunt interpreteren — zonder gegenereerde code.

**Workflow:**
1. Compileer het schema naar een binary: `flatc -b --schema dataset.fbs` → `dataset.bfbs`
2. Distribueer het `.bfbs` bestand als runtime-asset (naast de executable)
3. De consumer laadt het `.bfbs` en traverseert de data dynamisch via de reflection API

```cpp
// C++ runtime reflection
auto schema = reflection::GetSchema(bfbs_data);
auto root_table = schema->root_table();
for (auto field : *root_table->fields()) {
    // Dynamisch velden uitlezen zonder gegenereerde code
}
```

**Voordelen:**
- Geen hercompilatie nodig bij schema-wijzigingen — alleen het `.bfbs` bestand vervangen
- Ideaal voor tooling, debugging, en schema-agnostische viewers
- Forward-compatible: oude consumers kunnen nieuwe velden negeren

**Nadelen:**
- Significant langzamer dan gegenereerde code (runtime lookups i.p.v. compile-time offsets)
- Complexer om te implementeren (je bouwt in feite een interpreter)
- Niet geschikt voor de performance-kritische hot path

---

### Oplossing C: `flatc --conform` in CI/CD

Voeg een CI-stap toe die automatisch controleert of schema-wijzigingen backward-compatible zijn:

```bash
flatc --conform old_schema.bfbs new_schema.fbs
```

Dit faalt als je:
- Een bestaand veld verwijdert
- Een veldtype wijzigt
- Veld-ID's herordent

**Voordelen:**
- Vangt breaking changes op vóórdat ze in productie komen
- Combineert goed met Oplossing A (pre-build hooks)

**Nadelen:**
- Lost het hercompilatie-probleem niet op — het maakt het alleen veiliger

---

### Oplossing D: FlexBuffers (Schemaless Variant)

Google biedt naast FlatBuffers ook **FlexBuffers** — een schemaloos, zelf-beschrijvend binair formaat. Het is onderdeel van dezelfde library.

```cpp
// Schrijven zonder schema
flexbuffers::Builder fbb;
fbb.Map([&]() {
    fbb.String("api_version", "2.0.0");
    fbb.Double("temperature", 24.5);
    fbb.Vector("sensors", [&]() {
        fbb.Double(22.1);
        fbb.Double(23.4);
    });
});
```

**Voordelen:**
- Helemaal geen schema-definitie nodig
- Geen `flatc` compilatie-stap
- Velden kunnen dynamisch toegevoegd worden zonder enige rebuild

**Nadelen:**
- Verlies van type-safety op compile-time
- Tragere access dan FlatBuffers (runtime type-checks)
- Beperktere taalondersteuning (C++, Java, JavaScript, Rust — maar geen officiële C#)
- Geen zero-copy access guarantees

---

### Aanbeveling

**Oplossing A** (Pre-Build Hooks) is de pragmatische keuze: de hercompilatie blijft nodig maar wordt onzichtbaar. Combineer met **Oplossing C** (`--conform`) in CI om breaking changes te detecten. **Oplossing B** (Reflection) is waardevol voor tooling maar niet voor de hot path.

---

## 6. Samenvattingsmatrix

| Nadeel | Aanbevolen Oplossing | Complexiteit | Impact |
|---|---|---|---|
| Geen method-calls / RPC | Command Channel (tweede MMF) | 🟡 Matig | Hoog — maakt systeem interactief |
| Producer moet eerst draaien | Named "Ready" Event | 🟢 Laag | Hoog — elimineert retry-loop |
| Unidirectioneel ontwerp | Symmetrische Dual-Channel | 🟡 Matig | Hoog — volledige bidirectionaliteit |
| Geen auto proxy-generatie | C-Core DLL + FFI bindings | 🔴 Hoog | Middel — alleen relevant bij >2 talen |
| Schema vereist hercompilatie | Pre-Build Hooks + `--conform` CI | 🟢 Laag | Middel — DX verbetering |

### Prioritering (Aanbevolen Implementatievolgorde)

1. **Ready Event** (oplossing voor #2) — laagste effort, hoogste kwaliteitswinst
2. **Pre-Build Hooks** (oplossing voor #5) — kleine CMake/MSBuild wijziging, grote DX winst
3. **Command Channel** (oplossing voor #1) — maakt het systeem echt bruikbaar als volledig IPC framework
4. **Dual-Channel** (oplossing voor #3) — volgt natuurlijk uit #3, zelfde patronen
5. **C-Core DLL** (oplossing voor #4) — alleen als er concrete Python/Rust clients komen
