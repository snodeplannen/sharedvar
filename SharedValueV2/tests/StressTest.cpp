#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <cassert>
#include "SharedValue.hpp"

using namespace SharedValueV2;

class StressObserver : public ISharedValueObserver<int> {
public:
    std::atomic<int> updateCount{0};

    void OnValueChanged(const int& /*newValue*/) override {
        updateCount++;
    }

    void OnDateTimeChanged(const std::wstring& /*newDateTime*/) override {
        updateCount++;
    }
};

void runStressTest() {
    SharedValue<int, LocalMutexPolicy> sv(0);
    StressObserver obs;
    sv.Subscribe(&obs);

    const int NUM_THREADS = 50;
    const int ITERATIONS = 1000;
    std::vector<std::thread> threads;

    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&sv, ITERATIONS, i]() {
            for (int k = 0; k < ITERATIONS; ++k) {
                if (k % 2 == 0) {
                    sv.SetValue(i * k);
                } else {
                    sv.GetValue();
                }
                if (k % 100 == 0) {
                    sv.GetCurrentUTCDateTime();
                }
            }
        });
    }

    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    sv.Unsubscribe(&obs);

    std::cout << "StressTest complete." << std::endl;
    std::cout << "Observer caught " << obs.updateCount.load() << " updates." << std::endl;
    std::cout << "No deadlocks occurred." << std::endl;
}

int main() {
    std::cout << "Starting Continuous Stress Test..." << std::endl;
    runStressTest();
    return 0;
}
