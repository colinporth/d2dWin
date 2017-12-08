#pragma once
//{{{  includes
#include <memory.h>
#include <iostream>
#include "mfxdefs.h"
#include "strings_defs.h"
#include "file_defs.h"
#include "time_defs.h"
//}}}

#define _MSDK_API (MFX_VERSION_MAJOR*256+MFX_VERSION_MINOR)
#define MSDK_API(M,MM) (M*256+MM)

enum {
  MFX_HANDLE_GFXS3DCONTROL = 0x100, /* A handle to the IGFXS3DControl instance */
  MFX_HANDLE_DEVICEWINDOW  = 0x101  /* A handle to the render window */
  }; //mfxHandleType

#define D3D_SURFACES_SUPPORT 1
#define MFX_D3D11_SUPPORT 0

enum {
#define __DECLARE(type) MFX_MONITOR_ ## type
  __DECLARE(Unknown) = 0,
  __DECLARE(AUTO) = __DECLARE(Unknown),
  __DECLARE(VGA),
  __DECLARE(DVII),
  __DECLARE(DVID),
  __DECLARE(DVIA),
  __DECLARE(Composite),
  __DECLARE(SVIDEO),
  __DECLARE(LVDS),
  __DECLARE(Component),
  __DECLARE(9PinDIN),
  __DECLARE(HDMIA),
  __DECLARE(HDMIB),
  __DECLARE(eDP),
  __DECLARE(TV),
  __DECLARE(DisplayPort),
#if defined(DRM_MODE_CONNECTOR_VIRTUAL) // from libdrm 2.4.59
  __DECLARE(VIRTUAL),
#endif
#if defined(DRM_MODE_CONNECTOR_DSI) // from libdrm 2.4.59
  __DECLARE(DSI),
#endif
  __DECLARE(MAXNUMBER)
#undef __DECLARE
};

// affects win32 winnt version macro
#include "time_defs.h"
#include "sample_utils.h"

#define MSDK_DEC_WAIT_INTERVAL 300000
#define MSDK_ENC_WAIT_INTERVAL 300000
#define MSDK_VPP_WAIT_INTERVAL 300000
#define MSDK_SURFACE_WAIT_INTERVAL 20000
#define MSDK_DEVICE_FREE_WAIT_INTERVAL 30000
#define MSDK_WAIT_INTERVAL MSDK_DEC_WAIT_INTERVAL+3*MSDK_VPP_WAIT_INTERVAL+MSDK_ENC_WAIT_INTERVAL // an estimate for the longest pipeline we have in samples

#define MSDK_INVALID_SURF_IDX 0xFFFF

#define MSDK_MAX_FILENAME_LEN 1024

#define MSDK_PRINT_RET_MSG(ERR,MSG) {msdk_stringstream tmpStr1;tmpStr1<<std::endl<<"[ERROR], sts=" \
    <<StatusToString(ERR)<<"("<<ERR<<")"<<", "<<__FUNCTION__<<", "<<MSG<<" at "<<__FILE__<<":"<<__LINE__<<std::endl;msdk_err<<tmpStr1.str();}

#define MSDK_TRACE_LEVEL(level, ERR) if (level <= msdk_trace_get_level()) {msdk_err<<NoFullPath(MSDK_STRING(__FILE__)) << MSDK_STRING(" :")<< __LINE__ <<MSDK_STRING(" [") \
    <<level<<MSDK_STRING("] ") << ERR << std::endl;}

#define MSDK_TRACE_CRITICAL(ERR) MSDK_TRACE_LEVEL(MSDK_TRACE_LEVEL_CRITICAL, ERR)
#define MSDK_TRACE_ERROR(ERR) MSDK_TRACE_LEVEL(MSDK_TRACE_LEVEL_ERROR, ERR)
#define MSDK_TRACE_WARNING(ERR) MSDK_TRACE_LEVEL(MSDK_TRACE_LEVEL_WARNING, ERR)
#define MSDK_TRACE_INFO(ERR) MSDK_TRACE_LEVEL(MSDK_TRACE_LEVEL_INFO, ERR)
#define MSDK_TRACE_DEBUG(ERR) MSDK_TRACE_LEVEL(MSDK_TRACE_LEVEL_DEBUG, ERR)

#define MSDK_CHECK_ERROR(P, X, ERR)              {if ((X) == (P)) {msdk_stringstream tmpStr2;tmpStr2<<MSDK_STRING(#X)<<MSDK_STRING("==")<<MSDK_STRING(#P)<<MSDK_STRING(" error"); \
    MSDK_PRINT_RET_MSG(ERR, tmpStr2.str().c_str()); return ERR;}}

#define MSDK_CHECK_NOT_EQUAL(P, X, ERR)          {if ((X) != (P)) {msdk_stringstream tmpStr3;tmpStr3<<MSDK_STRING(#X)<<MSDK_STRING("!=")<<MSDK_STRING(#P)<<MSDK_STRING(" error"); \
    MSDK_PRINT_RET_MSG(ERR, tmpStr3.str().c_str()); return ERR;}}

#define MSDK_CHECK_STATUS(X, MSG)               {if ((X) < MFX_ERR_NONE) {MSDK_PRINT_RET_MSG(X, MSG); return X;}}
#define MSDK_CHECK_PARSE_RESULT(P, X, ERR)       {if ((X) > (P)) {return ERR;}}

#define MSDK_CHECK_STATUS_SAFE(X, FUNC, ADD)     {if ((X) < MFX_ERR_NONE) {ADD; MSDK_PRINT_RET_MSG(X, FUNC); return X;}}
#define MSDK_IGNORE_MFX_STS(P, X)                {if ((X) == (P)) {P = MFX_ERR_NONE;}}
#define MSDK_CHECK_POINTER(P, ...)               {if (!(P)) {msdk_stringstream tmpStr4;tmpStr4<<MSDK_STRING(#P)<<MSDK_STRING(" pointer is NULL");MSDK_PRINT_RET_MSG(MFX_ERR_NULL_PTR, tmpStr4.str().c_str());return __VA_ARGS__;}}
#define MSDK_CHECK_POINTER_NO_RET(P)             {if (!(P)) {msdk_stringstream tmpStr4;tmpStr4<<MSDK_STRING(#P)<<MSDK_STRING(" pointer is NULL");MSDK_PRINT_RET_MSG(MFX_ERR_NULL_PTR, tmpStr4.str().c_str());return;}}
#define MSDK_CHECK_POINTER_SAFE(P, ERR, ADD)     {if (!(P)) {ADD; return ERR;}}
#define MSDK_BREAK_ON_ERROR(P)                   {if (MFX_ERR_NONE != (P)) break;}
#define MSDK_SAFE_DELETE_ARRAY(P)                {if (P) {delete[] P; P = NULL;}}
#define MSDK_SAFE_RELEASE(X)                     {if (X) { X->Release(); X = NULL; }}
#define MSDK_SAFE_FREE(X)                        {if (X) { free(X); X = NULL; }}

#ifndef MSDK_SAFE_DELETE
  #define MSDK_SAFE_DELETE(P)                      {if (P) {delete P; P = NULL;}}
#endif // MSDK_SAFE_DELETE

#define MSDK_ZERO_MEMORY(VAR)                    {memset(&VAR, 0, sizeof(VAR));}
#define MSDK_MAX(A, B)                           (((A) > (B)) ? (A) : (B))
#define MSDK_MIN(A, B)                           (((A) < (B)) ? (A) : (B))
#define MSDK_ALIGN16(value)                      (((value + 15) >> 4) << 4) // round up to a multiple of 16
#define MSDK_ALIGN32(value)                      (((value + 31) >> 5) << 5) // round up to a multiple of 32
#define MSDK_ALIGN(value, alignment)             (alignment) * ( (value) / (alignment) + (((value) % (alignment)) ? 1 : 0))
#define MSDK_ARRAY_LEN(value)                    (sizeof(value) / sizeof(value[0]))

#ifndef UNREFERENCED_PARAMETER
  #define UNREFERENCED_PARAMETER(par) (par)
#endif

#define MFX_IMPL_VIA_MASK(x) (0x0f00 & (x))

// Deprecated
#define MSDK_PRINT_RET_MSG_(ERR) {msdk_printf(MSDK_STRING("\nReturn on error: error code %d,\t%s\t%d\n\n"), (int)ERR, MSDK_STRING(__FILE__), __LINE__);}
#define MSDK_CHECK_RESULT(P, X, ERR)             {if ((X) > (P)) {MSDK_PRINT_RET_MSG_(ERR); return ERR;}}
#define MSDK_CHECK_RESULT_SAFE(P, X, ERR, ADD)   {if ((X) > (P)) {ADD; MSDK_PRINT_RET_MSG_(ERR); return ERR;}}
