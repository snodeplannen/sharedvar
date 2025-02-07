#pragma once
#include "pch.h"
#include "resource.h"
#include "ATLProjectcomserver_i.h"

using namespace ATL;

class ATL_NO_VTABLE CMathOperations :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMathOperations, &CLSID_MathOperations>,
    public IDispatchImpl<IMathOperations, &IID_IMathOperations>
{
public:
    DECLARE_REGISTRY_RESOURCEID(IDR_MATHOPERATIONS)

    BEGIN_COM_MAP(CMathOperations)
        COM_INTERFACE_ENTRY(IMathOperations)
        COM_INTERFACE_ENTRY(IDispatch)
    END_COM_MAP()

    // IMathOperations
    STDMETHOD(Add)(DOUBLE a, DOUBLE b, DOUBLE* result);
    STDMETHOD(Subtract)(DOUBLE a, DOUBLE b, DOUBLE* result);
    STDMETHOD(Multiply)(DOUBLE a, DOUBLE b, DOUBLE* result);
    STDMETHOD(Divide)(DOUBLE a, DOUBLE b, DOUBLE* result);
};

OBJECT_ENTRY_AUTO(__uuidof(MathOperations), CMathOperations) 