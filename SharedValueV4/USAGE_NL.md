# SharedValueV4 — Praktijkvoorbeelden & Usages

Dit document maakt de abstracte concepten van de **SharedValueV4 Dual-Channel Engine** direct tastbaar in echte, real-world scenario's. Omdat het systeem gebaseerd is op universele Windows Kernel Objecten (`CreateFileMapping`, `CreateMutex`), en universele serialisatie (`Google FlatBuffers`), is het **taal-agnostisch**.

Hier zie je drie veelvoorkomende real-world architecturen en de bijbehorende pseudocode implementaties over diverse talen heen.

---

## Scenario 1: De HFT (High-Frequency Trading) Setup — C++ stuur, C# GUI reageert

**Concept:** 
Een C++ applicatie is zwaar geoptimaliseerd om beurskoersen in microseconden binnen te hengelen (HFT). C++ mag absoluut niet vertragen door UI-rendering te verzorgen. Aan de andere kant draait een gelikte, trage WPF/Avalonia C# app die grafieken toont. Zodra een trader in de C# app klikt op "KLEP_DICHT", moet C++ dit onmiddellijk in real-time verwerken.

### Stap 1: C++ Initialiseert de Dual-Channel en Pompt Acties (Host)

C++ treedt op als `Host` (`isPrimaryHost = true`). Hij maakt de kanalen aan en zendt continu data, maar luistert óók op de achtergrond.

```cpp
// main.cpp (C++)
#include "SharedValueEngine.hpp"

// Callback: Ontvang opdrachten vanuit de C# UI
void OnTraderCommand(const uint8_t* pData, size_t size) {
    auto command = GetTradeCommand(pData); // FlatBuffer unpack (zero-copy)
    if (strcmp(command->action()->c_str(), "KLEP_DICHT") == 0) {
        TradingEngine::EmergencyHalt(command->portfolio_id());
    }
}

int main() {
    // 1. Initialiseer kanalen
    SharedValueEngine engineC2P(L"Trading_C2P", true); // Consumer-to-Producer (C# to C++)
    SharedValueEngine engineP2C(L"Trading_P2C", true); // Producer-to-Consumer (C++ to C#)

    // 2. Registreer luisteraar in de achtergrond 
    engineC2P.StartListening(&OnTraderCommand);

    while (tradingIsActive) {
        auto koersenArray = TradingEngine::ExtractLatestTickers();
        
        flatbuffers::FlatBufferBuilder builder;
        // ... (Flatbuffer creatie logica ...)
        
        // 3. Stuur bulkdata in ~15 nanoseconden!
        engineP2C.WriteData(builder.GetBufferPointer(), builder.GetSize());
        
        std::this_thread::sleep_for(std::chrono::microseconds(50)); // Hypersnelle loop
    }
}
```

### Stap 2: C# UI Luistert en Voert Commando's uit (Client)

C# is de 'Client' (`isPrimaryHost = false`). Hij start en blijft wachten zonder CPU overhead (slaapt) tot C++ gereed is, en leest daarna de tick-prijzen.

```csharp
// ViewModel.cs (C# WPF)
using SharedMemMap;

public class TradingViewModel 
{
    private SharedValueEngine _engineP2C;
    private SharedValueEngine _engineC2P;

    public void ConnectToEngine()
    {
        // false: C# wacht (zonder crashes) tot C++ het netwerk optuigt
        _engineP2C = new SharedValueEngine("Trading_P2C", false); 
        _engineC2P = new SharedValueEngine("Trading_C2P", false); 

        // Als er HFT data binnenkomt, trigger UI hook
        _engineP2C.OnDataReady += (koersData) => {
            UpdateGrafiekLokaal(koersData);
        };
        _engineP2C.StartListening();
    }

    public void OnNoodstopKlik(int portfolioId)
    {
        // Bouw FlatBuffer commando en zend razendsnel terug!
        var builder = new FlatBufferBuilder(1024);
        var actieStr = builder.CreateString("KLEP_DICHT");
        // ...
        _engineC2P.WriteData(builder.SizedByteArray()); 
    }
}
```

---

## Scenario 2: De "AI Interop" — Python Machine Learning Model communiceert met C#

Veel AI/Python scripts (`PyTorch`, `TensorFlow`) zijn zwaar gebonden aan Python. Om C# met AI te laten spreken, kun je V4 implementeren via de Python `mmap` module en `cffi` / `ctypes` bindings met FlatBuffers.

**Concept:** 
C# verwerkt real-time camerabeelden als pixelarrays. Python pikt dit razendsnel op uit gedeeld geheugen, voert een Object Detectie analyse (YOLO) uit, en plaatst de bounding-boxes terug in het gedeelde geheugen.

### C# pompt beelden in RAM (Host)

C# neemt nu de regie!

```csharp
// ComputerVisionManager.cs (C#)
var engineP2C = new SharedValueEngine("CameraFrame_P2C", true); // C# = Host
var engineC2P = new SharedValueEngine("Detection_C2P", true);

// Wacht op AI objecten uit Python
engineC2P.OnDataReady += (aiData) => {
    DrawBoundingBoxesOnScreen(aiData.DetectedObjects);
};
engineC2P.StartListening();

// Loop en pomp webcam logica
while(camera.IsRunning) {
    byte[] rgbFrame = camera.GetRawFrameBytes();
    
    // Converteer naar Flatbuffer Bytes en vuur
    engineP2C.WriteData(rgbFrameFlatbuffer); 
}
```

### Python slokt frames op en vuurt AI (Client)

Python heeft geen C# backend nodig. We roepen pure Windows APIs aan:

```python
# yolo_engine.py
import mmap
import ctypes
from flatbuffers import Builder
import dataset.SharedDataset as SharedDataset # FlatC auto-gen voor Python!

kernel32 = ctypes.windll.kernel32

# 1. Native toegang tot de MMF (Identiek aan C++ of C#)
buf_size = 10 * 1024 * 1024
shmem_p2c = mmap.mmap(-1, buf_size, tagname="Global\\CameraFrame_P2C_Map", access=mmap.ACCESS_READ)
shmem_c2p = mmap.mmap(-1, buf_size, tagname="Global\\Detection_C2P_Map", access=mmap.ACCESS_WRITE)

# 2. Open de Mutex en Events via kernel
mutex_p2c = kernel32.OpenMutexW(0x00100000, False, "Global\\CameraFrame_P2C_Mutex")
event_p2c = kernel32.OpenEventW(0x00100000, False, "Global\\CameraFrame_P2C_Event")

while True:
    # 3. Luister op exact 0% CPU!
    kernel32.WaitForSingleObject(event_p2c, 0xFFFFFFFF) 
    
    kernel32.WaitForSingleObject(mutex_p2c, 5000) # Lock Mutex
    
    # 4. Pak payload grootte (8 bytes = size_t uint64) en data
    # (Pseudocode mmap parsing...)
    size_bytes = shmem_p2c[:8] 
    data = shmem_p2c[8:8+length]
    
    kernel32.ReleaseMutex(mutex_p2c)
    
    # 5. Geef data aan YOLO engine
    image_bytes = GetFlatBufferPayload(data)
    boxes = model.predict(image_bytes)
    
    # 6. Bouw FlatBuffer en schrijf naar shmem_c2p...
```
*(FlatBuffers voorziet volledig out-of-the-box Python codegen via `flatc --python`!)*

---

## Scenario 3: Modulaire Games C++ met Rust en C# Plugins

De V4 architectuur is uitermate geschikt voor plugin systemen waarbij een main Game Engine is geschreven in C++, maar ontwikkelaars mods/plugins kunnen schrijven in `Rust` of `Python` buiten de scope van de applicatie.

Omdat de interface enkel en alleen leunt op de `<naam>_Map`, `<naam>_Mutex` en `<naam>_Event` strings en een `dataset.fbs` schema, kan *elk* programma (in elke geprogrammeerde taal!) naadloos als Plugin functioneren, zolang ze die regels volgen. Ze hoeven niet dezelfde memory heap of dll injection methodes te delen (Out-of-Process Plugins, extreem crash-veilig).

Dit maakt SharedValueV4 de perfecte **Agonistische IPC Communicatie Bus**.

---

## Scenario 4: Legacy Enterprise integratie — VBScript (COM Wrapper)

**Concept:** 
Binnen oude bedrijfsnetwerken draaien nog talloze legacy `VBScript` of `VBA` (Excel) applicaties. Omdat VBScript *geen* native toegang heeft tot Windows Kernel Objecten of FlatBuffers parsing, kunnen dergelijke talen niet direct praten via Memory-Mapped Files. Om dit toch te koppelen bouwen we een flinterdunne **COM-Interop wrapper via C#**.

1. C# (via C# Class Library) handelt de bliksemsnelle MMF/FlatBuffer communicatie af.
2. We exporteren de C# DLL als een "COM Visible" systeem en registreren deze in Windows (`regasm`).
3. VBScript creëert een instantie van dit COM object en haalt de data op alsof het een simpele string is.

### 4.1 De C# COM-Interop Wrapper (Bruggenbouwer)

Dit is de uitgebreide C# class die fungeert als vertaler. Hij vangt MMF updates af, en exposeert rijke methodes aan VBS/VBA. Let op: de wrapper vertaalt complexe FlatBuffer-objecten naar simpele COM-types (`string`, `int`, `bool`) die VBScript begrijpt.

```csharp
// SharedValueV4ComWrapper.cs (C# Class Library - COM Visible)
using System;
using System.Runtime.InteropServices;
using SharedMemMap;

[ComVisible(true)]
[Guid("12345678-ABCD-1234-ABCD-1234567890AB")]
[ProgId("SharedValueV4.Wrapper")]
[ClassInterface(ClassInterfaceType.AutoDual)]
public class V4Wrapper : IDisposable
{
    private SharedValueEngine _engineP2C;
    private SharedValueEngine _engineC2P;
    private SharedDataset _latestDataset;
    private string _lastError = "";
    private bool _isConnected = false;
    private DateTime _lastUpdateTime = DateTime.MinValue;

    /// <summary>Verbind met de SharedValueV4 engine (P2C kanaal)</summary>
    public bool Connect(string channelName)
    {
        try
        {
            _engineP2C = new SharedValueEngine(channelName + "_P2C", false);
            _engineP2C.OnDataReady += (dataset) => {
                _latestDataset = dataset;
                _lastUpdateTime = DateTime.Now;
            };
            _engineP2C.StartListening();
            _isConnected = true;
            return true;
        }
        catch (Exception ex)
        {
            _lastError = ex.Message;
            return false;
        }
    }

    /// <summary>Verbind ook het C2P kanaal voor bidirectionele communicatie</summary>
    public bool ConnectBidirectional(string channelName)
    {
        if (!Connect(channelName)) return false;
        try
        {
            _engineC2P = new SharedValueEngine(channelName + "_C2P", false);
            return true;
        }
        catch (Exception ex)
        {
            _lastError = ex.Message;
            return false;
        }
    }

    /// <summary>Haal de meest recente API versie op</summary>
    public string GetApiVersion()
    {
        return _latestDataset?.ApiVersion ?? "Geen data";
    }

    /// <summary>Haal het tijdstip van de laatste update op</summary>
    public string GetLastUpdatedUtc()
    {
        return _latestDataset?.LastUpdatedUtc ?? "Geen data";
    }

    /// <summary>Haal het aantal rijen op in de huidige dataset</summary>
    public int GetRowCount()
    {
        return _latestDataset?.RowsLength ?? 0;
    }

    /// <summary>Haal een specifieke rij-ID op (0-based index)</summary>
    public string GetRowId(int index)
    {
        if (_latestDataset == null || index >= _latestDataset.RowsLength)
            return "Index buiten bereik";
        return _latestDataset.Rows(index)?.RowId ?? "";
    }

    /// <summary>Controleer of een rij actief is</summary>
    public bool GetRowIsActive(int index)
    {
        if (_latestDataset == null || index >= _latestDataset.RowsLength)
            return false;
        return _latestDataset.Rows(index)?.IsActive ?? false;
    }

    /// <summary>Haal temperatuur op van een geneste detail</summary>
    public double GetTemperature(int rowIndex, int detailIndex)
    {
        var row = _latestDataset?.Rows(rowIndex);
        if (row == null) return -999.0;
        var detail = row?.Details(detailIndex);
        return detail?.Temperature ?? -999.0;
    }

    /// <summary>Haal vochtigheid op van een geneste detail</summary>
    public double GetHumidity(int rowIndex, int detailIndex)
    {
        var row = _latestDataset?.Rows(rowIndex);
        if (row == null) return -999.0;
        var detail = row?.Details(detailIndex);
        return detail?.Humidity ?? -999.0;
    }

    /// <summary>Geeft alle data als platte CSV-string (voor eenvoudige VBS verwerking)</summary>
    public string GetAllDataAsCsv()
    {
        if (_latestDataset == null) return "";
        var lines = new System.Text.StringBuilder();
        lines.AppendLine("RowId;IsActive;Temperature;Humidity;StatusCode");
        for (int r = 0; r < _latestDataset.RowsLength; r++)
        {
            var row = _latestDataset.Rows(r);
            for (int d = 0; d < row?.DetailsLength; d++)
            {
                var det = row?.Details(d);
                lines.AppendLine($"{row?.RowId};{row?.IsActive};{det?.Temperature};{det?.Humidity};{det?.StatusCode}");
            }
        }
        return lines.ToString();
    }

    /// <summary>Stuur een tekst-commando terug naar de producer (via C2P kanaal)</summary>
    public bool SendCommand(string commandText)
    {
        if (_engineC2P == null) { _lastError = "C2P kanaal niet verbonden"; return false; }
        try
        {
            var builder = new Google.FlatBuffers.FlatBufferBuilder(1024);
            // Bouw een simpel FlatBuffer commando...
            var cmdOffset = builder.CreateString(commandText);
            // (FlatBuffer encoding logica...)
            _engineC2P.WriteData(builder.SizedByteArray());
            return true;
        }
        catch (Exception ex) { _lastError = ex.Message; return false; }
    }

    /// <summary>Geeft de laatste foutmelding terug</summary>
    public string GetLastError() => _lastError;

    /// <summary>Controleert of de engine verbonden is</summary>
    public bool IsConnected() => _isConnected;

    /// <summary>Controleert of er verse data beschikbaar is (binnen de afgelopen N seconden)</summary>
    public bool HasFreshData(int maxAgeSeconds)
    {
        return (DateTime.Now - _lastUpdateTime).TotalSeconds <= maxAgeSeconds;
    }

    public void Dispose()
    {
        _engineP2C?.Dispose();
        _engineC2P?.Dispose();
    }
}
```

**Registratie in Windows (eenmalig als admin):**
```powershell
# Compileer de COM wrapper als DLL
dotnet build -c Release SharedValueV4ComWrapper.csproj

# Registreer voor 64-bit COM clients (inclusief cscript.exe 64-bit)
C:\Windows\Microsoft.NET\Framework64\v4.0.30319\RegAsm.exe /codebase SharedValueV4ComWrapper.dll
```

---

### 4.2 VBS Voorbeeld 1: Basis Polling — Live Data Ophalen

Het simpelste gebruik: verbind en poll elke seconde de meest recente sensordata.

```vbscript
' voorbeeld_basis_polling.vbs
' ============================
' Simpelste VBScript client voor SharedValueV4.
' Haalt elke seconde de nieuwste data op uit het gedeelde geheugen.
'
' Gebruik: cscript //nologo voorbeeld_basis_polling.vbs

Option Explicit

Dim objEngine, strData, intRows, i

' 1. Creëer het COM-object 
Set objEngine = CreateObject("SharedValueV4.Wrapper")

' 2. Verbind met het kanaal (de naam moet matchen met de C++ producer)
If Not objEngine.Connect("SensorData") Then
    WScript.Echo "FOUT: Kan niet verbinden - " & objEngine.GetLastError()
    WScript.Quit 1
End If

WScript.Echo "=== SharedValueV4 VBScript Monitor ==="
WScript.Echo "Verbonden! Luisteren naar live sensordata..."
WScript.Echo String(50, "=")

' 3. Poll-loop: de C# wrapper ontvangt MMF-data op de achtergrond,
'    wij halen simpelweg de meest recente waarden op.
Do While True
    ' Wacht even tot eerste data er is
    If objEngine.HasFreshData(5) Then
        intRows = objEngine.GetRowCount()
        
        WScript.Echo ""
        WScript.Echo "[" & Now() & "] API Versie: " & objEngine.GetApiVersion()
        WScript.Echo "  Laatste update : " & objEngine.GetLastUpdatedUtc()
        WScript.Echo "  Aantal rijen   : " & intRows
        
        ' Loop door elke rij
        For i = 0 To intRows - 1
            WScript.Echo "  Rij " & i & ": " & objEngine.GetRowId(i) & _
                         " | Actief: " & objEngine.GetRowIsActive(i) & _
                         " | Temp: " & FormatNumber(objEngine.GetTemperature(i, 0), 2) & " °C" & _
                         " | Vocht: " & FormatNumber(objEngine.GetHumidity(i, 0), 1) & " %"
        Next
    Else
        WScript.Echo "[" & Now() & "] Wachten op data van C++ producer..."
    End If
    
    WScript.Sleep 1000
Loop
```

---

### 4.3 VBS Voorbeeld 2: Event-Driven Logboek naar Bestand

Dit script schrijft continu sensordata weg naar een CSV-logbestand. Ideaal voor onbeheerde servers die 24/7 draaien.

```vbscript
' voorbeeld_csv_logger.vbs
' =========================
' Schrijft SharedValueV4 data weg naar een CSV-logbestand.
' Geschikt voor onbeheerde Windows servers of scheduled tasks.
'
' Gebruik: cscript //nologo voorbeeld_csv_logger.vbs

Option Explicit

Const LOG_FILE       = "C:\Logs\SharedValueV4\sensorlog.csv"
Const POLL_INTERVAL  = 2000  ' milliseconden
Const MAX_LOG_LINES  = 100000

Dim objEngine, objFSO, objFile
Dim strCsvData, strPrevData, intLineCount

' --- Initialisatie ---
Set objFSO = CreateObject("Scripting.FileSystemObject")

' Maak log-directory aan als die niet bestaat
Dim strLogDir
strLogDir = objFSO.GetParentFolderName(LOG_FILE)
If Not objFSO.FolderExists(strLogDir) Then
    objFSO.CreateFolder strLogDir
End If

' Open of creëer CSV-bestand (append mode = 8)
If objFSO.FileExists(LOG_FILE) Then
    Set objFile = objFSO.OpenTextFile(LOG_FILE, 8) ' ForAppending
    intLineCount = 0 ' We tellen niet bij bestaand bestand
Else
    Set objFile = objFSO.CreateTextFile(LOG_FILE, True)
    objFile.WriteLine "Timestamp;RowId;IsActive;Temperature;Humidity;StatusCode"
    intLineCount = 1
End If

' --- Verbind met SharedValueV4 ---
Set objEngine = CreateObject("SharedValueV4.Wrapper")

If Not objEngine.Connect("ProductieLijn") Then
    objFile.WriteLine Now() & ";ERROR;Kan niet verbinden;" & objEngine.GetLastError()
    objFile.Close
    WScript.Quit 1
End If

WScript.Echo "CSV Logger gestart. Schrijven naar: " & LOG_FILE
strPrevData = ""

' --- Hoofd-loop ---
Do While True
    If objEngine.HasFreshData(10) Then
        ' Haal alle data op als CSV
        strCsvData = objEngine.GetAllDataAsCsv()
        
        ' Schrijf alleen als de data gewijzigd is (voorkom duplicaten)
        If strCsvData <> strPrevData And Len(strCsvData) > 0 Then
            ' Voeg timestamp toe aan elke regel
            Dim arrLines, strLine
            arrLines = Split(strCsvData, vbCrLf)
            
            Dim j
            For j = 1 To UBound(arrLines) ' Skip header (index 0)
                If Len(Trim(arrLines(j))) > 0 Then
                    objFile.WriteLine Now() & ";" & arrLines(j)
                    intLineCount = intLineCount + 1
                End If
            Next
            
            strPrevData = strCsvData
            
            ' Log-rotatie: als bestand te groot wordt, begin opnieuw
            If intLineCount > MAX_LOG_LINES Then
                objFile.Close
                objFSO.CopyFile LOG_FILE, Replace(LOG_FILE, ".csv", "_" & _
                    Year(Now()) & Right("0" & Month(Now()), 2) & _
                    Right("0" & Day(Now()), 2) & ".csv")
                Set objFile = objFSO.CreateTextFile(LOG_FILE, True)
                objFile.WriteLine "Timestamp;RowId;IsActive;Temperature;Humidity;StatusCode"
                intLineCount = 1
                WScript.Echo "[" & Now() & "] Log geroteerd."
            End If
        End If
    End If
    
    WScript.Sleep POLL_INTERVAL
Loop
```

---

### 4.4 VBS Voorbeeld 3: Bidirectioneel — Commando's Terugsturen

Dit script leest data, analyseert waarden, en stuurt **automatisch commando's terug** naar de C++ producer via het C2P-kanaal (bijv. nood-afsluit bij overschrijding drempelwaarden).

```vbscript
' voorbeeld_bidirectioneel.vbs
' =============================
' Leest sensordata en stuurt automatisch commando's terug naar de C++ producer
' als drempelwaarden worden overschreden.
'
' Gebruik: cscript //nologo voorbeeld_bidirectioneel.vbs

Option Explicit

Const TEMP_DREMPEL_HOOG   = 85.0   ' Celsius - noodstop
Const TEMP_DREMPEL_WARN   = 70.0   ' Celsius - waarschuwing
Const VOCHT_DREMPEL_LAAG  = 20.0   ' Procent - te droog
Const CHECK_INTERVAL      = 500    ' milliseconden (2x per seconde)

Dim objEngine, boolNoodstopVerstuurd

' --- Verbind bidirectioneel ---
Set objEngine = CreateObject("SharedValueV4.Wrapper")

If Not objEngine.ConnectBidirectional("FabrieksSensoren") Then
    WScript.Echo "FATAAL: " & objEngine.GetLastError()
    WScript.Quit 1
End If

WScript.Echo "=== Bidirectionele Monitor ==="
WScript.Echo "Drempels: NOODSTOP > " & TEMP_DREMPEL_HOOG & "°C | " & _
             "WAARSCHUWING > " & TEMP_DREMPEL_WARN & "°C | " & _
             "DROOG < " & VOCHT_DREMPEL_LAAG & "%"
WScript.Echo ""

boolNoodstopVerstuurd = False

Do While True
    If objEngine.HasFreshData(3) Then
        Dim intRijen, r, dblTemp, dblVocht, strRijId
        intRijen = objEngine.GetRowCount()
        
        For r = 0 To intRijen - 1
            strRijId = objEngine.GetRowId(r)
            dblTemp  = objEngine.GetTemperature(r, 0)
            dblVocht = objEngine.GetHumidity(r, 0)
            
            ' === NOODSTOP: Temperatuur kritiek ===
            If dblTemp > TEMP_DREMPEL_HOOG And Not boolNoodstopVerstuurd Then
                WScript.Echo "[!!!] NOODSTOP - Sensor " & strRijId & _
                             ": " & dblTemp & "°C > " & TEMP_DREMPEL_HOOG & "°C"
                
                ' Stuur commando terug naar C++ via het C2P kanaal
                If objEngine.SendCommand("EMERGENCY_SHUTDOWN|" & strRijId & "|" & dblTemp) Then
                    WScript.Echo "      -> Noodstop commando verzonden naar C++ engine!"
                    boolNoodstopVerstuurd = True
                Else
                    WScript.Echo "      -> FOUT bij verzenden: " & objEngine.GetLastError()
                End If
            
            ' === WAARSCHUWING: Temperatuur hoog ===
            ElseIf dblTemp > TEMP_DREMPEL_WARN Then
                WScript.Echo "[!] WAARSCHUWING - Sensor " & strRijId & _
                             ": " & FormatNumber(dblTemp, 1) & "°C"
                objEngine.SendCommand "ALERT_HIGH_TEMP|" & strRijId & "|" & dblTemp
            
            ' === WAARSCHUWING: Luchtvochtigheid te laag ===
            ElseIf dblVocht < VOCHT_DREMPEL_LAAG And dblVocht > 0 Then
                WScript.Echo "[!] DROOGTE - Sensor " & strRijId & _
                             ": vochtigheid " & FormatNumber(dblVocht, 1) & "%"
                objEngine.SendCommand "ALERT_LOW_HUMIDITY|" & strRijId & "|" & dblVocht
            
            ' === Normaal ===
            Else
                WScript.Echo "[OK] Sensor " & strRijId & ": " & _
                             FormatNumber(dblTemp, 1) & "°C / " & _
                             FormatNumber(dblVocht, 1) & "%"
            End If
        Next
    End If
    
    WScript.Sleep CHECK_INTERVAL
Loop
```

---

### 4.5 VBS Voorbeeld 4: Excel VBA — Live Dashboard

Binnen Excel VBA kun je exact hetzelfde COM-object aanroepen om een live dashboard te bouwen. De macro haalt data op en vult cellen direct in het werkblad.

```vb
' === Excel VBA Module ===
' Voeg toe via: ALT+F11 → Insert → Module
' Vereist: SharedValueV4ComWrapper.dll geregistreerd via regasm

Option Explicit

Private objEngine As Object
Private bConnected As Boolean

' --- Knop "Verbinden" ---
Sub ConnectToEngine()
    Set objEngine = CreateObject("SharedValueV4.Wrapper")
    
    If objEngine.Connect("ProductieDashboard") Then
        bConnected = True
        MsgBox "Verbonden met SharedValueV4!", vbInformation
        
        ' Start automatische refresh elke 2 seconden
        Application.OnTime Now + TimeValue("00:00:02"), "RefreshDashboard"
    Else
        MsgBox "Fout: " & objEngine.GetLastError(), vbCritical
    End If
End Sub

' --- Automatische refresh-cyclus ---
Sub RefreshDashboard()
    If Not bConnected Then Exit Sub
    If objEngine Is Nothing Then Exit Sub
    
    Dim ws As Worksheet
    Set ws = ThisWorkbook.Sheets("Dashboard")
    
    ' Header
    ws.Range("A1").Value = "SharedValueV4 Live Dashboard"
    ws.Range("A1").Font.Bold = True
    ws.Range("A1").Font.Size = 14
    
    ws.Range("A3").Value = "API Versie:"
    ws.Range("B3").Value = objEngine.GetApiVersion()
    ws.Range("A4").Value = "Laatste Update:"
    ws.Range("B4").Value = objEngine.GetLastUpdatedUtc()
    ws.Range("A5").Value = "Verversingstijd:"
    ws.Range("B5").Value = Now()
    
    ' Tabel headers
    ws.Range("A7").Value = "Rij"
    ws.Range("B7").Value = "ID"
    ws.Range("C7").Value = "Actief"
    ws.Range("D7").Value = "Temperatuur (°C)"
    ws.Range("E7").Value = "Vochtigheid (%)"
    ws.Range("A7:E7").Font.Bold = True
    ws.Range("A7:E7").Interior.Color = RGB(41, 128, 185)
    ws.Range("A7:E7").Font.Color = vbWhite
    
    ' Vul data
    Dim i As Integer
    Dim intRows As Integer
    intRows = objEngine.GetRowCount()
    
    For i = 0 To intRows - 1
        Dim nRow As Integer
        nRow = 8 + i
        
        ws.Cells(nRow, 1).Value = i
        ws.Cells(nRow, 2).Value = objEngine.GetRowId(i)
        ws.Cells(nRow, 3).Value = IIf(objEngine.GetRowIsActive(i), "Ja", "Nee")
        
        Dim dblTemp As Double
        dblTemp = objEngine.GetTemperature(i, 0)
        ws.Cells(nRow, 4).Value = Round(dblTemp, 2)
        ws.Cells(nRow, 5).Value = Round(objEngine.GetHumidity(i, 0), 1)
        
        ' Kleurcodering: rood bij hoge temperatuur, groen bij normaal
        If dblTemp > 70 Then
            ws.Cells(nRow, 4).Interior.Color = RGB(231, 76, 60)
            ws.Cells(nRow, 4).Font.Color = vbWhite
        ElseIf dblTemp > 50 Then
            ws.Cells(nRow, 4).Interior.Color = RGB(243, 156, 18)
        Else
            ws.Cells(nRow, 4).Interior.Color = RGB(39, 174, 96)
            ws.Cells(nRow, 4).Font.Color = vbWhite
        End If
    Next i
    
    ' Plan volgende refresh
    Application.OnTime Now + TimeValue("00:00:02"), "RefreshDashboard"
End Sub

' --- Knop "Stoppen" ---
Sub StopDashboard()
    bConnected = False
    Set objEngine = Nothing
    MsgBox "Dashboard gestopt.", vbInformation
End Sub

' --- Knop "Exporteer Snapshot" ---
Sub ExportSnapshot()
    If objEngine Is Nothing Then Exit Sub
    
    Dim strCsv As String
    strCsv = objEngine.GetAllDataAsCsv()
    
    Dim strPath As String
    strPath = ThisWorkbook.Path & "\snapshot_" & Format(Now(), "yyyymmdd_hhmmss") & ".csv"
    
    Open strPath For Output As #1
    Print #1, strCsv
    Close #1
    
    MsgBox "Snapshot opgeslagen: " & strPath, vbInformation
End Sub
```

---

### 4.6 VBS Voorbeeld 5: Windows Scheduled Task — Geautomatiseerde Health Check

Dit script is ontworpen om periodiek via Windows Task Scheduler te draaien. Het verbindt kort, controleert de gezondheid van de data-pipeline, en schrijft een statusrapport weg.

```vbscript
' voorbeeld_health_check.vbs
' ===========================
' Korte health-check script voor Windows Task Scheduler.
' Verbindt, controleert datakwaliteit, en schrijft rapport.
'
' Task Scheduler Actie:
'   Program: cscript.exe
'   Arguments: //nologo "C:\Scripts\voorbeeld_health_check.vbs"
'   Interval: elke 5 minuten

Option Explicit

Const CHANNEL_NAME   = "ProductieLijn"
Const REPORT_DIR     = "C:\HealthReports\SharedValueV4"
Const MAX_DATA_AGE   = 30     ' seconden - als data ouder is, is er een probleem
Const CONNECT_TIMEOUT = 10000 ' milliseconden - max wachttijd voor verbinding

Dim objEngine, objFSO, objShell
Dim strStatus, strReport, intExitCode
Dim dtStart, intElapsed

Set objFSO   = CreateObject("Scripting.FileSystemObject")
Set objShell = CreateObject("WScript.Shell")

' Maak rapportdirectory aan
If Not objFSO.FolderExists(REPORT_DIR) Then
    objFSO.CreateFolder REPORT_DIR
End If

dtStart = Now()
strStatus = "OK"
intExitCode = 0

' === Stap 1: Probeer te verbinden ===
Set objEngine = CreateObject("SharedValueV4.Wrapper")

Dim boolConnected
boolConnected = False

' Probeer te verbinden met timeout
Dim dtTimeout
dtTimeout = DateAdd("s", CONNECT_TIMEOUT / 1000, Now())

Do While Not boolConnected And Now() < dtTimeout
    boolConnected = objEngine.Connect(CHANNEL_NAME)
    If Not boolConnected Then WScript.Sleep 500
Loop

If Not boolConnected Then
    strStatus = "KRITIEK"
    strReport = "Kan niet verbinden met kanaal '" & CHANNEL_NAME & "': " & _
                objEngine.GetLastError()
    intExitCode = 2
    GoTo WriteReport
End If

' === Stap 2: Wacht kort op data ===
WScript.Sleep 3000 ' Geef de achtergrond-engine 3 sec om data te ontvangen

' === Stap 3: Controleer data beschikbaarheid ===
If Not objEngine.HasFreshData(MAX_DATA_AGE) Then
    strStatus = "WAARSCHUWING"
    strReport = "Geen verse data ontvangen binnen " & MAX_DATA_AGE & " seconden. " & _
                "Producer mogelijk gestopt of vastgelopen."
    intExitCode = 1
    GoTo WriteReport
End If

' === Stap 4: Controleer data integriteit ===
Dim intRijen
intRijen = objEngine.GetRowCount()

If intRijen = 0 Then
    strStatus = "WAARSCHUWING"
    strReport = "Verbinding OK maar dataset bevat 0 rijen."
    intExitCode = 1
    GoTo WriteReport
End If

' Controleer op ongeldige waarden
Dim r, intFouten, strDetails
intFouten = 0
strDetails = ""

For r = 0 To intRijen - 1
    Dim dblT, dblH
    dblT = objEngine.GetTemperature(r, 0)
    dblH = objEngine.GetHumidity(r, 0)
    
    If dblT = -999.0 Or dblH = -999.0 Then
        intFouten = intFouten + 1
        strDetails = strDetails & "  - Sensor " & objEngine.GetRowId(r) & _
                     ": ongeldige waarden (T=" & dblT & ", H=" & dblH & ")" & vbCrLf
    End If
    
    If dblT > 100 Or dblT < -50 Then
        intFouten = intFouten + 1
        strDetails = strDetails & "  - Sensor " & objEngine.GetRowId(r) & _
                     ": temperatuur buiten bereik (" & dblT & "°C)" & vbCrLf
    End If
Next

If intFouten > 0 Then
    strStatus = "WAARSCHUWING"
    strReport = intFouten & " datakwaliteitsprobleem(en) gedetecteerd:" & vbCrLf & strDetails
    intExitCode = 1
Else
    strReport = "Alles in orde. " & intRijen & " sensoren actief. " & _
                "API: " & objEngine.GetApiVersion() & " | " & _
                "Laatste data: " & objEngine.GetLastUpdatedUtc()
End If

WriteReport:
    ' === Schrijf rapport ===
    intElapsed = DateDiff("s", dtStart, Now())
    
    Dim strReportFile, objReport
    strReportFile = REPORT_DIR & "\health_" & _
                    Year(Now()) & Right("0" & Month(Now()), 2) & _
                    Right("0" & Day(Now()), 2) & "_" & _
                    Right("0" & Hour(Now()), 2) & Right("0" & Minute(Now()), 2) & ".txt"
    
    Set objReport = objFSO.CreateTextFile(strReportFile, True)
    objReport.WriteLine "=========================================="
    objReport.WriteLine " SharedValueV4 Health Check Report"
    objReport.WriteLine "=========================================="
    objReport.WriteLine "Tijdstip     : " & Now()
    objReport.WriteLine "Kanaal       : " & CHANNEL_NAME
    objReport.WriteLine "Status       : " & strStatus
    objReport.WriteLine "Duur         : " & intElapsed & " seconden"
    objReport.WriteLine "------------------------------------------"
    objReport.WriteLine strReport
    objReport.WriteLine "=========================================="
    objReport.Close
    
    ' Schrijf status ook naar stdout (voor Task Scheduler logging)
    WScript.Echo "[" & strStatus & "] " & strReport
    
    ' === Optioneel: Windows Event Log schrijven bij problemen ===
    If strStatus <> "OK" Then
        objShell.LogEvent 2, "SharedValueV4 Health Check: " & strStatus & _
                             " - " & Left(strReport, 200)
    End If
    
    Set objEngine = Nothing
    WScript.Quit intExitCode
```

---

### Samenvatting VBScript Integratie

| VBS Voorbeeld | Doel | Richting | Geschikt voor |
| :--- | :--- | :--- | :--- |
| **4.2 Basis Polling** | Live data tonen op console | P2C (lezen) | Snel testen, debuggen |
| **4.3 CSV Logger** | Automatisch loggen naar bestand | P2C (lezen) | Onbeheerde servers, 24/7 monitoring |
| **4.4 Bidirectioneel** | Lezen + commando's terugsturen | P2C + C2P | Noodstop-systemen, drempel-bewaking |
| **4.5 Excel VBA** | Live dashboard in spreadsheet | P2C (lezen) | Management rapportages, KPI dashboards |
| **4.6 Health Check** | Periodieke diagnostiek | P2C (lezen) | Scheduled Tasks, infra-monitoring |

> **Kernprincipe:** VBScript spreekt uitsluitend met de C# COM-wrapper. De bliksemsnelle MMF/FlatBuffer-laag draait volledig verborgen in de achtergrond. Legacy-scripts profiteren zo van nanoseconde-latency zonder enige kennis van Memory-Mapped Files, Mutexes, of FlatBuffers.
