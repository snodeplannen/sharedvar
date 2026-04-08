// SharedValue.cpp
#include "pch.h"
#include "SharedValue.h"

STDMETHODIMP CSharedValue::SetValue(VARIANT value)
{
    CComVariant varValue;
    HRESULT hr = varValue.Copy(&value);
    if (FAILED(hr)) return hr;

    // Delgate to modern Core which handles thread safety and notification automatically
    m_core.SetValue(varValue);
    
    return S_OK;
}

STDMETHODIMP CSharedValue::GetValue(VARIANT* value)
{
    if (!value) return E_POINTER;
    
    CComVariant varValue = m_core.GetValue();
    return varValue.Detach(value);
}

STDMETHODIMP CSharedValue::GetCurrentUTCDateTime(BSTR* dateTime)
{
    if (!dateTime) return E_POINTER;
    
    std::wstring dt = m_core.GetCurrentUTCDateTime();
    *dateTime = CComBSTR(dt.c_str()).Detach();
    
    return S_OK;
}

STDMETHODIMP CSharedValue::GetCurrentUserLogin(BSTR* login)
{
    if (!login) return E_POINTER;
    
    std::wstring dt = m_core.GetCurrentUserLogin();
    *login = CComBSTR(dt.c_str()).Detach();
    
    return S_OK;
}

STDMETHODIMP CSharedValue::LockSharedValue(LONG timeoutMs)
{
    if (m_core.LockSharedValueTimeout(timeoutMs)) {
        return S_OK;
    }
    return E_ACCESSDENIED;
}

STDMETHODIMP CSharedValue::Unlock()
{
    m_core.Unlock();
    return S_OK;
}

STDMETHODIMP CSharedValue::Subscribe(ISharedValueCallback* callback)
{
    if (!callback) return E_POINTER;

    CComCritSecLock<CComAutoCriticalSection> lock(m_csCallbacks);
    m_callbacks.push_back(callback);
    return S_OK;
}

STDMETHODIMP CSharedValue::Unsubscribe(ISharedValueCallback* callback)
{
    if (!callback) return E_POINTER;

    CComCritSecLock<CComAutoCriticalSection> lock(m_csCallbacks);
    auto it = std::find_if(m_callbacks.begin(), m_callbacks.end(),
        [callback](const CComPtr<ISharedValueCallback>& ptr) {
            return ptr.p == callback;
        });

    if (it != m_callbacks.end())
        m_callbacks.erase(it);

    return S_OK;
}

void CSharedValue::OnValueChanged(const CComVariant& newValue)
{
    std::vector<CComPtr<ISharedValueCallback>> callbacksCopy;
    {
        // Thread-safe copy to avoid deadlocks!
        CComCritSecLock<CComAutoCriticalSection> lock(m_csCallbacks);
        callbacksCopy = m_callbacks;
    }
    
    CComVariant valCopy = newValue;
    for (auto& cb : callbacksCopy) {
        cb->OnValueChanged(valCopy);
    }
}

void CSharedValue::OnDateTimeChanged(const std::wstring& newDateTime)
{
    std::vector<CComPtr<ISharedValueCallback>> callbacksCopy;
    {
        CComCritSecLock<CComAutoCriticalSection> lock(m_csCallbacks);
        callbacksCopy = m_callbacks;
    }
    
    CComBSTR bstrDt(newDateTime.c_str());
    for (auto& cb : callbacksCopy) {
        cb->OnDateTimeChanged(bstrDt);
    }
}