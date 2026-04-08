Dim sv, proxy
Set sv = CreateObject("ATLProjectcomserverExe.SharedValue")
Set proxy = sv.GetValue()
proxy.AddRow "from_vbs_1", "AAA"
