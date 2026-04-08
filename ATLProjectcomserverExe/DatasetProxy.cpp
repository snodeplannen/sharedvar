// DatasetProxy.cpp
#include "pch.h"
#include "DatasetProxy.h"
#include <atlsafe.h>

STDMETHODIMP CDatasetProxy::SetStorageMode(LONG mode)
{
    COM_METHOD_BODY(CLSID_DatasetProxy, IID_IDatasetProxy,
        if (mode != 0 && mode != 1) {
            throw SharedValueV2::InvalidStorageModeException(mode);
        }
        auto sm = (mode == 0) ? SharedValueV2::StorageMode::Ordered
                              : SharedValueV2::StorageMode::Unordered;
        m_store->SetStorageMode(sm)
    )
}

STDMETHODIMP CDatasetProxy::GetStorageMode(LONG* mode)
{
    if (!mode) return E_POINTER;
    COM_METHOD_BODY(CLSID_DatasetProxy, IID_IDatasetProxy,
        *mode = static_cast<LONG>(m_store->GetStorageMode())
    )
}

STDMETHODIMP CDatasetProxy::AddRow(BSTR key, BSTR value)
{
    if (!key || !value) return E_POINTER;
    COM_METHOD_BODY(CLSID_DatasetProxy, IID_IDatasetProxy,
        m_store->AddRow(std::wstring(key), std::wstring(value))
    )
}

STDMETHODIMP CDatasetProxy::GetRowData(BSTR key, BSTR* data)
{
    if (!key || !data) return E_POINTER;
    COM_METHOD_BODY(CLSID_DatasetProxy, IID_IDatasetProxy,
        auto result = m_store->GetRowOrThrow(std::wstring(key));
        *data = CComBSTR(result.c_str()).Detach()
    )
}

STDMETHODIMP CDatasetProxy::UpdateRow(BSTR key, BSTR value, VARIANT_BOOL* success)
{
    if (!key || !value || !success) return E_POINTER;
    COM_METHOD_BODY(CLSID_DatasetProxy, IID_IDatasetProxy,
        *success = m_store->UpdateRow(std::wstring(key), std::wstring(value))
            ? VARIANT_TRUE : VARIANT_FALSE
    )
}

STDMETHODIMP CDatasetProxy::RemoveRow(BSTR key, VARIANT_BOOL* success)
{
    if (!key || !success) return E_POINTER;
    COM_METHOD_BODY(CLSID_DatasetProxy, IID_IDatasetProxy,
        *success = m_store->RemoveRow(std::wstring(key))
            ? VARIANT_TRUE : VARIANT_FALSE
    )
}

STDMETHODIMP CDatasetProxy::Clear()
{
    COM_METHOD_BODY(CLSID_DatasetProxy, IID_IDatasetProxy,
        m_store->Clear()
    )
}

STDMETHODIMP CDatasetProxy::GetRecordCount(LONG* count)
{
    if (!count) return E_POINTER;
    COM_METHOD_BODY(CLSID_DatasetProxy, IID_IDatasetProxy,
        *count = static_cast<LONG>(m_store->GetRecordCount())
    )
}

STDMETHODIMP CDatasetProxy::HasKey(BSTR key, VARIANT_BOOL* exists)
{
    if (!key || !exists) return E_POINTER;
    COM_METHOD_BODY(CLSID_DatasetProxy, IID_IDatasetProxy,
        *exists = m_store->HasKey(std::wstring(key)) ? VARIANT_TRUE : VARIANT_FALSE
    )
}

STDMETHODIMP CDatasetProxy::FetchPageKeys(LONG startIndex, LONG limit, VARIANT* keys)
{
    if (!keys) return E_POINTER;
    COM_METHOD_BODY(CLSID_DatasetProxy, IID_IDatasetProxy,
        auto keyVec = m_store->FetchKeys(static_cast<size_t>(startIndex),
                                          static_cast<size_t>(limit));
        CComSafeArray<BSTR> sa(static_cast<ULONG>(keyVec.size()));
        for (LONG i = 0; i < static_cast<LONG>(keyVec.size()); ++i) {
            sa.SetAt(i, CComBSTR(keyVec[i].c_str()).Detach());
        }
        CComVariant var;
        var.vt = VT_ARRAY | VT_BSTR;
        var.parray = sa.Detach();
        var.Detach(keys)
    )
}

STDMETHODIMP CDatasetProxy::FetchPageData(LONG startIndex, LONG limit, VARIANT* keysAndValues)
{
    if (!keysAndValues) return E_POINTER;
    COM_METHOD_BODY(CLSID_DatasetProxy, IID_IDatasetProxy,
        auto page = m_store->FetchPage(static_cast<size_t>(startIndex),
                                        static_cast<size_t>(limit));
        // Build 2D SAFEARRAY: [rows][2] (key, value)
        SAFEARRAYBOUND bounds[2];
        bounds[0].lLbound = 0; bounds[0].cElements = static_cast<ULONG>(page.size());
        bounds[1].lLbound = 0; bounds[1].cElements = 2;
        SAFEARRAY* psa = SafeArrayCreate(VT_VARIANT, 2, bounds);
        for (LONG row = 0; row < static_cast<LONG>(page.size()); ++row) {
            LONG idx[2];
            CComVariant vKey(page[row].first.c_str());
            CComVariant vVal(page[row].second.c_str());
            idx[0] = row; idx[1] = 0;
            SafeArrayPutElement(psa, idx, &vKey);
            idx[0] = row; idx[1] = 1;
            SafeArrayPutElement(psa, idx, &vVal);
        }
        VariantInit(keysAndValues);
        keysAndValues->vt = VT_ARRAY | VT_VARIANT;
        keysAndValues->parray = psa
    )
}

STDMETHODIMP CDatasetProxy::RegisterEventCallback(IEventCallback* callback)
{
    if (!callback) return E_POINTER;
    m_bridge.AddCallback(callback);
    return S_OK;
}

STDMETHODIMP CDatasetProxy::UnregisterEventCallback(IEventCallback* callback)
{
    if (!callback) return E_POINTER;
    m_bridge.RemoveCallback(callback);
    return S_OK;
}
