# DatasetProxy Pattern: C# ↔ VBScript Data Sharing via SharedValueV2

Volledige herstructurering van het project met een C# DatasetProxy die via de COM SharedValue bridge grote datasets deelt met VBScript-clients, inclusief paginering en in-place mutatie.

## User Review Required

> [!IMPORTANT]
> Dit plan herstructureert het project grondig. Oude rommelcode wordt verwijderd, de COM IDL wordt uitgebreid met een `IDatasetProxy` interface, en er komen twee volledige testprojecten bij (C# + VBScript). Graag akkoord voordat ik begin.

> [!WARNING]
> De volgende bestanden/mappen worden **verwijderd** als oud afval:
> - `CMakeLists.txt` (root) — macOS copy-paste, niet gerelateerd aan dit project
> - `src/main.cpp` + `src/` — lege "Hello World", nergens in gebruik
> - `include/cursor_com_test/` + `include/` — lege map
> - `lib/` — lege map
> - `assets/` — lege map
> - `docs/` — lege map
> - `ConsoleTest/` — verouderde test, wordt vervangen door nieuwe testprojecten
> - `build.log` — tijdelijk debug-bestand
> - `build.7z` — 11MB archief van oude Build output
> - `midl_compile.cmd` — verouderd handmatig MIDL script

---

## Proposed Changes

### Component 1: Opruimen Oude Code

#### [DELETE] `CMakeLists.txt` (root)
macOS Clang / Ninja CMake config, volstrekt irrelevant.

#### [DELETE] `src/main.cpp` + `src/` directory
Lege "Hello World", nergens gebruikt.

#### [DELETE] `include/`, `lib/`, `assets/`, `docs/`
Lege mappen zonder inhoud.

#### [DELETE] `ConsoleTest/` directory
Vervangen door de nieuwe C# en VBScript testprojecten.

#### [DELETE] `build.log`, `build.7z`, `midl_compile.cmd`
Tijdelijke/verouderde bestanden.

---

### Component 2: Nieuwe COM Interface — `IDatasetProxy`

De kern van dit plan. We voegen een nieuw COM-interface toe dat de "Proxy/Paginering" architectuur blootstelt, zodat zowel C# als VBScript er native mee kunnen communiceren.

#### [MODIFY] [ATLProjectcomserver.idl](file:///c:/csharp/cursor_com_test/ATLProjectcomserver.idl)
Toevoegen van een nieuw `IDatasetProxy` interface en bijbehorende `DatasetProxy` coclass:

```idl
interface IDatasetProxy : IDispatch
{
    // Totaal aantal records in de dataset
    HRESULT GetRecordCount([out, retval] LONG* count);
    
    // Haal een pagina op met sleutels (startIndex, limit) → SAFEARRAY van BSTR
    HRESULT FetchPageKeys([in] LONG startIndex, [in] LONG limit, 
                          [out, retval] VARIANT* keys);
    
    // Haal de data voor 1 specifieke rij op (geneste info als string)
    HRESULT GetRowData([in] BSTR key, [out, retval] BSTR* data);
    
    // Voeg een rij toe (key + value)
    HRESULT AddRow([in] BSTR key, [in] BSTR value);
    
    // Verwijder een rij
    HRESULT RemoveRow([in] BSTR key, [out, retval] VARIANT_BOOL* success);
    
    // Wis de gehele dataset
    HRESULT Clear();
};
```

#### [NEW] [DatasetProxy.h](file:///c:/csharp/cursor_com_test/DatasetProxy.h)
ATL class `CDatasetProxy` die `IDatasetProxy` implementeert. Intern leunt deze op een nieuwe `SharedValueV2::DatasetStore<>` template.

#### [NEW] [DatasetProxy.cpp](file:///c:/csharp/cursor_com_test/DatasetProxy.cpp)
Implementatie van alle methoden. De `FetchPageKeys` methode retourneert een `VT_ARRAY|VT_BSTR` SAFEARRAY zodat VBScript dit netjes als array ontvangt.

#### [NEW] [DatasetProxy.rgs](file:///c:/csharp/cursor_com_test/DatasetProxy.rgs)
Registry script voor de nieuwe coclass.

#### [MODIFY] [resource.h](file:///c:/csharp/cursor_com_test/resource.h)
Toevoegen: `#define IDR_DATASETPROXY 204`

#### [MODIFY] [ATLProjectcomserver.vcxproj](file:///c:/csharp/cursor_com_test/ATLProjectcomserver.vcxproj)
Toevoegen van `DatasetProxy.cpp`, `DatasetProxy.h`, `DatasetProxy.rgs` aan de build.

---

### Component 3: SharedValueV2 Core — `DatasetStore<>`

Een nieuwe header-only template class, ontworpen als een thread-safe, gepagineerde key-value store met Observer-notifications.

#### [NEW] [SharedValueV2/include/DatasetStore.hpp](file:///c:/csharp/cursor_com_test/SharedValueV2/include/DatasetStore.hpp)
Template class met de volgende eigenschappen:
- **Intern:** `std::map<std::wstring, TValue>` als ordered storage
- **Thread-safe:** Gebruikt dezelfde `MutexPolicy` als `SharedValue<T>`
- **Observer Pattern:** Hergebruikt `IDatasetObserver<TValue>` voor CRUD-notificaties
- **Paginering:** `FetchKeys(startIndex, limit)` retourneert een slice van keys
- **In-Place Access:** `AccessInPlace(key, mutator)` voor directe row-mutatie binnen de lock

```cpp
template <typename TValue, typename MutexPolicy = LocalMutexPolicy>
class DatasetStore {
    // GetRecordCount(), FetchKeys(), GetRow(), AddRow(), RemoveRow(), Clear()
    // AccessInPlace(key, callback) — voor in-place mutatie
    // Subscribe/Unsubscribe observers
};
```

#### [NEW] [SharedValueV2/include/DatasetObserver.hpp](file:///c:/csharp/cursor_com_test/SharedValueV2/include/DatasetObserver.hpp)
Observer interface specifiek voor dataset-events:
```cpp
template <typename TValue>
class IDatasetObserver {
    virtual void OnRowAdded(const std::wstring& key, const TValue& value) = 0;
    virtual void OnRowRemoved(const std::wstring& key) = 0;
    virtual void OnDatasetCleared() = 0;
};
```

---

### Component 4: C++ Unit & Stress Tests

#### [MODIFY] [SharedValueV2/tests/UnitTests.cpp](file:///c:/csharp/cursor_com_test/SharedValueV2/tests/UnitTests.cpp)
Uitbreiden met tests voor `DatasetStore`:
- `TestDatasetAddAndGet` — Rijen toevoegen en ophalen
- `TestDatasetPagination` — Paginering met FetchKeys
- `TestDatasetRemoveAndClear` — Verwijderen en legen
- `TestDatasetObserver` — Notificaties afvangen bij CRUD
- `TestDatasetInPlaceAccess` — In-place mutatie test

#### [MODIFY] [SharedValueV2/tests/StressTest.cpp](file:///c:/csharp/cursor_com_test/SharedValueV2/tests/StressTest.cpp)
Uitbreiden met een `DatasetStore` stresstest:
- 50 threads doen `AddRow` / `RemoveRow` / `FetchKeys` gelijktijdig
- Validators checken op data-integriteit na afloop

#### [MODIFY] [SharedValueV2/CMakeLists.txt](file:///c:/csharp/cursor_com_test/SharedValueV2/CMakeLists.txt)
(Geen wijziging nodig — tests linken al tegen de header-only lib)

---

### Component 5: C# Interop Test Project

#### [NEW] `tests/CSharpProxyTest/CSharpProxyTest.csproj`
Een .NET console-applicatie die:
1. De COM server registreert (of refereert naar de Type Library)
2. Een `DatasetProxy` COM-object instantieert via `CreateInstance`
3. 1000 rijen met geneste data toevoegt via `AddRow`
4. `FetchPageKeys` aanroept om pagina's van 100 op te halen
5. Specifieke rijen opvraagt via `GetRowData`
6. Alles in een `SharedValue` stopt en er weer uitleest (de volledige flow)
7. Assert-checks uitvoert op correctheid

#### [NEW] `tests/CSharpProxyTest/Program.cs`
De C# implementatie van bovenstaande flow.

---

### Component 6: VBScript Interop Test

#### [NEW] `tests/VBScriptTest/TestDatasetProxy.vbs`
Een VBScript dat:
1. `CreateObject("ATLProjectcomserver.DatasetProxy")` aanroept
2. Rijen toevoegt met `AddRow`
3. `FetchPageKeys` ophaalt en itereert
4. Specifieke `GetRowData` calls doet
5. De resultaten naar de console schrijft als validatie

#### [NEW] `tests/VBScriptTest/TestSharedValueBridge.vbs`
Een VBScript dat de volledige C# → SharedValue → VBScript flow demonstreert:
1. `CreateObject("ATLProjectcomserver.SharedValue")` aanroept
2. `CreateObject("ATLProjectcomserver.DatasetProxy")` als proxy maakt
3. Via `SharedValue.SetValue(proxy)` het proxy-object opslaat
4. Via `SharedValue.GetValue()` het proxy-object weer ophaalt  
5. De opgehaalde proxy bevraagt met `FetchPageKeys` en `GetRowData`

---

### Component 7: Documentatie Updates

#### [MODIFY] [README.md](file:///c:/csharp/cursor_com_test/README.md)
Toevoegen van DatasetProxy sectie en verwijzing naar testprojecten.

#### [MODIFY] [ARCHITECTURE.md](file:///c:/csharp/cursor_com_test/ARCHITECTURE.md)
Toevoegen van DatasetStore architectuur en proxy-pattern uitleg.

#### [MODIFY] [INSTALL.md](file:///c:/csharp/cursor_com_test/INSTALL.md)
Instructies voor het draaien van de C# en VBScript tests.

---

## Open Questions

> [!IMPORTANT]
> **Vraag 1:** Wil je dat de `DatasetProxy` intern een `std::map` (altijd gesorteerd op key) of een `std::unordered_map` (sneller bij >100k records, maar ongesorteerd) gebruikt? Het plan gaat nu uit van `std::map` (gesorteerd).

> [!IMPORTANT]
> **Vraag 2:** Wil je de C# test als `.NET Framework 4.8` (compatibel met je huidige VS2022 / COM-tooling) of als `.NET 8+` bouwen?

## Verification Plan

### Automated Tests
1. `cmake --build build --config Release` in `SharedValueV2/` → UnitTests.exe + StressTest.exe slagen
2. MSBuild van `ATLProjectcomserver.sln` → 0 errors, 0 warnings
3. `regsvr32` registratie van de DLL
4. `dotnet run` van CSharpProxyTest → Alle asserts slagen  
5. `cscript tests\VBScriptTest\TestDatasetProxy.vbs` → Output validatie
6. `cscript tests\VBScriptTest\TestSharedValueBridge.vbs` → End-to-end flow succesvol

### Manual Verification
- Visuele inspectie van VBScript output
- Controleren dat de oude rommelbestanden daadwerkelijk verwijderd zijn
