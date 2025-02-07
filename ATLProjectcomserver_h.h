

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0628 */
/* at Tue Jan 19 04:14:07 2038
 */
/* Compiler settings for ATLProjectcomserver.idl:
    Oicf, W1, Zp8, env=Win64 (32b run), target_arch=AMD64 8.01.0628 
    protocol : all , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */



/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 500
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif /* __RPCNDR_H_VERSION__ */

#ifndef COM_NO_WINDOWS_H
#include "windows.h"
#include "ole2.h"
#endif /*COM_NO_WINDOWS_H*/

#ifndef __ATLProjectcomserver_h_h__
#define __ATLProjectcomserver_h_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#ifndef DECLSPEC_XFGVIRT
#if defined(_CONTROL_FLOW_GUARD_XFG)
#define DECLSPEC_XFGVIRT(base, func) __declspec(xfg_virtual(base, func))
#else
#define DECLSPEC_XFGVIRT(base, func)
#endif
#endif

/* Forward Declarations */ 

#ifndef __IMathOperations_FWD_DEFINED__
#define __IMathOperations_FWD_DEFINED__
typedef interface IMathOperations IMathOperations;

#endif 	/* __IMathOperations_FWD_DEFINED__ */


#ifndef __ISharedValueCallback_FWD_DEFINED__
#define __ISharedValueCallback_FWD_DEFINED__
typedef interface ISharedValueCallback ISharedValueCallback;

#endif 	/* __ISharedValueCallback_FWD_DEFINED__ */


#ifndef __ISharedValue_FWD_DEFINED__
#define __ISharedValue_FWD_DEFINED__
typedef interface ISharedValue ISharedValue;

#endif 	/* __ISharedValue_FWD_DEFINED__ */


#ifndef __MathOperations_FWD_DEFINED__
#define __MathOperations_FWD_DEFINED__

#ifdef __cplusplus
typedef class MathOperations MathOperations;
#else
typedef struct MathOperations MathOperations;
#endif /* __cplusplus */

#endif 	/* __MathOperations_FWD_DEFINED__ */


#ifndef __SharedValueCallback_FWD_DEFINED__
#define __SharedValueCallback_FWD_DEFINED__

#ifdef __cplusplus
typedef class SharedValueCallback SharedValueCallback;
#else
typedef struct SharedValueCallback SharedValueCallback;
#endif /* __cplusplus */

#endif 	/* __SharedValueCallback_FWD_DEFINED__ */


#ifndef __SharedValue_FWD_DEFINED__
#define __SharedValue_FWD_DEFINED__

#ifdef __cplusplus
typedef class SharedValue SharedValue;
#else
typedef struct SharedValue SharedValue;
#endif /* __cplusplus */

#endif 	/* __SharedValue_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"
#include "ocidl.h"
#include "shobjidl.h"

#ifdef __cplusplus
extern "C"{
#endif 


#ifndef __IMathOperations_INTERFACE_DEFINED__
#define __IMathOperations_INTERFACE_DEFINED__

/* interface IMathOperations */
/* [unique][nonextensible][dual][uuid][object] */ 


EXTERN_C const IID IID_IMathOperations;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("488e9f3c-299b-4fe1-8b25-a2b9c2a23506")
    IMathOperations : public IDispatch
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Add( 
            /* [in] */ DOUBLE a,
            /* [in] */ DOUBLE b,
            /* [retval][out] */ DOUBLE *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Subtract( 
            /* [in] */ DOUBLE a,
            /* [in] */ DOUBLE b,
            /* [retval][out] */ DOUBLE *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Multiply( 
            /* [in] */ DOUBLE a,
            /* [in] */ DOUBLE b,
            /* [retval][out] */ DOUBLE *result) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Divide( 
            /* [in] */ DOUBLE a,
            /* [in] */ DOUBLE b,
            /* [retval][out] */ DOUBLE *result) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IMathOperationsVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IMathOperations * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IMathOperations * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IMathOperations * This);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfoCount)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IMathOperations * This,
            /* [out] */ UINT *pctinfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfo)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IMathOperations * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetIDsOfNames)
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IMathOperations * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        DECLSPEC_XFGVIRT(IDispatch, Invoke)
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IMathOperations * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        DECLSPEC_XFGVIRT(IMathOperations, Add)
        HRESULT ( STDMETHODCALLTYPE *Add )( 
            IMathOperations * This,
            /* [in] */ DOUBLE a,
            /* [in] */ DOUBLE b,
            /* [retval][out] */ DOUBLE *result);
        
        DECLSPEC_XFGVIRT(IMathOperations, Subtract)
        HRESULT ( STDMETHODCALLTYPE *Subtract )( 
            IMathOperations * This,
            /* [in] */ DOUBLE a,
            /* [in] */ DOUBLE b,
            /* [retval][out] */ DOUBLE *result);
        
        DECLSPEC_XFGVIRT(IMathOperations, Multiply)
        HRESULT ( STDMETHODCALLTYPE *Multiply )( 
            IMathOperations * This,
            /* [in] */ DOUBLE a,
            /* [in] */ DOUBLE b,
            /* [retval][out] */ DOUBLE *result);
        
        DECLSPEC_XFGVIRT(IMathOperations, Divide)
        HRESULT ( STDMETHODCALLTYPE *Divide )( 
            IMathOperations * This,
            /* [in] */ DOUBLE a,
            /* [in] */ DOUBLE b,
            /* [retval][out] */ DOUBLE *result);
        
        END_INTERFACE
    } IMathOperationsVtbl;

    interface IMathOperations
    {
        CONST_VTBL struct IMathOperationsVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IMathOperations_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IMathOperations_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IMathOperations_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IMathOperations_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IMathOperations_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IMathOperations_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IMathOperations_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define IMathOperations_Add(This,a,b,result)	\
    ( (This)->lpVtbl -> Add(This,a,b,result) ) 

#define IMathOperations_Subtract(This,a,b,result)	\
    ( (This)->lpVtbl -> Subtract(This,a,b,result) ) 

#define IMathOperations_Multiply(This,a,b,result)	\
    ( (This)->lpVtbl -> Multiply(This,a,b,result) ) 

#define IMathOperations_Divide(This,a,b,result)	\
    ( (This)->lpVtbl -> Divide(This,a,b,result) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMathOperations_INTERFACE_DEFINED__ */


#ifndef __ISharedValueCallback_INTERFACE_DEFINED__
#define __ISharedValueCallback_INTERFACE_DEFINED__

/* interface ISharedValueCallback */
/* [unique][nonextensible][dual][uuid][object] */ 


EXTERN_C const IID IID_ISharedValueCallback;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("e7639719-258c-46a3-b349-c3c96ac4b46c")
    ISharedValueCallback : public IDispatch
    {
    public:
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE OnValueChanged( 
            /* [in] */ VARIANT newValue) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE OnDateTimeChanged( 
            /* [in] */ BSTR newDateTime) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct ISharedValueCallbackVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ISharedValueCallback * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ISharedValueCallback * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ISharedValueCallback * This);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfoCount)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            ISharedValueCallback * This,
            /* [out] */ UINT *pctinfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfo)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            ISharedValueCallback * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetIDsOfNames)
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            ISharedValueCallback * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        DECLSPEC_XFGVIRT(IDispatch, Invoke)
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            ISharedValueCallback * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        DECLSPEC_XFGVIRT(ISharedValueCallback, OnValueChanged)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *OnValueChanged )( 
            ISharedValueCallback * This,
            /* [in] */ VARIANT newValue);
        
        DECLSPEC_XFGVIRT(ISharedValueCallback, OnDateTimeChanged)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *OnDateTimeChanged )( 
            ISharedValueCallback * This,
            /* [in] */ BSTR newDateTime);
        
        END_INTERFACE
    } ISharedValueCallbackVtbl;

    interface ISharedValueCallback
    {
        CONST_VTBL struct ISharedValueCallbackVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISharedValueCallback_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define ISharedValueCallback_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define ISharedValueCallback_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define ISharedValueCallback_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define ISharedValueCallback_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define ISharedValueCallback_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define ISharedValueCallback_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define ISharedValueCallback_OnValueChanged(This,newValue)	\
    ( (This)->lpVtbl -> OnValueChanged(This,newValue) ) 

#define ISharedValueCallback_OnDateTimeChanged(This,newDateTime)	\
    ( (This)->lpVtbl -> OnDateTimeChanged(This,newDateTime) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __ISharedValueCallback_INTERFACE_DEFINED__ */


#ifndef __ISharedValue_INTERFACE_DEFINED__
#define __ISharedValue_INTERFACE_DEFINED__

/* interface ISharedValue */
/* [unique][nonextensible][dual][uuid][object] */ 


EXTERN_C const IID IID_ISharedValue;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("aced7d5d-0559-4e35-a972-42d9871ee14a")
    ISharedValue : public IDispatch
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE SetValue( 
            /* [in] */ VARIANT value) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetValue( 
            /* [retval][out] */ VARIANT *value) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetCurrentUTCDateTime( 
            /* [retval][out] */ BSTR *dateTime) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetCurrentUserLogin( 
            /* [retval][out] */ BSTR *login) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE LockSharedValue( 
            /* [in] */ LONG timeoutMs) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Unlock( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Subscribe( 
            /* [in] */ ISharedValueCallback *callback) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Unsubscribe( 
            /* [in] */ ISharedValueCallback *callback) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct ISharedValueVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ISharedValue * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ISharedValue * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ISharedValue * This);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfoCount)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            ISharedValue * This,
            /* [out] */ UINT *pctinfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfo)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            ISharedValue * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetIDsOfNames)
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            ISharedValue * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        DECLSPEC_XFGVIRT(IDispatch, Invoke)
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            ISharedValue * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        DECLSPEC_XFGVIRT(ISharedValue, SetValue)
        HRESULT ( STDMETHODCALLTYPE *SetValue )( 
            ISharedValue * This,
            /* [in] */ VARIANT value);
        
        DECLSPEC_XFGVIRT(ISharedValue, GetValue)
        HRESULT ( STDMETHODCALLTYPE *GetValue )( 
            ISharedValue * This,
            /* [retval][out] */ VARIANT *value);
        
        DECLSPEC_XFGVIRT(ISharedValue, GetCurrentUTCDateTime)
        HRESULT ( STDMETHODCALLTYPE *GetCurrentUTCDateTime )( 
            ISharedValue * This,
            /* [retval][out] */ BSTR *dateTime);
        
        DECLSPEC_XFGVIRT(ISharedValue, GetCurrentUserLogin)
        HRESULT ( STDMETHODCALLTYPE *GetCurrentUserLogin )( 
            ISharedValue * This,
            /* [retval][out] */ BSTR *login);
        
        DECLSPEC_XFGVIRT(ISharedValue, LockSharedValue)
        HRESULT ( STDMETHODCALLTYPE *LockSharedValue )( 
            ISharedValue * This,
            /* [in] */ LONG timeoutMs);
        
        DECLSPEC_XFGVIRT(ISharedValue, Unlock)
        HRESULT ( STDMETHODCALLTYPE *Unlock )( 
            ISharedValue * This);
        
        DECLSPEC_XFGVIRT(ISharedValue, Subscribe)
        HRESULT ( STDMETHODCALLTYPE *Subscribe )( 
            ISharedValue * This,
            /* [in] */ ISharedValueCallback *callback);
        
        DECLSPEC_XFGVIRT(ISharedValue, Unsubscribe)
        HRESULT ( STDMETHODCALLTYPE *Unsubscribe )( 
            ISharedValue * This,
            /* [in] */ ISharedValueCallback *callback);
        
        END_INTERFACE
    } ISharedValueVtbl;

    interface ISharedValue
    {
        CONST_VTBL struct ISharedValueVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISharedValue_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define ISharedValue_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define ISharedValue_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define ISharedValue_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define ISharedValue_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define ISharedValue_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define ISharedValue_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define ISharedValue_SetValue(This,value)	\
    ( (This)->lpVtbl -> SetValue(This,value) ) 

#define ISharedValue_GetValue(This,value)	\
    ( (This)->lpVtbl -> GetValue(This,value) ) 

#define ISharedValue_GetCurrentUTCDateTime(This,dateTime)	\
    ( (This)->lpVtbl -> GetCurrentUTCDateTime(This,dateTime) ) 

#define ISharedValue_GetCurrentUserLogin(This,login)	\
    ( (This)->lpVtbl -> GetCurrentUserLogin(This,login) ) 

#define ISharedValue_LockSharedValue(This,timeoutMs)	\
    ( (This)->lpVtbl -> LockSharedValue(This,timeoutMs) ) 

#define ISharedValue_Unlock(This)	\
    ( (This)->lpVtbl -> Unlock(This) ) 

#define ISharedValue_Subscribe(This,callback)	\
    ( (This)->lpVtbl -> Subscribe(This,callback) ) 

#define ISharedValue_Unsubscribe(This,callback)	\
    ( (This)->lpVtbl -> Unsubscribe(This,callback) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __ISharedValue_INTERFACE_DEFINED__ */



#ifndef __ATLProjectcomserverLib_LIBRARY_DEFINED__
#define __ATLProjectcomserverLib_LIBRARY_DEFINED__

/* library ATLProjectcomserverLib */
/* [version][uuid] */ 


EXTERN_C const IID LIBID_ATLProjectcomserverLib;

EXTERN_C const CLSID CLSID_MathOperations;

#ifdef __cplusplus

class DECLSPEC_UUID("b97b4532-1222-4229-a090-d625c1e9532b")
MathOperations;
#endif

EXTERN_C const CLSID CLSID_SharedValueCallback;

#ifdef __cplusplus

class DECLSPEC_UUID("6818dd57-f9e6-45be-aa20-ea4b5b658af3")
SharedValueCallback;
#endif

EXTERN_C const CLSID CLSID_SharedValue;

#ifdef __cplusplus

class DECLSPEC_UUID("71ef033e-e1b5-4985-bc7b-7687426bbdbb")
SharedValue;
#endif
#endif /* __ATLProjectcomserverLib_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

unsigned long             __RPC_USER  BSTR_UserSize(     unsigned long *, unsigned long            , BSTR * ); 
unsigned char * __RPC_USER  BSTR_UserMarshal(  unsigned long *, unsigned char *, BSTR * ); 
unsigned char * __RPC_USER  BSTR_UserUnmarshal(unsigned long *, unsigned char *, BSTR * ); 
void                      __RPC_USER  BSTR_UserFree(     unsigned long *, BSTR * ); 

unsigned long             __RPC_USER  VARIANT_UserSize(     unsigned long *, unsigned long            , VARIANT * ); 
unsigned char * __RPC_USER  VARIANT_UserMarshal(  unsigned long *, unsigned char *, VARIANT * ); 
unsigned char * __RPC_USER  VARIANT_UserUnmarshal(unsigned long *, unsigned char *, VARIANT * ); 
void                      __RPC_USER  VARIANT_UserFree(     unsigned long *, VARIANT * ); 

unsigned long             __RPC_USER  BSTR_UserSize64(     unsigned long *, unsigned long            , BSTR * ); 
unsigned char * __RPC_USER  BSTR_UserMarshal64(  unsigned long *, unsigned char *, BSTR * ); 
unsigned char * __RPC_USER  BSTR_UserUnmarshal64(unsigned long *, unsigned char *, BSTR * ); 
void                      __RPC_USER  BSTR_UserFree64(     unsigned long *, BSTR * ); 

unsigned long             __RPC_USER  VARIANT_UserSize64(     unsigned long *, unsigned long            , VARIANT * ); 
unsigned char * __RPC_USER  VARIANT_UserMarshal64(  unsigned long *, unsigned char *, VARIANT * ); 
unsigned char * __RPC_USER  VARIANT_UserUnmarshal64(unsigned long *, unsigned char *, VARIANT * ); 
void                      __RPC_USER  VARIANT_UserFree64(     unsigned long *, VARIANT * ); 

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


