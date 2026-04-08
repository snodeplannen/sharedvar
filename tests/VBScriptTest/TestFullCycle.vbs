' TestFullCycle.vbs — Volledige CRUD-cyclus in VBScript
On Error Resume Next

Dim proxy, sv, proxy2, cnt, ok, timeoutVal

' 1. Aanmaken en vullen
Set proxy = CreateObject("ATLProjectcomserver.DatasetProxy")
If Err.Number <> 0 Then
    WScript.Echo "FAIL CreateProxy: " & Err.Description
    WScript.Quit 1
End If

Set sv = CreateObject("ATLProjectcomserver.SharedValue")
If Err.Number <> 0 Then
    WScript.Echo "FAIL CreateShared: " & Err.Description
    WScript.Quit 1
End If

proxy.AddRow "cfg_timeout", "30"
If Err.Number <> 0 Then
    WScript.Echo "FAIL AddRow1: " & Err.Description
    WScript.Quit 1
End If

proxy.AddRow "cfg_retries", "5"
proxy.AddRow "cfg_maxconn", "100"

sv.SetValue proxy
If Err.Number <> 0 Then
    WScript.Echo "FAIL SetValue: " & Err.Description
    Err.Clear
End If

' 2. Ophalen
cnt = proxy.GetRecordCount()
WScript.Echo "Records: " & cnt

' 3. Muteer
ok = proxy.UpdateRow("cfg_timeout", "60")
ok = proxy.RemoveRow("cfg_retries")
If Err.Number <> 0 Then
    WScript.Echo "FAIL Mutatie: " & Err.Description
    WScript.Quit 1
End If

' 4. Valideer
cnt = proxy.GetRecordCount()
timeoutVal = proxy.GetRowData("cfg_timeout")
WScript.Echo "Na mutatie: " & cnt & " records, Timeout=" & timeoutVal

If cnt = 2 And timeoutVal = "60" Then
    WScript.Echo "PASS: Volledige CRUD-cyclus succesvol!"
Else
    WScript.Echo "FAIL: count=" & cnt & " timeout=" & timeoutVal
    WScript.Quit 1
End If
