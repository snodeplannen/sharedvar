// UnitTests.cpp — SharedValueV2 Unit Tests
// Tests for SharedValue, DatasetStore, StorageEngine, Errors, and EventBus
#include "../include/SharedValue.hpp"
#include "../include/DatasetStore.hpp"
#include "../include/EventBus.hpp"
#include "../include/Errors.hpp"
#include <iostream>
#include <cassert>
#include <string>

using namespace SharedValueV2;

int testsPassed = 0;
int testsFailed = 0;

#define TEST(name) void name()
#define RUN_TEST(name) do { \
    std::cout << "  Running " << #name << "..."; \
    try { name(); testsPassed++; std::cout << " PASS" << std::endl; } \
    catch (const std::exception& e) { testsFailed++; std::cout << " FAIL: " << e.what() << std::endl; } \
} while(0)
#define ASSERT_EQ(a, b) do { if ((a) != (b)) throw std::runtime_error("Assertion failed: " #a " != " #b); } while(0)
#define ASSERT_TRUE(x) do { if (!(x)) throw std::runtime_error("Assertion failed: " #x); } while(0)
#define ASSERT_THROWS(expr, excType) do { \
    bool caught = false; \
    try { expr; } catch (const excType&) { caught = true; } \
    if (!caught) throw std::runtime_error("Expected exception " #excType " not thrown"); \
} while(0)

// ============ SharedValue Tests ============

TEST(TestSharedValueGetSet) {
    SharedValue<int> sv(42);
    ASSERT_EQ(sv.GetValue(), 42);
    sv.SetValue(100);
    ASSERT_EQ(sv.GetValue(), 100);
}

TEST(TestSharedValueObserver) {
    SharedValue<int> sv(0);
    struct Obs : ISharedValueObserver<int> {
        int lastValue = -1;
        void OnValueChanged(const int& v) override { lastValue = v; }
        void OnDateTimeChanged(const std::wstring&) override {}
    } obs;
    sv.Subscribe(&obs);
    sv.SetValue(77);
    ASSERT_EQ(obs.lastValue, 77);
    sv.Unsubscribe(&obs);
    sv.SetValue(88);
    ASSERT_EQ(obs.lastValue, 77); // unchanged after unsubscribe
}

TEST(TestSharedValueSystemInfo) {
    SharedValue<int> sv;
    auto dt = sv.GetCurrentUTCDateTime();
    ASSERT_TRUE(!dt.empty());
    auto login = sv.GetCurrentUserLogin();
    ASSERT_TRUE(!login.empty());
}

// ============ DatasetStore Tests ============

TEST(TestDatasetAddAndGet) {
    DatasetStore<std::wstring> ds;
    ds.AddRow(L"k1", L"v1");
    ds.AddRow(L"k2", L"v2");
    ASSERT_EQ(ds.GetRecordCount(), 2u);
    auto v = ds.GetRow(L"k1");
    ASSERT_TRUE(v.has_value());
    ASSERT_EQ(v.value(), L"v1");
}

TEST(TestDatasetUpdate) {
    DatasetStore<std::wstring> ds;
    ds.AddRow(L"k1", L"v1");
    bool ok = ds.UpdateRow(L"k1", L"v1_updated");
    ASSERT_TRUE(ok);
    ASSERT_EQ(ds.GetRowOrThrow(L"k1"), L"v1_updated");
    bool notFound = ds.UpdateRow(L"xxx", L"val");
    ASSERT_TRUE(!notFound);
}

TEST(TestDatasetPagination) {
    DatasetStore<std::wstring> ds;
    for (int i = 0; i < 100; ++i) {
        ds.AddRow(L"key_" + std::to_wstring(i), L"val_" + std::to_wstring(i));
    }
    auto page1 = ds.FetchKeys(0, 10);
    ASSERT_EQ(page1.size(), 10u);
    auto page2 = ds.FetchKeys(90, 20);
    ASSERT_EQ(page2.size(), 10u); // only 10 remain
    auto fullPage = ds.FetchPage(0, 5);
    ASSERT_EQ(fullPage.size(), 5u);
}

TEST(TestDatasetRemoveAndClear) {
    DatasetStore<std::wstring> ds;
    ds.AddRow(L"a", L"1");
    ds.AddRow(L"b", L"2");
    ASSERT_TRUE(ds.RemoveRow(L"a"));
    ASSERT_EQ(ds.GetRecordCount(), 1u);
    ASSERT_TRUE(!ds.HasKey(L"a"));
    ds.Clear();
    ASSERT_EQ(ds.GetRecordCount(), 0u);
}

TEST(TestDatasetObserver) {
    DatasetStore<std::wstring> ds;
    struct Obs : IDatasetObserver<std::wstring> {
        int added = 0, updated = 0, removed = 0, cleared = 0;
        void OnRowAdded(const std::wstring&, const std::wstring&) override { added++; }
        void OnRowUpdated(const std::wstring&, const std::wstring&) override { updated++; }
        void OnRowRemoved(const std::wstring&) override { removed++; }
        void OnDatasetCleared() override { cleared++; }
    } obs;
    ds.Subscribe(&obs);
    ds.AddRow(L"k1", L"v1");
    ds.UpdateRow(L"k1", L"v2");
    ds.RemoveRow(L"k1");
    ds.AddRow(L"k2", L"v2");
    ds.Clear();
    ASSERT_EQ(obs.added, 2);
    ASSERT_EQ(obs.updated, 1);
    ASSERT_EQ(obs.removed, 1);
    ASSERT_EQ(obs.cleared, 1);
}

TEST(TestDatasetInPlaceAccess) {
    DatasetStore<std::wstring> ds;
    ds.AddRow(L"counter", L"0");
    ds.AccessInPlace(L"counter", [](std::wstring& val) {
        int n = std::stoi(val);
        val = std::to_wstring(n + 10);
    });
    ASSERT_EQ(ds.GetRowOrThrow(L"counter"), L"10");
}

TEST(TestDatasetOrderedEngine) {
    DatasetStore<std::wstring> ds(StorageMode::Ordered);
    ds.AddRow(L"charlie", L"3");
    ds.AddRow(L"alpha", L"1");
    ds.AddRow(L"bravo", L"2");
    auto keys = ds.FetchKeys(0, 10);
    ASSERT_EQ(keys[0], L"alpha");
    ASSERT_EQ(keys[1], L"bravo");
    ASSERT_EQ(keys[2], L"charlie");
}

TEST(TestDatasetUnorderedEngine) {
    DatasetStore<std::wstring> ds(StorageMode::Unordered);
    ds.AddRow(L"x", L"1");
    ds.AddRow(L"y", L"2");
    ds.AddRow(L"z", L"3");
    ASSERT_EQ(ds.GetRecordCount(), 3u);
    ASSERT_EQ(ds.GetRowOrThrow(L"y"), L"2");
}

TEST(TestDatasetRuntimeSwitch) {
    DatasetStore<std::wstring> ds(StorageMode::Ordered);
    // Switch on empty store — should succeed
    ds.SetStorageMode(StorageMode::Unordered);
    ds.AddRow(L"k1", L"v1");
    // Switch on non-empty store — should throw
    ASSERT_THROWS(ds.SetStorageMode(StorageMode::Ordered), StoreModeException);
    // Clear and switch — should succeed
    ds.Clear();
    ds.SetStorageMode(StorageMode::Ordered);
}

// ============ Error Tests ============

TEST(TestErrorKeyNotFound) {
    DatasetStore<std::wstring> ds;
    ASSERT_THROWS(ds.GetRowOrThrow(L"missing"), KeyNotFoundException);
}

TEST(TestErrorDuplicateKey) {
    DatasetStore<std::wstring> ds;
    ds.AddRow(L"dup", L"val");
    ASSERT_THROWS(ds.AddRow(L"dup", L"val2"), DuplicateKeyException);
}

TEST(TestErrorStoreModeNotEmpty) {
    DatasetStore<std::wstring> ds;
    ds.AddRow(L"k", L"v");
    ASSERT_THROWS(ds.SetStorageMode(StorageMode::Unordered), StoreModeException);
}

// ============ EventBus Tests ============

TEST(TestEventBusEmit) {
    EventBus<> bus;
    struct Listener : IEventListener {
        int count = 0;
        void OnEvent(const MutationEvent&) override { count++; }
    } listener;
    bus.Subscribe(&listener);
    bus.Emit(EventType::ValueSet);
    bus.Emit(EventType::RowAdded, L"k1");
    ASSERT_EQ(listener.count, 2);
    bus.Unsubscribe(&listener);
    bus.Emit(EventType::ValueSet);
    ASSERT_EQ(listener.count, 2); // no more events
}

TEST(TestEventBusSequenceId) {
    EventBus<> bus;
    struct Listener : IEventListener {
        std::vector<uint64_t> ids;
        void OnEvent(const MutationEvent& e) override { ids.push_back(e.sequenceId); }
    } listener;
    bus.Subscribe(&listener);
    bus.Emit(EventType::RowAdded);
    bus.Emit(EventType::RowUpdated);
    bus.Emit(EventType::RowRemoved);
    ASSERT_EQ(listener.ids.size(), 3u);
    ASSERT_TRUE(listener.ids[0] < listener.ids[1]);
    ASSERT_TRUE(listener.ids[1] < listener.ids[2]);
}

TEST(TestEventBusDatasetIntegration) {
    DatasetStore<std::wstring> ds;
    struct Listener : IEventListener {
        std::vector<EventType> types;
        void OnEvent(const MutationEvent& e) override { types.push_back(e.type); }
    } listener;
    ds.GetEventBus().Subscribe(&listener);
    ds.AddRow(L"k1", L"v1");       // RowAdded
    ds.UpdateRow(L"k1", L"v2");    // RowUpdated
    ds.RemoveRow(L"k1");           // RowRemoved
    ds.AddRow(L"k2", L"v2");       // RowAdded
    ds.Clear();                     // DatasetCleared
    ASSERT_EQ(listener.types.size(), 5u);
    ASSERT_EQ(static_cast<int>(listener.types[0]), static_cast<int>(EventType::RowAdded));
    ASSERT_EQ(static_cast<int>(listener.types[1]), static_cast<int>(EventType::RowUpdated));
    ASSERT_EQ(static_cast<int>(listener.types[2]), static_cast<int>(EventType::RowRemoved));
    ASSERT_EQ(static_cast<int>(listener.types[3]), static_cast<int>(EventType::RowAdded));
    ASSERT_EQ(static_cast<int>(listener.types[4]), static_cast<int>(EventType::DatasetCleared));
}

TEST(TestEventBusSharedValueIntegration) {
    SharedValue<int> sv;
    struct Listener : IEventListener {
        std::vector<EventType> types;
        void OnEvent(const MutationEvent& e) override { types.push_back(e.type); }
    } listener;
    sv.GetEventBus().Subscribe(&listener);
    sv.SetValue(42);   // ValueSet
    sv.SetValue(100);  // ValueSet
    ASSERT_EQ(listener.types.size(), 2u);
    ASSERT_EQ(static_cast<int>(listener.types[0]), static_cast<int>(EventType::ValueSet));
}

TEST(TestEventBusMetadata) {
    DatasetStore<std::wstring> ds;
    struct Listener : IEventListener {
        MutationEvent lastEvent;
        void OnEvent(const MutationEvent& e) override { lastEvent = e; }
    } listener;
    ds.GetEventBus().Subscribe(&listener);
    ds.AddRow(L"server01", L"online");
    ASSERT_EQ(listener.lastEvent.key, L"server01");
    ASSERT_EQ(listener.lastEvent.newValue, L"online");
    ASSERT_EQ(static_cast<int>(listener.lastEvent.type), static_cast<int>(EventType::RowAdded));

    ds.UpdateRow(L"server01", L"offline");
    ASSERT_EQ(listener.lastEvent.oldValue, L"online");
    ASSERT_EQ(listener.lastEvent.newValue, L"offline");
}

// ============ Main ============

int main() {
    std::cout << "=== SharedValue Tests ===" << std::endl;
    RUN_TEST(TestSharedValueGetSet);
    RUN_TEST(TestSharedValueObserver);
    RUN_TEST(TestSharedValueSystemInfo);

    std::cout << "\n=== DatasetStore Tests ===" << std::endl;
    RUN_TEST(TestDatasetAddAndGet);
    RUN_TEST(TestDatasetUpdate);
    RUN_TEST(TestDatasetPagination);
    RUN_TEST(TestDatasetRemoveAndClear);
    RUN_TEST(TestDatasetObserver);
    RUN_TEST(TestDatasetInPlaceAccess);
    RUN_TEST(TestDatasetOrderedEngine);
    RUN_TEST(TestDatasetUnorderedEngine);
    RUN_TEST(TestDatasetRuntimeSwitch);

    std::cout << "\n=== Error Handling Tests ===" << std::endl;
    RUN_TEST(TestErrorKeyNotFound);
    RUN_TEST(TestErrorDuplicateKey);
    RUN_TEST(TestErrorStoreModeNotEmpty);

    std::cout << "\n=== EventBus Tests ===" << std::endl;
    RUN_TEST(TestEventBusEmit);
    RUN_TEST(TestEventBusSequenceId);
    RUN_TEST(TestEventBusDatasetIntegration);
    RUN_TEST(TestEventBusSharedValueIntegration);
    RUN_TEST(TestEventBusMetadata);

    std::cout << "\n========================================" << std::endl;
    std::cout << "Results: " << testsPassed << " passed, " << testsFailed << " failed" << std::endl;
    return testsFailed > 0 ? 1 : 0;
}
