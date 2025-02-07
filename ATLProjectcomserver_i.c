

/* this ALWAYS GENERATED file contains the IIDs and CLSIDs */

/* link this file in with the server and any clients */


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



#ifdef __cplusplus
extern "C"{
#endif 


#include <rpc.h>
#include <rpcndr.h>

#ifdef _MIDL_USE_GUIDDEF_

#ifndef INITGUID
#define INITGUID
#include <guiddef.h>
#undef INITGUID
#else
#include <guiddef.h>
#endif

#define MIDL_DEFINE_GUID(type,name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
        DEFINE_GUID(name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8)

#else // !_MIDL_USE_GUIDDEF_

#ifndef __IID_DEFINED__
#define __IID_DEFINED__

typedef struct _IID
{
    unsigned long x;
    unsigned short s1;
    unsigned short s2;
    unsigned char  c[8];
} IID;

#endif // __IID_DEFINED__

#ifndef CLSID_DEFINED
#define CLSID_DEFINED
typedef IID CLSID;
#endif // CLSID_DEFINED

#define MIDL_DEFINE_GUID(type,name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
        EXTERN_C __declspec(selectany) const type name = {l,w1,w2,{b1,b2,b3,b4,b5,b6,b7,b8}}

#endif // !_MIDL_USE_GUIDDEF_

MIDL_DEFINE_GUID(IID, IID_IMathOperations,0x488e9f3c,0x299b,0x4fe1,0x8b,0x25,0xa2,0xb9,0xc2,0xa2,0x35,0x06);


MIDL_DEFINE_GUID(IID, IID_ISharedValueCallback,0xe7639719,0x258c,0x46a3,0xb3,0x49,0xc3,0xc9,0x6a,0xc4,0xb4,0x6c);


MIDL_DEFINE_GUID(IID, IID_ISharedValue,0xaced7d5d,0x0559,0x4e35,0xa9,0x72,0x42,0xd9,0x87,0x1e,0xe1,0x4a);


MIDL_DEFINE_GUID(IID, LIBID_ATLProjectcomserverLib,0x86be8df9,0x669e,0x4443,0xb1,0xa0,0xbd,0x69,0xea,0x9e,0x40,0xcc);


MIDL_DEFINE_GUID(CLSID, CLSID_MathOperations,0xb97b4532,0x1222,0x4229,0xa0,0x90,0xd6,0x25,0xc1,0xe9,0x53,0x2b);


MIDL_DEFINE_GUID(CLSID, CLSID_SharedValueCallback,0x6818dd57,0xf9e6,0x45be,0xaa,0x20,0xea,0x4b,0x5b,0x65,0x8a,0xf3);


MIDL_DEFINE_GUID(CLSID, CLSID_SharedValue,0x71ef033e,0xe1b5,0x4985,0xbc,0x7b,0x76,0x87,0x42,0x6b,0xbd,0xbb);

#undef MIDL_DEFINE_GUID

#ifdef __cplusplus
}
#endif



