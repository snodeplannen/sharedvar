#include <iostream>
#include <atlbase.h>
#include <comdef.h>
#include "ATLProjectcomserver_i.h"

int main()
{
    // COM initialiseren
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr)) {
        std::cout << "COM initialisatie mislukt" << std::endl;
        return 1;
    }

    try {
        // COM object aanmaken
        CComPtr<IMathOperations> mathOps;
        hr = mathOps.CoCreateInstance(CLSID_MathOperations);
        if (FAILED(hr)) {
            throw _com_error(hr);
        }

        // Test berekeningen
        double result;
        hr = mathOps->Add(5, 3, &result);
        if (SUCCEEDED(hr)) {
            std::cout << "5 + 3 = " << result << std::endl;
        }

        // Meer tests hier...

    }
    catch (_com_error& e) {
        std::cout << "Error: " << e.ErrorMessage() << std::endl;
    }

    // COM opruimen
    CoUninitialize();
    return 0;
} 