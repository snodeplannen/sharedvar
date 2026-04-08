On Error Resume Next
WScript.Echo "Consumer Started."

Dim sv, val
Set sv = CreateObject("ATLProjectcomserverExe.SharedValue")
val = sv.GetValue()
WScript.Echo "Consumer kreeg: " & CStr(val)
