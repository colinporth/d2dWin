//{{{
/*****************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or
nondisclosure agreement with Intel Corporation and may not be copied
or disclosed except in accordance with the terms of that agreement.
Copyright(c) 2005-2014 Intel Corporation. All Rights Reserved.

*****************************************************************************/
//}}}
#include "common_utils.h"

#include "common_directx11.h"

//{{{
mfxStatus Initialize(mfxIMPL impl, mfxVersion ver, MFXVideoSession* pSession, mfxFrameAllocator* pmfxAllocator, bool bCreateSharedHandles)
{
    mfxStatus sts = MFX_ERR_NONE;

    impl |= MFX_IMPL_VIA_D3D11;

    // Initialize Intel Media SDK Session
    sts = pSession->Init (impl, &ver);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // If mfxFrameAllocator is provided it means we need to setup DirectX device and memory allocator
    if (pmfxAllocator) {
        // Create DirectX device context
        mfxHDL deviceHandle;
        sts = CreateHWDevice(*pSession, &deviceHandle, NULL, bCreateSharedHandles);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        // Provide device manager to Media SDK
        sts = pSession->SetHandle (DEVICE_MGR_TYPE, deviceHandle);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        pmfxAllocator->pthis  = *pSession; // We use Media SDK session ID as the allocation identifier
        pmfxAllocator->Alloc  = simple_alloc;
        pmfxAllocator->Free   = simple_free;
        pmfxAllocator->Lock   = simple_lock;
        pmfxAllocator->Unlock = simple_unlock;
        pmfxAllocator->GetHDL = simple_gethdl;

        // Since we are using video memory we must provide Media SDK with an external allocator
        sts = pSession->SetFrameAllocator(pmfxAllocator);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    return sts;
}
//}}}
//{{{
void Release()
{
    CleanupHWDevice();
}
//}}}

//{{{
void mfxGetTime(mfxTime* timestamp)
{
    QueryPerformanceCounter(timestamp);
}
//}}}
//{{{
double TimeDiffMsec(mfxTime tfinish, mfxTime tstart)
{
    static LARGE_INTEGER tFreq = { 0 };

    if (!tFreq.QuadPart) QueryPerformanceFrequency(&tFreq);

    double freq = (double)tFreq.QuadPart;
    return 1000.0 * ((double)tfinish.QuadPart - (double)tstart.QuadPart) / freq;
}
//}}}

//{{{
void ClearYUVSurfaceVMem(mfxMemId memId)
{
    ClearYUVSurfaceD3D(memId);
}
//}}}
//{{{
void ClearRGBSurfaceVMem(mfxMemId memId)
{
    ClearRGBSurfaceD3D(memId);
}
//}}}
