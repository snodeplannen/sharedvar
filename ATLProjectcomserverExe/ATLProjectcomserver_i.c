

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


MIDL_DEFINE_GUID(IID, IID_IEventCallback,0xDEC06BDB,0x7655,0x4E71,0x99,0x37,0x11,0x0B,0x78,0xFC,0xC5,0x76);


MIDL_DEFINE_GUID(IID, IID_ISharedValue,0x8D55631F,0x1994,0x4F36,0xA3,0xA3,0xD5,0xB4,0x0E,0xB0,0xE0,0xDB);


MIDL_DEFINE_GUID(IID, IID_IDatasetProxy,0x50D4D0DB,0x6D9E,0x455F,0x8D,0x6C,0x74,0x9C,0xDB,0xCF,0x19,0x49);


MIDL_DEFINE_GUID(IID, LIBID_ATLProjectcomserverLib,0xB0A0188F,0x59B6,0x42A5,0xAD,0x3A,0x9D,0x3C,0xBE,0x07,0x92,0x53);


MIDL_DEFINE_GUID(CLSID, CLSID_MathOperations,0x1CE8C512,0xFB0A,0x4C47,0xB3,0xCD,0x44,0x21,0x9B,0xDC,0x8D,0xDF);


MIDL_DEFINE_GUID(CLSID, CLSID_SharedValueCallback,0x6818dd57,0xf9e6,0x45be,0xaa,0x20,0xea,0x4b,0x5b,0x65,0x8a,0xf3);


MIDL_DEFINE_GUID(CLSID, CLSID_SharedValue,0xA5B21149,0xFB8C,0x4E50,0x8C,0x52,0x65,0xF3,0xDC,0x4A,0xFE,0xBF);


MIDL_DEFINE_GUID(CLSID, CLSID_DatasetProxy,0x1D85075B,0x6ECB,0x4BE4,0x83,0x17,0x9D,0xDA,0x91,0x29,0x2E,0xD8);

#undef MIDL_DEFINE_GUID

#ifdef __cplusplus
}
#endif



