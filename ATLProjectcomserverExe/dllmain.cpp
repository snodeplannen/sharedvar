#include "pch.h"
#include "resource.h"
#include "ATLProjectcomserver_i.h"
#include "dllmain.h"

CATLProjectcomserverExeModule _AtlModule;

// ATL EXE Entry Point
extern "C" int WINAPI _tWinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, 
                                LPTSTR /*lpCmdLine*/, int nShowCmd)
{
    _AtlModule.Lock(); // PREVENT THE SERVER FROM EXITING PREMATURELY!
    return _AtlModule.WinMain(nShowCmd);
} 