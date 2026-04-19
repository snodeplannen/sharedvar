# Globale Architectuur: SharedValue Ecosysteem

> **Scope:** Dit document biedt een high-level architectonisch overzicht van het volledige `SharedValue` ecosysteem. Het fungeert als een routeringsdocument. Voor diepgaande technische details, zie de specifieke architectuurdocumenten van elke afzonderlijke generatie.

Het **SharedValue** project is geëvolueerd door vier belangrijke architectonische paradigma's om het probleem van Inter-Process Communication (IPC) en het delen van data op Windows op te lossen. De kernuitdaging is het delen van status (state) op een veilige, efficiënte manier tussen verschillende programmeertalen (C++, C#, VBScript, Python) zonder data races.

## Architectonische Generaties

```mermaid
graph TD
    Client["Client Applicaties<br/>(C#, Python, VBS, C++)"]
    
    subgraph "Generatie 2: COM / RPC"
        V2["SharedValueV2<br/>COM Out-of-Process Server"]
    end
    
    subgraph "Generaties 3, 4, 5: Kernel Memory-Mapped Files"
        direction TB
        V3["SharedValueV3<br/>(MemMap Unidirectioneel)"]
        V4["SharedValueV4<br/>(MemMap Dual-Channel)"]
        V5["SharedValueV5<br/>(MemMap Dynamisch Schema)"]
    end

    Client -->|"Trage, rijke COM API"| V2
    Client -->|"Ultrasnelle Geheugen Pointers"| V3
    Client -->|"Ultrasnel + Kanaal Terug"| V4
    Client -->|"Dynamisch Schema + DataTable API"| V5
```

### 1. SharedValueV2: De COM/RPC Server
**Patroon:** Singleton Monitor Pattern via Out-of-Process COM Server (`LocalServer32`).<br>
**Transport:** Microsoft RPC via Local Named Pipes.

In deze architectuur draait een gecentraliseerde `ATLProjectcomserver.exe` als Windows achtergrondproces. Alle clients communiceren met deze server via COM Interface Pointers (`ISharedValue`, `IDatasetProxy`). Omdat meerdere processen dezelfde C++ singleton aanspreken, garandeert native C++ `std::mutex` locking de thread-safety.

```mermaid
sequenceDiagram
    participant Client as Client (bijv. VBScript)
    participant RPC as Windows RPC (Named Pipes)
    participant Server as COM Server EXE
    participant Eng as C++ SharedValue Engine

    Client->>RPC: SetValue(VARIANT) - Marshaling
    Note over RPC: Trage Kernel Transitie (~1-10 μs)
    RPC->>Server: LRPC Ontvangst
    Server->>Eng: std::lock_guard -> m_value = x
    Eng-->>Server: HRESULT S_OK
    Server-->>RPC: Unmarshaling
    RPC-->>Client: Return Control
```

*   **Het Verschil:** V2 gebruikt **marshaling**. Data wordt verpakt, over een IPC kanaal gestuurd, en aan de andere kant uitgepakt. Dit is traag, maar uiterst stabiel en makkelijk te gebruiken vanuit trage scripttalen via simpele objectoriëntatie.
*   **Voordelen:** Makkelijk aan te roepen vanuit scripttalen zoals VBScript. Rijke objectgeoriënteerde API en robuuste foutafhandeling (exceptions naar HRESULT).
*   **Nadelen:** Hoge latency per call door RPC marshaling. Vereist `regsvr32` / installatie met beheerdersrechten (Registry vervuiling).
*   **Deep Dive:** [SharedValueV2 Architectuur](SharedValueV2/ARCHITECTURE_NL.md)

### 2. SharedValueV3 (MemMap): Unidirectionele FlatBuffers
**Patroon:** Zero-copy Kernel Memory Sharing.<br>
**Transport:** Windows Memory-Mapped Files (`Global\...`) + Named Events.

Om de COM bottleneck te omzeilen, schrijft V3 directe binaire data (via Google FlatBuffers) in een gedeelde Windows kernel pagina. Consumenten mappen precies dezelfde pagina in hun eigen geheugenruimte. Ze krijgen meldingen via een Windows Event Handle wanneer nieuwe data arriveert, waardoor hun threads ontwaken met 0% CPU-belasting tijdens rust.

```mermaid
flowchart LR
    subgraph Producer [Producer Proces (C++)]
        FB[Bouw FlatBuffer] --> L1(Lock Named Mutex)
        L1 --> W[MemCpy naar MMF]
        W --> U1(Unlock Mutex)
        U1 --> E(Set Named Event)
    end
    
    subgraph Shared [Windows OS Kernel]
        MMF[(Memory-Mapped File<br/>10 MB)]
    end
    
    subgraph Consumer [Consumer Proces (C#)]
        E -.->|Ontwaakt Thread!| R1(WaitOne Event)
        R1 --> C2(Lock Named Mutex)
        MMF -.->|Geheugen Pointer| R2[ReadArray MMF]
        C2 --> R2
        R2 --> C3(Unlock Mutex)
        C3 --> R3[Lees FlatBuffer]
    end
    
    W -.-> MMF
    style MMF fill:#1b4332,color:#fff
```

*   **Het Verschil:** Er is **geen data-overdracht**. Zowel de producer als consumer kijken naar exact stroom aan elektronen in het RAM (via virtuele paging). Het event-mechanisme triggert enkel de lezing. Hierdoor duikt de latency naar nanosecondes.
*   **Voordelen:** Ultrasnelle latency (~50 ns). Geen string/object serialisatie overhead dankzij Google FlatBuffers structuren.
*   **Nadelen:** Strikt unidirectioneel (Push of Pull gescheiden). Schema ligt star vast door compile-time code generatie.
*   **Deep Dive:** [SharedValueV3 Architectuur](SharedValueV3_MemMap/ARCHITECTURE_NL.md)

### 3. SharedValueV4: Dual-Channel Bidirectioneel
**Patroon:** Symmetrische Sockets over Gedeeld Geheugen.<br>
**Transport:** Dubbele Memory-Mapped Files (P2C en C2P) + Ready Events Handshake.

V4 bouwt voort op V3 door een retourkanaal te introduceren. Het creëert een symmetrisch systeem waarbij beide partijen kunnen fungeren als Producent en Consument. Er is een robuust "Ready Event" handshake-algoritme ingebouwd om te voorkomen dat een proces in het geheugen schrijft voordat de andere partij luistert.

```mermaid
graph TD
    subgraph Host [Host Applicatie]
        direction TB
        HW[Write P2C]
        HR[Read C2P]
    end

    subgraph Kernel [2x Memory-Mapped Files]
        P2C[(P2C_Map<br/>Host -> Client)]
        C2P[(C2P_Map<br/>Client -> Host)]
    end

    subgraph Client [Client Applicatie]
        direction TB
        CR[Read P2C]
        CW[Write C2P]
    end

    HW -->|Mutex locked| P2C
    P2C -.->|Event 1 triggers| CR
    
    CW -->|Mutex locked| C2P
    C2P -.->|Event 2 triggers| HR
    
    style P2C fill:#2a9d8f,color:#fff
    style C2P fill:#e76f51,color:#fff
```

*   **Het Verschil:** Waar V3 een simpele 'fire & forget' waterleiding is, is V4 vergelijkbaar met een razendsnelle full-duplex TCP socket (maar dan puur in het RAM gerealiseerd). Perfect voor RPC-achtige bidirectionele flows met High Frequency eisen.
*   **Voordelen:** Bidirectionele realtime IPC geschikt voor High-Frequency Trading en closed-loop control systemen (>100.000 berichten/sec).
*   **Nadelen:** Setup complexiteit van twee kanalen tegelijkertijd beheren. Schema vereist nog steeds pre-compilatie via `flatc`.
*   **Deep Dive:** [SharedValueV4 Architectuur](SharedValueV4/ARCHITECTURE_NL.md)

### 4. SharedValueV5: Dynamisch Schema "DataTable" IPC
**Patroon:** Self-describing Binaire Layout + ADO.NET-stijl DataSets.<br>
**Transport:** Memory-Mapped Files met ingebedde dynamische schema's.

V5 lost de compile-time belemmering van FlatBuffers op. Elke taal (zelfs VBScript via een C# COM-wrapperlaag) kan nu dynamisch tabelkolommen definiëren tijdens programmatijd (bijv. `AddColumn("Temperatuur", Double)`). De datastructuur wordt direct in het geheugen in een zelfbeschrijvend binair formaat opgeslagen. Consumenten verwerken eenvoudigweg de map-header om het schema te doorgronden, zonder gecompileerde code te hoeven genereren.

```mermaid
block-beta
    columns 1
    
    block:header
        columns 2
        H1["16-byte Master Header (Magic + Version)"]
        H2["Table Directory (Offset Pointers)"]
    end
    
    block:tables
        columns 3
        T1["Tabel 1: 'Config'<br/>(Schema Headers + Rijen)"]
        T2["Tabel 2: 'Sensoren'<br/>(Schema Headers + Rijen)"]
        T3["Tabel N...<br/>..."]
    end
    
    block:strings
        S["Variabele String & Blob Pool (Om fragmentatie te killen)"]
    end

    style header fill:#264653,color:#fff
    style tables fill:#2a9d8f,color:#fff
    style strings fill:#e76f51,color:#fff
```

*   **Het Verschil:** V3 en V4 sturen pure ruwe bit-stromen over; ze hebben géén idee wat ze sturen, het `flatc` contract moet beiderzijds ingebakken zitten. V5 verpakt de **blauwdruk van de tabel(len)** ín het geheugen. Het fungeert eigenlijk als een ultrasnelle in-memory database engine vergelijkbaar met .NET DataSets.
*   **Voordelen:** Volledig en programmatisch dynamisch. Uitstekende integratie met VBScript, Python en VBA tools. Prachtige iteratieve en achterwaarts compatibele schema-evolutie tijdens live draaiende operaties.
*   **Nadelen:** De index-berekening voor kolommen (de 'dynamic schema lookup') voegt zo'n kleine overhead toe vergeleken met V4 arrays (~50-100ns).
*   **Deep Dive:** [SharedValueV5 Architectuur](SharedValueV5/ARCHITECTURE_NL.md)

## Gemeenschappelijke Architectonische Principes (V3-V5)

Terwijl V2 steunt op Windows COM, delen versies 3, 4 en 5 een fundamenteel onderliggende set OS logica om lock-vrije geheugensynchronisatie of snelle mutexen af te dwingen:

1.  **Memory-Mapped File (`CreateFileMapping` / `MapViewOfFile`)**
    Creëert een allokatie in de Windows Kernel paging file. Pointers binnen deze regio wijzen naar fysiek exact hetzelfde RAM geheugen.
2.  **Named Mutex (`CreateMutex`)**
    Verzekert dat wanneer de Producent grote, continue geheugenblokken of FlatBuffer bomen schrijft, een Consument de buffer pas na ontgrendeling kan evalueren.
3.  **Named Event (`CreateEvent`)**
    Voorkomt CPU spin-locking perrons. Consumenten roepen een blokkerende `WaitForSingleObject` op. De Windows context-scheduler maakt ze letterlijk per microseconde wakker wanneer de Event-flag wordt getriggerd gezet.
