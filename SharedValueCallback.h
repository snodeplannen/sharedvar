// SharedValueCallback.h
#pragma once
#include "pch.h"
#include <atlbase.h>
#include <atlcom.h>
#include <vector>
#include "resource.h"
#include "ATLProjectcomserver_i.h"
//#include "..\ATLProject-comserver\ATLProjectcomserver_i.c"

using namespace ATL;
class ATL_NO_VTABLE CSharedValueCallback :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CSharedValueCallback, &CLSID_SharedValueCallback>,
    public IDispatchImpl<ISharedValueCallback, &IID_ISharedValueCallback>
{
public:
    DECLARE_REGISTRY_RESOURCEID(IDR_SHAREDVALUECALLBACK)

    BEGIN_COM_MAP(CSharedValueCallback)
        COM_INTERFACE_ENTRY(ISharedValueCallback)
        COM_INTERFACE_ENTRY(IDispatch)
    END_COM_MAP()

    // ISharedValueCallback
    STDMETHOD(OnValueChanged)(VARIANT newValue);
    STDMETHOD(OnDateTimeChanged)(BSTR newDateTime);
};

OBJECT_ENTRY_AUTO(__uuidof(SharedValueCallback), CSharedValueCallback)