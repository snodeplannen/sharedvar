' vbs_producer_array.vbs — VBScript Producer (Parameter-Array Model)
' Usage: cscript vbs_producer_array.vbs
Option Explicit

Dim engine
Set engine = CreateObject("SharedValueV5.Engine")

engine.CreateTable "Sensoren"
engine.SelectTable "Sensoren"
engine.DefineColumn "SensorId",    "String"
engine.DefineColumn "Temperature", "Double"
engine.DefineColumn "Humidity",    "Double"
engine.DefineColumn "IsActive",    "Bool"
engine.DefineColumn "StatusCode",  "Int32"

engine.Connect "FabrieksData", True

' === Rijen in één call: waarden op volgorde van kolom-ordinals ===
engine.AddRowDirect "VBS_Sensor_01", 22.5, 65.0, True, 200
engine.AddRowDirect "VBS_Sensor_02", 18.3, 72.1, False, 503
engine.AddRowDirect "VBS_Sensor_03", 30.1, 44.0, True, 200

engine.Publish
WScript.Echo "3 rijen gepubliceerd (parameter-array model)!"
engine.Disconnect
