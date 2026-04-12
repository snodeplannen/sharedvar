# Changelog

All notable changes to this project will be documented in this file.

## [0.2.0] - 2026-04-09

### Added
- **End-to-End Test Suite**: Implemented `Run-MemMapTests.ps1` with 6 automated test scenarios covering basic data transfer, multi-row datasets, consumer-before-producer retry, connection timeout, rapid-fire stress testing, and boolean field validation.
- **CLI Arguments**: Extended both producer (`--count`, `--interval`, `--rows`, `--name`, `--linger`) and consumer (`--count`, `--timeout`, `--name`) with command-line arguments for test automation and flexible usage.
- **Exit Codes**: Added structured exit codes to the C# consumer (0=success, 2=connection timeout, 3=event timeout) for reliable test orchestration.
- **Producer Linger**: Added `--linger MS` parameter to keep kernel objects alive after the last write, solving test timing issues where the producer exited before the consumer could connect.
- **Kernel Object Lifecycle Docs**: Documented Windows kernel reference counting in ARCHITECTURE_EN.md with Mermaid sequence diagrams explaining why producer/consumer decoupling works in production and why `--linger` is needed only for tests.
- **SharedValueV3 Memory-Mapped Engine**: Designed and implemented a zero-overhead, ultra-fast `SharedValueV3_MemMap` cross-process communication engine using Windows Memory-Mapped Files, fully bypassing all COM/RPC bottlenecks.
- **FlatBuffers Integration**: Introduced Google FlatBuffers (`dataset.fbs`) for dynamic array nesting, unlimited depth, and forward/backward-compatible schema evolution within shared memory.
- **Central API Wrappers**: Created `SharedValueEngine.hpp` (C++) and `SharedValueEngine.cs` (C#) to elegantly abstract cross-process mutexes, named events, and memory-mapped I/O behind a single class.
- **Automated Schema Tooling**: Implemented `build_schema.ps1` to download `flatc.exe` (v24.3.25) and re-generate C++ and C# serialisation classes from the FlatBuffers schema.
- **Cross-Process Callback System**: Implemented Windows Named Events (`CreateEventW` / `EventWaitHandle`) for zero-CPU push notifications from C++ to C#, replacing COM Connection Points.
- **Robust Exception Hierarchies**: Built parallel exception frameworks in both C++ (`SharedValueException`, `SystemException`, `MutexException`, `MemoryMappedException`) and C# (`SharedValueException`, `EngineInitializationException`, `EngineTimeoutException`, `EngineCorruptedException`).
- **V3 Architecture Document**: Added comprehensive `SharedValueV3_MemMap/ARCHITECTURE_EN.md` with 15+ Mermaid diagrams covering system overview, data layout, synchronisation model, schema evolution, build pipeline, and V2-vs-V3 comparison.

### Changed
- **Root README Overhaul**: Updated the root `README_EN.md` to reflect both V2 (COM) and V3 (MemMap) architectures, expanded the directory tree, and consolidated documentation links.
- **`.gitignore` Expansion**: Comprehensively expanded `.gitignore` for C++ build artefacts (`.pch`, `.idb`, `.sdf`, CMake caches), C# build artefacts (`bin/`, `obj/`, `.nupkg`), IDE files, OS junk, Python tooling, credentials and secrets prevention (`.env`, `.pem`, `.key`, `.pfx`).

### Fixed
- **Clangd IDE Integration**: Created dedicated `.clangd` config for `SharedValueV3_MemMap/cpp_core/` to resolve FlatBuffers headers downloaded by CMake FetchContent (`build/_deps/flatbuffers-src/include`). Previously caused 20+ cascading "undeclared identifier" errors.
- **Clangd Harmonisation**: Aligned all three `.clangd` configs (root, ATL, MemMap) with consistent defines (`NOMINMAX`, `WIN32_LEAN_AND_MEAN`), dual Enterprise/Community MSVC and ATL/MFC include fallbacks, and relative project paths for portability.
- **CMake Compile Database**: Added `CMAKE_EXPORT_COMPILE_COMMANDS ON` to CMakeLists.txt for future Ninja-based builds.

---

## [0.1.0] - 2026-04-09

### Added
- **XML Documentation**: Configured C++, CMake, and C# projects to globally generate XML documentation files (`.xml` / `.xmldoc`) upon build for enhanced developer API referencing.
- **Out-of-Process COM Server**: Migrated `ATLProjectcomserverExe` to a `LocalServer32` EXE to support isolated cross-process data sharing.
- **Graceful Shutdown**: Added `ISharedValue::ShutdownServer()` to cleanly tear down the EXE server across RPC bounds without orphaned `DatasetProxy` locks.
- **Shutdown CLI Tools**: Added `stop_server.cpp` and `stop_server.vbs` as reference tools to gracefully exit the standalone COM server.
- **Integration Test Suite**: Added `tests\Run-CrossProcessTests.ps1` to orchestrate isolated producer/consumer lifecycle scenarios and validate array marshaling.

### Changed
- **Legacy InProcess Server Relocation**: Moved the original `ATLProjectcomserver` DLL implementation and its associated build assets from the project root into a dedicated `ATLProjectcomserverLegacy` subdirectory to improve repository organization. The `comserver_testtool` dependencies and MSBuild parameters were adjusted accordingly.
- **Variant SAFEARRAY**: Modified `DatasetProxy::FetchPageKeys` and `FetchPageData` to return `VT_ARRAY | VT_VARIANT` arrays instead of `VT_BSTR` to fix standard VBScript `Type Mismatch` interoperability issues.
- **Singleton Architecture**: `SharedValue` now instantiates the `DatasetProxy` server-side during `FinalConstruct` to ensure stable references are retained without client ownership dropping.
- **Diagnostic Tool**: Overhauled `Invoke-ComDiagnostics.ps1` to independently validate both the Legacy `InprocServer32` DLL and modern `LocalServer32` EXE registry mappings.

### Removed
- Cleaned up deprecated and fragmented VBScript testing files (`TestConsumerVBS.vbs`, `TestProducerString.vbs`, etc.) in favor of the consolidated `Run-CrossProcessTests.ps1` suite.

---

## Related Documentation

- [ARCHITECTURE_EN.md](ARCHITECTURE_EN.md) — Main architecture document for the entire COM Server project.
- [README_EN.md](README_EN.md) — Main documentation and starting point of the entire project.
- [INSTALL_EN.md](INSTALL_EN.md) — Global build and installation instructions.
- [ATLProjectcomserverExe/README_EN.md](ATLProjectcomserverExe/README_EN.md) — User guide and overview of the EXE COM Server variant.
- [SharedValueV2/README_EN.md](SharedValueV2/README_EN.md) — Introduction and overview of the SharedValueV2 C++20 engine.
- [SharedValueV3_MemMap/README_EN.md](SharedValueV3_MemMap/README_EN.md) — Quickstart for the ultra-fast Memory-Mapped FlatBuffers engine (V3).
- [SharedValueV3_MemMap/ARCHITECTURE_EN.md](SharedValueV3_MemMap/ARCHITECTURE_EN.md) — Extensive V3 architecture document with Mermaid diagrams.
