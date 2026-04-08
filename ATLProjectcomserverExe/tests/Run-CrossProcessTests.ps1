<#
.SYNOPSIS
    Cross-Process COM Test Suite

.DESCRIPTION
    Deze test suite toont stapsgewijs hoe de Out-Of-Process EXE COM Server reageert 
    op geïsoleerde clients die ad-hoc verbinden, data achterlaten, en verbreken.
#>

$ErrorActionPreference = "Stop"
$exePath = "c:\csharp\cursor_com_test\ATLProjectcomserverExe\x64\Release\ATLProjectcomserver.exe"

Write-Host "===========================" -ForegroundColor Cyan
Write-Host " COM EXE INTEGRATION TESTS " -ForegroundColor Cyan
Write-Host "===========================" -ForegroundColor Cyan

# Zorg dat oude instanties weg zijn
Stop-Process -Name "ATLProjectcomserver" -Force -ErrorAction SilentlyContinue | Out-Null
Start-Sleep -Seconds 1

Write-Host "`n[1] De server wordt automatisch on-demand gestart door Windows wanneer een client roept!" -ForegroundColor Green

# -------------------------------------------------------------
Write-Host "`n[2] SCENARIO 1: Producer schrijft string" -ForegroundColor Yellow
$producerStringVbs = @"
Set sv = CreateObject("ATLProjectcomserverExe.SharedValue")
sv.SetValue "Boodschap vanuit afgesloten Producer-proces!"
"@
$producerStringVbs | Out-File -FilePath "$env:TEMP\prod_str.vbs" -Encoding ASCII
& cscript.exe //Nologo "$env:TEMP\prod_str.vbs"
Write-Host "    -> Producer script is nu volledig afgesloten." -ForegroundColor DarkGray

# -------------------------------------------------------------
Write-Host "`n[3] SCENARIO 2: Consumer leest string" -ForegroundColor Yellow
$consumerStringVbs = @"
Set sv = CreateObject("ATLProjectcomserverExe.SharedValue")
val = CStr(sv.GetValue())
WScript.Echo "    Opgeslagen waarde door server behouden: " & val
"@
$consumerStringVbs | Out-File -FilePath "$env:TEMP\con_str.vbs" -Encoding ASCII
& cscript.exe //Nologo "$env:TEMP\con_str.vbs"

Write-Host "    -> Clean up memory before Proxy Test..." -ForegroundColor DarkGray
Stop-Process -Name "ATLProjectcomserver" -Force -ErrorAction SilentlyContinue | Out-Null
Start-Sleep -Seconds 1

# -------------------------------------------------------------
Write-Host "`n[4] SCENARIO 3: Producer vult de centrale DatasetProxy" -ForegroundColor Yellow
$producerProxyVbs = @"
Set sv = CreateObject("ATLProjectcomserverExe.SharedValue")
Set proxy = sv.GetValue() ' Haalt native server object op
proxy.AddRow "SERVER-01", "Verzending=Ok|Status=Actief"
proxy.AddRow "SERVER-02", "Verzending=Nee|Status=Offline"
WScript.Echo "    -> 2 rijen succesvol weggeschreven op de native COM proxy."
"@
$producerProxyVbs | Out-File -FilePath "$env:TEMP\prod_prox.vbs" -Encoding ASCII
& cscript.exe //Nologo "$env:TEMP\prod_prox.vbs"
Write-Host "    -> Producer script met zware data is nu afgesloten." -ForegroundColor DarkGray

# -------------------------------------------------------------
Write-Host "`n[5] SCENARIO 4: Consumer leest de rijen uit via COM Marshaling" -ForegroundColor Yellow
$consumerProxyVbs = @"
Set sv = CreateObject("ATLProjectcomserverExe.SharedValue")
Set proxy = sv.GetValue() 

cnt = proxy.GetRecordCount()
WScript.Echo "    -> Dataset uit EXE bevat " & cnt & " rijen."

keys = proxy.FetchPageKeys(0, 100)
If IsArray(keys) Then
    For k = 0 To UBound(keys)
        val = proxy.GetRowData(CStr(keys(k)))
        WScript.Echo "       Rij " & k & ": [" & keys(k) & "] => " & val
    Next
End If
"@
$consumerProxyVbs | Out-File -FilePath "$env:TEMP\con_prox.vbs" -Encoding ASCII
& cscript.exe //Nologo "$env:TEMP\con_prox.vbs"

# -------------------------------------------------------------
Write-Host "`n[6] SCENARIO 5: Graceful Shutdown" -ForegroundColor Yellow
Write-Host "    Controle: COM proces Id = " -NoNewline
Get-Process "ATLProjectcomserver" -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Id

$shutdownVbs = @"
Set sv = CreateObject("ATLProjectcomserverExe.SharedValue")
sv.ShutdownServer()
WScript.Echo "    -> Shutdown callback succesvol verzonden."
"@
$shutdownVbs | Out-File -FilePath "$env:TEMP\shutdown.vbs" -Encoding ASCII
& cscript.exe //Nologo "$env:TEMP\shutdown.vbs"

Start-Sleep -Seconds 1
$procs = Get-Process "ATLProjectcomserver" -ErrorAction SilentlyContinue
if (-not $procs) {
    Write-Host "    SUCCES: EXE server is naadloos gesloten." -ForegroundColor Green
} else {
    Write-Host "    FAIL: EXE server draait nog." -ForegroundColor Red
}

Write-Host "`nEINDE TESTS" -ForegroundColor Cyan
