On Error Resume Next
WScript.Echo "Producer Started."

Dim sv
Set sv = CreateObject("ATLProjectcomserverExe.SharedValue")
sv.SetValue "HELLO FROM PRODUCER"

WScript.Echo "Producer: SetValue gedaan. Wachten op consumer (10 sec)..."
WScript.Sleep 10000
WScript.Echo "Producer: Klaar."
