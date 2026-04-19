# Installation & Testing Guide

Since this project consists of a C++ COM Library (Windows) on one hand and a modern C++ header-only module (`SharedValueV2`) on the other, there are two respective ways to compile it depending on what you intend to achieve.

## 1. Building the Main COM Server (DLL)

To successfully build the `ATLProjectcomserver.dll`, you need the Enterprise build tools (or at least standard desktop dev tools).

1. Install **Visual Studio 2022** and select at least `Desktop development with C++` and `C++ ATL for latest v143 build tools` inside the installer.
2. Open the root solution in Visual Studio: `ATLProjectcomserver.sln`.
3. Choose the desired target at the top of the menu (usually: `Debug` | `x64`).   
4. Press `F7` or click `Build` > `Build Solution`. Visual Studio compiles all files (including merging the new SharedValueV2 headers).
5. Note: Your application will reside inside `\x64\Debug\ATLProjectcomserver.dll`.

### Registering the COM Server
Before a Windows program (such as C# or Python) can talk internally to these COM objects ("ProgIDs" like `ATLProjectcomserver.SharedValue`) or interfaces, the DLL must be registered with the OS.

Open your Terminal as **Administrator** and execute this:
```cmd
regsvr32 C:\yourpath\csharp\cursor_com_test\x64\Debug\ATLProjectcomserver.dll
```
*Use the following command for removal (unregister): `regsvr32 /u [path_to_dll]`*.

---

## 2. Building the C++ Tests (CMake)
If you do not wish to deal with COM and only want to engine-test the pure C++ architecture of SharedValue (possibly within your CI/CD system):

1. Ensure CMake (`cmake`) is reachable on your path in your terminal.
2. Navigate to the sub-directory `SharedValueV2`:
```cmd
cd c:\csharp\cursor_com_test\SharedValueV2
```
3. Generate the build environment and compile iterating release-mode:
```cmd
cmake -B build
cmake --build build --config Release
```
4. Successfully play back the generated executable tests afterwards. This is also how you check if the machine suffers from deadlocks/freezes:

```cmd
.\build\tests\Release\UnitTests.exe
.\build\tests\Release\StressTest.exe
```


## Related Documentation

- [README_EN.md](README_EN.md) — User guide and overview of the EXE COM Server variant.
- [ARCHITECTURE_EN.md](ARCHITECTURE_EN.md) — In-depth technical architecture of the Out-of-Process EXE COM Server, iteracting via RPC marshaling and the singleton lifecycle.
- [README_EN.md](../README_EN.md) — Main documentation and starting point of the entire project.
- [ARCHITECTURE_EN.md](../ARCHITECTURE_EN.md) — Main architecture document for the entire COM Server project.
- [README_EN.md](../SharedValueV2/README_EN.md) — Introduction and overview of the SharedValueV2 C++20 engine.
