#pragma once
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include "SharedValueEngine.hpp"
#include "SharedDataSet.hpp"
#include "BinarySerializer.hpp"

namespace SharedValueV5 {

/// High-level engine combining Dual-Channel IPC with SharedDataSet serialization.
/// The C2P (reverse) channel is lazily initialized in a background thread
/// to avoid blocking when the other side hasn't started yet.
class SharedValueV5Engine {
public:
    using DataSetCallback = std::function<void(const SharedDataSet&)>;

private:
    std::wstring m_channelName;
    bool m_isHost;
    size_t m_maxSize;

    std::unique_ptr<SharedMemMap::SharedValueEngine> m_engineP2C;
    std::unique_ptr<SharedMemMap::SharedValueEngine> m_engineC2P;
    std::mutex m_c2pMutex;
    std::atomic<bool> m_c2pReady{false};
    std::thread m_c2pInitThread;

    DataSetCallback m_reverseCallback;

public:

    SharedValueV5Engine(const std::wstring& channelName, bool isHost, size_t maxSizeInBytes = 10 * 1024 * 1024)
        : m_channelName(channelName), m_isHost(isHost), m_maxSize(maxSizeInBytes)
    {
        // Only create the P2C channel immediately
        m_engineP2C = std::make_unique<SharedMemMap::SharedValueEngine>(
            channelName + L"_P2C", isHost, maxSizeInBytes);
    }

    ~SharedValueV5Engine() {
        if (m_engineP2C) m_engineP2C->StopListening();
        if (m_engineC2P) m_engineC2P->StopListening();
        if (m_c2pInitThread.joinable()) m_c2pInitThread.join();
    }

    /// Publish a SharedDataSet to the forward (P2C) channel
    void Publish(const SharedDataSet& ds) {
        auto data = BinarySerializer::Serialize(ds);
        m_engineP2C->WriteData(data.data(), data.size());
    }

    /// Publish a SharedDataSet to the reverse (C2P) channel.
    /// Blocks until the C2P channel is ready.
    void PublishReverse(const SharedDataSet& ds) {
        if (!m_c2pReady) return; // Not ready yet, skip silently
        auto data = BinarySerializer::Serialize(ds);
        m_engineC2P->WriteData(data.data(), data.size());
    }

    /// Start listening for data on the forward (P2C) channel
    void StartListening(DataSetCallback callback) {
        m_engineP2C->StartListening([cb = std::move(callback)](const uint8_t* data, size_t len) {
            try {
                auto ds = BinarySerializer::Deserialize(data, len);
                cb(ds);
            } catch (const std::exception& ex) {
                std::cerr << "[V5Engine] P2C deserialize error: " << ex.what() << "\n";
            }
        });
    }

    /// Start listening on the reverse (C2P) channel.
    /// Initializes C2P in a background thread so it doesn't block.
    void StartListeningReverse(DataSetCallback callback) {
        m_reverseCallback = std::move(callback);

        m_c2pInitThread = std::thread([this]() {
            try {
                // This may block waiting for the other side's Ready Event
                auto engine = std::make_unique<SharedMemMap::SharedValueEngine>(
                    m_channelName + L"_C2P", !m_isHost, m_maxSize);

                // Start listening
                engine->StartListening([this](const uint8_t* data, size_t len) {
                    try {
                        auto ds = BinarySerializer::Deserialize(data, len);
                        if (m_reverseCallback) m_reverseCallback(ds);
                    } catch (const std::exception& ex) {
                        std::cerr << "[V5Engine] C2P deserialize error: " << ex.what() << "\n";
                    }
                });

                {
                    std::lock_guard<std::mutex> lock(m_c2pMutex);
                    m_engineC2P = std::move(engine);
                }
                m_c2pReady = true;
                std::cout << "[V5Engine] C2P reverse channel connected.\n";
            } catch (const std::exception& ex) {
                std::cerr << "[V5Engine] Failed to init C2P: " << ex.what() << "\n";
            }
        });
    }

    void StopListening() {
        if (m_engineP2C) m_engineP2C->StopListening();
        if (m_engineC2P) m_engineC2P->StopListening();
    }
};

} // namespace SharedValueV5
