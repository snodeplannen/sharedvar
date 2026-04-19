# SharedValueV5 WiX Installer Design

This document outlines the architectural implementation plan for constructing a Windows Installer (MSI) for the **SharedValueV5** engine using the WiX v4 Toolset.

## Rationale
Unlike V2, which was a native isolated C++ EXE server (`LocalServer32`), the V5 engine is rooted within .NET 8 and utilizes an `InprocServer32` DLL hosting environment (`comhost.dll`). Therefore, the installer must account for .NET 8 specific deployments, skipping traditional `regasm.exe` or `regsvr32` interactions that violate clean uninstalls.

## User Review Required
> [!IMPORTANT]
> **Standalone vs Integrated Installer**
> We currently have a WiX project named `SharedValueInstaller` for the legacy V2 COM EXE.
> Should we build a **completely distinct** WiX project (e.g. `SharedValueV5Installer.wixproj`) for cleaner CI/CD separation, or should we **merge** it into the existing `SharedValueInstaller` as a selectable Feature installation?
> *Recommendation:* Create a standalone `SharedValueV5Installer` project due to radically different dependency trees.

## Proposed Changes

### [New Project: SharedValueV5Installer]

A brand-new SDK-style `.wixproj` utilizing the WiX v4 Toolset.

#### [NEW] `SharedValueV5Installer/Package.wxs`
This file will define the overarching MSI parameters (`Version="0.4.0.0"`, `UpgradeCode`, `Manufacturer`). It builds the directories under `C:\Program Files\SharedValueV5`.

#### [NEW] `SharedValueV5Installer/Components.wxs`
This file carries the dense payload metadata. Instead of relying on `heat.exe` automation, we will explicitly define the registry hooks required by the `.NET 8 COMHost` stub. 

**Payload Files to Bundle:**
- `SharedValueV5.Com.comhost.dll` (The unmanaged entrypoint)
- `SharedValueV5.Com.dll` (The managed COM-visible assembly)
- `csharp_core.dll` (The SharedValue engine logic dependency)
- `SharedValueV5.Com.deps.json` (Critical for resolution)
- `SharedValueV5.Com.runtimeconfig.json` (Tells the COMHost to spin up .NET 8)

**Registry Architecture Mapping:**
Because V5 functions via `comhost.dll`, we will extract the GUID `B7C3D5E1-4A2F-4B89-9E6D-1F3A5C7D9E0B` mapped from `SharedValueV5Com.cs`. The WiX mapping will look like:
```xml
<RegistryKey Root="HKCR" Key="CLSID\{B7C3D5E1-4A2F-4B89-9E6D-1F3A5C7D9E0B}">
  <RegistryValue Type="string" Value="SharedValueV5Com Class" />
</RegistryKey>
<RegistryKey Root="HKCR" Key="CLSID\{B7C3D5E1-4A2F-4B89-9E6D-1F3A5C7D9E0B}\InprocServer32">
  <RegistryValue Type="string" Value="[#file_SharedValueV5_comhost_dll]" />
  <RegistryValue Type="string" Name="ThreadingModel" Value="Both" />
</RegistryKey>
<RegistryKey Root="HKCR" Key="CLSID\{B7C3D5E1-4A2F-4B89-9E6D-1F3A5C7D9E0B}\ProgID">
  <RegistryValue Type="string" Value="SharedValueV5.Engine" />
</RegistryKey>
<RegistryKey Root="HKCR" Key="SharedValueV5.Engine">
  <RegistryValue Type="string" Value="SharedValueV5Com Class" />
</RegistryKey>
<RegistryKey Root="HKCR" Key="SharedValueV5.Engine\CLSID">
  <RegistryValue Type="string" Value="{B7C3D5E1-4A2F-4B89-9E6D-1F3A5C7D9E0B}" />
</RegistryKey>
```

#### [MODIFY] `SharedValueV5/com_wrapper/com_wrapper.csproj`
(Optional Validation Check) ensuring MSBuild correctly outputs `comhost.dll` for x64 targets uniformly, allowing WiX to blindly reference `bin\Release\net8.0\win-x64\*`.

## Open Questions
> [!WARNING]
> 1. Do you want the Python Extension (`.pyd`) bundled within this MSI, or purely the robust COM ecosystem? 
> 2. Are we targeting only `x64` architecture, or must this installer handle `x86` mappings for 32-bit legacy VBA applications (e.g. 32-bit Excel)?

## Verification Plan

### Automated Tests
- N/A for deployment, but we can compile the `*.msi` and scan using `Invoke-ComDiagnostics.ps1`.
### Manual Verification
- Execute the V5 MSI.
- Run `csharp\sharedvar\SharedValueV5\examples\vbs_producer_cursor.vbs` without elevated privileges to confirm the `CreateObject("SharedValueV5.Engine")` resolution routes correctly to the `.NET 8` assembly in Program Files.
