On Error Resume Next
Dim sv, proxy, cnt, keys, i

Set sv = CreateObject("ATLProjectcomserverExe.SharedValue")
Set proxy = sv.GetValue()

cnt = proxy.GetRecordCount()
WScript.Echo "Dataset bevat " & cnt & " rijen"

keys = proxy.FetchPageKeys(0, 100)
If IsArray(keys) Then
    For i = 0 To UBound(keys)
        WScript.Echo "Key " & i & " is '" & CStr(keys(i)) & "'"
        Dim dat
        dat = proxy.GetRowData(CStr(keys(i)))
        WScript.Echo CStr(keys(i)) & " => " & CStr(dat)
    Next
End If
