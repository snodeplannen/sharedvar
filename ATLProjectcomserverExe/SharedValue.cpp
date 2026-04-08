#include "pch.h"
#include "SharedValue.h"
#include "dllmain.h"

STDMETHODIMP CSharedValue::SetValue(VARIANT value)
{
    COM_METHOD_BODY(CLSID_SharedValue, IID_ISharedValue,
        CComVariant varValue;
        HRESULT hr = varValue.Copy(&value);
        if (FAILED(hr)) throw std::runtime_error("Failed to copy VARIANT");
        m_core.SetValue(varValue)
    )
}

STDMETHODIMP CSharedValue::GetValue(VARIANT* value)
{
    if (!value) return E_POINTER;
    COM_METHOD_BODY(CLSID_SharedValue, IID_ISharedValue,
        CComVariant varValue = m_core.GetValue();
        varValue.Detach(value)
    )
}

STDMETHODIMP CSharedValue::GetCurrentUTCDateTime(BSTR* dateTime)
{
    if (!dateTime) return E_POINTER;
    COM_METHOD_BODY(CLSID_SharedValue, IID_ISharedValue,
        std::wstring dt = m_core.GetCurrentUTCDateTime();
        *dateTime = CComBSTR(dt.c_str()).Detach()
    )
}

STDMETHODIMP CSharedValue::GetCurrentUserLogin(BSTR* login)
{
    if (!login) return E_POINTER;
    COM_METHOD_BODY(CLSID_SharedValue, IID_ISharedValue,
        std::wstring dt = m_core.GetCurrentUserLogin();
        *login = CComBSTR(dt.c_str()).Detach()
    )
}

STDMETHODIMP CSharedValue::LockSharedValue(LONG timeoutMs)
{
    COM_METHOD_BODY(CLSID_SharedValue, IID_ISharedValue,
        if (!m_core.LockSharedValueTimeout(timeoutMs)) {
            throw SharedValueV2::SharedValueException(
                SharedValueV2::ErrorCode::LockTimeout, "Lock timeout");
        }
    )
}

STDMETHODIMP CSharedValue::Unlock()
{
    COM_METHOD_BODY(CLSID_SharedValue, IID_ISharedValue,
        m_core.Unlock()
    )
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

STDMETHODIMP CSharedValue::RegisterEventCallback(IEventCallback* callback)
{
    if (!callback) return E_POINTER;
    m_eventBridge.AddCallback(callback);
    return S_OK;
}

STDMETHODIMP CSharedValue::UnregisterEventCallback(IEventCallback* callback)
{
    if (!callback) return E_POINTER;
    m_eventBridge.RemoveCallback(callback);
    return S_OK;
}

void CSharedValue::OnValueChanged(const CComVariant& newValue)
{
    std::vector<CComPtr<ISharedValueCallback>> callbacksCopy;
    {
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
STDMETHODIMP CSharedValue::ShutdownServer()
{
    // Forcefully remove the global DatasetProxy to release its ATL module lock count
    m_core.SetValue(CComVariant());

    // Remove the arbitrary lock we placed in _tWinMain
    extern CATLProjectcomserverExeModule _AtlModule;
    _AtlModule.Unlock();
    return S_OK;
}