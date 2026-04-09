#pragma once

#include <atlbase.h>
#include <atlcom.h>
#include "Resource.h"
#include "ATLProjectcomserver_i.h"

class CATLProjectcomserverExeModule : public ATL::CAtlExeModuleT<CATLProjectcomserverExeModule>
{
public:
    DECLARE_LIBID(LIBID_ATLProjectcomserverLib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_ATLPROJECTCOMSERVER, "{B0A0188F-59B6-42A5-AD3A-9D3CBE079253}")
};
extern CATLProjectcomserverExeModule _AtlModule; 