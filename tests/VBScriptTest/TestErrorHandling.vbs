' TestErrorHandling.vbs — Test dat fouten correct doorborrelen
Option Explicit
On Error Resume Next

Dim proxy, result, ok
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
Err.Clear
proxy.SetStorageMode 1
If Err.Number <> 0 Then
    WScript.Echo "TEST 2 PASS: " & Err.Description
    Err.Clear
Else
    WScript.Echo "TEST 2 FAIL: StorageMode switch had moeten falen"
End If

' Test 3: Duplicate key  
Err.Clear
proxy.AddRow "key1", "value_dup"
If Err.Number <> 0 Then
    WScript.Echo "TEST 3 PASS: " & Err.Description
    Err.Clear
Else
    WScript.Echo "TEST 3 FAIL: Duplicate key had moeten falen"
End If

WScript.Echo "Error handling tests voltooid."
