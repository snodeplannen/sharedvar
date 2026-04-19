$ErrorActionPreference = "Stop"

$workspaceRoot = (Resolve-Path "..\").Path
$v5Root = Join-Path $workspaceRoot "SharedValueV5"
$stagingDir = Join-Path $PSScriptRoot "staging"

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
    if (Get-Command "python" -ErrorAction SilentlyContinue) {
        # Execute via the UV toolchain explicitly requested by the user
        & "C:\Users\administrator\.local\bin\uv.exe" run python setup.py build_ext --inplace
        # Grab whatever .pyd was generated (like sharedvalue5.cp312-win_amd64.pyd or just sharedvalue5.pyd)
        $pydFiles = Get-ChildItem -Filter "*.pyd"
        if ($pydFiles.Count -gt 0) {
            # Rename it strictly to sharedvalue5.pyd to match the WiX expectations
            Copy-Item $pydFiles[0].FullName "$stagingDir\python\sharedvalue5.pyd"
            Copy-Item "*.pyi" "$stagingDir\python\"
        } else {
            Write-Host "Warning: Python compilation failed or no .pyd emitted!" -ForegroundColor Yellow
        }
    } else {
        Write-Host "Warning: Python executable not found in PATH, skipping compilation..." -ForegroundColor Yellow
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
dotnet build SharedValueV5Installer.wixproj -a x64 -c Release

Write-Host "Done! MSI created in bin/Release" -ForegroundColor Green
