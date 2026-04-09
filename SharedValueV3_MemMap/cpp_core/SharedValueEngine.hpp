#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <iostream>
#include "SharedValueException.hpp"

namespace SharedMemMap {

class SharedValueEngine {
private:
    HANDLE m_hMapFile = NULL;
    void* m_pBuf = nullptr;
    HANDLE m_hMutex = NULL;
    HANDLE m_hEvent = NULL;
    size_t m_maxSize = 0;

public:
    // Create or open the memory mapped file engine
    SharedValueEngine(const std::wstring& name, size_t maxSizeInBytes = 1024 * 1024 * 10) 
        : m_maxSize(maxSizeInBytes) 
    {
        std::wstring mapName = L"Global\\" + name + L"_Map";
        std::wstring mutexName = L"Global\\" + name + L"_Mutex";
        std::wstring eventName = L"Global\\" + name + L"_Event";

        // 1. Create or Open Mutex
        m_hMutex = CreateMutexW(NULL, FALSE, mutexName.c_str());
        if (m_hMutex == NULL) {
            throw SystemException("CreateMutexW", GetLastError());
        }

        // 2. Create or Open Event (Auto-reset Event = FALSE, Manual-reset = FALSE -> Auto-reset, InitialState = FALSE)
        m_hEvent = CreateEventW(NULL, FALSE, FALSE, eventName.c_str());
        if (m_hEvent == NULL) {
            CloseHandle(m_hMutex);
            throw SystemException("CreateEventW", GetLastError());
        }

        // 3. Create or Open Shared Memory
        m_hMapFile = CreateFileMappingW(
            INVALID_HANDLE_VALUE,    // use paging file
            NULL,                    // default security
            PAGE_READWRITE,          // read/write access
            0,                       // maximum object size (high-order DWORD)
            static_cast<DWORD>(m_maxSize), // maximum object size (low-order DWORD)
            mapName.c_str());        // name of mapping object

        if (m_hMapFile == NULL) {
            DWORD err = GetLastError();
            CloseHandle(m_hEvent);
            CloseHandle(m_hMutex);
            throw SystemException("CreateFileMappingW", err);
        }

        // 4. Map View of File
        m_pBuf = MapViewOfFile(m_hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, m_maxSize);
        if (m_pBuf == NULL) {
            DWORD err = GetLastError();
            CloseHandle(m_hMapFile);
            CloseHandle(m_hEvent);
            CloseHandle(m_hMutex);
            throw SystemException("MapViewOfFile", err);
        }
    }

    ~SharedValueEngine() {
        if (m_pBuf) UnmapViewOfFile(m_pBuf);
        if (m_hMapFile) CloseHandle(m_hMapFile);
        if (m_hEvent) CloseHandle(m_hEvent);
        if (m_hMutex) CloseHandle(m_hMutex);
    }

    // Write flatbuffer data to shared memory under mutex lock, then signal event
    void WriteData(const uint8_t* data, size_t size) {
        if (size + sizeof(size_t) > m_maxSize) {
            throw MemoryMappedException("Data size exceeds shared memory capacity.");
        }

        // Acquire Mutex
        DWORD dwWaitResult = WaitForSingleObject(m_hMutex, 5000); // 5 sec timeout
        if (dwWaitResult == WAIT_TIMEOUT) {
            throw MutexException("Timeout waiting for Mutex ownership.");
        } else if (dwWaitResult == WAIT_ABANDONED) {
            // Other process crashed while holding the mutex. We own it now.
            std::cerr << "Warning: Mutex was abandoned by another process. Data might be corrupt.\n";
        } else if (dwWaitResult == WAIT_FAILED) {
            throw SystemException("WaitForSingleObject", GetLastError());
        }

        try {
            // Memory layout: [ size_t dataSize ] [ uint8_t[] flatbuffer_data ]
            size_t* pSize = static_cast<size_t*>(m_pBuf);
            *pSize = size;
            
            uint8_t* pDest = static_cast<uint8_t*>(m_pBuf) + sizeof(size_t);
            memcpy(pDest, data, size);

            // Release Mutex
            if (!ReleaseMutex(m_hMutex)) {
                throw SystemException("ReleaseMutex", GetLastError());
            }

            // Signal Event to notify listeners (e.g. C# clients)
            if (!SetEvent(m_hEvent)) {
                throw SystemException("SetEvent", GetLastError());
            }

        } catch (...) {
            // Ensure mutex is definitely released even on weird crashes before throwing
            ReleaseMutex(m_hMutex);
            throw;
        }
    }
};

} // namespace SharedMemMap
