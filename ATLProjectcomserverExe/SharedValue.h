// SharedValue.h
#pragma once
#include "pch.h"

#include <atlbase.h>
#include <atlcom.h>
#include "ComErrorHelper.h"
#include "SharedValueCallback.h"
#include "ATLProjectcomserver_i.h"
#include "DatasetProxy.h"

#include "SharedValueV2/include/SharedValue.hpp"
#include "SharedValueV2/include/LockPolicies.hpp"
#include "SharedValueV2/include/EventBus.hpp"

using namespace ATL;

// COM-to-EventBus bridge for SharedValue
class SharedValueEventBridge : public SharedValueV2::IEventListener {
public:
    void AddCallback(IEventCallback* cb) {
        if (!cb) return;
        CComPtr<IEventCallback> ptr(cb);
        m_callbacks.push_back(ptr);
    }
    void RemoveCallback(IEventCallback* cb) {
        if (!cb) return;
        auto it = std::find_if(m_callbacks.begin(), m_callbacks.end(),
            [cb](const CComPtr<IEventCallback>& p) { return p.p == cb; });
        if (it != m_callbacks.end()) m_callbacks.erase(it);
    }

    void OnEvent(const SharedValueV2::MutationEvent& event) override {
        auto tt = std::chrono::system_clock::to_time_t(event.timestamp);
        std::tm tm_buf;
#ifdef _WIN32
        gmtime_s(&tm_buf, &tt);
#else
        gmtime_r(&tt, &tm_buf);
#endif
        wchar_t tsBuf[64];
        wcsftime(tsBuf, 64, L"%Y-%m-%d %H:%M:%S", &tm_buf);

        CComBSTR bstrKey(event.key.c_str());
        CComBSTR bstrOld(event.oldValue.c_str());
        CComBSTR bstrNew(event.newValue.c_str());
        CComBSTR bstrSrc(event.source.c_str());
        CComBSTR bstrTs(tsBuf);

        for (auto& cb : m_callbacks) {
            cb->OnMutationEvent(
                static_cast<LONG>(event.type),
                bstrKey, bstrOld, bstrNew, bstrSrc, bstrTs,
                static_cast<LONG>(event.sequenceId));
        }
    }

private:
    std::vector<CComPtr<IEventCallback>> m_callbacks;
};

class ATL_NO_VTABLE CSharedValue :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CSharedValue, &CLSID_SharedValue>,
    public ISupportErrorInfo,
    public IDispatchImpl<ISharedValue, &IID_ISharedValue>,
    public SharedValueV2::ISharedValueObserver<CComVariant>
{
public:
    CSharedValue()
    {
        m_core.Subscribe(this);
        m_core.GetEventBus().Subscribe(&m_eventBridge);
    }

    ~CSharedValue()
    {
        m_core.GetEventBus().Unsubscribe(&m_eventBridge);
        m_core.Unsubscribe(this);
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_SHAREDVALUE)

    BEGIN_COM_MAP(CSharedValue)
        COM_INTERFACE_ENTRY(ISharedValue)
        COM_INTERFACE_ENTRY(IDispatch)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
    END_COM_MAP()

    DECLARE_CLASSFACTORY_SINGLETON(CSharedValue)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    HRESULT FinalConstruct() { 
        CComObject<CDatasetProxy>* pProxy = nullptr;
        HRESULT hr = CComObject<CDatasetProxy>::CreateInstance(&pProxy);
        if (SUCCEEDED(hr) && pProxy) {
            pProxy->AddRef();
            CComVariant varProxy;
            pProxy->QueryInterface(IID_IDispatch, (void**)&varProxy.pdispVal);
            if (varProxy.pdispVal) {
                varProxy.vt = VT_DISPATCH;
                m_core.SetValue(varProxy);
            }
            pProxy->Release();
        }
        return S_OK; 
    }

    void FinalRelease() {}

    // ISupportErrorInfo
    STDMETHOD(InterfaceSupportsErrorInfo)(REFIID riid) {
        if (InlineIsEqualGUID(riid, IID_ISharedValue)) return S_OK;
        return S_FALSE;
    }

    // ISharedValue
    STDMETHOD(SetValue)(VARIANT value);
    STDMETHOD(GetValue)(VARIANT* value);
    STDMETHOD_(HRESULT __stdcall, GetCurrentUTCDateTime)(BSTR* dateTime);
    STDMETHOD_(HRESULT __stdcall, GetCurrentUserLogin)(BSTR* login);
    STDMETHOD(LockSharedValue)(LONG timeoutMs);
    STDMETHOD(Unlock)();
    STDMETHOD(Subscribe)(ISharedValueCallback* callback);
    STDMETHOD(Unsubscribe)(ISharedValueCallback* callback);
    STDMETHOD(ShutdownServer)();
    
    // IEventCallback proxy
    STDMETHOD(RegisterEventCallback)(IEventCallback* callback);
    STDMETHOD(UnregisterEventCallback)(IEventCallback* callback);

    // Callbacks from V2 Core
    void OnValueChanged(const CComVariant& newValue) override;
    void OnDateTimeChanged(const std::wstring& newDateTime) override;

private:
    SharedValueV2::SharedValue<CComVariant, SharedValueV2::LocalMutexPolicy> m_core;
    
    mutable CComAutoCriticalSection m_csCallbacks;
    std::vector<CComPtr<ISharedValueCallback>> m_callbacks;
    SharedValueEventBridge m_eventBridge;
};

OBJECT_ENTRY_AUTO(__uuidof(SharedValue), CSharedValue)