# SharedValueV4 Architecture Implementation

We are upgrading the `SharedValue` architecture to V4, based on the findings from `research_memmap_v3_disadvantages_mitigations_EN.md`. V4 will address the most critical limitations of V3.

## Proposed Changes

We will create a new directory `SharedValueV4` (copying the base structure from V3) and implement the following prioritized features:

### 1. Ready Event (Producer / Consumer Handshake)
- **Problem**: In V3, the consumer (C#) throws an exception if it boots before the producer (C++).
- **Solution**: Introduce a `Global\[Name]_Ready` Named Event.
- **Implementation**:
  - The process acting as the primary host will set the event.
  - The other process will wait for this event using `WaitForSingleObject` (0% CPU impact).
  - This completely removes the need for `Thread.Sleep` retry loops manually implemented by clients.

### 2. Pre-Build Hooks for FlatBuffers
- **Problem**: Changing `dataset.fbs` requires manually running `build_schema.ps1` before compiling.
- **Solution**: Automate schema generation.
- **Implementation**:
  - **C++**: Add an `add_custom_command` to `CMakeLists.txt` that invokes `flatc --cpp ... dataset.fbs`.
  - **C#**: Add a `<Target BeforeTargets="BeforeBuild">` in the `.csproj` that executes `flatc --csharp ... dataset.fbs`.

### 3. Symmetrical Dual-Channel (Full Bi-directional Communication)
- **Problem**: V3 is strictly C++ -> C#. C# cannot invoke methods, send requests, or share bulk data back to C++.
- **Solution**: A full Symmetrical Dual-Channel implementation via two Identical MMF channels.
- **Implementation**:
  - Channel 1 (C++ Producer, C# Consumer): `Global\[Name]_P2C_Map`, `Global\[Name]_P2C_Mutex`, `Global\[Name]_P2C_Event`.
  - Channel 2 (C# Producer, C++ Consumer): `Global\[Name]_C2P_Map`, `Global\[Name]_C2P_Mutex`, `Global\[Name]_C2P_Event`.
  - Both processes will spin up a listener thread for their respective incoming channel.
  - This achieves true bidirectional scaling.

## Verification Plan

### Automated Tests
- I'll write automated integration tests covering the `Ready Event` (starting C# before C++ and verifying it hooks up correctly).
- I'll write a test for the `Dual-Channel` where the C# client sends a complex flatbuffer dataset back to the C++ process.

### Manual Verification
- Execute `dotnet build` to ensure `flatc` is perfectly invoked by MSBuild.
- Build the C++ project via `cmake` to verify the same.
