# DatasetProxy: Bidirectionele Cross-Language Data Sharing

Volledige herstructurering van het project rondom een universeel `DatasetProxy` COM-component. Elke COM-client (VBScript, C#, C++, PowerShell, Python) kan datasets **aanmaken, vullen, delen en consumeren** via de SharedValue bridge.

## User Review Required

> [!IMPORTANT]
> Dit plan is uitgebreid ten opzichte van de vorige versie:
> - **Bidirectioneel:** VBScript kan datasets aanmaken en vullen, C# kan ze consumeren — en vice versa
> - **Runtime Storage Selection:** Keuze tussen gesorteerd (`std::map`) en ongesorteerd (`std::unordered_map`) — **tijdens runtime** via Strategy Pattern
> - **Twee .NET testprojecten:** Zowel .NET Framework 4.8 als .NET 8+
> - **Oude rommelcode wordt opgeruimd**
> - **Centralized Error Handling:** Drie-lagen foutenbeheer (C++ exceptions → COM HRESULT+IErrorInfo → VBS Err.Description / C# COMException)

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

### Component 2: SharedValueV2 Core — Runtime Storage Selection via Strategy Pattern

De pure C++ engine. Header-only, geen COM-afhankelijkheid, volledig herbruikbaar.

> [!IMPORTANT]
> **Runtime keuze i.p.v. compile-time templates.** Omdat COM-clients (VBScript, C#) geen C++ template parameters kunnen doorgeven, gebruiken we het **Strategy Pattern** met runtime polymorfisme (`virtual` methods via een abstract `IStorageEngine` interface). De DatasetStore neemt bij constructie een `std::unique_ptr<IStorageEngine>` in ontvangst — de keuze (Ordered vs Unordered) wordt dus gemaakt op het moment dat het object wordt aangemaakt.

#### [NEW] [SharedValueV2/include/DatasetObserver.hpp](file:///c:/csharp/cursor_com_test/SharedValueV2/include/DatasetObserver.hpp)

Observer interface specifiek voor CRUD dataset-events:

```cpp
template <typename TValue>
class IDatasetObserver {
public:
    virtual ~IDatasetObserver() = default;
    virtual void OnRowAdded(const std::wstring& key, const TValue& value) = 0;
    virtual void OnRowUpdated(const std::wstring& key, const TValue& newValue) = 0;
    virtual void OnRowRemoved(const std::wstring& key) = 0;
    virtual void OnDatasetCleared() = 0;
};
```

#### [NEW] [SharedValueV2/include/StorageEngine.hpp](file:///c:/csharp/cursor_com_test/SharedValueV2/include/StorageEngine.hpp)

Abstract interface + twee concrete implementaties. De keuze is **runtime**:

```cpp
// --- Abstract Interface ---
template <typename TValue>
class IStorageEngine {
public:
    virtual ~IStorageEngine() = default;
    virtual void Insert(const std::wstring& key, const TValue& value) = 0;
    virtual bool Update(const std::wstring& key, const TValue& value) = 0;
    virtual bool Erase(const std::wstring& key) = 0;
    virtual void Clear() = 0;
    virtual size_t Size() const = 0;
    virtual std::optional<TValue> Find(const std::wstring& key) const = 0;
    virtual bool Contains(const std::wstring& key) const = 0;
    virtual std::vector<std::wstring> GetKeys(size_t start, size_t limit) const = 0;
    virtual std::vector<std::pair<std::wstring, TValue>> GetPage(size_t start, size_t limit) const = 0;
};

// --- Gesorteerd: O(log n) lookup, betrouwbare volgorde ---
template <typename TValue>
class OrderedStorageEngine : public IStorageEngine<TValue> {
private:
    std::map<std::wstring, TValue> m_data;
    // ... implementeert alle methods via std::map ...
};

// --- Ongesorteerd: O(1) amortized lookup, sneller bij >100k records ---
template <typename TValue>
class UnorderedStorageEngine : public IStorageEngine<TValue> {
private:
    std::unordered_map<std::wstring, TValue> m_data;
    // ... implementeert alle methods via std::unordered_map ...
};

// --- Enum voor runtime keuze ---
enum class StorageMode { Ordered = 0, Unordered = 1 };

// --- Factory ---
template <typename TValue>
std::unique_ptr<IStorageEngine<TValue>> CreateStorageEngine(StorageMode mode) {
    if (mode == StorageMode::Unordered)
        return std::make_unique<UnorderedStorageEngine<TValue>>();
    return std::make_unique<OrderedStorageEngine<TValue>>();
}
```

#### [NEW] [SharedValueV2/include/DatasetStore.hpp](file:///c:/csharp/cursor_com_test/SharedValueV2/include/DatasetStore.hpp)

De core class. De `MutexPolicy` blijft een compile-time template (interne Threading concern), maar de **Storage Engine wordt bij constructie runtime gekozen**:

```cpp
template <typename TValue, typename MutexPolicy = LocalMutexPolicy>
class DatasetStore {
public:
    // Default = Ordered. Maar de caller kan kiezen:
    explicit DatasetStore(StorageMode mode = StorageMode::Ordered)
        : m_engine(CreateStorageEngine<TValue>(mode)) {}

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

private:
    std::unique_ptr<IStorageEngine<TValue>> m_engine;
    mutable MutexPolicy m_mutex;
    std::vector<IDatasetObserver<TValue>*> m_observers;
};
```

Gebruik vanuit C++:
```cpp
// Gesorteerd (default)
DatasetStore<std::wstring> ordered;

// Ongesorteerd (runtime keuze)
DatasetStore<std::wstring> fast(StorageMode::Unordered);
```

- Alle notificaties worden **buiten de lock** afgevuurd (deadlock-free, net als bij SharedValue)
- `FetchKeys` maakt een slice-kopie — geen iteratoren die naar buiten lekken
- `AccessInPlace` pakt de lock, roept de callback op, release de lock, en notificeert observers

---

### Component 3: Centralized Error Handling — Drie-Lagen Architectuur

Een generiek, robuust error-systeem dat fouten vanuit de diepste C++ laag automatisch doorborrelt naar VBScript `Err.Description` en C# `COMException.Message`.

> [!IMPORTANT]
> Dit component is de **ruggengraat van alle foutafhandeling** in het hele project. Elke COM-methode wordt automatisch gewrapt zodat je als ontwikkelaar nooit handmatig HRESULT-codes hoeft te beheren.

#### [NEW] [SharedValueV2/include/Errors.hpp](file:///c:/csharp/cursor_com_test/SharedValueV2/include/Errors.hpp)

C++ exception hiërarchie — de bron van alle foutinformatie:

```cpp
namespace SharedValueV2 {

// Centraal enum met alle foutcodes
enum class ErrorCode : uint16_t {
    // Dataset fouten
    KeyNotFound         = 100,
    DuplicateKey        = 101,
    StoreModeNotEmpty   = 102,
    InvalidStorageMode  = 103,
    
    // SharedValue fouten
    LockTimeout         = 200,
    NullPointer         = 201,
    
    // Algemeen
    InvalidArgument     = 300,
    OutOfRange          = 301,
    InternalError       = 999
};

// Base exception class — alle SharedValueV2 fouten erven hiervan
class SharedValueException : public std::runtime_error {
public:
    ErrorCode code;
    
    SharedValueException(ErrorCode c, const std::string& msg)
        : std::runtime_error(msg), code(c) {}
};

// Concrete uitzonderingen voor leesbare code
class KeyNotFoundException : public SharedValueException {
public:
    KeyNotFoundException(const std::wstring& key)
        : SharedValueException(ErrorCode::KeyNotFound,
          "Key '" + narrow(key) + "' not found in dataset") {}
};

class DuplicateKeyException : public SharedValueException {
public:
    DuplicateKeyException(const std::wstring& key)
        : SharedValueException(ErrorCode::DuplicateKey,
          "Key '" + narrow(key) + "' already exists") {}
};

class StoreModeException : public SharedValueException {
public:
    StoreModeException()
        : SharedValueException(ErrorCode::StoreModeNotEmpty,
          "Cannot change storage mode: dataset is not empty") {}
};

} // namespace SharedValueV2
```

- Alle methods in `DatasetStore` en `SharedValue` gooien **typed exceptions** met een `ErrorCode` en een leesbaar bericht
- Geen HRESULT of COM-afhankelijkheid in de C++ core

#### [NEW] [ComErrorHelper.h](file:///c:/csharp/cursor_com_test/ComErrorHelper.h)

De COM-bridge vertaallaag. Twee cruciale onderdelen:

**1. `COM_METHOD_BODY` macro** — Wrapt élke COM-methode automatisch:

```cpp
#define COM_METHOD_BODY(clsid, iid, ...) \
    try { \
        __VA_ARGS__; \
        return S_OK; \
    } catch (const SharedValueV2::SharedValueException& e) { \
        return AtlReportError(clsid, CA2W(e.what()), iid, \
            MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, (WORD)e.code)); \
    } catch (const std::out_of_range& e) { \
        return AtlReportError(clsid, CA2W(e.what()), iid, E_BOUNDS); \
    } catch (const std::invalid_argument& e) { \
        return AtlReportError(clsid, CA2W(e.what()), iid, E_INVALIDARG); \
    } catch (const std::exception& e) { \
        return AtlReportError(clsid, CA2W(e.what()), iid, E_FAIL); \
    } catch (...) { \
        return AtlReportError(clsid, L"Unknown internal error", iid, E_UNEXPECTED); \
    }
```

**2. `ISupportErrorInfo`** — Elke COM-class implementeert dit zodat clients de rijke foutbeschrijving ontvangen.

Gebruik in een COM-methode:
```cpp
STDMETHODIMP CDatasetProxy::GetRowData(BSTR key, BSTR* data) {
    if (!data) return E_POINTER;
    COM_METHOD_BODY(CLSID_DatasetProxy, IID_IDatasetProxy,
        auto result = m_store.GetRow(std::wstring(key));
        *data = CComBSTR(result->c_str()).Detach()
    )
}
```

VBScript ziet dan automatisch:
```vbs
On Error Resume Next
result = proxy.GetRowData("onbestaandeKey")
If Err.Number <> 0 Then
    WScript.Echo "Fout: " & Err.Description
    ' Output: "Fout: Key 'onbestaandeKey' not found in dataset"
End If
```

C# ziet:
```csharp
try { proxy.GetRowData("onbestaandeKey"); }
catch (COMException ex) {
    Console.WriteLine(ex.Message);
    // Output: "Key 'onbestaandeKey' not found in dataset"
}
```

#### [MODIFY] [SharedValue.h](file:///c:/csharp/cursor_com_test/SharedValue.h)
Toevoegen van `ISupportErrorInfo` aan de COM_MAP. Include `ComErrorHelper.h`.

#### [MODIFY] [SharedValue.cpp](file:///c:/csharp/cursor_com_test/SharedValue.cpp)
Alle methode-bodies wrappen met `COM_METHOD_BODY`.

---

### Component 4: Nieuwe COM Interfaces — `IDatasetProxy`

Het COM-interface dat de C++ DatasetStore blootstelt aan de buitenwereld. Omdat het via `IDispatch` werkt, kan **elke** COM-client (VBS, C#, PowerShell, Python, JScript) dit direct aanspreken.

> [!IMPORTANT]
> **Bidirectioneel by design:** VBScript kan `CreateObject("ATLProjectcomserver.DatasetProxy")` aanroepen, rijen toevoegen, en het object vervolgens via `SharedValue.SetValue(proxy)` parkeren. Een C# applicatie kan dan `SharedValue.GetValue()` aanroepen en dezelfde proxy uitlezen, muteren, of aanvullen.

#### [MODIFY] [ATLProjectcomserver.idl](file:///c:/csharp/cursor_com_test/ATLProjectcomserver.idl)

Toevoegen van het `IDatasetProxy` interface en `DatasetProxy` coclass. De `SetStorageMode` methode maakt runtime-keuze mogelijk:

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
    // --- Storage Mode (MOET als eerste aangeroepen worden, vóór data) ---
    // 0 = Ordered (default), 1 = Unordered
    [id(1)] HRESULT SetStorageMode([in] LONG mode);
    [id(2)] HRESULT GetStorageMode([out, retval] LONG* mode);

    // --- CRUD ---
    [id(10)] HRESULT AddRow([in] BSTR key, [in] BSTR value);
    [id(11)] HRESULT GetRowData([in] BSTR key, [out, retval] BSTR* data);
    [id(12)] HRESULT UpdateRow([in] BSTR key, [in] BSTR value,
                               [out, retval] VARIANT_BOOL* success);
    [id(13)] HRESULT RemoveRow([in] BSTR key, [out, retval] VARIANT_BOOL* success);
    [id(14)] HRESULT Clear();

    // --- Query ---
    [id(20)] HRESULT GetRecordCount([out, retval] LONG* count);
    [id(21)] HRESULT HasKey([in] BSTR key, [out, retval] VARIANT_BOOL* exists);

    // --- Paginering ---
    [id(30)] HRESULT FetchPageKeys([in] LONG startIndex, [in] LONG limit,
                                   [out, retval] VARIANT* keys);
    [id(31)] HRESULT FetchPageData([in] LONG startIndex, [in] LONG limit,
                                   [out, retval] VARIANT* keysAndValues);
};

coclass DatasetProxy
{
    [default] interface IDatasetProxy;
};
```

Methoden explainer:
- `SetStorageMode(0)` = Ordered (gesorteerd, standaard). `SetStorageMode(1)` = Unordered (hash-based, sneller)
- `SetStorageMode` mag alleen worden aangeroepen als de dataset leeg is (retourneert `E_FAIL` als er data in zit)
- `FetchPageKeys` → retourneert `VT_ARRAY|VT_BSTR` (string array)
- `FetchPageData` → retourneert `VT_ARRAY|VT_VARIANT` (2D array: key-value paren)
- Alle strings als `BSTR` → universeel leesbaar in VBS, C#, PowerShell, Python

Gebruik vanuit VBScript:
```vbs
Set proxy = CreateObject("ATLProjectcomserver.DatasetProxy")
proxy.SetStorageMode 1  ' Unordered = snelle hashing
proxy.AddRow "key1", "value1"
```

Gebruik vanuit C#:
```csharp
dynamic proxy = Activator.CreateInstance(Type.GetTypeFromProgID("ATLProjectcomserver.DatasetProxy"));
proxy.SetStorageMode(1); // Unordered
proxy.AddRow("key1", "value1");
```

#### [NEW] [DatasetProxy.h](file:///c:/csharp/cursor_com_test/DatasetProxy.h)
ATL class `CDatasetProxy` die `IDatasetProxy` implementeert. Intern bezit deze een `SharedValueV2::DatasetStore<std::wstring>` instantie. In `FinalConstruct` wordt default `StorageMode::Ordered` geconfigureerd. `SetStorageMode` herbouwt de engine als de store leeg is.

#### [NEW] [DatasetProxy.cpp](file:///c:/csharp/cursor_com_test/DatasetProxy.cpp)
Volledige implementatie. `FetchPageKeys` bouwt een COM `SAFEARRAY` op vanuit de C++ vector. `FetchPageData` bouwt een 2D SAFEARRAY. `SetStorageMode` valideert dat de store leeg is voordat hij de engine herinstantiëert.

#### [NEW] [DatasetProxy.rgs](file:///c:/csharp/cursor_com_test/DatasetProxy.rgs)
Registry script: ProgID = `ATLProjectcomserver.DatasetProxy`

#### [MODIFY] [resource.h](file:///c:/csharp/cursor_com_test/resource.h)
Toevoegen: `#define IDR_DATASETPROXY 204`

#### [MODIFY] [ATLProjectcomserver.vcxproj](file:///c:/csharp/cursor_com_test/ATLProjectcomserver.vcxproj)
Toevoegen van `DatasetProxy.cpp/.h/.rgs` aan ClCompile/ClInclude/None ItemGroups.

---

### Component 5: C++ Unit & Stress Tests

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
| `TestDatasetOrderedEngine` | Verifieer gesorteerde key-volgorde met `StorageMode::Ordered` |
| `TestDatasetUnorderedEngine` | Verifieer werking met `StorageMode::Unordered` |
| `TestDatasetRuntimeSwitch` | Engine wisselen op lege store, falen op volle store |
| `TestErrorKeyNotFound` | `GetRow` op niet-bestaande key gooit `KeyNotFoundException` |
| `TestErrorDuplicateKey` | `AddRow` met bestaande key gooit `DuplicateKeyException` |
| `TestErrorStoreModeNotEmpty` | `SetStorageMode` op gevulde store gooit `StoreModeException` |

#### [MODIFY] [SharedValueV2/tests/StressTest.cpp](file:///c:/csharp/cursor_com_test/SharedValueV2/tests/StressTest.cpp)

Nieuwe `runDatasetStressTest()`:
- 50 threads: gelijktijdig `AddRow`, `RemoveRow`, `UpdateRow`, `FetchKeys`
- Validator-thread: controleert data-integriteit na elke ronde
- Draait twee keer: eerst met `StorageMode::Ordered`, daarna met `StorageMode::Unordered`

---

### Component 6: C# Interop Tests — Twee Projecten

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

4. **Error Handling Test:**
   - `GetRowData` op niet-bestaande key → verwacht `COMException` met description
   - `SetStorageMode` op gevulde store → verwacht `COMException`
   - Assert dat `ex.Message` de juiste foutbeschrijving bevat

---

### Component 7: VBScript Interop Tests

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

#### [NEW] `tests/VBScriptTest/TestErrorHandling.vbs`

VBScript test die controleert of fouten correct doorborrelen met `Err.Description`:

```vbs
On Error Resume Next
Set proxy = CreateObject("ATLProjectcomserver.DatasetProxy")

' Test 1: Niet-bestaande key ophalen
result = proxy.GetRowData("onbestaandeKey")
If Err.Number <> 0 Then
    WScript.Echo "TEST 1 PASS: " & Err.Description
    Err.Clear
Else
    WScript.Echo "TEST 1 FAIL: Geen fout bij onbestaande key"
End If

' Test 2: StorageMode wijzigen terwijl er data in zit
proxy.AddRow "key1", "value1"
proxy.SetStorageMode 1
If Err.Number <> 0 Then
    WScript.Echo "TEST 2 PASS: " & Err.Description
    Err.Clear
Else
    WScript.Echo "TEST 2 FAIL: StorageMode switch had moeten falen"
End If

WScript.Echo "Error handling tests voltooid."
```

---

### Component 8: Documentatie Updates

#### [MODIFY] [README.md](file:///c:/csharp/cursor_com_test/README.md)
- DatasetProxy sectie met bidirectionele flow diagram
- Links naar testprojecten en VBScript voorbeelden
- Nieuwe ProgID's tabel

#### [MODIFY] [ARCHITECTURE.md](file:///c:/csharp/cursor_com_test/ARCHITECTURE.md)
- DatasetStore architectuur met runtime Strategy Pattern
- IStorageEngine interface + OrderedStorageEngine vs UnorderedStorageEngine uitleg
- Drie-lagen Error Handling architectuur diagram
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
| 9 | `cscript tests\VBScriptTest\TestErrorHandling.vbs` | Alle "TEST PASS" berichten, foutbeschrijvingen correct |

### Manual Verification
- Oude rommelbestanden zijn verwijderd
- Visuele inspectie VBScript output
- Controleer dat DatasetProxy werkt vanuit PowerShell: `$p = New-Object -ComObject ATLProjectcomserver.DatasetProxy`
- Controleer dat PowerShell foutbeschrijvingen ontvangt bij foute aanroepen
