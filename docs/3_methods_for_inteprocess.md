# Cross-Process Architecture Redesign

De huidige `ATLProjectcomserver` is ontworpen als een **In-Process Server (.dll)**. Dit betekent dat het geheugen strikt geïsoleerd per proces is. Wanneer `cscript.exe` (VBScript) en `dotnet.exe` (C#) de COM-objecten aanroepen, laden ze beide onafhankelijk een kopie van de DLL in hun eigen geheugenruimtes (heaps).

Om echte data-sharing tussen gescheiden processen te realiseren, moeten we de proces-grens (`process boundary`) overbruggen.

> [!WARNING]
> **User Review Required**
> Kies één van de onderstaande migratiepaden. Optie 2 is de ontwerpkundig meest logische tussenstap, aangezien Optie 1 het projecttype verandert en Optie 3 de C++ code significant complexer maakt.

---

## Optie 1: Native Out-of-Process Server (ATL EXE)

We veranderen de output van het C++ project in Visual Studio van `.dll` naar `.exe` (`LocalServer32`). 

**Hoe het werkt:**
Wanneer een programma `CreateObject("ATLProjectcomserver.SharedValue")` aanroept, start de Windows COM Service (`svchost.exe`) onze EXE op de achtergrond. De EXE host de C++ Core (de *SharedValueV2* singleton). Zowel het VBS-script als de C#-app communiceren met dat ene gecentraliseerde EXE-proces via RPC (Remote Procedure Call) marshaling.

* **Voors:** De standaard "Microsoft-aanbevolen" manier om een cross-process COM singleton te maken. Stabiel.
* **Tegens:** Vereist een refactor van de C++ COM boilerplate (van `DllMain` naar `_tWinMain`, van `CAtlDllModuleT` naar `CAtlExeModuleT`). Trager dan in-process door RPC marshaling.

## Optie 2: Windows DLL Surrogate (`dllhost.exe`)

We behouden de huidige C++ DLL, maar we definiëren een nieuwe `AppID` (Application ID) in het Windows Register, specifiek voor onze classes. We configureren dit `AppID` met een lege `DllSurrogate` string.

**Hoe het werkt:**
Wanneer C# of VBS de COM server oproept, merkt Windows de `DllSurrogate` tag op. In plaats van de DLL direct in het aanroepende proces (bijv. `cscript.exe`) te laden, start Windows `dllhost.exe` (COM Surrogate) op de achtergrond. De DLL wordt exclusief in dát surrogate process geladen. Meerdere clients worden door Windows doorverwezen naar diezelfde `dllhost.exe`.

* **Voors:** **GEEN ENKELE** C++ code-wijziging vereist. Alleen de `.rgs` (registry scripts) van het project hoeven te worden geüpdatet, gevolgd door een rebuild & registerronde.
* **Tegens:** `dllhost.exe` is gecreëerd door Microsoft als een "lapmiddel" wrapper, de debugging errvaring kan lastiger zijn als er crashes gebeuren (aangezien `dllhost.exe` zal crashen in plaats van jouw eigen process).

## Optie 3: Windows Shared Memory (File Mapping)

We behouden de DLL (voor supersnelle in-process RPC en nul COM overhead), maar we herschrijven de C++ `StorageEngine` in de core. Alles wat via `AddRow` wordt toegevoegd, wordt niet meer in een lokale C++ `std::map` gezet, maar naar een via `CreateFileMapping` gereserveerd gedeeld RAM-geheugenblok weggeschreven dat door meerdere processen tegelijk gelezen mag worden.

* **Voors:** Ongeëvenaarde performance. Geen trage Windows RPC marshaling calls of IPC overhead.
* **Tegens:** Extreem complex. C++ STL containers (zoals `std::string` of `std::map`) gebruiken default-allocators die niet werken in process-overschrijdend gedeeld geheugen. We moeten custom memory-allocators schrijven.

---

## Voorgesteld Aanvalsplan (Advies: Start met Optie 2)

Als je akkoord bent met **Optie 2 (DllSurrogate)** om snel resultaat te boeken, zal ik het volgende doen:

### 1. Registry Scripts Updates
Ik pas `DatasetProxy.rgs` en `SharedValue.rgs` aan.
#### [MODIFY] `DatasetProxy.rgs`
- Voeg in de registry tree onder `NoRemove CLSID` een `val AppID = s '{nieuw-gedeelde-appid-guid}'` toe.
- Voeg onder `HKCR` een nieuwe root toe `NoRemove AppID { {nieuw-gedeelde-appid-guid} { val DllSurrogate = s '' } }`.

### 2. Registry Opruimen
Eerst voeren we het eerder gemaakte diagnostische script uit (of de unregister) om de *InprocServer32* configuraties veilig te veranderen. 

### 3. Build & Test Run
We her-bouwen, registreren, en voeren de originele **Consumer/Producer tests** uit in verschillende terminalvensters. De *Proxy* call van de consumer zou dan data moeten krijgen!

## Open Questions
- Welke van de drie opties geniet jouw voorkeur voor dit project?
- Is prestatie (lagere latency via `in-process` en `shared memory`) belangrijker dan architecturale netheid (COM EXE / RPC)?
