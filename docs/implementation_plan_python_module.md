# SharedValueV5 Python C-Extension Implementation Plan

This document outlines the strategy for physically implementing the previously theoretical Python `sharedvalue5` C-Extension and successfully bringing it to life using `pybind11`.

## Rationale
To provide blazing-fast, zero-overhead IPC mapping directly to the C++ core structures (`SharedDataSet`, `SharedTable`, `SharedRow`, `SharedValueV5Engine`), we will leverage standard Python C-Extensions. Doing this manually via `Python.h` is error-prone due to massive reference counting complexities. Instead, we will use **pybind11**—the industry standard for wrapping modern C++ into Python modules seamlessly. The build matrix will be fully isolated and orchestrated by `uv`, utilizing `pyproject.toml`.

## User Review Required
> [!IMPORTANT]
> The architectural document presented a slightly hybrid facade for Python where `SharedDataSet` inherently carried engine functionalities (e.g. `ds = sv5.SharedDataSet("Name", is_host=True)` and `ds.publish()`).
> However, natively in C++, these are separated (`SharedDataSet ds` and `SharedValueV5Engine engine`).
> **Decision:** The python wrapper module (`main.cpp`) will stitch a simplified python-friendly facade class `PySharedDataSet` that wraps both the C++ `SharedDataSet` and `SharedValueV5Engine` beneath the hood to match the exact API shown in our documentation. Do you agree with this facade pattern?

## Proposed Changes

### `SharedValueV5/python_ext/`

#### [NEW] `pyproject.toml`
Declares the build system leveraging `setuptools` and explicitly demanding `pybind11` to be available during the compilation loop.

#### [NEW] `setup.py`
A tiny wrapper invoking `pybind11`'s extension configuration, directing the compiler to include headers from `../cpp_core`.

#### [NEW] `src/main.cpp` (The C++ Wrapper)
A pybind11 driven file executing the logic:
- Exporting `ColumnType` enums mapping exactly to `sv5.DOUBLE`, `sv5.STRING`, etc.
- A `PySharedRow` class masking C++ generic typed accessors via deep `__getitem__` and `__setitem__` (`row["Temp"] = 25.5`). Converts C++ variants natively to PyObjects securely.
- A `PySharedTable` interacting dynamically yielding `PySharedRow`.
- The `PySharedDataSet` housing both native `SharedDataSet` and dynamically instantiating a scoped `SharedValueV5Engine` mapping to the `is_host` flag. It exposes `.publish()` and `.start_listening()`.

#### [MODIFY] `SharedValueV5Installer/build_sdk_installer.ps1`
We will re-enable the Python compilation execution block. With `.toml` + `uv` intact, the script will invoke `uv pip run python setup.py build_ext --inplace` to yield the final `.pyd` during MSI packing.

## Open Questions
None. The architecture holds all required API designs.

## Verification Plan

### Automated Tests
1. Initialize a localized environment: `uv venv` and `uv pip install pybind11`.
2. Compile the extension via `uv pip install -e .` from within the `python_ext` folder.
3. Once compiled, run `python_reader.py` independently while running `vbs_producer_array.vbs` alongside it to evaluate if Python can successfully unpack the native memory descriptors instantiated by VBScript/C#.
