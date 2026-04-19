#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <iostream>
#include <functional>
#include <thread>
#include <atomic>
#include "SharedValueException.hpp"

namespace SharedMemMap {

class SharedValueEngine {
private:
    HANDLE m_hMapFile = NULL;
    void* m_pBuf = nullptr;
    HANDLE m_hMutex = NULL;
    HANDLE m_hEvent = NULL;
    HANDLE m_hReadyEvent = NULL;
    size_t m_maxSize = 0;

    std::thread m_listenerThread;
    std::atomic<bool> m_isListening{false};
    std::function<void(const uint8_t*, size_t)> m_onDataReady;

    void ListenerLoop() {
        while (m_isListening) {
            DWORD dwWait = WaitForSingleObject(m_hEvent, INFINITE);
            if (!m_isListening) break; // Woken up to exit

            if (dwWait == WAIT_OBJECT_0) {
                // We got data, lock mutex and read
                ReadCurrentData();
            }
        }
    }

    void ReadCurrentData() {
        DWORD dwWaitResult = WaitForSingleObject(m_hMutex, 5000);
        if (dwWaitResult == WAIT_TIMEOUT) {
            std::cerr << "Timeout waiting for Mutex during read.\n";
            return;
        } else if (dwWaitResult == WAIT_ABANDONED) {
            std::cerr << "Warning: Mutex abandoned by other process.\n";
        }

        try {
            size_t dataSize = *static_cast<size_t*>(m_pBuf);
            if (dataSize > m_maxSize - sizeof(size_t)) {
                ReleaseMutex(m_hMutex);
                return; // Corrupt data
            }

            uint8_t* pSrc = static_cast<uint8_t*>(m_pBuf) + sizeof(size_t);
            // Call the callback
            if (m_onDataReady) {
                m_onDataReady(pSrc, dataSize);
            }

            ReleaseMutex(m_hMutex);
        } catch (...) {
            ReleaseMutex(m_hMutex);
        }
    }

public:
    // isPrimaryHost controls the ReadyEvent behavior. The primary host creates and SETS the ready event. 
    // The secondary host creates and WAITS for the ready event.
    SharedValueEngine(const std::wstring& name, bool isPrimaryHost, size_t maxSizeInBytes = 1024 * 1024 * 10) 
        : m_maxSize(maxSizeInBytes) 
    {
        std::wstring readyEventName = L"Global\\" + name + L"_Ready";
        
        if (!isPrimaryHost) {
            std::wcout << L"[Engine] Waiting for primary host Ready Event: " << readyEventName << L"\n";
            m_hReadyEvent = CreateEventW(NULL, TRUE, FALSE, readyEventName.c_str());
            if (m_hReadyEvent) {
                WaitForSingleObject(m_hReadyEvent, INFINITE);
            }
        }

        std::wstring mapName = L"Global\\" + name + L"_Map";
        std::wstring mutexName = L"Global\\" + name + L"_Mutex";
        std::wstring eventName = L"Global\\" + name + L"_Event";

        // 1. Create or Open Mutex
        m_hMutex = CreateMutexW(NULL, FALSE, mutexName.c_str());
        if (m_hMutex == NULL) {
            if (!isPrimaryHost && m_hReadyEvent) CloseHandle(m_hReadyEvent);
            throw SystemException("CreateMutexW", GetLastError());
        }

        // 2. Create or Open Event (Auto-reset Event = FALSE, Manual-reset = FALSE -> Auto-reset)
        m_hEvent = CreateEventW(NULL, FALSE, FALSE, eventName.c_str());
        if (m_hEvent == NULL) {
            CloseHandle(m_hMutex);
            if (!isPrimaryHost && m_hReadyEvent) CloseHandle(m_hReadyEvent);
            throw SystemException("CreateEventW", GetLastError());
        }

        // 3. Create or Open Shared Memory
        m_hMapFile = CreateFileMappingW(
            INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, static_cast<DWORD>(m_maxSize), mapName.c_str());
        if (m_hMapFile == NULL) {
            DWORD err = GetLastError();
            CloseHandle(m_hEvent);
            CloseHandle(m_hMutex);
            if (!isPrimaryHost && m_hReadyEvent) CloseHandle(m_hReadyEvent);
            throw SystemException("CreateFileMappingW", err);
        }

        // 4. Map View of File
        m_pBuf = MapViewOfFile(m_hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, m_maxSize);
        if (m_pBuf == NULL) {
            DWORD err = GetLastError();
            CloseHandle(m_hMapFile);
            CloseHandle(m_hEvent);
            CloseHandle(m_hMutex);
            if (!isPrimaryHost && m_hReadyEvent) CloseHandle(m_hReadyEvent);
            throw SystemException("MapViewOfFile", err);
        }

        if (isPrimaryHost) {
            // Wake up waiting consumers
            m_hReadyEvent = CreateEventW(NULL, TRUE, FALSE, readyEventName.c_str());
            if (m_hReadyEvent) {
                SetEvent(m_hReadyEvent);
            }
        }
    }

    ~SharedValueEngine() {
        StopListening();
        if (m_pBuf) UnmapViewOfFile(m_pBuf);
        if (m_hMapFile) CloseHandle(m_hMapFile);
        if (m_hEvent) CloseHandle(m_hEvent);
        if (m_hMutex) CloseHandle(m_hMutex);
        if (m_hReadyEvent) CloseHandle(m_hReadyEvent);
    }

    // Start listening in a background thread
    void StartListening(std::function<void(const uint8_t*, size_t)> callback) {
        if (m_isListening) return;
        m_onDataReady = callback;
        m_isListening = true;
        m_listenerThread = std::thread(&SharedValueEngine::ListenerLoop, this);
    }

    // Stop listening 
    void StopListening() {
        if (!m_isListening) return;
        m_isListening = false;
        SetEvent(m_hEvent); // wake up thread
        if (m_listenerThread.joinable()) {
            m_listenerThread.join();
        }
    }

    // Write data to shared memory under mutex lock, then signal event
    void WriteData(const uint8_t* data, size_t size) {
        if (size + sizeof(size_t) > m_maxSize) {
            throw MemoryMappedException("Data size exceeds shared memory capacity.");
        }

        DWORD dwWaitResult = WaitForSingleObject(m_hMutex, 5000); // 5 sec timeout
        if (dwWaitResult == WAIT_TIMEOUT) {
            throw MutexException("Timeout waiting for Mutex ownership.");
        } else if (dwWaitResult == WAIT_ABANDONED) {
            std::cerr << "Warning: Mutex was abandoned by another process. Data might be corrupt.\n";
        } else if (dwWaitResult == WAIT_FAILED) {
            throw SystemException("WaitForSingleObject", GetLastError());
        }

        try {
            size_t* pSize = static_cast<size_t*>(m_pBuf);
            *pSize = size;
            
            uint8_t* pDest = static_cast<uint8_t*>(m_pBuf) + sizeof(size_t);
            memcpy(pDest, data, size);

            if (!ReleaseMutex(m_hMutex)) {
                throw SystemException("ReleaseMutex", GetLastError());
            }

            if (!SetEvent(m_hEvent)) {
                throw SystemException("SetEvent", GetLastError());
            }

        } catch (...) {
            ReleaseMutex(m_hMutex);
            throw;
        }
    }
};

} // namespace SharedMemMap
