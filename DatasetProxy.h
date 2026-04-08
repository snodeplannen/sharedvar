// DatasetProxy.h
#pragma once
#include "pch.h"
#include <atlbase.h>
#include <atlcom.h>
#include "resource.h"
#include "ComErrorHelper.h"
#include "ATLProjectcomserver_i.h"

#include "SharedValueV2/include/DatasetStore.hpp"
#include "SharedValueV2/include/EventBus.hpp"

using namespace ATL;

// COM-to-EventBus bridge: forwards C++ events to COM IEventCallback clients
class ComEventBridge : public SharedValueV2::IEventListener {
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
    bool HasCallbacks() const { return !m_callbacks.empty(); }

    void OnEvent(const SharedValueV2::MutationEvent& event) override {
        // Convert timestamp to BSTR
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

class ATL_NO_VTABLE CDatasetProxy :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CDatasetProxy, &CLSID_DatasetProxy>,
    public ISupportErrorInfo,
    public IDispatchImpl<IDatasetProxy, &IID_IDatasetProxy>
{
public:
    CDatasetProxy() {}

    DECLARE_REGISTRY_RESOURCEID(IDR_DATASETPROXY)

    BEGIN_COM_MAP(CDatasetProxy)
        COM_INTERFACE_ENTRY(IDatasetProxy)
        COM_INTERFACE_ENTRY(IDispatch)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    HRESULT FinalConstruct() {
        m_store = std::make_unique<SharedValueV2::DatasetStore<std::wstring>>();
        m_store->GetEventBus().Subscribe(&m_bridge);
        return S_OK;
    }

    void FinalRelease() {
        if (m_store) {
            m_store->GetEventBus().Unsubscribe(&m_bridge);
        }
    }

    // ISupportErrorInfo
    STDMETHOD(InterfaceSupportsErrorInfo)(REFIID riid) {
        if (InlineIsEqualGUID(riid, IID_IDatasetProxy)) return S_OK;
        return S_FALSE;
    }

    // IDatasetProxy
    STDMETHOD(SetStorageMode)(LONG mode);
    STDMETHOD(GetStorageMode)(LONG* mode);
    STDMETHOD(AddRow)(BSTR key, BSTR value);
    STDMETHOD(GetRowData)(BSTR key, BSTR* data);
    STDMETHOD(UpdateRow)(BSTR key, BSTR value, VARIANT_BOOL* success);
    STDMETHOD(RemoveRow)(BSTR key, VARIANT_BOOL* success);
    STDMETHOD(Clear)();
    STDMETHOD(GetRecordCount)(LONG* count);
    STDMETHOD(HasKey)(BSTR key, VARIANT_BOOL* exists);
    STDMETHOD(FetchPageKeys)(LONG startIndex, LONG limit, VARIANT* keys);
    STDMETHOD(FetchPageData)(LONG startIndex, LONG limit, VARIANT* keysAndValues);
    STDMETHOD(RegisterEventCallback)(IEventCallback* callback);
    STDMETHOD(UnregisterEventCallback)(IEventCallback* callback);

private:
    std::unique_ptr<SharedValueV2::DatasetStore<std::wstring>> m_store;
    ComEventBridge m_bridge;
};

OBJECT_ENTRY_AUTO(__uuidof(DatasetProxy), CDatasetProxy)
