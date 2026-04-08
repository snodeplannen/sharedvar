// SharedValue.h
#pragma once
#include "pch.h"

#include <atlbase.h>
#include <atlcom.h>
#include "SharedValueCallback.h"
#include "ATLProjectcomserver_i.h"

#include "SharedValueV2/include/SharedValue.hpp"
#include "SharedValueV2/include/LockPolicies.hpp"

using namespace ATL;

class ATL_NO_VTABLE CSharedValue :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CSharedValue, &CLSID_SharedValue>,
    public IDispatchImpl<ISharedValue, &IID_ISharedValue>,
    public SharedValueV2::ISharedValueObserver<CComVariant>
{
public:
    CSharedValue()
    {
        // Subscribe to our own internal V2 core
        m_core.Subscribe(this);
    }

    ~CSharedValue()
    {
        m_core.Unsubscribe(this);
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_SHAREDVALUE)

    BEGIN_COM_MAP(CSharedValue)
        COM_INTERFACE_ENTRY(ISharedValue)
        COM_INTERFACE_ENTRY(IDispatch)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    HRESULT FinalConstruct()
    {
        return S_OK;
    }

    void FinalRelease()
    {
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

    // Callbacks from V2 Core
    void OnValueChanged(const CComVariant& newValue) override;
    void OnDateTimeChanged(const std::wstring& newDateTime) override;

private:
    SharedValueV2::SharedValue<CComVariant, SharedValueV2::LocalMutexPolicy> m_core;
    
    mutable CComAutoCriticalSection m_csCallbacks;
    std::vector<CComPtr<ISharedValueCallback>> m_callbacks;
};

OBJECT_ENTRY_AUTO(__uuidof(SharedValue), CSharedValue)