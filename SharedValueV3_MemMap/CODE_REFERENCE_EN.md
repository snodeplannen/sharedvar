# SharedValueV3 MemMap — Code Reference

This document provides an overview of the most important files, components, and classes in this implementation.

## 1. Schema (`schema.fbs`)
Central to the V3 architecture is the **pre-agreed structure**. Unlike V2, where the structure was discovered dynamically at runtime via type-flags, V3 expects a strict schema.

Main tables and structs in FlatBuffers:
- **`NestedDetail`** (Table): Contains an `id` (int) and a string `description`.
- **`DataRow`** (Table): Data structure per row with properties such as `id`, `timestamp`, `score`, a boolean `is_active` and an array of `NestedDetail` objects.
- **`SharedDataset`** (Table): The root node containing a vector or list of `DataRow` objects.

## 2. C++ Core (Producer)

The producer code is located in `cpp_core/`.

### `SharedValueEngine.hpp`
The C++ implementation of the inter-process transport layer.
- **`SharedValueEngine(name, maxSize)`**: Manages and opens three core Windows Kernel Objects based on the configured `name` (e.g., `Global\MyGlobalDataset_Map`):
  1. A **Mutex** to prevent torn reads during data transfer.
  2. An **Auto-Reset Event** for asynchronous client wakeups after writes.
  3. The actual **Memory Mapped File** handle (+ MapViewOfFile) for placing the binary data.
- **`WriteData(const uint8_t* data, size_t size)`**: Ensures safe exclusive access using a mutex lock (`WaitForSingleObject`). Checks if the data fits in the mapping, first copies a `size_t` metadata field followed directly by the raw serialized byte buffer via `memcpy`, unlocks the mutex, and signals any consumers via the Event (`SetEvent()`).

### `main.cpp`
Demonstrates how C++ code dynamically constructs tree structures using the `FlatBufferBuilder` (e.g., sub-vectors with `NestedDetail` and `SharedDataset`), finalizes with `.Finish()`, extracts the byte blocks, and sends them out using the engine.

## 3. C# Core (Consumer)

The consumer implementation is located in `csharp_core/`.

### `SharedValueEngine.cs`
The .NET wrapper using `System.IO.MemoryMappedFiles` and WaitHandles for IPC transfer.
- **OnDataReady Event (`Action<SharedDataset>`)**: Native C# event to which client code can subscribe.
- **`SharedValueEngine(name, maxSize)`**: Opens the corresponding Mutex, EventWaitHandle, and MemoryMappedFile. Expects these to be already created by the Producer (`OpenExisting` overloads, which throw if the producer is not alive or if permissions for the `Global\` prefix are insufficient).
- **`StartListening()`**: Uses a low-impact background listener Thread that blocks using the event's `.WaitOne()`. Consumes virtually 0% CPU if no writes occur.
- **`ReadCurrentData()`**: Arranges exclusive access via a timeout-based `Mutex.WaitOne()` and decodes the 8-byte headers. Passes the remaining bytes in a .NET ByteBuffer to the FlatBuffers-generated accessors (`SharedDataset.GetRootAsSharedDataset`). 

### `Program.cs`
The console entry point for C#; attaches a parse event function to `OnDataReady` and effortlessly extracts data as if they were standard C# POCO objects through the zero-copy abstraction of the nested detail arrays and properties.
