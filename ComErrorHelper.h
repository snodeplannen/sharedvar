#pragma once
// ComErrorHelper.h — Central COM error translation layer
//
// Usage in COM methods:
//   STDMETHODIMP CMyClass::MyMethod(BSTR arg) {
//       if (!arg) return E_POINTER;
//       COM_METHOD_BODY(CLSID_MyClass, IID_IMyInterface,
//           // ... your C++ logic here ...
//       )
//   }

#include <atlbase.h>
#include <atlcom.h>
#include "SharedValueV2/include/Errors.hpp"

#define COM_METHOD_BODY(clsid, iid, ...) \
    try { \
        __VA_ARGS__; \
        return S_OK; \
    } catch (const SharedValueV2::SharedValueException& e) { \
        return AtlReportError(clsid, CA2W(e.what()), iid, \
            MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, static_cast<WORD>(e.code))); \
    } catch (const std::out_of_range& e) { \
        return AtlReportError(clsid, CA2W(e.what()), iid, E_BOUNDS); \
    } catch (const std::invalid_argument& e) { \
        return AtlReportError(clsid, CA2W(e.what()), iid, E_INVALIDARG); \
    } catch (const std::exception& e) { \
        return AtlReportError(clsid, CA2W(e.what()), iid, E_FAIL); \
    } catch (...) { \
        return AtlReportError(clsid, L"Unknown internal error", iid, E_UNEXPECTED); \
    }
