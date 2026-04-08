' Producer loop
On Error Resume Next
WScript.Echo "Producer Started. PID: " & GetObject("winmgmts:root\cimv2").ExecQuery("Select * from Win32_Process where ProcessId = " & CreateObject("WScript.Network").ProcessID).ItemIndex
Dim proxy, sv, cnt
Set proxy = CreateObject("ATLProjectcomserver.DatasetProxy")
proxy.AddRow "server01", "status:online|cpu:45|mem:8192"
proxy.AddRow "server02", "status:offline|cpu:0|mem:0"

Set sv = CreateObject("ATLProjectcomserver.SharedValue")
sv.SetValue proxy

cnt = proxy.GetRecordCount()
WScript.Echo "Producer: Dataset met " & cnt & " rijen geparkeerd."
WScript.Echo "Producer: Wachten op consumer (10 seconden)..."

WScript.Sleep 10000
WScript.Echo "Producer: Klaar."
