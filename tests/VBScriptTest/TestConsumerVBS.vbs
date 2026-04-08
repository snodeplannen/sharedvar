' TestConsumerVBS.vbs — VBScript als CONSUMER
' Leest een dataset die door een andere client was ingesteld
On Error Resume Next

Dim sv, proxy, cnt, keys, k, d

Set sv = CreateObject("ATLProjectcomserver.SharedValue")
If Err.Number <> 0 Then
    WScript.Echo "FAIL: Kan SharedValue niet aanmaken: " & Err.Description
    WScript.Quit 1
End If

Set proxy = sv.GetValue()
If Err.Number <> 0 Then
    WScript.Echo "FAIL: Kan proxy niet ophalen: " & Err.Description
    WScript.Quit 1
End If

cnt = proxy.GetRecordCount()
WScript.Echo "Dataset bevat " & cnt & " rijen"

' Pagineer door alles heen
keys = proxy.FetchPageKeys(0, 100)
If IsArray(keys) Then
    For Each k In keys
        d = proxy.GetRowData(k)
        WScript.Echo k & " => " & d
    Next
    WScript.Echo "PASS: Alle " & (UBound(keys) + 1) & " rijen succesvol gelezen."
Else
    WScript.Echo "WARN: FetchPageKeys retourneerde geen array."
End If
