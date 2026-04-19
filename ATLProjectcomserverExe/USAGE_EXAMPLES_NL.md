# Gebruikersvoorbeelden (Usage Examples)

Zodra je de **SharedValue COM Server** hebt geïnstalleerd (of lokaal geregistreerd hebt via de build structuur), fungeert hij als een centrale Out-of-Process service resource. Je kunt verbinding opzetten vanuit vrijwel iedere taal die COM (Component Object Model) native begrijpt.

De datatransacties of lock-flows handelen robuuste file locks intern vloeiend af in de achtergrond dankzij de zwaargewicht **SharedValueV2 Engine C++ Core** bibliotheek. Zelfs als applicatie A the Dataset vastzet, wacht applicatielijn C netjes de wachtrij in zonder abrupte falen/crashes.

Hier zie je in details hoe je koppelingen in regelt per specifieke taal: **VBScript**, **C#**, en **C++** gefocust op de `SharedValue` én het array `DatasetProxy` object.

## 1. VBScript (Late Binding)

Voor administratieve scripting, DevOps testing, of vluchtige data overrides is de interpretter VBScript zeer robuust middels het klassieke Late-Binding pattern.

```vbscript
' ----------
' SharedValue Voorbeeld
' ----------
Set sharedVal = CreateObject("ATLProjectcomserverExe.SharedValue")

' Pas exclusieve blocking toe met Lock (5000 milliseconden timeout)
sharedVal.LockSharedValue(5000) 
sharedVal.SetValue "Exclusief weggeschreven payload door het script"
WScript.Echo "Waarde lokaal gedetecteerd: " & sharedVal.GetValue()
sharedVal.Unlock()


' ----------
' DatasetProxy Voorbeeld
' ----------
Set dataset = CreateObject("ATLProjectcomserverExe.DatasetProxy")

' Upload bulk settings / key-strings (bijv. in JSON vorm) in datarijen
dataset.AddRow "Config_Timeout", "1500"
dataset.AddRow "Server_URL", "https://api.myproject.com"

WScript.Echo "De actuele server recordlijst telt nu " & dataset.GetRecordCount() & " sleutels."

' Valideer of data presence geldig is
If dataset.HasKey("Server_URL") Then
    WScript.Echo "Url Gevonden: " & dataset.GetRowData("Server_URL")
End If

' Handmatige cell update & removal live operaties
dataset.UpdateRow "Config_Timeout", "2000"
dataset.RemoveRow "Server_URL"
```

## 2. C# (.NET) via Early Binding

Voor foutloze type-safe coding plus Visual Studio design checks koppel je the Type Library reference aan je .NET Projectomgeving (**Right click project** -> **Add COM Reference** -> kies `"ATLProjectcomserverExe 1.0 Type Library"`).

Hierdoor worden de native C++ COM struct headers direct naadloos geparceerd naar C# classes (.NET interop dlls):

```csharp
using System;
using ATLProjectcomserverLib;

class Program
{
    static void Main()
    {
        // ----------
        // SharedValue Voorbeeld
        // ----------
        ISharedValue sharedVal = new SharedValue();
        
        sharedVal.LockSharedValue(1500); // Cross-process lock spanning (voorkomt dirty arrays bij 1.5s theorie-test timeouts)
        try
        {
            sharedVal.SetValue("Shared record gereconstrueerd via C#-applicatie thread");
            Console.WriteLine($"Bevestigings state: {sharedVal.GetValue()}");
        }
        finally
        {
            sharedVal.Unlock(); // Lock resources opschonen
        } 


        // ----------
        // DatasetProxy Voorbeeld
        // ----------
        IDatasetProxy dataset = new DatasetProxy();
        
        // Switch backend memory format: Mode 0 = Ordered map (tree), Mode 1 = Unordered map (O(1) vluchtige hash-tables)
        dataset.SetStorageMode(1); 
        
        dataset.AddRow("In_Memory_101", "{ \"naam\": \"Arthur\", \"positie_score\": 45 }");
        dataset.AddRow("In_Memory_102", "{ \"naam\": \"Merlijn\", \"positie_score\": 90 }");
        
        Console.WriteLine($"Server rijen geregistreerd: {dataset.GetRecordCount()}");
        
        if (dataset.HasKey("In_Memory_101"))
        {
            Console.WriteLine($"Ontcijferde response Payload: {dataset.GetRowData("In_Memory_101")}");
        }

        dataset.UpdateRow("In_Memory_102", "{ \"naam\": \"Merlijn\", \"positie_score\": 120 }");
        dataset.Clear(); // Empty memory flush
    }
}
```

## 3. C++ (Native Client)

Om via hardline C++ programs in the tunnel te tappen zonder zware string casting, maakt de native compiler het super easy via the `#import` macro, dit mapt automatisch C++ smartpointers (`Ptr`) over alle objecten én beheerst de gehele geheugen Garbage collectie via typen in de achtergrond (`_bstr_t`).

```cpp
#include <iostream>
#include <Windows.h>
#include <comdef.h>

// Import COM binary logic automatisation via smart-proxy parsing
#import "ATLProjectcomserver.exe" no_namespace named_guids

int main() {
    // Initialiseer parallel communicatie poorten richting windows
    CoInitializeEx(NULL, COINIT_MULTITHREADED); 

    try {
        // ----------
        // SharedValue Voorbeeld
        // ----------
        ISharedValuePtr sharedVal(__uuidof(SharedValue)); 
        
        sharedVal->LockSharedValue(5000); 
        sharedVal->SetValue(_variant_t("Gechainde pointer push via de Native C++ structuur direct"));
        
        std::wcout << L"Server return reflectie string: " << sharedVal->GetValue().bstrVal << std::endl;
        sharedVal->Unlock();


        // ----------
        // DatasetProxy Voorbeeld
        // ----------
        IDatasetProxyPtr dataset(__uuidof(DatasetProxy));
        
        // Dynamisch omzetten array modes
        dataset->SetStorageMode(0);

        dataset->AddRow(_bstr_t("Session_Live_Alpha"), _bstr_t("Connectie_A_Running"));
        dataset->AddRow(_bstr_t("Session_Live_Beta"), _bstr_t("Connectie_B_Offline"));

        std::wcout << L"Datapool capaciteit record load: " << dataset->GetRecordCount() << std::endl;

        if (dataset->HasKey(_bstr_t("Session_Live_Alpha")) == VARIANT_TRUE) {
            _bstr_t jsonRow = dataset->GetRowData(_bstr_t("Session_Live_Alpha"));
            std::wcout << L"Response body Alpha (Row 1): " << jsonRow << std::endl;
        }

        // Live manipulaties
        dataset->UpdateRow(_bstr_t("Session_Live_Alpha"), _bstr_t("Voltooid"));
        dataset->RemoveRow(_bstr_t("Session_Live_Beta"));

    } catch (const _com_error& e) {
        std::wcerr << L"Hard Exception Native Loop Blockade!: " << e.ErrorMessage() << std::endl;
    }

    CoUninitialize();
    return 0;
}
```
