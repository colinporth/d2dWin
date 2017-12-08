//{{{  includes
#ifndef __PIPELINE_DECODE_H__
#define __PIPELINE_DECODE_H__

#include "sample_defs.h"

#if D3D_SURFACES_SUPPORT
#pragma warning(disable : 4201)
#include <d3d9.h>
#include <dxva2api.h>
#endif

#include <vector>
#include "hw_device.h"
#include "decode_render.h"
#include "mfx_buffering.h"
#include <memory>

#include "sample_utils.h"
#include "allocator.h"

#include "mfxjpeg.h"
#include "mfxplugin.h"
#include "mfxplugin++.h"
#include "mfxvideo.h"
#include "mfxvideo++.h"
#include "mfxvp8.h"
//}}}

enum MemType { SYSTEM_MEMORY = 0x00, D3D9_MEMORY   = 0x01, D3D11_MEMORY  = 0x02, };
enum eWorkMode { MODE_PERFORMANCE, MODE_RENDERING, MODE_FILE_DUMP };
enum eDecoderPostProc { MODE_DECODER_POSTPROC_AUTO  = 0x1, MODE_DECODER_POSTPROC_FORCE = 0x2 };
//{{{
struct sInputParams
{
    mfxU32 videoType;
    eWorkMode mode;
    MemType memType;
    bool    bUseHWLib; // true if application wants to use HW mfx library
    bool    bIsMVC; // true if Multi-View Codec is in use
    bool    bLowLat; // low latency mode
    bool    bCalLat; // latency calculation
    bool    bUseFullColorRange; //whether to use full color range
    mfxU16  nMaxFPS; //rendering limited by certain fps
    mfxU32  nWallCell;
    mfxU32  nWallW; //number of windows located in each row
    mfxU32  nWallH; //number of windows located in each column
    mfxU32  nWallMonitor; //monitor id, 0,1,.. etc
    bool    bWallNoTitle; //whether to show title for each window with fps value
    mfxU16  nDecoderPostProcessing;

    mfxU32  numViews; // number of views for Multi-View Codec
    mfxU32  nRotation; // rotation for Motion JPEG Codec
    mfxU16  nAsyncDepth; // asyncronous queue
    mfxU16  nTimeout; // timeout in seconds
    mfxU16  gpuCopy; // GPU Copy mode (three-state option)
    mfxU16  nThreadsNum;
    mfxI32  SchedulingType;
    mfxI32  Priority;

    mfxU16  scrWidth;
    mfxU16  scrHeight;

    mfxU16  Width;
    mfxU16  Height;

    mfxU32  fourcc;
    mfxU16  chromaType;
    mfxU32  nFrames;
    mfxU16  eDeinterlace;
    bool    outI420;

    bool    bPerfMode;
    bool    bRenderWin;
    mfxU32  nRenderWinX;
    mfxU32  nRenderWinY;

    mfxI32  monitorType;

    msdk_char     strSrcFile[MSDK_MAX_FILENAME_LEN];
    msdk_char     strDstFile[MSDK_MAX_FILENAME_LEN];

    sInputParams()
    {
        MSDK_ZERO_MEMORY(*this);
    }
  };
//}}}

//{{{
struct CPipelineStatistics
{
    CPipelineStatistics():
        m_input_count(0),
        m_output_count(0),
        m_synced_count(0),
        m_tick_overall(0),
        m_tick_fread(0),
        m_tick_fwrite(0),
        m_timer_overall(m_tick_overall)
    {
    }
    virtual ~CPipelineStatistics(){}

    mfxU32 m_input_count;     // number of received incoming packets (frames or bitstreams)
    mfxU32 m_output_count;    // number of delivered outgoing packets (frames or bitstreams)
    mfxU32 m_synced_count;

    msdk_tick m_tick_overall; // overall time passed during processing
    msdk_tick m_tick_fread;   // part of tick_overall: time spent to receive incoming data
    msdk_tick m_tick_fwrite;  // part of tick_overall: time spent to deliver outgoing data

    CAutoTimer m_timer_overall; // timer which corresponds to m_tick_overall

private:
    CPipelineStatistics(const CPipelineStatistics&);
    void operator=(const CPipelineStatistics&);
};
//}}}
//{{{
class CDecodingPipeline:
    public CBuffering,
    public CPipelineStatistics
{
public:
    CDecodingPipeline();
    virtual ~CDecodingPipeline();

    virtual mfxStatus Init(sInputParams *pParams);
    virtual mfxStatus RunDecoding();
    virtual void Close();
    virtual mfxStatus ResetDecoder(sInputParams *pParams);
    virtual mfxStatus ResetDevice();

    void SetMultiView();
    void SetExtBuffersFlag()       { m_bIsExtBuffers = true; }
    virtual void PrintInfo();

protected: // functions
    virtual mfxStatus CreateRenderingWindow(sInputParams *pParams);
    virtual mfxStatus InitMfxParams(sInputParams *pParams);

    // function for allocating a specific external buffer
    template <typename Buffer>
    mfxStatus AllocateExtBuffer();
    virtual void DeleteExtBuffers();

    virtual void AttachExtParam();

    virtual mfxStatus InitVppParams();
    virtual mfxStatus AllocAndInitVppFilters();
    virtual bool IsVppRequired(sInputParams *pParams);

    virtual mfxStatus CreateAllocator();
    virtual mfxStatus CreateHWDevice();
    virtual mfxStatus AllocFrames();
    virtual void DeleteFrames();
    virtual void DeleteAllocator();

    /** \brief Performs SyncOperation on the current output surface with the specified timeout.
     * @return MFX_ERR_NONE Output surface was successfully synced and delivered.
     * @return MFX_ERR_MORE_DATA Array of output surfaces is empty, need to feed decoder.
     * @return MFX_WRN_IN_EXECUTION Specified timeout have elapsed.
     * @return MFX_ERR_UNKNOWN An error has occurred.
     */
    virtual mfxStatus SyncOutputSurface(mfxU32 wait);
    virtual mfxStatus DeliverOutput(mfxFrameSurface1* frame);
    virtual void PrintPerFrameStat(bool force = false);

    virtual mfxStatus DeliverLoop(void);

    static unsigned int MFX_STDCALL DeliverThreadFunc(void* ctx);

protected: // variables
    CSmplYUVWriter          m_FileWriter;
    std::auto_ptr<CSmplBitstreamReader>  m_FileReader;
    mfxBitstream            m_mfxBS; // contains encoded data

    MFXVideoSession         m_mfxSession;
    mfxIMPL                 m_impl;
    MFXVideoDECODE*         m_pmfxDEC;
    MFXVideoVPP*            m_pmfxVPP;
    mfxVideoParam           m_mfxVideoParams;
    mfxVideoParam           m_mfxVppVideoParams;
    std::auto_ptr<MFXVideoUSER>  m_pUserModule;
    std::auto_ptr<MFXPlugin> m_pPlugin;
    std::vector<mfxExtBuffer *> m_ExtBuffers;
    mfxExtDecVideoProcessing m_DecoderPostProcessing;

    GeneralAllocator*       m_pGeneralAllocator;
    mfxAllocatorParams*     m_pmfxAllocatorParams;
    MemType                 m_memType;      // memory type of surfaces to use
    bool                    m_bExternalAlloc; // use memory allocator as external for Media SDK
    bool                    m_bDecOutSysmem; // use system memory between Decoder and VPP, if false - video memory
    mfxFrameAllocResponse   m_mfxResponse; // memory allocation response for decoder
    mfxFrameAllocResponse   m_mfxVppResponse;   // memory allocation response for vpp

    msdkFrameSurface*       m_pCurrentFreeSurface; // surface detached from free surfaces array
    msdkFrameSurface*       m_pCurrentFreeVppSurface; // VPP surface detached from free VPP surfaces array
    msdkOutputSurface*      m_pCurrentFreeOutputSurface; // surface detached from free output surfaces array
    msdkOutputSurface*      m_pCurrentOutputSurface; // surface detached from output surfaces array

    MSDKSemaphore*          m_pDeliverOutputSemaphore; // to access to DeliverOutput method
    MSDKEvent*              m_pDeliveredEvent; // to signal when output surfaces will be processed
    mfxStatus               m_error; // error returned by DeliverOutput method
    bool                    m_bStopDeliverLoop;

    eWorkMode               m_eWorkMode; // work mode for the pipeline
    bool                    m_bIsMVC; // enables MVC mode (need to support several files as an output)
    bool                    m_bIsExtBuffers; // indicates if external buffers were allocated
    bool                    m_bIsVideoWall; // indicates special mode: decoding will be done in a loop
    bool                    m_bIsCompleteFrame;
    mfxU32                  m_fourcc; // color format of vpp out, i420 by default
    bool                    m_bPrintLatency;
    bool                    m_bOutI420;

    mfxU16                  m_vppOutWidth;
    mfxU16                  m_vppOutHeight;

    mfxU32                  m_nTimeout; // enables timeout for video playback, measured in seconds
    mfxU16                  m_nMaxFps; // limit of fps, if isn't specified equal 0.
    mfxU32                  m_nFrames; //limit number of output frames

    mfxU16                  m_diMode;
    bool                    m_bVppIsUsed;
    bool                    m_bVppFullColorRange;
    std::vector<msdk_tick>  m_vLatency;

    msdk_tick               m_startTick;
    msdk_tick               m_delayTicks;

    mfxExtVPPDoNotUse       m_VppDoNotUse;      // for disabling VPP algorithms
    mfxExtVPPDeinterlacing  m_VppDeinterlacing;
    std::vector<mfxExtBuffer*> m_VppExtParams;

    mfxExtVPPVideoSignalInfo m_VppVideoSignalInfo;
    std::vector<mfxExtBuffer*> m_VppSurfaceExtParams;

    CHWDevice               *m_hwdev;
    CDecodeD3DRender         m_d3dRender;

    bool                    m_bRenderWin;
    mfxU32                  m_nRenderWinX;
    mfxU32                  m_nRenderWinY;
    mfxU32                  m_nRenderWinW;
    mfxU32                  m_nRenderWinH;

    mfxU32                  m_export_mode;
    mfxI32                  m_monitorType;

    bool                    m_bResetFileWriter;
    bool                    m_bResetFileReader;

private:
    CDecodingPipeline(const CDecodingPipeline&);
    void operator=(const CDecodingPipeline&);
};
//}}}

#endif // __PIPELINE_DECODE_H__
