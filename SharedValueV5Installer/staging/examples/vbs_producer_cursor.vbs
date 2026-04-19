' vbs_producer_cursor.vbs — VBScript Producer (Cursor Model)
' Usage: cscript vbs_producer_cursor.vbs
Option Explicit

Dim engine
Set engine = CreateObject("SharedValueV5.Engine")

' Tabel aanmaken en selecteren
engine.CreateTable "Sensoren"
engine.SelectTable "Sensoren"

' Schema definiëren
engine.DefineColumn "SensorId",    "String"
engine.DefineColumn "Temperature", "Double"
engine.DefineColumn "Humidity",    "Double"
engine.DefineColumn "IsActive",    "Bool"
engine.DefineColumn "StatusCode",  "Int32"

' Schema bevriezen
engine.LockSchema

' Tweede tabel
engine.CreateTable "Alarmen"
engine.SelectTable "Alarmen"
engine.DefineColumn "AlarmId",  "Int32"
engine.DefineColumn "Bericht",  "String"
engine.DefineColumn "Severity", "Int32"
engine.LockSchema

' Verbind als host
engine.Connect "FabrieksData", True

' === Data toevoegen: Cursor Model ===
engine.SelectTable "Sensoren"

engine.AddRow
engine.SetValue "SensorId",    "VBS_Sensor_01"
engine.SetValue "Temperature", 22.5
engine.SetValue "Humidity",    65.0
engine.SetValue "IsActive",    True
engine.SetValue "StatusCode",  200

engine.AddRow
engine.SetValue "SensorId",    "VBS_Sensor_02"
engine.SetValue "Temperature", 18.3
engine.SetValue "Humidity",    72.1
engine.SetValue "IsActive",    False
engine.SetValue "StatusCode",  503

' Alarm toevoegen
engine.SelectTable "Alarmen"
engine.AddRow
engine.SetValue "AlarmId",  1001
engine.SetValue "Bericht",  "Temperatuur hoog in zone A"
engine.SetValue "Severity", 3

' Stuur alles naar gedeeld geheugen
engine.Publish

WScript.Echo "Data gepubliceerd (cursor model)!"
engine.Disconnect
