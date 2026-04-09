Viewed ARCHITECTURE.md:1-800

Dat is een hele goede en belangrijke architectuurvraag! 

Het antwoord op je eerste vraag hangt sterk af van *hoe* die oplossingen precies verbonden worden in het **huidige** ontwerp.

### Vraag 1: Wordt er dan een COM server gebruikt?

**Kort antwoord: Ja.** In het *huidige* systeem moet de COM Server (`ATLProjectcomserverExe`) in dat scenario absoluut als "host" (knooppunt) worden gebruikt.

**Waarom?**
Als jouw C++ applicatie `SharedValueV2` (de C++ header-only engine) *rechtstreeks* in zijn eigen code includeert en instantieert, leeft die data volledig in het geïsoleerde geheugen van jouw C++ proces. Dit wordt in de architectuur bevestigd doordat de engine nu de `LocalMutexPolicy` (`std::mutex`) gebruikt. Een ander proces (zoals je C# applicatie) kan daar onmogelijk direct bij.

Om data te delen in het **huidige ontwerp**, ziet het scenario er zo uit:
1. De **COM Server** (`ATLProjectcomserverExe.exe`) is de eigenaar vande werkelijke `SharedValueV2` C++ instantie. 
2. Jouw C++ applicatie start. Via `CoCreateInstance(...)` start hij (als eerste client) de COM Server of verbindt hij hiermee via C++ COM-interfaces. De data wordt dan *binnen de COM server* aangemaakt (bijv. via `ISharedValue::SetValue` of `IDatasetProxy::AddRow`).
3. Daarna start de C# applicatie. Deze gebruikt `Activator.CreateInstance` naar dezelfde COM Server (het is immers een system-wide Singleton). Zo kan de C# app de data ophalen die door C++ via de server is gecreëerd.

> [!NOTE] 
> **Toekomstperspectief (Memory Mapped Files)**: In `ARCHITECTURE.md` (Hoofdstuk 9) staat een policy genaamd `NamedSystemMutexPolicy` omschreven als *toekomstig*. Mocht je `SharedValueV2` ooit zo ombouwen dat het direct gebruik maakt van Windows Shared Memory (Memory Mapped Files) met Named Mutexes, dan heb je technisch gezien de COM server niet meer nodig. C# zou dan wel intensief gebruik moeten maken van P/Invoke (native interop) om bij dat pure C++ geheugen te komen, wat erg ingewikkeld is (zeker met pointers, allocaties en C++ containers zoals `std::vector` en `std::wstring`). De COM server lost dit interop-probleem juist perfect op!

---

### Vraag 2: Hoe maakt de C# applicatie bij voorkeur gebruik van SharedValueV2?

De C# applicatie praat **nooit** direct met de C++ core-engine (`SharedValueV2`), maar praat exclusief tegen de COM-laag. Hier zijn in C# twee hele typische voorkeursmanieren voor:

#### 1. Late Binding (gebruikt in jullie huidige tests)
Dit is makkelijk in het gebruik, vereist geen build-afhankelijkheden (zoals dll-imports of TypeLibs), en gebruikt de .NET `dynamic` keyword.
```csharp
// ProgID opzoeken via het Windows Register
Type t = Type.GetTypeFromProgID("ATLProjectcomserverExe.DatasetProxy");
dynamic proxy = Activator.CreateInstance(t);

// Aanroepen via IDispatch (over de RPC named pipe naar de C++ COM Host)
proxy.AddRow("sensor_1", "actief");
object dataResult;
proxy.GetRowData("sensor_1", out dataResult); 
```

#### 2. Early Binding (Beste performance en type-safety)
Als het een stevige, grote C# applicatie is, ben je beter af met "Early Binding". Hierbij importeer je de COM Type Library (`.tlb` bestand geproduceerd door je project) in je .NET assembly. 
Dit geeft je **IntelliSense**, sterke typering, en het checkt fouten al tijdens het compilen in de C# omgeving:
```csharp
// Via een gegenereerde Interop.ATLProjectcomserverLib
var proxy = new DatasetProxy(); // Start de COM Server op of verbindt met de actieve instantie
proxy.AddRow("sensor_1", "actief");

string dataResult;
proxy.GetRowData("sensor_1", out dataResult);
```

### Conclusie voor jouw scenario:
In plaats van dat jouw C++ app direct gebruik maakt van `SharedValue<T> = new...`, treedt jouw native C++ applicatie op als een **COM Client** (gebruikmakend van de gegenereerde `_i.h` en `_i.c` C++ headers), waarbij de `ATLProjectcomserverExe` het cross-process doorgeefluik vormt naar de C# Client.
---

Ja, het gebruik van een Out-of-Process COM server heeft absoluut impact op de snelheid vergeleken met direct in-memory (in-process) data benaderen. Snelheid is bij dit specifieke architectuur-besluit de belangrijkste afweging.

Hier is de technische achtergrond over wat het voor de performance betekent en hoe het project dit al oplost:

### 1. Waar zit de vertraging? (De RPC Overhead)
Elke keer als jouw C++ of C# applicatie een functie aanroept op de COM Server (zoals `AddRow` of `GetValue`), gebeurt er "onder water" het volgende:
1. **Context Switch & ALPC**: Je applicatie pauzeert, het OS pakt het signaal op en stuurt het naar het COM Server-proces via Advanced Local Procedure Calls (ALPC / named pipes).
2. **Data Marshaling / Serialisatie**: Alle doorgegeven data (zoals strings en varianten) moet door de Proxy in een buffer worden gestopt en aan de ontvangende kant weer worden uitgepakt.
3. **Uitvoering & Terugweg**: De COM server voert de code de `SharedValueV2` actie in fracties van nanoseconden uit, en het hele ALPC + Marshaling traject gebeurt *weer* in omgekeerde richting.

**In getallen**: 
- Een directe C++ in-memory read: **~1 tot 5 nanoseconden**
- Een Out-of-Process COM call (zonder payload): **~5 tot 15 microseconden** (dat is al duizenden keren langzamer, maar voor een mens onzichtbaar).
- Een C# late-binding call (via `dynamic`) heeft nóg iets meer .NET overhead vanwege reflectie (RCW wrappers).

### 2. Is deze impact "merkbaar" of een probleem?
Dit hangt helemaal af van je de wensen van de software:

- 🟢 **Geen enkel probleem (Configuratie, UI-updates, Events)**: Als je data 10 keer per seconde, of zelfs 1.000 keer per seconde ophaalt of update via `ISharedValue` of individuele `GetRowData`, merk je hier totaal niks van. Het OS verwerkt dit in de ruis van de achtergrondprocessen.
- 🔴 **Absolute bottleneck (High-Frequency Trading, Video-frames, Real-time Audio)**: Als je C++ applicatie 500.000 keer per seconde kleine data-stukjes updatet (bijvoorbeeld een sensor stream), en C# probeert die telkens op te halen via individuele COM calls, dan gaat de CPU zwaar clippen op de RPC overhead. Dit is "chattiness" (te veel kleine commando's).

### 3. Hoe lost dit project dat prestatieprobleem op? (Batching)
Speciaal vanwege dit "chatty" RPC snelheidsverlies, is jullie `DatasetProxy` uitgerust met **Variant Arrays** en Paginering:
```
+FetchPageKeys(LONG, LONG, VARIANT*) HRESULT
+FetchPageData(LONG, LONG, VARIANT*) HRESULT
```
*Gevolg:* Als C# niet 1 variabele wil weten, maar 10.000 sensorwaarden in één klap, hoeft hij geen 10.000 enorm trage COM calls meer te maken. Je haalt met één enkele `FetchPageData` COM aanroep een grote `SAFEARRAY` in C# binnen. De RPC overhead heb je dan maar 1 keer gepakt voor 10.000 waardes! Hiermee red je de performance enorm.

### Samengevat
**Ja, het is langzamer in microseconden.** Het is sneller dan een lokale database of WebSocket, maar duizenden  keren langzamer dan pure pointer-geheugen toegang (`SharedValueV2` local). 

Zolang de C++ engine geen 50.000 COM updates per seconde forceert per veldje, maar slim **batching (arrays)** combineert met lokaal cachen, zal de C# applicatie nergens last van hebben. Mocht pure nanoseconde performance een absolute projecteis zijn, dán moet het `SharedValueV2` ontwerp worden afgesplitst naar Windows Memory-Mapped Files.