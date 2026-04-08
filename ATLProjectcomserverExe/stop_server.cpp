#include <windows.h>
#include <iostream>
#include <comdef.h>

// Import the TypeLib containing the COM interfaces. 
// Change the path if your build output is elsewhere.
#import "x64\\Release\\ATLProjectcomserver.exe" no_namespace named_guids

int main()
{
    // Initialize COM
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        std::cerr << "Failed to initialize COM library." << std::endl;
        return 1;
    }

    std::cout << "Connecting to ATLProjectcomserverExe..." << std::endl;

    // Use smart pointer to manage the COM object lifecycle
    ISharedValuePtr pSharedValue;
    
    // Create an instance of the COM class
    hr = pSharedValue.CreateInstance(CLSID_SharedValue);
    if (FAILED(hr)) {
        std::cerr << "Could not connect to the COM server. Is it running/registered?" << std::endl;
        CoUninitialize();
        return 1;
    }

    std::cout << "Successfully connected! Sending Shutdown signal..." << std::endl;

    // Call our custom Shutdown method
    hr = pSharedValue->ShutdownServer();
    if (SUCCEEDED(hr)) {
        std::cout << "Shutdown signal sent successfully. The server should now exit." << std::endl;
    } else {
        std::cerr << "Failed to send shutdown signal." << std::endl;
    }

    // Clean up
    pSharedValue = nullptr;
    CoUninitialize();

    return 0;
}
