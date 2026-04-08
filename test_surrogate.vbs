On Error Resume Next
WScript.Echo "Testing CreateObject..."
Dim o
Set o = CreateObject("ATLProjectcomserver.SharedValue")
If Err.Number <> 0 Then
    WScript.Echo "Err: " & Err.Number & " Desc: " & Err.Description
Else
    WScript.Echo "Success."
End If
