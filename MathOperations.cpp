#include "pch.h"
#include "MathOperations.h"

STDMETHODIMP CMathOperations::Add(DOUBLE a, DOUBLE b, DOUBLE* result)
{
    if (!result) return E_POINTER;
    *result = a + b;
    return S_OK;
}

STDMETHODIMP CMathOperations::Subtract(DOUBLE a, DOUBLE b, DOUBLE* result)
{
    if (!result) return E_POINTER;
    *result = a - b;
    return S_OK;
}

STDMETHODIMP CMathOperations::Multiply(DOUBLE a, DOUBLE b, DOUBLE* result)
{
    if (!result) return E_POINTER;
    *result = a * b;
    return S_OK;
}

STDMETHODIMP CMathOperations::Divide(DOUBLE a, DOUBLE b, DOUBLE* result)
{
    if (!result) return E_POINTER;
    if (b == 0) return E_INVALIDARG;
    *result = a / b;
    return S_OK;
} 