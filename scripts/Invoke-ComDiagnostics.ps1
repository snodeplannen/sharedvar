<#
.SYNOPSIS
    COM Server Diagnostisch Script — Controleert registratie, dependencies en TypeLib.

.DESCRIPTION
    Gebruik dit script om snel te verifiëren of de ATLProjectcomserver.dll correct
    is geregistreerd, alle dependencies laadbaar zijn, en de TypeLib beschikbaar is.

.EXAMPLE
    .\Invoke-ComDiagnostics.ps1
    .\Invoke-ComDiagnostics.ps1 -DllPath "x64\Release\ATLProjectcomserver.dll"
#>
param(
    [string]$DllPath = ""
)

$ErrorActionPreference = 'SilentlyContinue'
$script:passed = 0
$script:failed = 0
$script:warnings = 0

function Write-Check {
    param([string]$Name, [bool]$Ok, [string]$Detail = "")
    if ($Ok) {
        Write-Host "  [OK] $Name" -ForegroundColor Green
        if ($Detail) { Write-Host "       $Detail" -ForegroundColor DarkGray }
        $script:passed++
    } else {
        Write-Host "  [FAIL] $Name" -ForegroundColor Red
        if ($Detail) { Write-Host "         $Detail" -ForegroundColor Yellow }
        $script:failed++
    }
}

function Write-Warn {
    param([string]$Name, [string]$Detail = "")
    Write-Host "  [WARN] $Name" -ForegroundColor Yellow
    if ($Detail) { Write-Host "         $Detail" -ForegroundColor DarkGray }
    $script:warnings++
}

# === Resolve DLL path ===
$projectRoot = Split-Path $PSScriptRoot -Parent
if (-not $DllPath) {
    # Auto-detect: prefer Release, fallback to Debug
    $candidates = @(
        (Join-Path $projectRoot "x64\Release\ATLProjectcomserver.dll"),
        (Join-Path $projectRoot "x64\Debug\ATLProjectcomserver.dll")
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) { $DllPath = $c; break }
    }
}
if (-not $DllPath -or -not (Test-Path $DllPath)) {
    Write-Host "`n[ERROR] Kan DLL niet vinden. Geef het pad op via -DllPath." -ForegroundColor Red
    exit 1
}

$dllDir = Split-Path $DllPath -Parent
$isDebug = $DllPath -match "\\Debug\\"

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "  COM Server Diagnostiek" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  DLL: $DllPath"
Write-Host "  Config: $(if ($isDebug) { 'Debug' } else { 'Release' })"
Write-Host "  Platform: x64"
Write-Host "========================================`n" -ForegroundColor Cyan

# === 1. DLL Bestandscontrole ===
Write-Host "--- 1. DLL Bestand ---" -ForegroundColor Cyan
Write-Check "DLL bestaat" (Test-Path $DllPath) $DllPath
$dllInfo = Get-Item $DllPath -ErrorAction SilentlyContinue
if ($dllInfo) {
    Write-Check "DLL grootte > 0" ($dllInfo.Length -gt 0) "$([math]::Round($dllInfo.Length / 1KB)) KB"
    Write-Check "DLL recente build" ($dllInfo.LastWriteTime -gt (Get-Date).AddDays(-1)) "Laatste build: $($dllInfo.LastWriteTime)"
}

# === 2. CRT Dependencies ===
Write-Host "`n--- 2. C/C++ Runtime Dependencies ---" -ForegroundColor Cyan
$crtDlls = if ($isDebug) {
    @("MSVCP140D.dll", "VCRUNTIME140D.dll", "VCRUNTIME140_1D.dll", "ucrtbased.dll")
} else {
    @("MSVCP140.dll", "VCRUNTIME140.dll", "VCRUNTIME140_1.dll")
}
foreach ($crt in $crtDlls) {
    $found = Test-Path "$env:SystemRoot\System32\$crt"
    Write-Check "System32\$crt" $found $(if (-not $found) { "VCRedist niet geinstalleerd?" })
}
if ($isDebug) {
    Write-Warn "Debug build gedetecteerd" "Debug CRT DLLs zijn vaak niet beschikbaar voor COM-clients (VBScript, C#). Gebruik Release build."
}

# === 3. COM Registratie ===
Write-Host "`n--- 3. COM Registratie ---" -ForegroundColor Cyan
$comClasses = @(
    @{ Name = "SharedValue";     ProgID = "ATLProjectcomserver.SharedValue";     CLSID = "{71ef033e-e1b5-4985-bc7b-7687426bbdbb}" },
    @{ Name = "DatasetProxy";    ProgID = "ATLProjectcomserver.DatasetProxy";    CLSID = "{c1d2e3f4-a5b6-4c7d-8e9f-0a1b2c3d4e5f}" },
    @{ Name = "MathOperations";  ProgID = "ATLProjectcomserver.MathOperations";  CLSID = "{b97b4532-1222-4229-a090-d625c1e9532b}" }
)

foreach ($cls in $comClasses) {
    $progIdPath = "HKLM:\SOFTWARE\Classes\$($cls.ProgID)"
    $clsidPath  = "HKLM:\SOFTWARE\Classes\CLSID\$($cls.CLSID)\InprocServer32"

    $progIdExists = Test-Path $progIdPath
    Write-Check "$($cls.Name) ProgID" $progIdExists $cls.ProgID

    if (Test-Path $clsidPath) {
        $regDll = (Get-ItemProperty $clsidPath).'(default)'
        $threadModel = (Get-ItemProperty $clsidPath).ThreadingModel
        $dllMatch = ($regDll -eq $DllPath) -or ($regDll -eq (Resolve-Path $DllPath).Path)
        Write-Check "$($cls.Name) CLSID InprocServer32" (Test-Path $clsidPath) "DLL: $regDll"
        Write-Check "$($cls.Name) DLL pad klopt" $dllMatch $(if (-not $dllMatch) { "Verwacht: $DllPath" })
        Write-Check "$($cls.Name) ThreadingModel" ($threadModel -eq "Both") "ThreadingModel: $threadModel"
    } else {
        Write-Check "$($cls.Name) CLSID InprocServer32" $false "Niet geregistreerd!"
    }
    
    # AppID / Surrogate check
    $appIdKey = "HKLM:\SOFTWARE\Classes\CLSID\$($cls.CLSID)"
    if (Test-Path $appIdKey) {
        $subAppId = (Get-ItemProperty $appIdKey -ErrorAction SilentlyContinue).AppID
        if ($subAppId) {
            $surrogatePath = "HKLM:\SOFTWARE\Classes\AppID\$subAppId"
            if (Test-Path $surrogatePath) {
                # Check if DllSurrogate property exists (it's often an empty string)
                $hasSurrogate = (Get-ItemProperty $surrogatePath -ErrorAction SilentlyContinue).psobject.properties.match('DllSurrogate').Count -gt 0
                Write-Check "$($cls.Name) AppID DllSurrogate" $hasSurrogate "AppID: $subAppId"
            }
        }
    }
}

# === 4. TypeLib Registratie ===
Write-Host "`n--- 4. TypeLib Registratie ---" -ForegroundColor Cyan
$tlbGuid = "{86be8df9-669e-4443-b1a0-bd69ea9e40cc}"
$tlbRegBase = "HKLM:\SOFTWARE\Classes\TypeLib\$tlbGuid"
$tlbExists = Test-Path $tlbRegBase
Write-Check "TypeLib geregistreerd" $tlbExists $tlbGuid

if ($tlbExists) {
    # Zoek versie subkeys
    $versions = Get-ChildItem $tlbRegBase -ErrorAction SilentlyContinue
    foreach ($ver in $versions) {
        $winKey = Join-Path $ver.PSPath "0\win64"
        if (Test-Path $winKey) {
            $tlbFile = (Get-ItemProperty $winKey).'(default)'
            $tlbFileExists = Test-Path $tlbFile
            Write-Check "TypeLib bestand ($($ver.PSChildName))" $tlbFileExists "Pad: $tlbFile"
        } else {
            $winKey32 = Join-Path $ver.PSPath "0\win32"
            if (Test-Path $winKey32) {
                $tlbFile = (Get-ItemProperty $winKey32).'(default)'
                Write-Check "TypeLib bestand (win32)" (Test-Path $tlbFile) "Pad: $tlbFile"
            } else {
                Write-Warn "Geen win32/win64 subkey gevonden voor versie $($ver.PSChildName)"
            }
        }
    }
}

# === 5. Interface Registratie ===
Write-Host "`n--- 5. Interface Registratie ---" -ForegroundColor Cyan
$interfaces = @(
    @{ Name = "IDatasetProxy";  IID = "{d4e5f6a7-b8c9-4d0e-a1b2-c3d4e5f6a7b8}" },
    @{ Name = "ISharedValue";   IID = "{aced7d5d-0559-4e35-a972-42d9871ee14a}" },
    @{ Name = "IEventCallback"; IID = "{a3f1d8e2-7c4b-4a9e-b5d6-1e2f3a4b5c6d}" }
)
foreach ($iface in $interfaces) {
    $ifacePath = "HKLM:\SOFTWARE\Classes\Interface\$($iface.IID)"
    $exists = Test-Path $ifacePath
    Write-Check "$($iface.Name) interface" $exists $iface.IID
    if ($exists) {
        $proxyStub = Join-Path $ifacePath "ProxyStubClsid32"
        if (Test-Path $proxyStub) {
            $psClsid = (Get-ItemProperty $proxyStub).'(default)'
            Write-Check "$($iface.Name) ProxyStub" ($null -ne $psClsid) "CLSID: $psClsid"
        }
        $tlRef = Join-Path $ifacePath "TypeLib"
        if (Test-Path $tlRef) {
            $tlbRef = (Get-ItemProperty $tlRef).'(default)'
            Write-Check "$($iface.Name) TypeLib ref" ($tlbRef -eq $tlbGuid) "Ref: $tlbRef"
        }
    }
}

# === 6. Runtime Laadtest ===
Write-Host "`n--- 6. Runtime Laadtest ---" -ForegroundColor Cyan
try {
    $obj = New-Object -ComObject "ATLProjectcomserver.DatasetProxy"
    Write-Check "DatasetProxy kan worden aangemaakt" ($null -ne $obj) "COM instantiatie OK"
    
    # Methode test
    try {
        $count = $obj.GetRecordCount()
        Write-Check "GetRecordCount() werkt" ($true) "Count: $count"
    } catch {
        Write-Check "GetRecordCount() werkt" $false $_.Exception.Message
    }
    
    try {
        $obj.AddRow("diag_test", "diag_value")
        $val = $obj.GetRowData("diag_test")
        Write-Check "AddRow + GetRowData werkt" ($val -eq "diag_value") "Value: $val"
    } catch {
        Write-Check "AddRow + GetRowData werkt" $false $_.Exception.Message
    }
    
    [System.Runtime.InteropServices.Marshal]::ReleaseComObject($obj) | Out-Null
} catch {
    Write-Check "DatasetProxy kan worden aangemaakt" $false $_.Exception.Message
}

try {
    $sv = New-Object -ComObject "ATLProjectcomserver.SharedValue"
    Write-Check "SharedValue kan worden aangemaakt" ($null -ne $sv) "COM instantiatie OK"
    [System.Runtime.InteropServices.Marshal]::ReleaseComObject($sv) | Out-Null
} catch {
    Write-Check "SharedValue kan worden aangemaakt" $false $_.Exception.Message
}

# === Samenvatting ===
Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "  Resultaat: $script:passed OK, $script:failed FAIL, $script:warnings WARN" -ForegroundColor $(
    if ($script:failed -gt 0) { "Red" } elseif ($script:warnings -gt 0) { "Yellow" } else { "Green" }
)
Write-Host "========================================`n" -ForegroundColor Cyan

if ($script:failed -gt 0) {
    Write-Host "Aanbevelingen:" -ForegroundColor Yellow
    Write-Host "  1. Build met Release configuratie (geen debug CRT vereist)" -ForegroundColor DarkGray
    Write-Host "  2. Registreer met elevated: Start-Process regsvr32 -ArgumentList '/s `"<dll>`"' -Verb RunAs" -ForegroundColor DarkGray
    Write-Host "  3. Installeer VCRedist x64 als CRT DLLs ontbreken" -ForegroundColor DarkGray
    exit 1
}
exit 0
