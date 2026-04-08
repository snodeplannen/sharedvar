' TestEventCallbacks.vbs — Test event callback mechanisme
Option Explicit

Dim eventCount
eventCount = 0

Class EventCounter
    Public Sub OnMutationEvent(eventType, key, oldVal, newVal, source, ts, seqId)
        eventCount = eventCount + 1
        WScript.Echo "[Event #" & seqId & "] Type=" & eventType & _
                     " Key=" & key & " New=" & newVal
    End Sub
End Class

On Error Resume Next

Dim counter, proxy
Set counter = New EventCounter
Set proxy = CreateObject("ATLProjectcomserver.DatasetProxy")

If Err.Number <> 0 Then
    WScript.Echo "FAIL: Kan DatasetProxy niet aanmaken: " & Err.Description
    WScript.Quit 1
End If

proxy.RegisterEventCallback counter

' Voer mutaties uit
proxy.AddRow "k1", "v1"       ' Event: RowAdded
proxy.UpdateRow "k1", "v2", 0 ' Event: RowUpdated
proxy.RemoveRow "k1", 0       ' Event: RowRemoved

If eventCount = 3 Then
    WScript.Echo "TEST 1 PASS: " & eventCount & " events ontvangen"
Else
    WScript.Echo "TEST 1 FAIL: Verwacht 3 events, kreeg " & eventCount
End If

' Test unregister
proxy.UnregisterEventCallback counter
proxy.AddRow "k2", "v2"  ' Geen event meer verwacht

If eventCount = 3 Then
    WScript.Echo "TEST 2 PASS: Unregister werkt correct"
Else
    WScript.Echo "TEST 2 FAIL: Unregister faalde, events=" & eventCount
End If

WScript.Echo "Event callback tests voltooid."
