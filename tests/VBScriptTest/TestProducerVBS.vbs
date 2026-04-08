' TestProducerVBS.vbs — VBScript als PRODUCER
' Creëert een dataset en parkeert hem in SharedValue
On Error Resume Next

Dim proxy, sv, cnt

' 1. Dataset aanmaken en vullen
Set proxy = CreateObject("ATLProjectcomserver.DatasetProxy")
If Err.Number <> 0 Then
    WScript.Echo "FAIL: Kan DatasetProxy niet aanmaken: " & Err.Description
    WScript.Quit 1
End If

proxy.AddRow "server01", "status:online|cpu:45|mem:8192"
proxy.AddRow "server02", "status:offline|cpu:0|mem:0"
proxy.AddRow "server03", "status:online|cpu:92|mem:16384"

If Err.Number <> 0 Then
    WScript.Echo "FAIL: Fout bij AddRow: " & Err.Description
    WScript.Quit 1
End If

' 2. Parkeer in SharedValue bridge
Set sv = CreateObject("ATLProjectcomserver.SharedValue")
If Err.Number <> 0 Then
    WScript.Echo "FAIL: Kan SharedValue niet aanmaken: " & Err.Description
    WScript.Quit 1
End If

sv.SetValue proxy

cnt = proxy.GetRecordCount()

WScript.Echo "PASS: Dataset met " & cnt & " rijen geparkeerd."
