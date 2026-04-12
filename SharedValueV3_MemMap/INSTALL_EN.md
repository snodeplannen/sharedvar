# SharedValueV3 MemMap — Installation

This document describes how to build the SharedValueV3 MemMap implementation.

## Requirements
- Windows Operating System (required for Memory-Mapped Files kernel objects)
- CMake (for C++ project)
- MSVC compiler (Visual C++ workload in Visual Studio 2022)
- .NET 8.0 SDK or newer (for C# project)
- PowerShell (for running build scripts)

## 1. Building the FlatBuffers Schema
This project uses FlatBuffers for high-performance zero-copy serialization. The schema is located in `schema/schema.fbs`.

Run the provided script to generate the C++ headers and C# classes from this schema:
```powershell
.\build_schema.ps1
```
*(This uses the local `flatc.exe` inside `flatc_tools/` and places generated code in `cpp_core/dataset_generated.h` and `csharp_core/Generated/`)*

## 2. Building the C++ Producer (CMake)
The C++ application acts as the source of truth that shares data updates over shared memory.

```powershell
cd cpp_core
mkdir build
cd build
cmake ..
cmake --build . --config Release
```
*(The resulting executable `MemMapProducer.exe` will be located in `cpp_core/build/Release/`)*

## 3. Running the C# Consumer (.NET)
The C# application reads the data from shared memory extremely fast using the FlatBuffers library, managed automatically via NuGet in the project file.

```powershell
cd csharp_core
dotnet build
dotnet run
```

## Running Automated Tests
Once the environment is configured and built, you can run the cross-process integration tests locally:

```powershell
.\tests\Run-MemMapTests.ps1
```
See [tests/README_EN.md](tests/README_EN.md) for more details on test scenarios.
