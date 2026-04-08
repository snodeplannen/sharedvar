<#
.SYNOPSIS
    COM Server Diagnostisch Script — Controleert registratie, dependencies en TypeLib voor ZOWEL de DLL als de EXE variant.

.DESCRIPTION
    Gebruik dit script om snel te verifiëren of beide COM-server implementaties correct
    zijn geregistreerd in Windows.
#>
$ErrorActionPreference = 'SilentlyContinue'

$projects = @(
    @{
        Name = "In-Process DLL Server"
        FileName = "ATLProjectcomserver.dll"
        ServerKey = "InprocServer32"
        TypeLib = "{86be8df9-669e-4443-b1a0-bd69ea9e40cc}"
        Prefix = "ATLProjectcomserver"
        Classes = @(
            @{ Name = "SharedValue";     CLSID = "{71ef033e-e1b5-4985-bc7b-7687426bbdbb}" },
            @{ Name = "DatasetProxy";    CLSID = "{c1d2e3f4-a5b6-4c7d-8e9f-0a1b2c3d4e5f}" },
            @{ Name = "MathOperations";  CLSID = "{b97b4532-1222-4229-a090-d625c1e9532b}" }
        )
        Interfaces = @(
            @{ Name = "IDatasetProxy";  IID = "{d4e5f6a7-b8c9-4d0e-a1b2-c3d4e5f6a7b8}" },
            @{ Name = "ISharedValue";   IID = "{aced7d5d-0559-4e35-a972-42d9871ee14a}" },
            @{ Name = "IEventCallback"; IID = "{a3f1d8e2-7c4b-4a9e-b5d6-1e2f3a4b5c6d}" }
        )
    },
    @{
        Name = "Out-Of-Process EXE Server"
        FileName = "ATLProjectcomserver.exe"
        ServerKey = "LocalServer32"
        TypeLib = "{b0a0188f-59b6-42a5-ad3a-9d3cbe079253}"
        Prefix = "ATLProjectcomserverExe"
        Classes = @(
            @{ Name = "SharedValue";     CLSID = "{a5b21149-fb8c-4e50-8c52-65f3dc4afebf}" },
            @{ Name = "DatasetProxy";    CLSID = "{1d85075b-6ecb-4be4-8317-9dda91292ed8}" },
            @{ Name = "MathOperations";  CLSID = "{1ce8c512-fb0a-4c47-b3cd-44219bdc8ddf}" }
        )
        Interfaces = @(
            @{ Name = "IDatasetProxy";  IID = "{50d4d0db-6d9e-455f-8d6c-749cdbcf1949}" },
            @{ Name = "ISharedValue";   IID = "{8d55631f-1994-4f36-a3a3-d5b40eb0e0db}" },
            @{ Name = "IEventCallback"; IID = "{dec06bdb-7655-4e71-9937-110b78fcc576}" }
        )
    }
)

function Write-Check {
    param([string]$Name, [bool]$Ok, [string]$Detail = "")
    if ($Ok) {
        Write-Host "  [OK] $Name" -ForegroundColor Green
        if ($Detail) { Write-Host "       $Detail" -ForegroundColor DarkGray }
    } else {
        Write-Host "  [FAIL] $Name" -ForegroundColor Red
        if ($Detail) { Write-Host "         $Detail" -ForegroundColor Yellow }
    }
}

foreach ($proj in $projects) {
    Write-Host "`n========================================" -ForegroundColor Cyan
    Write-Host "  Diagnostiek: $($proj.Name)" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan

    Write-Host "`n--- 1. Klasse Registraties ---" -ForegroundColor Cyan
    foreach ($cls in $proj.Classes) {
        $progId = "$($proj.Prefix).$($cls.Name)"
        $progIdExists = Test-Path "HKLM:\SOFTWARE\Classes\$progId"
        Write-Check "$($cls.Name) ProgID" $progIdExists $progId

        $clsidPath = "HKLM:\SOFTWARE\Classes\CLSID\$($cls.CLSID)\$($proj.ServerKey)"
        if (Test-Path $clsidPath) {
            $regPath = (Get-ItemProperty $clsidPath).'(default)'
            Write-Check "$($cls.Name) CLSID $($proj.ServerKey)" $true "Pad: $regPath"
        } else {
            Write-Check "$($cls.Name) CLSID $($proj.ServerKey)" $false "Niet geregistreerd!"
        }
    }

    Write-Host "`n--- 2. TypeLib Registratie ---" -ForegroundColor Cyan
    $tlbExists = Test-Path "HKLM:\SOFTWARE\Classes\TypeLib\$($proj.TypeLib)"
    Write-Check "TypeLib geregistreerd" $tlbExists $proj.TypeLib

    Write-Host "`n--- 3. Interface & Proxy/Stub Registratie ---" -ForegroundColor Cyan
    foreach ($iface in $proj.Interfaces) {
        $ifacePath = "HKLM:\SOFTWARE\Classes\Interface\$($iface.IID)"
        $exists = Test-Path $ifacePath
        Write-Check "$($iface.Name) interface" $exists $iface.IID
        if ($exists) {
            $proxyStub = Join-Path $ifacePath "ProxyStubClsid32"
            if (Test-Path $proxyStub) {
                $psClsid = (Get-ItemProperty $proxyStub).'(default)'
                Write-Check "$($iface.Name) ProxyStub" ($null -ne $psClsid) "CLSID: $psClsid"
            }
        }
    }

    Write-Host "`n--- 4. Runtime Laadtest ---" -ForegroundColor Cyan
    try {
        $obj = New-Object -ComObject "$($proj.Prefix).SharedValue"
        Write-Check "SharedValue kan worden aangemaakt" ($null -ne $obj) "COM instantiatie OK"
        
        # Roep een simpele getter aan ter test of het proces werkelijk reageert
        try {
            $val = $obj.GetValue()
            Write-Check "ISharedValue::GetValue werkt over RPC" $true "Type: $(if ($null -eq $val) { 'Empty' } else { $val.GetType().Name })"
        } catch {
            Write-Check "ISharedValue::GetValue werkt over RPC" $false $_.Exception.Message
        }

        [System.Runtime.InteropServices.Marshal]::ReleaseComObject($obj) | Out-Null
    } catch {
        Write-Check "SharedValue kan worden aangemaakt" $false $_.Exception.Message
    }
}
Write-Host "`n=======================================`n" -ForegroundColor Cyan
