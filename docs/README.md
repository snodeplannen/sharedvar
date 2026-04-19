# Docs — Design Documentation

Background analyses and design documents that substantiate the architectural decisions of this project.

## Role within the project

These documents serve as historical references for the technical choices made during development, particularly the migration from an in-process DLL to an out-of-process EXE.

## File Overview

| File | Description |
|---|---|
| `3_methods_for_inteprocess_EN.md` | **Cross-Process Architecture Redesign** — Analysis of three migration strategies for cross-process data sharing: (1) Native Out-of-Process EXE (`LocalServer32`), (2) Windows DLL Surrogate (`dllhost.exe`), and (3) Shared Memory (`CreateFileMapping`). Includes pros and cons per option and the final recommendation. |
| `research_memmap_v3_disadvantages_mitigations_EN.md` | Examines architectural limitations of SharedValueV3 (MemMap) and outlines solutions for them. |
| `research_memory_mapped_files_sharedvar_new_design_EN.md` | Deep dive into Memory-Mapped files performance vs COM server usage. |

## Context

This documentation was drafted when the project operated exclusively as an In-Process DLL. It describes the evaluation that culminated in the selection of **Option 1 (ATL EXE)**, which was subsequently implemented in the [`ATLProjectcomserverExe/`](../ATLProjectcomserverExe/) directory.


## Related Documentation

- [3_methods_for_inteprocess_EN.md](3_methods_for_inteprocess_EN.md) — Background document addressing the theory behind Inter-Process Communication (IPC).
- [README_EN.md](../README_EN.md) — Main documentation and starting point of the entire project.
- [ARCHITECTURE_EN.md](../ARCHITECTURE_EN.md) — Main architecture document for the entire COM Server project.
- [README_EN.md](../ATLProjectcomserverExe/README_EN.md) — User guide and overview of the EXE COM Server variant.
- [README_EN.md](../SharedValueV2/README_EN.md) — Introduction and overview of the SharedValueV2 C++20 engine.
