# Changelog

All notable changes to this project will be documented in this file.

## [0.1.0] - 2026-04-09
### Added
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


## Gerelateerde Documentatie

- [ARCHITECTURE.md](ARCHITECTURE.md) — Hoofd architectuurdocument voor het gehele COM Server project.
- [README.md](README.md) — Hoofddocumentatie en startpunt van het gehele project.
- [INSTALL.md](INSTALL.md) — Globale bouw- en installatie-instructies.
- [README.md](ATLProjectcomserverExe/README.md) — Gebruikershandleiding en overzicht van de EXE COM Server variant.
- [README.md](SharedValueV2/README.md) — Introductie en overzicht van de SharedValueV2 C++20 engine.
