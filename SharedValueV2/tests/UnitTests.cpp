#include <iostream>
#include <cassert>
#include "SharedValue.hpp"

using namespace SharedValueV2;

class TestObserver : public ISharedValueObserver<int> {
public:
    int lastValue = 0;
    std::wstring lastDateTime = L"";
    int valueHitCount = 0;
    int dtHitCount = 0;

    void OnValueChanged(const int& newValue) override {
        lastValue = newValue;
        valueHitCount++;
    }

    void OnDateTimeChanged(const std::wstring& newDateTime) override {
        lastDateTime = newDateTime;
        dtHitCount++;
    }
};

void TestBasicGetSet() {
    SharedValue<int, LocalMutexPolicy> sv(42);
    assert(sv.GetValue() == 42);

    sv.SetValue(100);
    assert(sv.GetValue() == 100);
    std::cout << "TestBasicGetSet passed" << std::endl;
}

void TestObserverNotifications() {
    SharedValue<int, LocalMutexPolicy> sv(0);
    TestObserver obs;

    sv.Subscribe(&obs);

    sv.SetValue(5);
    assert(obs.lastValue == 5);
    assert(obs.valueHitCount == 1);

    sv.GetCurrentUTCDateTime();
    assert(obs.dtHitCount == 1);
    assert(obs.lastDateTime.length() > 0);

    sv.Unsubscribe(&obs);

    sv.SetValue(10);
    assert(obs.lastValue == 5); // Should not have updated
    assert(obs.valueHitCount == 1);

    std::cout << "TestObserverNotifications passed" << std::endl;
}

void TestSystemInfo() {
    SharedValue<int, LocalMutexPolicy> sv;
    std::wstring dt = sv.GetCurrentUTCDateTime();
    assert(dt.length() > 0);
    assert(dt.find(L"-") != std::wstring::npos);

    std::wstring user = sv.GetCurrentUserLogin();
    assert(user.length() > 0);
    std::wcout << L"User: " << user << std::endl;

    std::cout << "TestSystemInfo passed" << std::endl;
}

int main() {
    TestBasicGetSet();
    TestObserverNotifications();
    TestSystemInfo();

    std::cout << "All Unit Tests Passed!" << std::endl;
    return 0;
}
