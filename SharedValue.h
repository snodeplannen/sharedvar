// SharedValue.h
#pragma once
#include "pch.h"

#include <atlbase.h>
#include <atlcom.h>
#include <atlsync.h>
#include <vector>
#include <chrono>
#include <format>
#include "SharedValueCallback.h"
//#include "SharedValue.h"
#include "ATLProjectcomserver_i.h"
using namespace ATL;

class ATL_NO_VTABLE CSharedValue :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CSharedValue, &CLSID_SharedValue>,
    public IDispatchImpl<ISharedValue, &IID_ISharedValue>
{
public:
    CSharedValue() : m_lockCount(0), m_lockEvent(NULL)
    {
    }

    ~CSharedValue()
    {
        if (m_lockEvent) {
            CloseHandle(m_lockEvent);
        }
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_SHAREDVALUE)

    using CComObjectRootEx<CComMultiThreadModel>::Lock;

    BEGIN_COM_MAP(CSharedValue)
        COM_INTERFACE_ENTRY(ISharedValue)
        COM_INTERFACE_ENTRY(IDispatch)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    HRESULT FinalConstruct()
    {
        m_lockEvent = CreateEvent(NULL, TRUE, TRUE, L"Global\\SharedValueLock");
        return m_lockEvent == NULL ? E_FAIL : S_OK;
    }

    void FinalRelease()
    {
        if (m_lockEvent) {
            CloseHandle(m_lockEvent);
            m_lockEvent = NULL;
        }
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

private:
    CComVariant m_value;
    mutable CComAutoCriticalSection m_cs;
    HANDLE m_lockEvent;
    LONG m_lockCount;
    std::vector<CComPtr<ISharedValueCallback>> m_callbacks;

    void NotifyValueChanged();
    void NotifyDateTimeChanged();
};

OBJECT_ENTRY_AUTO(__uuidof(SharedValue), CSharedValue)