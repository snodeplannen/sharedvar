$ErrorActionPreference = "Stop"

$FLATC_VERSION = "24.3.25"
$FLATC_URL = "https://github.com/google/flatbuffers/releases/download/v$FLATC_VERSION/Windows.flatc.binary.zip"
$ZIP_PATH = "$PSScriptRoot\flatc.zip"
$EXTRACT_DIR = "$PSScriptRoot\flatc_tools"
$FLATC_EXE = "$EXTRACT_DIR\flatc.exe"

# 1. Download and extract flatc if not present
if (-not (Test-Path $FLATC_EXE)) {
    Write-Host "Downloading flatc.exe version $FLATC_VERSION..."
    Invoke-WebRequest -Uri $FLATC_URL -OutFile $ZIP_PATH
    Write-Host "Extracting to $EXTRACT_DIR..."
    Expand-Archive -Path $ZIP_PATH -DestinationPath $EXTRACT_DIR -Force
    Remove-Item $ZIP_PATH -Force
}

# 2. Compile schemas
$SCHEMA_DIR = "$PSScriptRoot\schema"
$CPP_OUT = "$PSScriptRoot\cpp_core"
$CS_OUT = "$PSScriptRoot\csharp_core\Generated"

# Ensure output directories exist
if (-not (Test-Path $CS_OUT)) { New-Item -ItemType Directory -Path $CS_OUT | Out-Null }

Write-Host "Compiling FlatBuffers schema..."
# --cpp for C++ header, --csharp for C# classes
& $FLATC_EXE --cpp --csharp -o $CPP_OUT $SCHEMA_DIR\dataset.fbs
& $FLATC_EXE --csharp -o $CS_OUT $SCHEMA_DIR\dataset.fbs

if ($LASTEXITCODE -eq 0) {
    Write-Host "Schema successfully compiled!" -ForegroundColor Green
} else {
    Write-Host "Error compiling schema!" -ForegroundColor Red
}
