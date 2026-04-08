# Technische Architectuur: SharedValue & COM Interop

Deze applicatie bestaat uit twee losgekoppelde conceptuele lagen: een moderne **C++ Engine** (`SharedValueV2`) en daarbovenop een **COM Wrapper Layer**.

Door dit onderscheid in lagen kan de superieure deadlock-vrije code vanuit C++ zonder de baggage van ATL en COM overgedragen en hergebruikt worden, terwijl de frontwaartse COM interface voor interoperabiliteit met software zoals C# / .NET / Python bewaard blijft.

## 1. De Backend/C++ Engine (SharedValueV2)
Gevestigd in een header-only framework. Omdat data (state) intensief benaderd wordt, kent dit model de volgende **Design Patterns**:

### Monitor Pattern
Het simpelweg vasthangen van locks levert gevaar op dat men soms een variabele wegschrijft *buiten* de context van een Mutex. *SharedValueV2* combineert de data opslag en de synchronisatie mechanismen strak gekoppeld. Omdat de state intern (`private`) geresolveerd is in de C++ class, kan deze louter in speciale functiegelijke "scope-blocks" gediend worden waaraan direct het slot (`std::lock_guard`) gelieerd wordt. Resource leaks worden hierdoor ge-elimineerd.

### Observer Pattern (Ontkoppeld / Deadlock Free)
Opgeworpen `Events` hebben historisch gezien COM servers vaak laten vastlopen omdat de pointer van de *Client* crashte terwijl de list geresolveerd werd, of de actie te langzaam was. In SharedValueV2 is the Observer Pattern deadlock-vrij gemaakt door the "Notify" lijst uit de exclusieve Critical Section te halen.

Er wordt eerst lokaal the vector gecopieerd terweil de class lock-vastgeklampt is. Hierna wordt de class unlock vrijgelaten en gaat the thread over de the _gekopieerde list_ loopen om veilige callbacks (naar de C#-wereld bijvoorbeeld) over te mikken. Geen dode server meer mocht observer traag lopen!

### Policy-Based Design (Strategy Pattern)
Omdat de behoefte van in-process sneller en anders opereer dan een globale of cross-process (system-breed) Mutex, slikt the `SharedValue` header the abstracte LockPolicy als C++ Type (`<T, Policy>`). Meegeleverd komen `LocalMutexPolicy`, en `NamedSystemMutexPolicy` in Windows.
We passen zo Dependency Inversion Principle toe en reduceren overhead voor simpele threads naat `NullMutexPolicy` (Zero-Overhead).

## 2. De COM Wrapper Layer

De klassieke ATL/COM server implementatie (te vinden in de root directory) doet nu niets meer anders dan communiceren en vertalen:
- `SharedValue.cpp/h` bezit een implementatie van de moderne `SharedValueV2::SharedValue`-klasse en praat daar on-the-fly tegenop.
- ATL types (De objecten van COM, bv `VARIANT`, `CComVariant`, `BSTR`) worden binnen in the *SharedValueV2* doorgeladen en gediend richting de client COM-handlers.
- *Event mapping:* The `CSharedValue` C++ class subscribe zelf op the local *SharedValueV2* kernel interface `ISharedValueObserver` en converteert die raw-data om naar COM-compatibele SafeArrays en VARIANTs aanroepjes en vuurt dit door naar alle COM-clients (ISharedValueCallback) door een eigen wrapper-lock `m_csCallbacks`.
