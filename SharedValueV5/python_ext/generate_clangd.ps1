$ErrorActionPreference = "Stop"

Write-Host "Generating .clangd for local IntelliSense C++ linting..." -ForegroundColor Cyan

# Ensure uv is installed
$uvPath = Get-Command "uv" -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source
if ([string]::IsNullOrWhiteSpace($uvPath)) {
    $uvLocal = Join-Path $env:USERPROFILE ".local\bin\uv.exe"
    $uvCargo = Join-Path $env:USERPROFILE ".cargo\bin\uv.exe"
    if (Test-Path $uvLocal) { $uvPath = $uvLocal }
    elseif (Test-Path $uvCargo) { $uvPath = $uvCargo }
    else { throw "UV is not installed. Please install UV before generating clangd files." }
}

# Ask python for header locations dynamically inside the isolated uv toolchain
$includeScript = @"
import sysconfig
import os
try:
    import pybind11
    print(sysconfig.get_path('include'))
    print(pybind11.get_include())
except ImportError:
    pass
"@

Write-Host "Executing UV context to discover CPython & Pybind11 include dirs..."
# Create a temporary file because passing long inline strings to uv run python on windows can sometimes be problematic
$tempPy = Join-Path $env:TEMP "get_includes.py"
Set-Content $tempPy $includeScript

# Execute with --with to guarantee pybind11 exists during probing
$paths = & $uvPath run --quiet --with pybind11 python $tempPy
Remove-Item $tempPy -ErrorAction SilentlyContinue

if ($paths.Count -lt 2) {
    throw "Failed to probe Python/Pybind11 include paths. Output was: $($paths -join ' ')"
}

$pyInclude = $paths[0].Replace("\", "/")
$pbInclude = $paths[1].Replace("\", "/")
$cppCore = (Resolve-Path "..\cpp_core").Path.Replace("\", "/")

$clangdFormat = @"
CompileFlags:
  Add:
    - "-I$pyInclude"
    - "-I$pbInclude"
    - "-I$cppCore"
    - "-std=c++17"
    - "-D_MSVC_LANG=201703L"
    - "-DWIN32"
    - "-D_WINDOWS"
"@

# Write it to the python_ext directory specifically
$target = Join-Path $PSScriptRoot ".clangd"
Set-Content $target $clangdFormat

Write-Host "Successfully generated .clangd at $target" -ForegroundColor Green
