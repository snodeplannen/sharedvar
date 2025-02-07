// SharedValue.cpp
#include "pch.h"
#include "SharedValue.h"
#include <Lmcons.h> // Voor Windows username functies

STDMETHODIMP CSharedValue::SetValue(VARIANT value)
{
    if (WaitForSingleObject(m_lockEvent, 0) != WAIT_OBJECT_0)
        return E_ACCESSDENIED;

    HRESULT hr = S_OK;
    {
        CComCritSecLock<CComAutoCriticalSection> lock(m_cs);
        hr = m_value.Copy(&value);
        if (SUCCEEDED(hr))
            NotifyValueChanged();
    }
    return hr;
}

STDMETHODIMP CSharedValue::GetValue(VARIANT* value)
{
    if (!value) return E_POINTER;

    CComCritSecLock<CComAutoCriticalSection> lock(m_cs);
    return m_value.Copy(value);
}

STDMETHODIMP CSharedValue::GetCurrentUTCDateTime(BSTR* dateTime)
{
    if (!dateTime) return E_POINTER;

    auto now = std::chrono::system_clock::now();
    auto utc_time = std::chrono::system_clock::to_time_t(now);
    std::tm utc_tm;
    gmtime_s(&utc_tm, &utc_time);

    wchar_t buffer[64];
    std::wcsftime(buffer, sizeof(buffer) / sizeof(wchar_t),
        L"%Y-%m-%d %H:%M:%S", &utc_tm);

    *dateTime = CComBSTR(buffer).Detach();
    NotifyDateTimeChanged();
    return S_OK;
}

STDMETHODIMP CSharedValue::GetCurrentUserLogin(BSTR* login)
{
    if (!login) return E_POINTER;

    wchar_t username[UNLEN + 1];
    DWORD username_len = UNLEN + 1;
    if (!GetUserNameW(username, &username_len))
        return HRESULT_FROM_WIN32(GetLastError());

    *login = CComBSTR(username).Detach();
    return S_OK;
}

STDMETHODIMP CSharedValue::LockSharedValue(LONG timeoutMs)
{
    if (WaitForSingleObject(m_lockEvent, timeoutMs) != WAIT_OBJECT_0)
        return E_ACCESSDENIED;

    InterlockedIncrement(&m_lockCount);
    ResetEvent(m_lockEvent);
    return S_OK;
}

STDMETHODIMP CSharedValue::Unlock()
{
    if (InterlockedDecrement(&m_lockCount) == 0)
        SetEvent(m_lockEvent);
    return S_OK;
}

STDMETHODIMP CSharedValue::Subscribe(ISharedValueCallback* callback)
{
    if (!callback) return E_POINTER;

    CComCritSecLock<CComAutoCriticalSection> lock(m_cs);
    m_callbacks.push_back(callback);
    return S_OK;
}

STDMETHODIMP CSharedValue::Unsubscribe(ISharedValueCallback* callback)
{
    if (!callback) return E_POINTER;

    CComCritSecLock<CComAutoCriticalSection> lock(m_cs);
    auto it = std::find_if(m_callbacks.begin(), m_callbacks.end(),
        [callback](const CComPtr<ISharedValueCallback>& ptr) {
            return ptr.p == callback;
        });

    if (it != m_callbacks.end())
        m_callbacks.erase(it);

    return S_OK;
}

void CSharedValue::NotifyValueChanged()
{
    CComCritSecLock<CComAutoCriticalSection> lock(m_cs);
    for (auto& callback : m_callbacks)
    {
        callback->OnValueChanged(m_value);
    }
}

void CSharedValue::NotifyDateTimeChanged()
{
    CComBSTR dateTime;
    if (SUCCEEDED(GetCurrentUTCDateTime(&dateTime)))
    {
        CComCritSecLock<CComAutoCriticalSection> lock(m_cs);
        for (auto& callback : m_callbacks)
        {
            callback->OnDateTimeChanged(dateTime);
        }
    }
}