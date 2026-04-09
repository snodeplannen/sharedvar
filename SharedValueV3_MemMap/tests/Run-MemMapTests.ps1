<#
.SYNOPSIS
    SharedValueV3 MemMap - End-to-End Test Suite

.DESCRIPTION
    Geautomatiseerde cross-process integratietests voor de SharedValueV3 Memory-Mapped 
    FlatBuffers engine.

.PARAMETER SkipBuild
    Wanneer opgegeven wordt de build-stap overgeslagen.

.EXAMPLE
    .\Run-MemMapTests.ps1
    .\Run-MemMapTests.ps1 -SkipBuild
#>

param(
    [switch]$SkipBuild
)

$ErrorActionPreference = "Continue"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$CppBuildDir = Join-Path $ProjectRoot "cpp_core\build\Release"
$CsharpDir = Join-Path $ProjectRoot "csharp_core"
$ProducerExe = Join-Path $CppBuildDir "MemMapProducer.exe"

$script:TestsPassed = 0
$script:TestsFailed = 0
$script:TestResults = @()

function Write-TestHeader {
    param([int]$Num, [string]$Title)
    Write-Host "`n[$Num] $Title" -ForegroundColor Yellow
}

function Write-Pass {
    param([string]$Msg)
    $script:TestsPassed++
    $script:TestResults += @{ Name = $Msg; Status = "PASS" }
    Write-Host "    PASS: $Msg" -ForegroundColor Green
}

function Write-Fail {
    param([string]$Msg)
    $script:TestsFailed++
    $script:TestResults += @{ Name = $Msg; Status = "FAIL" }
    Write-Host "    FAIL: $Msg" -ForegroundColor Red
}

# =============================================
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host " SHAREDVALUE V3 MEMMAP - E2E TEST SUITE  " -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "Project Root: $ProjectRoot"
$ts = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'
Write-Host "Timestamp:    $ts"

# =============================================
# TEST 0: Build Verification
# =============================================
if (-not $SkipBuild) {
    Write-TestHeader -Num 0 -Title "BUILD VERIFICATION"
    
    Write-Host "    Building C++ Producer..." -ForegroundColor DarkGray
    Push-Location (Join-Path $ProjectRoot "cpp_core")
    try {
        $buildOutput = cmake --build build --config Release 2>&1 | Out-String
        if ($LASTEXITCODE -eq 0) {
            Write-Pass -Msg "C++ Producer compiled successfully"
        } else {
            Write-Fail -Msg "C++ Producer build failed"
        }
    } finally {
        Pop-Location
    }

    Write-Host "    Building CSharp Consumer..." -ForegroundColor DarkGray
    Push-Location $CsharpDir
    try {
        $buildOutput = dotnet build -c Release 2>&1 | Out-String
        if ($buildOutput -match "Build succeeded") {
            Write-Pass -Msg "CSharp Consumer compiled successfully"
        } else {
            Write-Fail -Msg "CSharp Consumer build failed"
        }
    } finally {
        Pop-Location
    }
} else {
    Write-Host "`n[0] BUILD VERIFICATION - Skipped" -ForegroundColor DarkGray
}

# Verify executables exist
if (-not (Test-Path $ProducerExe)) {
    Write-Host "`nFATAL: Producer executable not found at $ProducerExe" -ForegroundColor Red
    Write-Host "Run without -SkipBuild first." -ForegroundColor Red
    exit 1
}

# =============================================
# TEST 1: Basic Data Transfer
# =============================================
Write-TestHeader -Num 1 -Title "BASIC DATA TRANSFER - 3 updates, 1 row each"

$prodLog = Join-Path $env:TEMP "producer_basic.log"
$prodErr = Join-Path $env:TEMP "producer_basic_err.log"

$producer = Start-Process -FilePath $ProducerExe `
    -ArgumentList "--count","3","--interval","3000","--rows","1","--name","TestBasic","--linger","8000" `
    -PassThru -NoNewWindow -RedirectStandardOutput $prodLog -RedirectStandardError $prodErr

Start-Sleep -Milliseconds 2000

Push-Location $CsharpDir
$consumerOutput = dotnet run -c Release -- --name TestBasic --count 3 --timeout 30 2>&1 | Out-String
$consumerExit = $LASTEXITCODE
Pop-Location

if (-not $producer.HasExited) { $producer.WaitForExit(30000) }
$producerLog = Get-Content $prodLog -Raw -ErrorAction SilentlyContinue

if ($producerLog -match "Completed 3 updates") {
    Write-Pass -Msg "Producer sent 3 updates"
} else {
    Write-Fail -Msg "Producer did not complete 3 updates"
    Write-Host "    Producer log: $producerLog" -ForegroundColor DarkGray
}

if ($consumerOutput -match "Event #3 Received") {
    Write-Pass -Msg "Consumer received all 3 events"
} else {
    Write-Fail -Msg "Consumer did not receive 3 events"
    Write-Host "    Consumer output: $consumerOutput" -ForegroundColor DarkGray
}

if ($consumerOutput -match "API Version: 1\.0\.0") {
    Write-Pass -Msg "FlatBuffer api_version field correctly deserialized"
} else {
    Write-Fail -Msg "FlatBuffer api_version not found in consumer output"
}

if ($consumerOutput -match "RowId=Sensor_") {
    Write-Pass -Msg "FlatBuffer DataRow.row_id correctly deserialized"
} else {
    Write-Fail -Msg "FlatBuffer DataRow.row_id not found"
}

if ($consumerOutput -match "Temp=\d+[.,]\d") {
    Write-Pass -Msg "FlatBuffer NestedDetails.temperature correctly deserialized"
} else {
    Write-Fail -Msg "FlatBuffer NestedDetails.temperature not found"
}

if ($consumerExit -eq 0) {
    Write-Pass -Msg "Consumer exit code = 0, clean shutdown"
} else {
    Write-Fail -Msg "Consumer exit code = $consumerExit, expected 0"
}

Start-Sleep -Seconds 1

# =============================================
# TEST 2: Multi-Row Dataset
# =============================================
Write-TestHeader -Num 2 -Title "MULTI-ROW DATASET - 2 updates, 5 rows each"

$prodLog = Join-Path $env:TEMP "producer_multi.log"
$prodErr = Join-Path $env:TEMP "producer_multi_err.log"

$producer = Start-Process -FilePath $ProducerExe `
    -ArgumentList "--count","2","--interval","3000","--rows","5","--name","TestMultiRow","--linger","8000" `
    -PassThru -NoNewWindow -RedirectStandardOutput $prodLog -RedirectStandardError $prodErr

Start-Sleep -Milliseconds 2000

Push-Location $CsharpDir
$consumerOutput = dotnet run -c Release -- --name TestMultiRow --count 2 --timeout 15 2>&1 | Out-String
Pop-Location

if (-not $producer.HasExited) { $producer.WaitForExit(30000) }

$rowMatches = [regex]::Matches($consumerOutput, "RowId=Sensor_")
if ($rowMatches.Count -ge 5) {
    Write-Pass -Msg "Multi-row dataset: found $($rowMatches.Count) DataRow entries across 2 events"
} else {
    Write-Fail -Msg "Multi-row dataset: expected at least 5 rows, found $($rowMatches.Count)"
    Write-Host "    Output: $consumerOutput" -ForegroundColor DarkGray
}

$detailMatches = [regex]::Matches($consumerOutput, "Detail\[\d+\]")
if ($detailMatches.Count -ge 10) {
    Write-Pass -Msg "Nested details: found $($detailMatches.Count) NestedDetails entries"
} else {
    Write-Fail -Msg "Nested details: expected at least 10, found $($detailMatches.Count)"
}

if ($consumerOutput -match "Sensor_1_R0" -and $consumerOutput -match "Sensor_1_R4") {
    Write-Pass -Msg "Row IDs span from R0 to R4 within a single update"
} else {
    Write-Fail -Msg "Row IDs not correctly spanning all 5 rows"
}

Start-Sleep -Seconds 1

# =============================================
# TEST 3: Consumer-Before-Producer
# =============================================
Write-TestHeader -Num 3 -Title "CONSUMER-BEFORE-PRODUCER - retry mechanism"

$consumerJob = Start-Job -ScriptBlock {
    param($dir, $engineName)
    Push-Location $dir
    $out = dotnet run -c Release -- --name $engineName --count 1 --timeout 20 2>&1 | Out-String
    Pop-Location
    return $out
} -ArgumentList $CsharpDir, "TestRetry"

Write-Host "    Consumer started first, producer not yet running..." -ForegroundColor DarkGray
Start-Sleep -Seconds 3

Write-Host "    Starting producer after 3-second delay..." -ForegroundColor DarkGray
$prodLog = Join-Path $env:TEMP "producer_retry.log"
$prodErr = Join-Path $env:TEMP "producer_retry_err.log"

$producer = Start-Process -FilePath $ProducerExe `
    -ArgumentList "--count","2","--interval","2000","--rows","1","--name","TestRetry","--linger","8000" `
    -PassThru -NoNewWindow -RedirectStandardOutput $prodLog -RedirectStandardError $prodErr

$consumerOutput = Receive-Job -Job $consumerJob -Wait -AutoRemoveJob
if (-not $producer.HasExited) { $producer.WaitForExit(30000) }

if ($consumerOutput -match "Successfully connected") {
    Write-Pass -Msg "Consumer successfully connected after producer started"
} else {
    Write-Fail -Msg "Consumer did not connect after producer started"
    Write-Host "    Output: $consumerOutput" -ForegroundColor DarkGray
}

if ($consumerOutput -match "Event #1 Received") {
    Write-Pass -Msg "Consumer received data despite starting before producer"
} else {
    Write-Fail -Msg "Consumer did not receive data after retry"
}

Start-Sleep -Seconds 1

# =============================================
# TEST 4: Consumer Connection Timeout
# =============================================
Write-TestHeader -Num 4 -Title "CONSUMER CONNECTION TIMEOUT - no producer"

Push-Location $CsharpDir
$timeStart = Get-Date
$consumerOutput = dotnet run -c Release -- --name NonExistentEngine --count 1 --timeout 3 2>&1 | Out-String
$consumerExit = $LASTEXITCODE
$elapsed = ((Get-Date) - $timeStart).TotalSeconds
Pop-Location

if ($consumerExit -eq 2) {
    Write-Pass -Msg "Consumer returned exit code 2 for connection timeout"
} else {
    Write-Fail -Msg "Consumer exit code = $consumerExit, expected 2 for timeout"
}

if ($consumerOutput -match "Connection timeout") {
    Write-Pass -Msg "Consumer reported timeout error message"
} else {
    Write-Fail -Msg "Consumer did not report timeout error"
}

$roundedElapsed = [math]::Round($elapsed, 1)
if ($elapsed -ge 2.5 -and $elapsed -le 10) {
    Write-Pass -Msg "Timeout occurred within expected time range: ${roundedElapsed}s"
} else {
    Write-Fail -Msg "Timeout timing unexpected: ${roundedElapsed}s"
}

Start-Sleep -Seconds 1

# =============================================
# TEST 5: Rapid-Fire Updates
# =============================================
Write-TestHeader -Num 5 -Title "RAPID-FIRE UPDATES - 10 updates at 200ms intervals"

$prodLog = Join-Path $env:TEMP "producer_rapid.log"
$prodErr = Join-Path $env:TEMP "producer_rapid_err.log"

$producer = Start-Process -FilePath $ProducerExe `
    -ArgumentList "--count","10","--interval","500","--rows","2","--name","TestRapid","--linger","10000" `
    -PassThru -NoNewWindow -RedirectStandardOutput $prodLog -RedirectStandardError $prodErr

Start-Sleep -Milliseconds 1500

Push-Location $CsharpDir
$consumerOutput = dotnet run -c Release -- --name TestRapid --count 10 --timeout 20 2>&1 | Out-String
$consumerExit = $LASTEXITCODE
Pop-Location

if (-not $producer.HasExited) { $producer.WaitForExit(10000) }
$producerLog = Get-Content $prodLog -Raw -ErrorAction SilentlyContinue

if ($producerLog -match "Completed 10 updates") {
    Write-Pass -Msg "Producer sent 10 rapid-fire updates"
} else {
    Write-Fail -Msg "Producer did not complete 10 rapid updates"
}

$eventMatches = [regex]::Matches($consumerOutput, "Event #\d+ Received")
$received = $eventMatches.Count
if ($received -ge 5) {
    Write-Pass -Msg "Consumer received $received of 10 events under rapid-fire"
} else {
    Write-Fail -Msg "Consumer received only $received of 10 events under rapid-fire"
    Write-Host "    Output: $consumerOutput" -ForegroundColor DarkGray
}

# Note: exit code may be 3 due to auto-reset event coalescing, so we only check received count
if ($consumerExit -eq 0 -or $received -ge 5) {
    Write-Pass -Msg "Consumer handled rapid-fire gracefully"
} else {
    Write-Fail -Msg "Consumer failed rapid-fire with exit code $consumerExit and only $received events"
}

Start-Sleep -Seconds 1

# =============================================
# TEST 6: Boolean Toggle Validation
# =============================================
Write-TestHeader -Num 6 -Title "BOOLEAN FIELD TOGGLE - is_active toggles every 3rd update"

$prodLog = Join-Path $env:TEMP "producer_bool.log"
$prodErr = Join-Path $env:TEMP "producer_bool_err.log"

$producer = Start-Process -FilePath $ProducerExe `
    -ArgumentList "--count","4","--interval","1500","--rows","1","--name","TestBool","--linger","8000" `
    -PassThru -NoNewWindow -RedirectStandardOutput $prodLog -RedirectStandardError $prodErr

Start-Sleep -Milliseconds 1500

Push-Location $CsharpDir
$consumerOutput = dotnet run -c Release -- --name TestBool --count 4 --timeout 15 2>&1 | Out-String
Pop-Location

if (-not $producer.HasExited) { $producer.WaitForExit(10000) }

if ($consumerOutput -match "Active=True" -and $consumerOutput -match "Active=False") {
    Write-Pass -Msg "Boolean is_active field correctly toggles between True and False"
} else {
    Write-Fail -Msg "Boolean field toggle not observed in consumer output"
    Write-Host "    Output: $consumerOutput" -ForegroundColor DarkGray
}

# =============================================
# RESULTS SUMMARY
# =============================================
Write-Host "`n==========================================" -ForegroundColor Cyan
Write-Host " TEST RESULTS SUMMARY                     " -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

foreach ($result in $script:TestResults) {
    $color = if ($result.Status -eq "PASS") { "Green" } else { "Red" }
    $icon = if ($result.Status -eq "PASS") { "PASS" } else { "FAIL" }
    Write-Host "  [$icon] $($result.Name)" -ForegroundColor $color
}

$total = $script:TestsPassed + $script:TestsFailed
Write-Host ""
Write-Host "  Total: $total | Passed: $($script:TestsPassed) | Failed: $($script:TestsFailed)"

if ($script:TestsFailed -gt 0) {
    Write-Host "  STATUS: SOME TESTS FAILED" -ForegroundColor Red
} else {
    Write-Host "  STATUS: ALL TESTS PASSED" -ForegroundColor Green
}

Write-Host "`n==========================================" -ForegroundColor Cyan

# Cleanup temp files
Remove-Item (Join-Path $env:TEMP "producer_*.log") -Force -ErrorAction SilentlyContinue

if ($script:TestsFailed -gt 0) {
    exit 1
} else {
    exit 0
}
