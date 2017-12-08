//{{{  includes
#include <assert.h>
#include <algorithm>

#include "allocator.h"
#include "mfx_samples_config.h"
#include "d3d_allocator.h"
#include "d3d11_allocator.h"

#include "sample_defs.h"
//}}}
#define ID_BUFFER MFX_MAKEFOURCC('B','U','F','F')
#define ID_FRAME  MFX_MAKEFOURCC('F','R','M','E')
#pragma warning(disable : 4100)

//{{{
MFXFrameAllocator::MFXFrameAllocator()
{
    pthis = this;
    Alloc = Alloc_;
    Lock  = Lock_;
    Free  = Free_;
    Unlock = Unlock_;
    GetHDL = GetHDL_;
}
//}}}
MFXFrameAllocator::~MFXFrameAllocator() {}
//{{{
mfxStatus MFXFrameAllocator::Alloc_(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    if (0 == pthis)
        return MFX_ERR_MEMORY_ALLOC;

    MFXFrameAllocator& self = *(MFXFrameAllocator *)pthis;

    return self.AllocFrames(request, response);
}
//}}}
//{{{
mfxStatus MFXFrameAllocator::Lock_(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr)
{
    if (0 == pthis)
        return MFX_ERR_MEMORY_ALLOC;

    MFXFrameAllocator& self = *(MFXFrameAllocator *)pthis;

    return self.LockFrame(mid, ptr);
}
//}}}
//{{{
mfxStatus MFXFrameAllocator::Unlock_(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr)
{
    if (0 == pthis)
        return MFX_ERR_MEMORY_ALLOC;

    MFXFrameAllocator& self = *(MFXFrameAllocator *)pthis;

    return self.UnlockFrame(mid, ptr);
}
//}}}
//{{{
mfxStatus MFXFrameAllocator::Free_(mfxHDL pthis, mfxFrameAllocResponse *response)
{
    if (0 == pthis)
        return MFX_ERR_MEMORY_ALLOC;

    MFXFrameAllocator& self = *(MFXFrameAllocator *)pthis;

    return self.FreeFrames(response);
}
//}}}
//{{{
mfxStatus MFXFrameAllocator::GetHDL_(mfxHDL pthis, mfxMemId mid, mfxHDL *handle)
{
    if (0 == pthis)
        return MFX_ERR_MEMORY_ALLOC;

    MFXFrameAllocator& self = *(MFXFrameAllocator *)pthis;

    return self.GetFrameHDL(mid, handle);
}
//}}}

//{{{
BaseFrameAllocator::BaseFrameAllocator()
{
}
//}}}
//{{{
BaseFrameAllocator::~BaseFrameAllocator() {}
//}}}
//{{{
mfxStatus BaseFrameAllocator::CheckRequestType(mfxFrameAllocRequest *request)
{
    if (0 == request)
        return MFX_ERR_NULL_PTR;

    // check that Media SDK component is specified in request
    if ((request->Type & MEMTYPE_FROM_MASK) != 0)
        return MFX_ERR_NONE;
    else
        return MFX_ERR_UNSUPPORTED;
}
//}}}
//{{{
mfxStatus BaseFrameAllocator::AllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    if (0 == request || 0 == response || 0 == request->NumFrameSuggested)
        return MFX_ERR_MEMORY_ALLOC;

    if (MFX_ERR_NONE != CheckRequestType(request))
        return MFX_ERR_UNSUPPORTED;

    mfxStatus sts = MFX_ERR_NONE;

    if ( // External Frames
        ((request->Type & MFX_MEMTYPE_EXTERNAL_FRAME) &&
         (request->Type & (MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_FROM_ENC | MFX_MEMTYPE_FROM_PAK)))
         // Exception: Internal Frames for FEI ENC / PAK reconstructs
         ||
        ((request->Type & MFX_MEMTYPE_INTERNAL_FRAME) &&
         (request->Type & (MFX_MEMTYPE_FROM_ENC | MFX_MEMTYPE_FROM_PAK)))
       )
    {
        bool foundInCache = false;
        // external decoder allocations
        std::list<UniqueResponse>::iterator
            it = m_ExtResponses.begin(),
            et = m_ExtResponses.end();
        UniqueResponse checker(*response, request->Info.Width, request->Info.Height, request->Type);
        for (; it != et; ++it)
        {
            // same decoder and same size
            if (request->AllocId == it->AllocId && checker(*it))
            {
                // check if enough frames were allocated
                if (request->NumFrameSuggested > it->NumFrameActual)
                    return MFX_ERR_MEMORY_ALLOC;

                it->m_refCount++;
                // return existing response
                *response = (mfxFrameAllocResponse&)*it;
                foundInCache = true;
            }
        }

        if (!foundInCache)
        {
            sts = AllocImpl(request, response);
            if (sts == MFX_ERR_NONE)
            {
                response->AllocId = request->AllocId;
                m_ExtResponses.push_back(UniqueResponse(*response, request->Info.Width, request->Info.Height, UniqueResponse::CropMemoryTypeToStore(request->Type)));
            }
        }
    }
    else
    {
        // internal allocations

        // reserve space before allocation to avoid memory leak
        m_responses.push_back(mfxFrameAllocResponse());

        sts = AllocImpl(request, response);
        if (sts == MFX_ERR_NONE)
        {
            m_responses.back() = *response;
        }
        else
        {
            m_responses.pop_back();
        }
    }

    return sts;
}
//}}}
//{{{
mfxStatus BaseFrameAllocator::FreeFrames(mfxFrameAllocResponse *response)
{
    if (response == 0)
        return MFX_ERR_INVALID_HANDLE;

    mfxStatus sts = MFX_ERR_NONE;

    // check whether response is an external decoder response
    std::list<UniqueResponse>::iterator i =
        std::find_if( m_ExtResponses.begin(), m_ExtResponses.end(), std::bind1st(IsSame(), *response));

    if (i != m_ExtResponses.end())
    {
        if ((--i->m_refCount) == 0)
        {
            sts = ReleaseResponse(response);
            m_ExtResponses.erase(i);
        }
        return sts;
    }

    // if not found so far, then search in internal responses
    std::list<mfxFrameAllocResponse>::iterator i2 =
        std::find_if(m_responses.begin(), m_responses.end(), std::bind1st(IsSame(), *response));

    if (i2 != m_responses.end())
    {
        sts = ReleaseResponse(response);
        m_responses.erase(i2);
        return sts;
    }

    // not found anywhere, report an error
    return MFX_ERR_INVALID_HANDLE;
}
//}}}
//{{{
mfxStatus BaseFrameAllocator::Close()
{
    std::list<UniqueResponse> ::iterator i;
    for (i = m_ExtResponses.begin(); i!= m_ExtResponses.end(); i++)
    {
        ReleaseResponse(&*i);
    }
    m_ExtResponses.clear();

    std::list<mfxFrameAllocResponse> ::iterator i2;
    for (i2 = m_responses.begin(); i2!= m_responses.end(); i2++)
    {
        ReleaseResponse(&*i2);
    }

    return MFX_ERR_NONE;
}
//}}}

//{{{
MFXBufferAllocator::MFXBufferAllocator()
{
    pthis = this;
    Alloc = Alloc_;
    Lock  = Lock_;
    Free  = Free_;
    Unlock = Unlock_;
}
//}}}
MFXBufferAllocator::~MFXBufferAllocator() {}
//{{{
mfxStatus MFXBufferAllocator::Alloc_(mfxHDL pthis, mfxU32 nbytes, mfxU16 type, mfxMemId *mid)
{
    if (0 == pthis)
        return MFX_ERR_MEMORY_ALLOC;

    MFXBufferAllocator& self = *(MFXBufferAllocator *)pthis;

    return self.AllocBuffer(nbytes, type, mid);
}
//}}}
//{{{
mfxStatus MFXBufferAllocator::Lock_(mfxHDL pthis, mfxMemId mid, mfxU8 **ptr)
{
    if (0 == pthis)
        return MFX_ERR_MEMORY_ALLOC;

    MFXBufferAllocator& self = *(MFXBufferAllocator *)pthis;

    return self.LockBuffer(mid, ptr);
}
//}}}
//{{{
mfxStatus MFXBufferAllocator::Unlock_(mfxHDL pthis, mfxMemId mid)
{
    if (0 == pthis)
        return MFX_ERR_MEMORY_ALLOC;

    MFXBufferAllocator& self = *(MFXBufferAllocator *)pthis;

    return self.UnlockBuffer(mid);
}
//}}}
//{{{
mfxStatus MFXBufferAllocator::Free_(mfxHDL pthis, mfxMemId mid)
{
    if (0 == pthis)
        return MFX_ERR_MEMORY_ALLOC;

    MFXBufferAllocator& self = *(MFXBufferAllocator *)pthis;

    return self.FreeBuffer(mid);
}
//}}}

//{{{
GeneralAllocator::GeneralAllocator()
{
};
//}}}
GeneralAllocator::~GeneralAllocator() {};
//{{{
mfxStatus GeneralAllocator::Init (mfxAllocatorParams *pParams)
{
    mfxStatus sts = MFX_ERR_NONE;

    D3DAllocatorParams *d3dAllocParams = dynamic_cast<D3DAllocatorParams*>(pParams);
    if (d3dAllocParams)
      m_D3DAllocator.reset(new D3DFrameAllocator);

    D3D11AllocatorParams *d3d11AllocParams = dynamic_cast<D3D11AllocatorParams*>(pParams);
    if (d3d11AllocParams)
      m_D3DAllocator.reset(new D3D11FrameAllocator);

    if (m_D3DAllocator.get())
    {
        sts = m_D3DAllocator.get()->Init(pParams);
        MSDK_CHECK_STATUS(sts, "m_D3DAllocator.get failed");
    }

    m_SYSAllocator.reset(new SysMemFrameAllocator);
    sts = m_SYSAllocator.get()->Init(0);
    MSDK_CHECK_STATUS(sts, "m_SYSAllocator.get failed");

    return sts;
}
//}}}
//{{{
mfxStatus GeneralAllocator::Close()
{
    mfxStatus sts = MFX_ERR_NONE;
    if (m_D3DAllocator.get())
    {
        sts = m_D3DAllocator.get()->Close();
        MSDK_CHECK_STATUS(sts, "m_D3DAllocator.get failed");
    }

    sts = m_SYSAllocator.get()->Close();
    MSDK_CHECK_STATUS(sts, "m_SYSAllocator.get failed");

   return sts;
}
//}}}
//{{{
mfxStatus GeneralAllocator::LockFrame (mfxMemId mid, mfxFrameData *ptr)
{
    if (isD3DMid(mid) && m_D3DAllocator.get())
        return m_D3DAllocator.get()->Lock(m_D3DAllocator.get(), mid, ptr);
    else
        return m_SYSAllocator.get()->Lock(m_SYSAllocator.get(),mid, ptr);
}
//}}}
//{{{
mfxStatus GeneralAllocator::UnlockFrame (mfxMemId mid, mfxFrameData *ptr)
{
    if (isD3DMid(mid) && m_D3DAllocator.get())
        return m_D3DAllocator.get()->Unlock(m_D3DAllocator.get(), mid, ptr);
    else
        return m_SYSAllocator.get()->Unlock(m_SYSAllocator.get(),mid, ptr);
}
//}}}
//{{{
mfxStatus GeneralAllocator::GetFrameHDL (mfxMemId mid, mfxHDL *handle)
{
    if (isD3DMid(mid) && m_D3DAllocator.get())
        return m_D3DAllocator.get()->GetHDL(m_D3DAllocator.get(), mid, handle);
    else
        return m_SYSAllocator.get()->GetHDL(m_SYSAllocator.get(), mid, handle);
}
//}}}
//{{{
mfxStatus GeneralAllocator::ReleaseResponse (mfxFrameAllocResponse *response)
{
    // try to ReleaseResponse via D3D allocator
    if (isD3DMid(response->mids[0]) && m_D3DAllocator.get())
        return m_D3DAllocator.get()->Free(m_D3DAllocator.get(),response);
    else
        return m_SYSAllocator.get()->Free(m_SYSAllocator.get(), response);
}
//}}}
//{{{
mfxStatus GeneralAllocator::AllocImpl (mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    mfxStatus sts;
    if ((request->Type & MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET || request->Type & MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET) && m_D3DAllocator.get())
    {
        sts = m_D3DAllocator.get()->Alloc(m_D3DAllocator.get(), request, response);
        MSDK_CHECK_NOT_EQUAL(MFX_ERR_NONE, sts, sts);
        StoreFrameMids(true, response);
    }
    else
    {
        sts = m_SYSAllocator.get()->Alloc(m_SYSAllocator.get(), request, response);
        MSDK_CHECK_NOT_EQUAL(MFX_ERR_NONE, sts, sts);
        StoreFrameMids(false, response);
    }
    return sts;
}
//}}}
//{{{
bool GeneralAllocator::isD3DMid (mfxHDL mid)
{
    std::map<mfxHDL, bool>::iterator it;
    it = m_Mids.find(mid);
    if (it == m_Mids.end())
        return false; // sys mem allocator will check validity of mid further
    else
        return it->second;
}
//}}}
//{{{
void GeneralAllocator::StoreFrameMids (bool isD3DFrames, mfxFrameAllocResponse *response)
{
    for (mfxU32 i = 0; i < response->NumFrameActual; i++)
        m_Mids.insert(std::pair<mfxHDL, bool>(response->mids[i], isD3DFrames));
}
//}}}

//{{{
SysMemFrameAllocator::SysMemFrameAllocator()
: m_pBufferAllocator(0), m_bOwnBufferAllocator(false)
{
}
//}}}
//{{{
SysMemFrameAllocator::~SysMemFrameAllocator() {
  Close();
  }
//}}}
//{{{
mfxStatus SysMemFrameAllocator::Init(mfxAllocatorParams *pParams)
{
    // check if any params passed from application
    if (pParams)
    {
        SysMemAllocatorParams *pSysMemParams = 0;
        pSysMemParams = dynamic_cast<SysMemAllocatorParams *>(pParams);
        if (!pSysMemParams)
            return MFX_ERR_NOT_INITIALIZED;

        m_pBufferAllocator = pSysMemParams->pBufferAllocator;
        m_bOwnBufferAllocator = false;
    }

    // if buffer allocator wasn't passed from application create own
    if (!m_pBufferAllocator)
    {
        m_pBufferAllocator = new SysMemBufferAllocator;
        if (!m_pBufferAllocator)
            return MFX_ERR_MEMORY_ALLOC;

        m_bOwnBufferAllocator = true;
    }

    return MFX_ERR_NONE;
}
//}}}
//{{{
mfxStatus SysMemFrameAllocator::Close()
{
    mfxStatus sts = BaseFrameAllocator::Close();

    if (m_bOwnBufferAllocator)
    {
        delete m_pBufferAllocator;
        m_pBufferAllocator = 0;
    }
    return sts;
}
//}}}
//{{{
mfxStatus SysMemFrameAllocator::LockFrame(mfxMemId mid, mfxFrameData *ptr)
{
    if (!m_pBufferAllocator)
        return MFX_ERR_NOT_INITIALIZED;

    if (!ptr)
        return MFX_ERR_NULL_PTR;

    // If allocator uses pointers instead of mids, no further action is required
    if (!mid && ptr->Y)
        return MFX_ERR_NONE;

    sFrame *fs = 0;
    mfxStatus sts = m_pBufferAllocator->Lock(m_pBufferAllocator->pthis, mid,(mfxU8 **)&fs);

    if (MFX_ERR_NONE != sts)
        return sts;

    if (ID_FRAME != fs->id)
    {
        m_pBufferAllocator->Unlock(m_pBufferAllocator->pthis, mid);
        return MFX_ERR_INVALID_HANDLE;
    }

    mfxU16 Width2 = (mfxU16)MSDK_ALIGN32(fs->info.Width);
    mfxU16 Height2 = (mfxU16)MSDK_ALIGN32(fs->info.Height);
    ptr->B = ptr->Y = (mfxU8 *)fs + MSDK_ALIGN32(sizeof(sFrame));

    switch (fs->info.FourCC)
    {
    case MFX_FOURCC_NV12:
        ptr->U = ptr->Y + Width2 * Height2;
        ptr->V = ptr->U + 1;
        ptr->Pitch = Width2;
        break;
    case MFX_FOURCC_NV16:
        ptr->U = ptr->Y + Width2 * Height2;
        ptr->V = ptr->U + 1;
        ptr->Pitch = Width2;
        break;
    case MFX_FOURCC_YV12:
        ptr->V = ptr->Y + Width2 * Height2;
        ptr->U = ptr->V + (Width2 >> 1) * (Height2 >> 1);
        ptr->Pitch = Width2;
        break;
    case MFX_FOURCC_UYVY:
        ptr->U = ptr->Y;
        ptr->Y = ptr->U + 1;
        ptr->V = ptr->U + 2;
        ptr->Pitch = 2 * Width2;
        break;
    case MFX_FOURCC_YUY2:
        ptr->U = ptr->Y + 1;
        ptr->V = ptr->Y + 3;
        ptr->Pitch = 2 * Width2;
        break;
    case MFX_FOURCC_RGB3:
        ptr->G = ptr->B + 1;
        ptr->R = ptr->B + 2;
        ptr->Pitch = 3 * Width2;
        break;
    case MFX_FOURCC_RGB4:
    case MFX_FOURCC_A2RGB10:
        ptr->G = ptr->B + 1;
        ptr->R = ptr->B + 2;
        ptr->A = ptr->B + 3;
        ptr->Pitch = 4 * Width2;
        break;
     case MFX_FOURCC_R16:
        ptr->Y16 = (mfxU16 *)ptr->B;
        ptr->Pitch = 2 * Width2;
        break;
    case MFX_FOURCC_P010:
        ptr->U = ptr->Y + Width2 * Height2 * 2;
        ptr->V = ptr->U + 2;
        ptr->Pitch = Width2 * 2;
        break;
    case MFX_FOURCC_P210:
        ptr->U = ptr->Y + Width2 * Height2 * 2;
        ptr->V = ptr->U + 2;
        ptr->Pitch = Width2 * 2;
        break;
    case MFX_FOURCC_AYUV:
        ptr->V = ptr->B;
        ptr->U = ptr->V + 1;
        ptr->Y = ptr->V + 2;
        ptr->A = ptr->V + 3;
        ptr->Pitch = 4 * Width2;
        break;
    default:
        return MFX_ERR_UNSUPPORTED;
    }

    return MFX_ERR_NONE;
}
//}}}
//{{{
mfxStatus SysMemFrameAllocator::UnlockFrame(mfxMemId mid, mfxFrameData *ptr)
{
    if (!m_pBufferAllocator)
        return MFX_ERR_NOT_INITIALIZED;

    // If allocator uses pointers instead of mids, no further action is required
    if (!mid && ptr->Y)
        return MFX_ERR_NONE;

    mfxStatus sts = m_pBufferAllocator->Unlock(m_pBufferAllocator->pthis, mid);

    if (MFX_ERR_NONE != sts)
        return sts;

    if (NULL != ptr)
    {
        ptr->Pitch = 0;
        ptr->Y     = 0;
        ptr->U     = 0;
        ptr->V     = 0;
        ptr->A     = 0;
    }

    return MFX_ERR_NONE;
}
//}}}
//{{{
mfxStatus SysMemFrameAllocator::GetFrameHDL(mfxMemId mid, mfxHDL *handle)
{
    return MFX_ERR_UNSUPPORTED;
}
//}}}
//{{{
mfxStatus SysMemFrameAllocator::CheckRequestType(mfxFrameAllocRequest *request)
{
    mfxStatus sts = BaseFrameAllocator::CheckRequestType(request);
    if (MFX_ERR_NONE != sts)
        return sts;

    if ((request->Type & MFX_MEMTYPE_SYSTEM_MEMORY) != 0)
        return MFX_ERR_NONE;
    else
        return MFX_ERR_UNSUPPORTED;
}
//}}}
//{{{
mfxStatus SysMemFrameAllocator::AllocImpl(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    if (!m_pBufferAllocator)
        return MFX_ERR_NOT_INITIALIZED;

    mfxU32 numAllocated = 0;

    mfxU32 Width2 = MSDK_ALIGN32(request->Info.Width);
    mfxU32 Height2 = MSDK_ALIGN32(request->Info.Height);
    mfxU32 nbytes;

    switch (request->Info.FourCC)
    {
    case MFX_FOURCC_YV12:
    case MFX_FOURCC_NV12:
        nbytes = Width2*Height2 + (Width2>>1)*(Height2>>1) + (Width2>>1)*(Height2>>1);
        break;
    case MFX_FOURCC_NV16:
        nbytes = Width2*Height2 + (Width2>>1)*(Height2) + (Width2>>1)*(Height2);
        break;
    case MFX_FOURCC_RGB3:
        nbytes = Width2*Height2 + Width2*Height2 + Width2*Height2;
        break;
    case MFX_FOURCC_RGB4:
    case MFX_FOURCC_AYUV:
        nbytes = Width2*Height2 + Width2*Height2 + Width2*Height2 + Width2*Height2;
        break;
    case MFX_FOURCC_UYVY:
    case MFX_FOURCC_YUY2:
        nbytes = Width2*Height2 + (Width2>>1)*(Height2) + (Width2>>1)*(Height2);
        break;
    case MFX_FOURCC_R16:
        nbytes = 2*Width2*Height2;
        break;
    case MFX_FOURCC_P010:
        nbytes = Width2*Height2 + (Width2>>1)*(Height2>>1) + (Width2>>1)*(Height2>>1);
        nbytes *= 2;
        break;
    case MFX_FOURCC_A2RGB10:
        nbytes = Width2*Height2*4; // 4 bytes per pixel
        break;
    case MFX_FOURCC_P210:
        nbytes = Width2*Height2 + (Width2>>1)*(Height2) + (Width2>>1)*(Height2);
        nbytes *= 2; // 16bits
        break;
    default:
        return MFX_ERR_UNSUPPORTED;
    }

    safe_array<mfxMemId> mids(new mfxMemId[request->NumFrameSuggested]);
    if (!mids.get())
        return MFX_ERR_MEMORY_ALLOC;

    // allocate frames
    for (numAllocated = 0; numAllocated < request->NumFrameSuggested; numAllocated ++)
    {
        mfxStatus sts = m_pBufferAllocator->Alloc(m_pBufferAllocator->pthis,
            nbytes + MSDK_ALIGN32(sizeof(sFrame)), request->Type, &(mids.get()[numAllocated]));

        if (MFX_ERR_NONE != sts)
            break;

        sFrame *fs;
        sts = m_pBufferAllocator->Lock(m_pBufferAllocator->pthis, mids.get()[numAllocated], (mfxU8 **)&fs);

        if (MFX_ERR_NONE != sts)
            break;

        fs->id = ID_FRAME;
        fs->info = request->Info;
        m_pBufferAllocator->Unlock(m_pBufferAllocator->pthis, mids.get()[numAllocated]);
    }

    // check the number of allocated frames
    if (numAllocated < request->NumFrameSuggested)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    response->NumFrameActual = (mfxU16) numAllocated;
    response->mids = mids.release();

    return MFX_ERR_NONE;
}
//}}}
//{{{
mfxStatus SysMemFrameAllocator::ReleaseResponse(mfxFrameAllocResponse *response)
{
    if (!response)
        return MFX_ERR_NULL_PTR;

    if (!m_pBufferAllocator)
        return MFX_ERR_NOT_INITIALIZED;

    mfxStatus sts = MFX_ERR_NONE;

    if (response->mids)
    {
        for (mfxU32 i = 0; i < response->NumFrameActual; i++)
        {
            if (response->mids[i])
            {
                sts = m_pBufferAllocator->Free(m_pBufferAllocator->pthis, response->mids[i]);
                if (MFX_ERR_NONE != sts)
                    return sts;
            }
        }
    }

    delete [] response->mids;
    response->mids = 0;

    return sts;
}
//}}}

//{{{
SysMemBufferAllocator::SysMemBufferAllocator()
{

}
//}}}
SysMemBufferAllocator::~SysMemBufferAllocator() {}
//{{{
mfxStatus SysMemBufferAllocator::AllocBuffer(mfxU32 nbytes, mfxU16 type, mfxMemId *mid)
{
    if (!mid)
        return MFX_ERR_NULL_PTR;

    if (0 == (type & MFX_MEMTYPE_SYSTEM_MEMORY))
        return MFX_ERR_UNSUPPORTED;

    mfxU32 header_size = MSDK_ALIGN32(sizeof(sBuffer));
    mfxU8 *buffer_ptr = (mfxU8 *)calloc(header_size + nbytes + 32, 1);

    if (!buffer_ptr)
        return MFX_ERR_MEMORY_ALLOC;

    sBuffer *bs = (sBuffer *)buffer_ptr;
    bs->id = ID_BUFFER;
    bs->type = type;
    bs->nbytes = nbytes;
    *mid = (mfxHDL) bs;
    return MFX_ERR_NONE;
}
//}}}
//{{{
mfxStatus SysMemBufferAllocator::LockBuffer(mfxMemId mid, mfxU8 **ptr)
{
    if (!ptr)
        return MFX_ERR_NULL_PTR;

    sBuffer *bs = (sBuffer *)mid;

    if (!bs)
        return MFX_ERR_INVALID_HANDLE;
    if (ID_BUFFER != bs->id)
        return MFX_ERR_INVALID_HANDLE;

    *ptr = (mfxU8*)((size_t)((mfxU8 *)bs+MSDK_ALIGN32(sizeof(sBuffer))+31)&(~((size_t)31)));
    return MFX_ERR_NONE;
}
//}}}
//{{{
mfxStatus SysMemBufferAllocator::UnlockBuffer(mfxMemId mid)
{
    sBuffer *bs = (sBuffer *)mid;

    if (!bs || ID_BUFFER != bs->id)
        return MFX_ERR_INVALID_HANDLE;

    return MFX_ERR_NONE;
}
//}}}
//{{{
mfxStatus SysMemBufferAllocator::FreeBuffer(mfxMemId mid)
{
    sBuffer *bs = (sBuffer *)mid;
    if (!bs || ID_BUFFER != bs->id)
        return MFX_ERR_INVALID_HANDLE;

    free(bs);
    return MFX_ERR_NONE;
}
//}}}
