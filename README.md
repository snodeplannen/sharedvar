# ATL COM Server & SharedValueV2 Project

Welkom bij dit ATL COM Server project. Deze repository bevat een klassiek **Windows C++ ATL/COM component** gecombineerd met een hypermoderne, thread-safe, **C++20 header-only core (SharedValueV2)**. 

Oorspronkelijk was dit project ontwikkeld als leerschool voor het uitwisselen en opslaan van variabelen (`VARIANT`) tussen diverse client applicaties. Het is onlangs volledig geherstructureerd voor high-performance multithreading zonder vastlopers of deadlocks.

## Functionaliteit

Via deze COM Server DLL worden onafhankelijke functionaliteiten blootgesteld (te benaderen vanuit C++, C#, Python e.a.):

1. **`MathOperations`** (`IMathOperations`): Een eenvoudige component voor stateless basis rekenkundige berekeningen (Add, Subtract, Multiply, Divide).
2. **`SharedValue`** (`ISharedValue`): Het hoofdonderdeel van dit project. Een iteratie waarbij een in-process memory state veilig bekeken (`GetValue`), ontgrendeld geraadpleegd gepauzeerd (`LockSharedValue`) en weggescreven (`SetValue`) kan worden. Tevens ondersteunt deze objecten event-subscriptie voor observer notificaties.
3. **`SharedValueCallback`** (`ISharedValueCallback`): C++ proxy-interface voor het observer-pattern, zodat externe scripts en integraties live te horen krijgen wanner er datetime- of inhoudelijk wijzigingen aan de state zijn gemaakt.

## Project Architectuur & Ontwerp
Ben je geïnteresseerd in de interne robuustheid en de Design Patterns (zoals *Policy-Based Design* en *Monitor Pattern*)? 
Vind een volledig overzicht in: **[ARCHITECTURE.md](ARCHITECTURE.md)**.

## Compatibiliteit
- Visual Studio 2022.
- Ondersteunt x64 architectuur (en Win32 waar geconfigureerd).
- Te registreren in het Windows Register via `regsvr32`.

## Hoe te Bouwen, Installeren en Testen
Wil je dit project zelf compileren op een verse machine of lokaal de ingebouwde CMake Multithreaded Chaos tests (StressTest) draaien?
Lees de installatie- en debug handleiding in: **[INSTALL.md](INSTALL.md)**.