#pragma once
#include <string>

namespace SharedValueV2 {

template <typename TValue>
class IDatasetObserver {
public:
    virtual ~IDatasetObserver() = default;
    virtual void OnRowAdded(const std::wstring& key, const TValue& value) = 0;
    virtual void OnRowUpdated(const std::wstring& key, const TValue& newValue) = 0;
    virtual void OnRowRemoved(const std::wstring& key) = 0;
    virtual void OnDatasetCleared() = 0;
};

} // namespace SharedValueV2
