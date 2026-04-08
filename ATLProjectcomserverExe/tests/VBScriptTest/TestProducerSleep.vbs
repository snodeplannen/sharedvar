Dim sv, proxy
Set sv = CreateObject("ATLProjectcomserverExe.SharedValue")
Set proxy = sv.GetValue() ' Get Server's Native Proxy!
proxy.AddRow "server01", "status:online|cpu:45|mem:8192"
WScript.Echo "Sleeping for 60s..."
WScript.Sleep 60000
