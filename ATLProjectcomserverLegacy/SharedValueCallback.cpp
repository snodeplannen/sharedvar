// SharedValueCallback.cpp
#include "pch.h"
#include "SharedValueCallback.h"

STDMETHODIMP CSharedValueCallback::OnValueChanged(VARIANT newValue)
{
    // Implementeer hier de callback logica
    return S_OK;
}

STDMETHODIMP CSharedValueCallback::OnDateTimeChanged(BSTR newDateTime)
{
    // Implementeer hier de callback logica
    return S_OK;
}