# Usage Examples

Once you have installed the **SharedValue COM Server** (or locally registered its executables), the environment naturally functions as a centralized Out-of-Process service beacon. You practically possess connectivity freedom tapping inside via virtually any language inherently supporting COM (Component Object Model) natively.

The complex data transactions or lock-flow boundaries autonomously handle rigorous synchronization flawlessly navigating the overarching backgrounds heavily powered leveraging the **SharedValueV2 Engine C++ Core** baseline library logic. Should Application A aggressively claim dataset locks, parallel Application C queues sequentially completely bypassing abrupt failures/application crashes.

Here we detail effectively coupling language environments charting **VBScript**, **C#**, and **C++** explicitly concentrating observing the `SharedValue` logic intertwined alongside `DatasetProxy` object structures.

## 1. VBScript (Late Binding)

Serving administrative scripting sequences, DevOps testing triggers, or aggressive volatile overriding tests, the VBScript interpreter excels performing powerfully leveraging classic Late-Binding maneuvers.

```vbscript
' ----------
' SharedValue Example
' ----------
Set sharedVal = CreateObject("ATLProjectcomserverExe.SharedValue")

' Enforce exclusive cross-process locks avoiding collisions (5000 milliseconds timeout gap)
sharedVal.LockSharedValue(5000) 
sharedVal.SetValue "Exclusive chunk pushed navigating the VBScript payload!"
WScript.Echo "Server value accurately detected locally: " & sharedVal.GetValue()
sharedVal.Unlock()


' ----------
' DatasetProxy Example
' ----------
Set dataset = CreateObject("ATLProjectcomserverExe.DatasetProxy")

' Systematically upload JSON settings / strings dropping towards distinct data-rows
dataset.AddRow "Config_Timeout", "1500"
dataset.AddRow "Server_URL", "https://api.myproject.com"

WScript.Echo "Current server table bounds counts exactly " & dataset.GetRecordCount() & " row iterations."

' Effectively validate dataset presence keys
If dataset.HasKey("Server_URL") Then
    WScript.Echo "Url Match Isolated: " & dataset.GetRowData("Server_URL")
End If

' Hand-trigger cells update parameters & isolated table deletions
dataset.UpdateRow "Config_Timeout", "2000"
dataset.RemoveRow "Server_URL"
```

## 2. C# (.NET) via Early Binding

To strictly maintain bullet-proof coding paired securely inside Visual Studio completion tools, you seamlessly enforce Type Library references directly enveloping your .NET ecosystem (**Right click project** -> **Add COM Reference** -> toggle `"ATLProjectcomserverExe 1.0 Type Library"`).

Subsequently mapping those native DLL's directly transposes Native C++ COM mappings straight towards elegant encapsulated C# wrappers cleanly (.NET interop layouts):

```csharp
using System;
using ATLProjectcomserverLib;

class Program
{
    static void Main()
    {
        // ----------
        // SharedValue Example
        // ----------
        ISharedValue sharedVal = new SharedValue();
        
        sharedVal.LockSharedValue(1500); // Block dirty arrays assuming 1.5s timeouts looping!
        try
        {
            sharedVal.SetValue("Shared dataset footprint restored through direct C# threaded injections.");
            Console.WriteLine($"Server State Flagged: {sharedVal.GetValue()}");
        }
        finally
        {
            sharedVal.Unlock(); // Guarantee block cleanups circumventing program crashes
        } 


        // ----------
        // DatasetProxy Example
        // ----------
        IDatasetProxy dataset = new DatasetProxy();
        
        // Mutate dynamic memory: Mode 0 = Ordered trees, Mode 1 = O(1) hash-tables
        dataset.SetStorageMode(1); 
        
        dataset.AddRow("Memory_Obj_101", "{ \"username\": \"Arthur\", \"position_score\": 45 }");
        dataset.AddRow("Memory_Obj_102", "{ \"username\": \"Merlin\", \"position_score\": 90 }");
        
        Console.WriteLine($"Server array elements heavily structured computing sizes: {dataset.GetRecordCount()}");
        
        if (dataset.HasKey("Memory_Obj_101"))
        {
            Console.WriteLine($"Decoded response payload extracting logic: {dataset.GetRowData("Memory_Obj_101")}");
        }

        dataset.UpdateRow("Memory_Obj_102", "{ \"username\": \"Merlin\", \"position_score\": 120 }");
        dataset.Clear(); // Ruthless buffer flush eliminating everything 
    }
}
```

## 3. C++ (Native Client)

Plugging effectively inside native C++ application tunnels devoid of messy string casting arrays, the native compiler introduces hyper-fluid automation exploiting the `#import` macro. Background compiling flawlessly spawns highly optimized Smart-Pointers (`Ptr`), resolving extensive garbage memory clearance loops utilizing background BSTR classes inherently wrapped navigating `_bstr_t`.

```cpp
#include <iostream>
#include <Windows.h>
#include <comdef.h>

// Leverage COM binary proxy compilations effortlessly routing exact GUID components
#import "ATLProjectcomserver.exe" no_namespace named_guids

int main() {
    // Initiate Windows thread communication sockets parallel bindings 
    CoInitializeEx(NULL, COINIT_MULTITHREADED); 

    try {
        // ----------
        // SharedValue Example
        // ----------
        ISharedValuePtr sharedVal(__uuidof(SharedValue)); 
        
        sharedVal->LockSharedValue(5000); 
        sharedVal->SetValue(_variant_t("Chained string push directed out via straight Native C++ struct trees"));
        
        std::wcout << L"Return buffer feedback loops verified string: " << sharedVal->GetValue().bstrVal << std::endl;
        sharedVal->Unlock();


        // ----------
        // DatasetProxy Example
        // ----------
        IDatasetProxyPtr dataset(__uuidof(DatasetProxy));
        
        // Dynamically shift backend layouts arrays
        dataset->SetStorageMode(0);

        dataset->AddRow(_bstr_t("Client_Live_Alpha"), _bstr_t("Connection_A_Actively_Running"));
        dataset->AddRow(_bstr_t("Client_Live_Beta"), _bstr_t("Connection_B_Offline_Flagged"));

        std::wcout << L"Server record capacity array sizes calculated: " << dataset->GetRecordCount() << std::endl;

        if (dataset->HasKey(_bstr_t("Client_Live_Alpha")) == VARIANT_TRUE) {
            _bstr_t jsonRow = dataset->GetRowData(_bstr_t("Client_Live_Alpha"));
            std::wcout << L"Extracted body mapping string payloads Alpha: " << jsonRow << std::endl;
        }

        // Live iterations & dataset mutations 
        dataset->UpdateRow(_bstr_t("Client_Live_Alpha"), _bstr_t("Execution Confirmed"));
        dataset->RemoveRow(_bstr_t("Client_Live_Beta"));

    } catch (const _com_error& e) {
        std::wcerr << L"Exceptional blockade failure triggered dropping connections!: " << e.ErrorMessage() << std::endl;
    }

    CoUninitialize();
    return 0;
}
```
