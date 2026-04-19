param(
    [string]$ChannelName = "V5Test",
    [int]$EventCount = 5,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$rootDir = Split-Path -Parent $scriptDir

Write-Host "=== SharedValueV5 End-to-End Test ===" -ForegroundColor Cyan
Write-Host "Channel: $ChannelName | Events: $EventCount"
Write-Host ""

# --- Build ---
if (-not $SkipBuild) {
    Write-Host "[BUILD] Building C# core..." -ForegroundColor Yellow
    dotnet build "$rootDir\csharp_core" --nologo -v q 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL] C# build failed!" -ForegroundColor Red; exit 1 }
    
    Write-Host "[BUILD] Building C++ producer..." -ForegroundColor Yellow
    cmake --build "$rootDir\cpp_core\build" --config Release 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL] C++ build failed!" -ForegroundColor Red; exit 1 }
    
    Write-Host "[BUILD] All builds succeeded!" -ForegroundColor Green
}

# --- Paths ---
$cppProducer = "$rootDir\cpp_core\build\Release\cpp_producer.exe"

if (-not (Test-Path $cppProducer)) {
    Write-Host "[FAIL] C++ producer not found at: $cppProducer" -ForegroundColor Red
    exit 1
}

# --- Start order: Consumer FIRST (it blocks on Ready Event), then Producer ---
# This ensures the consumer is connected before the producer starts publishing.

Write-Host "[TEST] Starting C# Consumer first (waits for producer)..." -ForegroundColor Cyan
$consumerProc = Start-Process -FilePath "dotnet" `
    -ArgumentList "run","--project","$rootDir\csharp_core","--","--name",$ChannelName,"--count",$EventCount `
    -PassThru -NoNewWindow `
    -RedirectStandardOutput "$rootDir\tests\consumer_output.txt" `
    -RedirectStandardError "$rootDir\tests\consumer_error.txt"

# Give the consumer a moment to start and enter the wait state
Start-Sleep -Seconds 2

Write-Host "[TEST] Starting C++ Producer (host)..." -ForegroundColor Cyan
$producerProc = Start-Process -FilePath $cppProducer `
    -ArgumentList "--name", $ChannelName, "--count", ($EventCount + 2), "--delay", "2000" `
    -PassThru -NoNewWindow `
    -RedirectStandardOutput "$rootDir\tests\producer_output.txt" `
    -RedirectStandardError "$rootDir\tests\producer_error.txt"

# --- Wait for consumer (it exits after --count events) ---
$timeout = 60
Write-Host "[TEST] Waiting for consumer to receive $EventCount events (timeout: ${timeout}s)..." -ForegroundColor Yellow
$exited = $consumerProc.WaitForExit($timeout * 1000)

# --- Cleanup ---
if (-not $producerProc.HasExited) { $producerProc.Kill() }
if (-not $exited) {
    if (-not $consumerProc.HasExited) { $consumerProc.Kill() }
    Write-Host "[FAIL] Consumer timed out!" -ForegroundColor Red
    
    Write-Host "`n=== Consumer Output ===" -ForegroundColor DarkGray
    Get-Content "$rootDir\tests\consumer_output.txt" -ErrorAction SilentlyContinue
    Write-Host "`n=== Consumer Errors ===" -ForegroundColor DarkGray
    Get-Content "$rootDir\tests\consumer_error.txt" -ErrorAction SilentlyContinue
    exit 1
}

# --- Results ---
Write-Host ""
Write-Host "=== RESULTS ===" -ForegroundColor Cyan

$consumerOutput = Get-Content "$rootDir\tests\consumer_output.txt" -Raw -ErrorAction SilentlyContinue
$producerOutput = Get-Content "$rootDir\tests\producer_output.txt" -Raw -ErrorAction SilentlyContinue

$passed = $true

if ($exited -and $consumerOutput -match "Event #") {
    Write-Host "[PASS] Consumer exited successfully" -ForegroundColor Green
} else {
    Write-Host "[FAIL] Consumer did not complete properly" -ForegroundColor Red
    $passed = $false
}

# Count received events
$eventMatches = [regex]::Matches($consumerOutput, "Event #\d+")
$eventCount = $eventMatches.Count
if ($eventCount -ge $EventCount) {
    Write-Host "[PASS] Events: received $eventCount/$EventCount" -ForegroundColor Green
} else {
    Write-Host "[FAIL] Events: received $eventCount/$EventCount" -ForegroundColor Red
    $passed = $false
}

# Check multi-table
if ($consumerOutput -match "Sensoren") {
    Write-Host "[PASS] Multi-table: Sensoren table detected" -ForegroundColor Green
} else {
    Write-Host "[FAIL] Multi-table: Sensoren table NOT detected" -ForegroundColor Red
    $passed = $false
}

if ($consumerOutput -match "Alarmen") {
    Write-Host "[PASS] Multi-table: Alarmen table detected" -ForegroundColor Green
} else {
    Write-Host "[WARN] Alarmen table not always present (only every 3rd cycle)" -ForegroundColor Yellow
}

# Check schema autodiscovery
if ($consumerOutput -match "Temperature\(Double\)") {
    Write-Host "[PASS] Schema autodiscovery: Column types detected" -ForegroundColor Green
} else {
    Write-Host "[WARN] Could not verify schema autodiscovery from output" -ForegroundColor Yellow
}

# Check schema locking
if ($consumerOutput -match "locked") {
    Write-Host "[PASS] Schema lock: locked status detected" -ForegroundColor Green
} else {
    Write-Host "[WARN] Schema lock status not in output" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "=== Producer Output (last 5 lines) ===" -ForegroundColor DarkGray
$producerOutput -split "`n" | Select-Object -Last 5

Write-Host ""
Write-Host "=== Consumer Output (first 30 lines) ===" -ForegroundColor DarkGray
$consumerOutput -split "`n" | Select-Object -First 30

Write-Host ""
if ($passed) {
    Write-Host "=== ALL TESTS PASSED ===" -ForegroundColor Green
} else {
    Write-Host "=== SOME TESTS FAILED ===" -ForegroundColor Red
    exit 1
}
