$ErrorActionPreference = "Stop"

$workspaceRoot = (Resolve-Path "$PSScriptRoot\..\").Path
$v5Root = Join-Path $workspaceRoot "SharedValueV5"
$stagingDir = Join-Path $PSScriptRoot "staging"

function Get-OrInstallUv {
    $uvPath = Get-Command "uv" -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source
    if ([string]::IsNullOrWhiteSpace($uvPath)) {
        $localUv = Join-Path $env:USERPROFILE ".local\bin\uv.exe"
        $cargoUv = Join-Path $env:USERPROFILE ".cargo\bin\uv.exe"
        if (Test-Path $localUv) {
            $uvPath = $localUv
        } elseif (Test-Path $cargoUv) {
            $uvPath = $cargoUv
        } else {
            Write-Host "UV toolchain not found. Automatically installing uv..." -ForegroundColor Yellow
            Invoke-Expression ((New-Object System.Net.WebClient).DownloadString('https://astral.sh/uv/install.ps1'))
            
            if (Test-Path $localUv) { $uvPath = $localUv }
            elseif (Test-Path $cargoUv) { $uvPath = $cargoUv }
            else { throw "Failed to automatically install or locate uv." }
        }
    }
    return $uvPath
}

Write-Host "Creating Staging Directories at $stagingDir..." -ForegroundColor Cyan
if (Test-Path $stagingDir) { Remove-Item -Force -Recurse $stagingDir }
New-Item -ItemType Directory -Path "$stagingDir\dotnet" | Out-Null
New-Item -ItemType Directory -Path "$stagingDir\cpp" | Out-Null
New-Item -ItemType Directory -Path "$stagingDir\python" | Out-Null
New-Item -ItemType Directory -Path "$stagingDir\examples" | Out-Null

Write-Host "1. Building .NET 8 Dependencies (Core + COM Wrappers)" -ForegroundColor Cyan
Set-Location "$v5Root\com_wrapper"
dotnet publish -c Release -r win-x64 --self-contained false

$publishPath = "$v5Root\com_wrapper\bin\Release\net8.0\win-x64\publish"
Copy-Item "$publishPath\SharedValueV5.dll" "$stagingDir\dotnet\"
Copy-Item "$publishPath\SharedValueV5.Com.dll" "$stagingDir\dotnet\"
Copy-Item "$publishPath\SharedValueV5.Com.comhost.dll" "$stagingDir\dotnet\"
Copy-Item "$publishPath\*.json" "$stagingDir\dotnet\"

Write-Host "2. Copying C++ Source/Header SDK" -ForegroundColor Cyan
Copy-Item "$v5Root\cpp_core\*.hpp" "$stagingDir\cpp\"

Write-Host "3. Building Python C-Extensions via setuptools" -ForegroundColor Cyan
if (Test-Path "$v5Root\python_ext") {
    Set-Location "$v5Root\python_ext"
    
    $uvTool = Get-OrInstallUv
    # Execute via the UV toolchain explicitly requested by the user with dynamic dependencies
    & $uvTool run --with setuptools --with pybind11 python setup.py build_ext --inplace
    
    # Grab whatever .pyd was generated (like sharedvalue5.cp312-win_amd64.pyd or just sharedvalue5.pyd)
    $pydFiles = @(Get-ChildItem -Filter "*.pyd" -Recurse | Where-Object { $_.FullName -notmatch "\\\.venv\\" -and $_.FullName -notmatch "\\\.tox\\" })
    if ($pydFiles.Count -gt 0) {
        # Rename it strictly to sharedvalue5.pyd to match the WiX expectations
        Copy-Item $pydFiles[0].FullName "$stagingDir\python\sharedvalue5.pyd"
        if (Test-Path "*.pyi") {
            Copy-Item "*.pyi" "$stagingDir\python\"
        }
    } else {
        Write-Host "Warning: Python compilation failed or no .pyd emitted!" -ForegroundColor Yellow
    }
} else {
    Write-Host "Warning: python_ext source directory not present, skipping python components..." -ForegroundColor Yellow
}

Write-Host "4. Staging Examples" -ForegroundColor Cyan
Copy-Item "$v5Root\examples\vbs_producer_cursor.vbs" "$stagingDir\examples\"
Copy-Item "$v5Root\examples\vbs_producer_array.vbs" "$stagingDir\examples\"
Copy-Item "$v5Root\examples\vbs_consumer.vbs" "$stagingDir\examples\"

Write-Host "5. Compiling WiX Toolset Installer (MSI)" -ForegroundColor Cyan
Set-Location $PSScriptRoot
# You must have wix installed (dotnet tool install --global wix)
dotnet build SharedValueV5Installer.wixproj -property:Platform=x64 -c Release

Write-Host "Done! MSI created in bin/Release" -ForegroundColor Green
