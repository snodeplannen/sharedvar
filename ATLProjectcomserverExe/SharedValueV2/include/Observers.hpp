#pragma once
#include <string>

namespace SharedValueV2 {

template <typename T>
class ISharedValueObserver {
public:
    virtual ~ISharedValueObserver() = default;

    // Called when the value is updated
    virtual void OnValueChanged(const T& newValue) = 0;

    // Called when datetime is updated
    virtual void OnDateTimeChanged(const std::wstring& newDateTime) = 0;
};

} // namespace SharedValueV2
