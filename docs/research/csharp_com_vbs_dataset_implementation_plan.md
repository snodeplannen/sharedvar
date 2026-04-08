# DatasetProxy: Bidirectionele Cross-Language Data Sharing

Volledige herstructurering van het project rondom een universeel `DatasetProxy` COM-component. Elke COM-client (VBScript, C#, C++, PowerShell, Python) kan datasets **aanmaken, vullen, delen en consumeren** via de SharedValue bridge.

## User Review Required

> [!IMPORTANT]
> Dit plan is uitgebreid ten opzichte van de vorige versie:
> - **Bidirectioneel:** VBScript kan datasets aanmaken en vullen, C# kan ze consumeren — en vice versa
> - **Policy-Based Storage:** Keuze tussen gesorteerd (`std::map`) en ongesorteerd (`std::unordered_map`)
> - **Twee .NET testprojecten:** Zowel .NET Framework 4.8 als .NET 8+
> - **Oude rommelcode wordt opgeruimd**

> [!WARNING]
> De volgende bestanden/mappen worden **verwijderd** als oud afval:
> - `CMakeLists.txt` (root) — macOS copy-paste, niet gerelateerd
> - `src/main.cpp` + `src/` — lege "Hello World"
> - `include/`, `lib/`, `assets/`, `docs/` — lege mappen
> - `ConsoleTest/` — verouderd, vervangen door nieuwe testprojecten
> - `build.log`, `build.7z`, `midl_compile.cmd` — tijdelijke/verouderde bestanden

---

## Proposed Changes

### Component 1: Opruimen Oude Code

#### [DELETE] Verouderde bestanden en lege mappen
| Pad | Reden |
|-----|-------|
| `CMakeLists.txt` (root) | macOS Clang/Ninja config, irrelevant |
| `src/main.cpp` + `src/` | Lege "Hello World" stub |
| `include/` | Lege map met lege submap |
| `lib/`, `assets/`, `docs/` | Lege mappen |
| `ConsoleTest/` | Verouderde C++ test client |
| `build.log` | Tijdelijk debug bestand |
| `build.7z` | 11MB oud build-archief |
| `midl_compile.cmd` | Handmatig MIDL script, niet meer nodig |

---

### Component 2: SharedValueV2 Core — `DatasetStore<>` met Policy-Based Storage

De pure C++ engine. Header-only, geen COM-afhankelijkheid, volledig herbruikbaar.

#### [NEW] [SharedValueV2/include/DatasetObserver.hpp](file:///c:/csharp/cursor_com_test/SharedValueV2/include/DatasetObserver.hpp)

Observer interface specifiek voor CRUD dataset-events:

```cpp
template <typename TValue>
class IDatasetObserver {
    virtual void OnRowAdded(const std::wstring& key, const TValue& value) = 0;
    virtual void OnRowUpdated(const std::wstring& key, const TValue& newValue) = 0;
    virtual void OnRowRemoved(const std::wstring& key) = 0;
    virtual void OnDatasetCleared() = 0;
};
```

#### [NEW] [SharedValueV2/include/StoragePolicies.hpp](file:///c:/csharp/cursor_com_test/SharedValueV2/include/StoragePolicies.hpp)

Policy-Based Design voor de onderliggende container. De gebruiker kiest bij compilatie:

```cpp
// Gesorteerd op key — betrouwbare volgorde, O(log n) lookup
template <typename TKey, typename TValue>
class OrderedStoragePolicy {
    using container_type = std::map<TKey, TValue>;
    // begin(), end(), find(), insert(), erase(), size(), clear()
};

// Ongesorteerd — O(1) amortized lookup, sneller bij >100k records
template <typename TKey, typename TValue>
class UnorderedStoragePolicy {
    using container_type = std::unordered_map<TKey, TValue>;
    // begin(), end(), find(), insert(), erase(), size(), clear()
};
```

#### [NEW] [SharedValueV2/include/DatasetStore.hpp](file:///c:/csharp/cursor_com_test/SharedValueV2/include/DatasetStore.hpp)

De core template class, drievoudig geparametriseerd:

```cpp
template <
    typename TValue,
    typename MutexPolicy = LocalMutexPolicy,
    template <typename, typename> class StoragePolicy = OrderedStoragePolicy
>
class DatasetStore {
public:
    // --- CRUD ---
    void AddRow(const std::wstring& key, const TValue& value);
    bool UpdateRow(const std::wstring& key, const TValue& value);
    bool RemoveRow(const std::wstring& key);
    void Clear();

    // --- Query ---
    size_t GetRecordCount() const;
    std::optional<TValue> GetRow(const std::wstring& key) const;
    bool HasKey(const std::wstring& key) const;

    // --- Paginering ---
    std::vector<std::wstring> FetchKeys(size_t startIndex, size_t limit) const;

    // --- In-Place Mutatie (C++ only, niet COM-exposed) ---
    void AccessInPlace(const std::wstring& key, std::function<void(TValue&)> mutator);

    // --- Bulk operaties ---
    std::vector<std::pair<std::wstring, TValue>> FetchPage(size_t start, size_t limit) const;

    // --- Observer Pub/Sub ---
    void Subscribe(IDatasetObserver<TValue>* observer);
    void Unsubscribe(IDatasetObserver<TValue>* observer);
};
```

- Alle notificaties worden **buiten de lock** afgevuurd (deadlock-free, net als bij SharedValue)
- `FetchKeys` maakt een slice-kopie — geen iteratoren die naar buiten lekken
- `AccessInPlace` pakt de lock, roept de callback op, release de lock, en notificeert observers

---

### Component 3: Nieuwe COM Interfaces — `IDatasetProxy`

Het COM-interface dat de C++ DatasetStore blootstelt aan de buitenwereld. Omdat het via `IDispatch` werkt, kan **elke** COM-client (VBS, C#, PowerShell, Python, JScript) dit direct aanspreken.

> [!IMPORTANT]
> **Bidirectioneel by design:** VBScript kan `CreateObject("ATLProjectcomserver.DatasetProxy")` aanroepen, rijen toevoegen, en het object vervolgens via `SharedValue.SetValue(proxy)` parkeren. Een C# applicatie kan dan `SharedValue.GetValue()` aanroepen en dezelfde proxy uitlezen, muteren, of aanvullen.

#### [MODIFY] [ATLProjectcomserver.idl](file:///c:/csharp/cursor_com_test/ATLProjectcomserver.idl)

Toevoegen van het `IDatasetProxy` interface en `DatasetProxy` coclass:

```idl
[
    object,
    uuid(... nieuwe GUID ...),
    dual,
    nonextensible,
    pointer_default(unique)
]
interface IDatasetProxy : IDispatch
{
    // --- CRUD ---
    [id(1)] HRESULT AddRow([in] BSTR key, [in] BSTR value);
    [id(2)] HRESULT GetRowData([in] BSTR key, [out, retval] BSTR* data);
    [id(3)] HRESULT UpdateRow([in] BSTR key, [in] BSTR value,
                              [out, retval] VARIANT_BOOL* success);
    [id(4)] HRESULT RemoveRow([in] BSTR key, [out, retval] VARIANT_BOOL* success);
    [id(5)] HRESULT Clear();

    // --- Query ---
    [id(6)] HRESULT GetRecordCount([out, retval] LONG* count);
    [id(7)] HRESULT HasKey([in] BSTR key, [out, retval] VARIANT_BOOL* exists);

    // --- Paginering ---
    [id(8)] HRESULT FetchPageKeys([in] LONG startIndex, [in] LONG limit,
                                  [out, retval] VARIANT* keys);
    [id(9)] HRESULT FetchPageData([in] LONG startIndex, [in] LONG limit,
                                  [out, retval] VARIANT* keysAndValues);
};

coclass DatasetProxy
{
    [default] interface IDatasetProxy;
};
```

Methoden explainer:
- `FetchPageKeys` → retourneert `VT_ARRAY|VT_BSTR` (string array)
- `FetchPageData` → retourneert `VT_ARRAY|VT_VARIANT` (2D array: key-value paren)
- Alle strings als `BSTR` → universeel leesbaar in VBS, C#, PowerShell, Python

#### [NEW] [DatasetProxy.h](file:///c:/csharp/cursor_com_test/DatasetProxy.h)
ATL class `CDatasetProxy` die `IDatasetProxy` implementeert. Intern wrapper rondom `SharedValueV2::DatasetStore<std::wstring, LocalMutexPolicy, OrderedStoragePolicy>`.

#### [NEW] [DatasetProxy.cpp](file:///c:/csharp/cursor_com_test/DatasetProxy.cpp)
Volledige implementatie. `FetchPageKeys` bouwt een COM `SAFEARRAY` op vanuit de C++ vector. `FetchPageData` bouwt een 2D SAFEARRAY.

#### [NEW] [DatasetProxy.rgs](file:///c:/csharp/cursor_com_test/DatasetProxy.rgs)
Registry script: ProgID = `ATLProjectcomserver.DatasetProxy`

#### [MODIFY] [resource.h](file:///c:/csharp/cursor_com_test/resource.h)
Toevoegen: `#define IDR_DATASETPROXY 204`

#### [MODIFY] [ATLProjectcomserver.vcxproj](file:///c:/csharp/cursor_com_test/ATLProjectcomserver.vcxproj)
Toevoegen van `DatasetProxy.cpp/.h/.rgs` aan ClCompile/ClInclude/None ItemGroups.

---

### Component 4: C++ Unit & Stress Tests

#### [MODIFY] [SharedValueV2/tests/UnitTests.cpp](file:///c:/csharp/cursor_com_test/SharedValueV2/tests/UnitTests.cpp)

Uitbreiden met DatasetStore tests:

| Test | Omschrijving |
|------|-------------|
| `TestDatasetAddAndGet` | CRUD rijen toevoegen en ophalen |
| `TestDatasetUpdate` | Bestaande rij overschrijven |
| `TestDatasetPagination` | FetchKeys met start/limit |
| `TestDatasetRemoveAndClear` | Verwijderen en wissen |
| `TestDatasetObserver` | Observer notifications per CRUD-event |
| `TestDatasetInPlaceAccess` | In-place mutatie via callback |
| `TestDatasetOrderedPolicy` | Verifieer gesorteerde key-volgorde |
| `TestDatasetUnorderedPolicy` | Verifieer werking met unordered_map |

#### [MODIFY] [SharedValueV2/tests/StressTest.cpp](file:///c:/csharp/cursor_com_test/SharedValueV2/tests/StressTest.cpp)

Nieuwe `runDatasetStressTest()`:
- 50 threads: gelijktijdig `AddRow`, `RemoveRow`, `UpdateRow`, `FetchKeys`
- Validator-thread: controleert data-integriteit na elke ronde
- Test zowel `OrderedStoragePolicy` als `UnorderedStoragePolicy`

---

### Component 5: C# Interop Tests — Twee Projecten

#### [NEW] `tests/CSharpNet48Test/` — .NET Framework 4.8

| Bestand | Doel |
|---------|------|
| `CSharpNet48Test.csproj` | .NET Framework 4.8 console project met COM reference |
| `Program.cs` | Test flows (zie hieronder) |

#### [NEW] `tests/CSharpNet8Test/` — .NET 8+

| Bestand | Doel |
|---------|------|
| `CSharpNet8Test.csproj` | .NET 8 console project met `System.Runtime.InteropServices` |
| `Program.cs` | Identieke test flows maar met modern .NET |

**Test Flows in beide projecten:**

1. **Producer Flow (C# → SharedValue → VBS):**
   - `DatasetProxy` instantiëren via COM
   - 1000 rijen toevoegen met gesimuleerd geneste data (bv. `"user:123|name:Jan|dept:IT"`)
   - Proxy in `SharedValue.SetValue()` parkeren
   - Paginering en data-integriteit valideren

2. **Consumer Flow (VBS → SharedValue → C#):**
   - `SharedValue.GetValue()` aanroepen → proxy-object ophalen
   - `GetRecordCount`, `FetchPageKeys`, `GetRowData` aanroepen
   - Assert dat de data identiek is aan wat eerder was ingesteld

3. **Bidirectionele Mutatie Test:**
   - C# maakt proxy, vult met data, parkeert in SharedValue
   - C# haalt proxy terug, voegt extra rij toe, verwijdert een rij
   - Controleert dat `GetRecordCount` klopt na mutatie

---

### Component 6: VBScript Interop Tests

#### [NEW] `tests/VBScriptTest/TestProducerVBS.vbs`

VBScript als **producer** — creëert een dataset en parkeert hem:

```vbs
' VBScript MAAKT de dataset en deelt hem
Set proxy = CreateObject("ATLProjectcomserver.DatasetProxy")
proxy.AddRow "server01", "status:online|cpu:45|mem:8192"
proxy.AddRow "server02", "status:offline|cpu:0|mem:0"
proxy.AddRow "server03", "status:online|cpu:92|mem:16384"

' Parkeer in de SharedValue bridge voor C# of C++ consumptie
Set shared = CreateObject("ATLProjectcomserver.SharedValue")
shared.SetValue proxy

WScript.Echo "Dataset met " & proxy.GetRecordCount() & " rijen geparkeerd."
WScript.Echo "C# of C++ kan nu SharedValue.GetValue() aanroepen!"
```

#### [NEW] `tests/VBScriptTest/TestConsumerVBS.vbs`

VBScript als **consumer** — leest een dataset die door C# was aangemaakt:

```vbs
' VBScript LEEST de dataset die door C# was ingesteld
Set shared = CreateObject("ATLProjectcomserver.SharedValue")
Set proxy = shared.GetValue()

count = proxy.GetRecordCount()
WScript.Echo "Dataset bevat " & count & " rijen"

' Pagineer door alles heen
keys = proxy.FetchPageKeys(0, 100)
For Each k In keys
    data = proxy.GetRowData(k)
    WScript.Echo k & " => " & data
Next
```

#### [NEW] `tests/VBScriptTest/TestFullCycle.vbs`

VBScript doet de complete cirkel: produceren → opslaan → ophalen → muteren → valideren:

```vbs
Set proxy = CreateObject("ATLProjectcomserver.DatasetProxy")
Set shared = CreateObject("ATLProjectcomserver.SharedValue")

' 1. Vul
proxy.AddRow "cfg_timeout", "30"
proxy.AddRow "cfg_retries", "5"
shared.SetValue proxy

' 2. Haal op via SharedValue (simuleer andere client)
Set proxy2 = shared.GetValue()
WScript.Echo "Records: " & proxy2.GetRecordCount()

' 3. Muteer
proxy2.UpdateRow "cfg_timeout", "60"
proxy2.RemoveRow "cfg_retries"

' 4. Valideer
WScript.Echo "Na mutatie: " & proxy2.GetRecordCount() & " records"
WScript.Echo "Timeout = " & proxy2.GetRowData("cfg_timeout")
```

---

### Component 7: Documentatie Updates

#### [MODIFY] [README.md](file:///c:/csharp/cursor_com_test/README.md)
- DatasetProxy sectie met bidirectionele flow diagram
- Links naar testprojecten en VBScript voorbeelden
- Nieuwe ProgID's tabel

#### [MODIFY] [ARCHITECTURE.md](file:///c:/csharp/cursor_com_test/ARCHITECTURE.md)
- DatasetStore template architectuur
- Policy-Based Storage uitleg (Ordered vs Unordered)
- Bidirectionele data-flow diagram
- Proxy Pattern uitleg

#### [MODIFY] [INSTALL.md](file:///c:/csharp/cursor_com_test/INSTALL.md)
- Instructies voor C# .NET 4.8 en .NET 8 tests
- VBScript test instructies (`cscript`)
- COM registratie stappen

---

## Verification Plan

### Automated Tests

| Stap | Commando | Verwacht resultaat |
|------|---------|-------------------|
| 1 | `cmake -B build && cmake --build build --config Release` in `SharedValueV2/` | UnitTests + StressTest slagen |
| 2 | MSBuild `ATLProjectcomserver.sln` (Debug\|x64) | 0 errors, 0 warnings |
| 3 | `regsvr32 x64\Debug\ATLProjectcomserver.dll` | Registratie succesvol |
| 4 | `dotnet run` in `tests/CSharpNet8Test/` | Alle asserts slagen |
| 5 | MSBuild `tests/CSharpNet48Test/` + run | Alle asserts slagen |
| 6 | `cscript tests\VBScriptTest\TestProducerVBS.vbs` | "Dataset geparkeerd" |
| 7 | `cscript tests\VBScriptTest\TestConsumerVBS.vbs` | Rijen correct uitgelezen |
| 8 | `cscript tests\VBScriptTest\TestFullCycle.vbs` | Volledige CRUD-cyclus klopt |

### Manual Verification
- Oude rommelbestanden zijn verwijderd
- Visuele inspectie VBScript output
- Controleer dat DatasetProxy werkt vanuit PowerShell: `$p = New-Object -ComObject ATLProjectcomserver.DatasetProxy`
