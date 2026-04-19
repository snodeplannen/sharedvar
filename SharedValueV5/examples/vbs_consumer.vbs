' vbs_consumer.vbs — VBScript Consumer (Schema Autodiscovery)
' Usage: cscript vbs_consumer.vbs
Option Explicit

Dim engine
Set engine = CreateObject("SharedValueV5.Engine")
engine.Connect "FabrieksData", False

WScript.Echo "Verbonden! Schema ontdekken..."
WScript.Echo "Aantal tabellen: " & engine.TableCount

Dim t, r, c
For t = 0 To engine.TableCount - 1
    Dim strTableName
    strTableName = engine.TableName(t)
    engine.SelectTable strTableName

    WScript.Echo ""
    WScript.Echo "=== Tabel: " & strTableName & " ==="
    WScript.Echo "  Kolommen: " & engine.ColumnCount
    WScript.Echo "  Rijen:    " & engine.RowCount
    WScript.Echo "  Locked:   " & engine.IsSchemaLocked

    ' Kolom-headers
    Dim strHeader
    strHeader = ""
    For c = 0 To engine.ColumnCount - 1
        strHeader = strHeader & engine.ColumnName(c) & " (" & _
                    engine.ColumnTypeName(c) & ")" & vbTab
    Next
    WScript.Echo "  " & strHeader

    ' Data tonen
    For r = 0 To engine.RowCount - 1
        Dim strRow
        strRow = "  "
        For c = 0 To engine.ColumnCount - 1
            strRow = strRow & engine.GetValueByIndex(r, c) & vbTab
        Next
        WScript.Echo strRow
    Next
Next

engine.Disconnect
