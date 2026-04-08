// StressTest.cpp — SharedValueV2 Concurrent Stress Tests
#include "../include/SharedValue.hpp"
#include "../include/DatasetStore.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

using namespace SharedValueV2;

std::atomic<int> errors{0};

void runSharedValueStressTest() {
    std::cout << "  SharedValue stress test (50 threads, 1000 ops each)..." << std::endl;
    SharedValue<int> sv(0);
    std::vector<std::thread> threads;

    for (int t = 0; t < 50; ++t) {
        threads.emplace_back([&sv, t]() {
            for (int i = 0; i < 1000; ++i) {
                sv.SetValue(t * 1000 + i);
                volatile int val = sv.GetValue();
                (void)val;
            }
        });
    }
    for (auto& t : threads) t.join();
    std::cout << "    SharedValue stress test passed (no crashes/deadlocks)" << std::endl;
}

void runDatasetStressTest(StorageMode mode, const std::string& label) {
    std::cout << "  DatasetStore stress test [" << label << "] (50 threads, 200 ops each)..." << std::endl;
    DatasetStore<std::wstring> ds(mode);
    std::vector<std::thread> threads;
    std::atomic<int> addCount{0};

    for (int t = 0; t < 50; ++t) {
        threads.emplace_back([&ds, &addCount, t]() {
            for (int i = 0; i < 200; ++i) {
                std::wstring key = L"t" + std::to_wstring(t) + L"_k" + std::to_wstring(i);
                try {
                    ds.AddRow(key, L"value_" + std::to_wstring(i));
                    addCount++;
                } catch (...) {}

                // Read operations
                ds.GetRecordCount();
                ds.HasKey(key);
                ds.FetchKeys(0, 10);

                // Update
                ds.UpdateRow(key, L"updated_" + std::to_wstring(i));

                // Occasional remove
                if (i % 5 == 0) {
                    ds.RemoveRow(key);
                }
            }
        });
    }
    for (auto& t : threads) t.join();

    auto remaining = ds.GetRecordCount();
    std::cout << "    " << label << " stress test passed. Added: " << addCount
              << ", Remaining: " << remaining << std::endl;
}

int main() {
    auto start = std::chrono::high_resolution_clock::now();

    std::cout << "=== Stress Tests ===" << std::endl;
    runSharedValueStressTest();
    runDatasetStressTest(StorageMode::Ordered, "Ordered");
    runDatasetStressTest(StorageMode::Unordered, "Unordered");

    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "\n========================================" << std::endl;
    std::cout << "All stress tests passed in " << ms << " ms" << std::endl;
    return 0;
}
