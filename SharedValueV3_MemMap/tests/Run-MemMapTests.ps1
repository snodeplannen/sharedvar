<#
.SYNOPSIS
    SharedValueV3 MemMap — End-to-End Test Suite

.DESCRIPTION
    Geautomatiseerde cross-process integratietests voor de SharedValueV3 Memory-Mapped 
    FlatBuffers engine. Valideert de volledige producer → consumer pipeline inclusief:
    - Build verificatie (CMake C++ en dotnet C#)
    - Kernel object creatie (MMF, Mutex, Event)
    - FlatBuffer serialisatie en deserialisatie
    - Named Event signalering en callback delivery
    - Multi-row datasets met geneste objecten
    - Consumer-before-producer retry mechanisme
    - Graceful shutdown en resource cleanup

.PARAMETER SkipBuild
    Wanneer opgegeven wordt de build-stap overgeslagen (nuttig bij herhaalde tests).

.EXAMPLE
    .\Run-MemMapTests.ps1
    .\Run-MemMapTests.ps1 -SkipBuild
#>

param(
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$CppBuildDir = "$ProjectRoot\cpp_core\build\Release"
$CsharpDir = "$ProjectRoot\csharp_core"
$ProducerExe = "$CppBuildDir\MemMapProducer.exe"

$TestsPassed = 0
$TestsFailed = 0
$TestResults = @()

function Write-TestHeader($testNum, $title) {
    Write-Host "`n[$testNum] $title" -ForegroundColor Yellow
}

function Write-Pass($message) {
    $script:TestsPassed++
    $script:TestResults += @{ Name = $message; Status = "PASS" }
    Write-Host "    ✅ PASS: $message" -ForegroundColor Green
}

function Write-Fail($message) {
    $script:TestsFailed++
    $script:TestResults += @{ Name = $message; Status = "FAIL" }
    Write-Host "    ❌ FAIL: $message" -ForegroundColor Red
}

# =============================================
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host " SHAREDVALUE V3 MEMMAP — E2E TEST SUITE  " -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "Project Root: $ProjectRoot"
Write-Host "Timestamp:    $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"

# =============================================
# TEST 0: Build Verification
# =============================================
if (-not $SkipBuild) {
    Write-TestHeader 0 "BUILD VERIFICATION"
    
    Write-Host "    Building C++ Producer (CMake + MSVC)..." -ForegroundColor DarkGray
    Push-Location "$ProjectRoot\cpp_core"
    try {
        $buildOutput = cmake --build build --config Release 2>&1 | Out-String
        if ($LASTEXITCODE -eq 0) {
            Write-Pass "C++ Producer (MemMapProducer.exe) compiled successfully"
        } else {
            Write-Fail "C++ Producer build failed: $buildOutput"
        }
    } finally {
        Pop-Location
    }

    Write-Host "    Building C# Consumer (dotnet)..." -ForegroundColor DarkGray
    Push-Location $CsharpDir
    try {
        $buildOutput = dotnet build -c Release 2>&1 | Out-String
        if ($buildOutput -match "Build succeeded" -and $buildOutput -notmatch "Error\(s\):\s*[1-9]") {
            $warningMatch = [regex]::Match($buildOutput, "(\d+) Warning\(s\)")
            $warnings = if ($warningMatch.Success) { $warningMatch.Groups[1].Value } else { "0" }
            Write-Pass "C# Consumer compiled successfully ($warnings warnings)"
        } else {
            Write-Fail "C# Consumer build failed"
        }
    } finally {
        Pop-Location
    }
} else {
    Write-Host "`n[0] BUILD VERIFICATION — Skipped (-SkipBuild)" -ForegroundColor DarkGray
}

# Verify executables exist
if (-not (Test-Path $ProducerExe)) {
    Write-Host "`nFATAL: Producer executable not found at $ProducerExe" -ForegroundColor Red
    Write-Host "Run without -SkipBuild first." -ForegroundColor Red
    exit 1
}

# =============================================
# TEST 1: Basic Data Transfer (Producer → Consumer)
# =============================================
Write-TestHeader 1 "BASIC DATA TRANSFER (3 updates, 1 row each)"

# Start producer in background: send 3 updates at 1-second intervals
$producer = Start-Process -FilePath $ProducerExe `
    -ArgumentList "--count 3 --interval 1000 --rows 1 --name TestBasic" `
    -PassThru -NoNewWindow -RedirectStandardOutput "$env:TEMP\producer_basic.log" `
    -RedirectStandardError "$env:TEMP\producer_basic_err.log"

Start-Sleep -Milliseconds 500  # Let producer initialize

# Start consumer: expect 3 events, timeout 15 seconds
Push-Location $CsharpDir
$consumerOutput = dotnet run -c Release -- --name TestBasic --count 3 --timeout 15 2>&1 | Out-String
$consumerExit = $LASTEXITCODE
Pop-Location

# Wait for producer to finish
if (-not $producer.HasExited) { $producer.WaitForExit(10000) }
$producerLog = Get-Content "$env:TEMP\producer_basic.log" -Raw -ErrorAction SilentlyContinue

# Validate producer output
if ($producerLog -match "Completed 3 updates") {
    Write-Pass "Producer sent 3 updates"
} else {
    Write-Fail "Producer did not complete 3 updates"
    Write-Host "    Producer log: $producerLog" -ForegroundColor DarkGray
}

# Validate consumer received all events
if ($consumerOutput -match "Event #3 Received") {
    Write-Pass "Consumer received all 3 events"
} else {
    Write-Fail "Consumer did not receive 3 events"
    Write-Host "    Consumer output: $consumerOutput" -ForegroundColor DarkGray
}

# Validate FlatBuffer content
if ($consumerOutput -match "API Version: 1\.0\.0") {
    Write-Pass "FlatBuffer api_version field correctly deserialized"
} else {
    Write-Fail "FlatBuffer api_version not found in consumer output"
}

if ($consumerOutput -match "RowId=Sensor_") {
    Write-Pass "FlatBuffer DataRow.row_id correctly deserialized"
} else {
    Write-Fail "FlatBuffer DataRow.row_id not found"
}

if ($consumerOutput -match "Temp=\d+\.\d") {
    Write-Pass "FlatBuffer NestedDetails.temperature correctly deserialized"
} else {
    Write-Fail "FlatBuffer NestedDetails.temperature not found"
}

if ($consumerExit -eq 0) {
    Write-Pass "Consumer exit code = 0 (clean shutdown)"
} else {
    Write-Fail "Consumer exit code = $consumerExit (expected 0)"
}

Start-Sleep -Seconds 1  # Allow kernel objects to be released

# =============================================
# TEST 2: Multi-Row Dataset (5 rows per update)
# =============================================
Write-TestHeader 2 "MULTI-ROW DATASET (2 updates, 5 rows each)"

$producer = Start-Process -FilePath $ProducerExe `
    -ArgumentList "--count 2 --interval 1000 --rows 5 --name TestMultiRow" `
    -PassThru -NoNewWindow -RedirectStandardOutput "$env:TEMP\producer_multi.log" `
    -RedirectStandardError "$env:TEMP\producer_multi_err.log"

Start-Sleep -Milliseconds 500

Push-Location $CsharpDir
$consumerOutput = dotnet run -c Release -- --name TestMultiRow --count 2 --timeout 15 2>&1 | Out-String
Pop-Location

if (-not $producer.HasExited) { $producer.WaitForExit(10000) }

# Count rows in output
$rowMatches = [regex]::Matches($consumerOutput, "RowId=Sensor_")
if ($rowMatches.Count -ge 5) {
    Write-Pass "Multi-row dataset: found $($rowMatches.Count) DataRow entries across 2 events"
} else {
    Write-Fail "Multi-row dataset: expected ≥5 rows, found $($rowMatches.Count)"
    Write-Host "    Output: $consumerOutput" -ForegroundColor DarkGray
}

# Verify nested details are present for each row
$detailMatches = [regex]::Matches($consumerOutput, "Detail\[\d+\]")
if ($detailMatches.Count -ge 10) {
    Write-Pass "Nested details: found $($detailMatches.Count) NestedDetails entries (2 per row)"
} else {
    Write-Fail "Nested details: expected ≥10, found $($detailMatches.Count)"
}

# Verify different row IDs
if ($consumerOutput -match "Sensor_1_R0" -and $consumerOutput -match "Sensor_1_R4") {
    Write-Pass "Row IDs span from R0 to R4 within a single update"
} else {
    Write-Fail "Row IDs not correctly spanning all 5 rows"
}

Start-Sleep -Seconds 1

# =============================================
# TEST 3: Consumer-Before-Producer (Retry Mechanism)
# =============================================
Write-TestHeader 3 "CONSUMER-BEFORE-PRODUCER (retry mechanism)"

# Start consumer FIRST (producer not yet running)
$consumerJob = Start-Job -ScriptBlock {
    param($dir, $name)
    Push-Location $dir
    $output = dotnet run -c Release -- --name $name --count 1 --timeout 20 2>&1 | Out-String
    Pop-Location
    return $output
} -ArgumentList $CsharpDir, "TestRetry"

Write-Host "    Consumer started first (producer not yet running)..." -ForegroundColor DarkGray
Start-Sleep -Seconds 3  # Let consumer retry a few times

# Now start producer
Write-Host "    Starting producer after 3-second delay..." -ForegroundColor DarkGray
$producer = Start-Process -FilePath $ProducerExe `
    -ArgumentList "--count 1 --interval 500 --rows 1 --name TestRetry" `
    -PassThru -NoNewWindow -RedirectStandardOutput "$env:TEMP\producer_retry.log" `
    -RedirectStandardError "$env:TEMP\producer_retry_err.log"

# Wait for consumer job to complete
$consumerOutput = Receive-Job -Job $consumerJob -Wait -AutoRemoveJob
if (-not $producer.HasExited) { $producer.WaitForExit(10000) }

if ($consumerOutput -match "Successfully connected") {
    Write-Pass "Consumer successfully connected after producer started"
} else {
    Write-Fail "Consumer did not connect after producer started"
    Write-Host "    Output: $consumerOutput" -ForegroundColor DarkGray
}

if ($consumerOutput -match "Event #1 Received") {
    Write-Pass "Consumer received data despite starting before producer"
} else {
    Write-Fail "Consumer did not receive data after retry"
}

Start-Sleep -Seconds 1

# =============================================
# TEST 4: Consumer Connection Timeout
# =============================================
Write-TestHeader 4 "CONSUMER CONNECTION TIMEOUT (no producer)"

Push-Location $CsharpDir
$timeStart = Get-Date
$consumerOutput = dotnet run -c Release -- --name NonExistentEngine --count 1 --timeout 3 2>&1 | Out-String
$consumerExit = $LASTEXITCODE
$elapsed = ((Get-Date) - $timeStart).TotalSeconds
Pop-Location

if ($consumerExit -eq 2) {
    Write-Pass "Consumer returned exit code 2 (connection timeout)"
} else {
    Write-Fail "Consumer exit code = $consumerExit (expected 2 for timeout)"
}

if ($consumerOutput -match "Connection timeout") {
    Write-Pass "Consumer reported timeout error message"
} else {
    Write-Fail "Consumer did not report timeout error"
}

if ($elapsed -ge 2.5 -and $elapsed -le 8) {
    Write-Pass "Timeout occurred within expected time range ($([math]::Round($elapsed, 1))s)"
} else {
    Write-Fail "Timeout timing unexpected: $([math]::Round($elapsed, 1))s (expected 3-6s)"
}

Start-Sleep -Seconds 1

# =============================================
# TEST 5: Rapid-Fire Updates (Stress Test)
# =============================================
Write-TestHeader 5 "RAPID-FIRE UPDATES (10 updates at 200ms intervals)"

$producer = Start-Process -FilePath $ProducerExe `
    -ArgumentList "--count 10 --interval 200 --rows 2 --name TestRapid" `
    -PassThru -NoNewWindow -RedirectStandardOutput "$env:TEMP\producer_rapid.log" `
    -RedirectStandardError "$env:TEMP\producer_rapid_err.log"

Start-Sleep -Milliseconds 300

Push-Location $CsharpDir
$consumerOutput = dotnet run -c Release -- --name TestRapid --count 10 --timeout 20 2>&1 | Out-String
$consumerExit = $LASTEXITCODE
Pop-Location

if (-not $producer.HasExited) { $producer.WaitForExit(10000) }
$producerLog = Get-Content "$env:TEMP\producer_rapid.log" -Raw -ErrorAction SilentlyContinue

if ($producerLog -match "Completed 10 updates") {
    Write-Pass "Producer sent 10 rapid-fire updates"
} else {
    Write-Fail "Producer did not complete 10 rapid updates"
}

# Count how many events consumer received (may miss some due to auto-reset event)
$eventMatches = [regex]::Matches($consumerOutput, "Event #\d+ Received")
$received = $eventMatches.Count
if ($received -ge 5) {
    Write-Pass "Consumer received $received/10 events under rapid-fire (≥5 expected due to auto-reset coalescing)"
} else {
    Write-Fail "Consumer received only $received/10 events under rapid-fire"
    Write-Host "    Output: $consumerOutput" -ForegroundColor DarkGray
}

if ($consumerExit -eq 0) {
    Write-Pass "Consumer exited cleanly after rapid-fire test"
} else {
    Write-Fail "Consumer exit code = $consumerExit after rapid-fire"
}

Start-Sleep -Seconds 1

# =============================================
# TEST 6: Boolean Toggle Validation
# =============================================
Write-TestHeader 6 "BOOLEAN FIELD TOGGLE (is_active toggles every 3rd update)"

$producer = Start-Process -FilePath $ProducerExe `
    -ArgumentList "--count 4 --interval 500 --rows 1 --name TestBool" `
    -PassThru -NoNewWindow -RedirectStandardOutput "$env:TEMP\producer_bool.log" `
    -RedirectStandardError "$env:TEMP\producer_bool_err.log"

Start-Sleep -Milliseconds 300

Push-Location $CsharpDir
$consumerOutput = dotnet run -c Release -- --name TestBool --count 4 --timeout 15 2>&1 | Out-String
Pop-Location

if (-not $producer.HasExited) { $producer.WaitForExit(10000) }

# Update 1,2 should have Active=True, update 3 should have Active=False, update 4 Active=True
if ($consumerOutput -match "Active=True" -and $consumerOutput -match "Active=False") {
    Write-Pass "Boolean is_active field correctly toggles between True and False"
} else {
    Write-Fail "Boolean field toggle not observed in consumer output"
    Write-Host "    Output: $consumerOutput" -ForegroundColor DarkGray
}

# =============================================
# RESULTS SUMMARY
# =============================================
Write-Host "`n==========================================" -ForegroundColor Cyan
Write-Host " TEST RESULTS SUMMARY                     " -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

foreach ($result in $TestResults) {
    $color = if ($result.Status -eq "PASS") { "Green" } else { "Red" }
    $icon = if ($result.Status -eq "PASS") { "✅" } else { "❌" }
    Write-Host "  $icon $($result.Name)" -ForegroundColor $color
}

$total = $TestsPassed + $TestsFailed
Write-Host "`n  Total: $total | " -NoNewline
Write-Host "Passed: $TestsPassed" -ForegroundColor Green -NoNewline
Write-Host " | " -NoNewline
if ($TestsFailed -gt 0) {
    Write-Host "Failed: $TestsFailed" -ForegroundColor Red
} else {
    Write-Host "Failed: 0" -ForegroundColor Green
}

Write-Host "`n==========================================" -ForegroundColor Cyan

# Cleanup temp files
Remove-Item "$env:TEMP\producer_*.log" -Force -ErrorAction SilentlyContinue

if ($TestsFailed -gt 0) {
    exit 1
} else {
    exit 0
}
