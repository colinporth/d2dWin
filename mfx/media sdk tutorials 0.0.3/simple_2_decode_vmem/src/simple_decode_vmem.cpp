#define _CRT_SECURE_NO_WARNINGS
#include "common_utils.h"

int main (int argc, char** argv) {

  // Intel Media SDK decode pipeline setup
  // - In this example we are decoding an AVC (H.264) stream
  // - Video memory surfaces are used to store the decoded frames
  //   (Note that when using HW acceleration video surfaces are prefered, for better performance)
  MFXVideoSession session;
  mfxFrameAllocator mfxAllocator;
  mfxStatus sts = Initialize (MFX_IMPL_AUTO, {0, 1}, &session, &mfxAllocator);

  MFXVideoDECODE mfxDEC (session);

  mfxVideoParam mfxVideoParams;
  memset (&mfxVideoParams, 0, sizeof(mfxVideoParams));
  mfxVideoParams.mfx.CodecId = MFX_CODEC_MPEG2;
  mfxVideoParams.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;

  // Prepare Media SDK bit stream buffer
  mfxBitstream mfxBS;
  memset (&mfxBS, 0, sizeof(mfxBS));
  mfxBS.MaxLength = 1024 * 1024;
  mfxBS.Data = new mfxU8 [mfxBS.MaxLength];
  MSDK_CHECK_POINTER (mfxBS.Data, MFX_ERR_MEMORY_ALLOC);

  // Parse bit stream, searching for header and fill video parameters structure
  FILE* fSource = fopen (argv[1], "rb");
  sts = ReadBitStreamData (&mfxBS, fSource);
  MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

  sts = mfxDEC.DecodeHeader (&mfxBS, &mfxVideoParams);
  MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
  MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
  // sts = mfxDEC.Query (&mfxVideoParams, &mfxVideoParams);

  // Query number of required surfaces for decoder
  mfxFrameAllocRequest Request;
  memset (&Request, 0, sizeof(Request));
  sts = mfxDEC.QueryIOSurf (&mfxVideoParams, &Request);
  MSDK_IGNORE_MFX_STS (sts, MFX_WRN_PARTIAL_ACCELERATION);
  MSDK_CHECK_RESULT (sts, MFX_ERR_NONE, sts);
  mfxU16 numSurfaces = Request.NumFrameSuggested;
  Request.Type |= WILL_READ;

  // Allocate surfaces for decoder
  mfxFrameAllocResponse mfxResponse;
  sts = mfxAllocator.Alloc (mfxAllocator.pthis, &Request, &mfxResponse);
  MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

  // Allocate surface headers (mfxFrameSurface1) for decoder
  mfxFrameSurface1** pmfxSurfaces = new mfxFrameSurface1*[numSurfaces];
  MSDK_CHECK_POINTER(pmfxSurfaces, MFX_ERR_MEMORY_ALLOC);
  for (int i = 0; i < numSurfaces; i++) {
    pmfxSurfaces[i] = new mfxFrameSurface1;
    memset (pmfxSurfaces[i], 0, sizeof(mfxFrameSurface1));
    memcpy (&(pmfxSurfaces[i]->Info), &(mfxVideoParams.mfx.FrameInfo), sizeof(mfxFrameInfo));
    // MID (memory id) represents one video NV12 surface
    pmfxSurfaces[i]->Data.MemId = mfxResponse.mids[i];
    }

  // Initialize the Media SDK decoder
  sts = mfxDEC.Init (&mfxVideoParams);
  MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
  MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

  // Start decoding the frames
  mfxTime tStart, tEnd;
  mfxGetTime (&tStart);

  mfxSyncPoint syncp;
  mfxFrameSurface1* pmfxOutSurface = NULL;
  int nIndex = 0;
  mfxU32 nFrame = 0;

  // Stage 1: Main decoding loop
  while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts || MFX_ERR_MORE_SURFACE == sts) {
    if (MFX_WRN_DEVICE_BUSY == sts)
      MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call to DecodeFrameAsync

    if (MFX_ERR_MORE_DATA == sts) {
      sts = ReadBitStreamData (&mfxBS, fSource);       // Read more data into input bit stream
      MSDK_BREAK_ON_ERROR(sts);
      }

    if (MFX_ERR_MORE_SURFACE == sts || MFX_ERR_NONE == sts) {
      nIndex = GetFreeSurfaceIndex (pmfxSurfaces, numSurfaces);        // Find free frame surface
      MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, nIndex, MFX_ERR_MEMORY_ALLOC);
      }

    // Decode a frame asychronously (returns immediately)
    //  - If input bitstream contains multiple frames DecodeFrameAsync will start decoding multiple frames, and remove them from bitstream
    sts = mfxDEC.DecodeFrameAsync (&mfxBS, pmfxSurfaces[nIndex], &pmfxOutSurface, &syncp);

    // Ignore warnings if output is available,
    // if no output and no action required just repeat the DecodeFrameAsync call
    if (MFX_ERR_NONE < sts && syncp)
      sts = MFX_ERR_NONE;
    if (MFX_ERR_NONE == sts)
      sts = session.SyncOperation (syncp, 60000);
    if (MFX_ERR_NONE == sts) {
      ++nFrame;

      sts = mfxAllocator.Lock (mfxAllocator.pthis, pmfxOutSurface->Data.MemId, &(pmfxOutSurface->Data));
      MSDK_BREAK_ON_ERROR(sts);

      //printf ("%d memId %d %4.1f\n", pmfxOutSurface->Data.FrameOrder, pmfxOutSurface->Data.MemId, pmfxOutSurface->Data.TimeStamp/3600.0f);
      //sts = WriteRawFrame(pmfxOutSurface, fSink);

      sts = mfxAllocator.Unlock (mfxAllocator.pthis, pmfxOutSurface->Data.MemId, &(pmfxOutSurface->Data));
      MSDK_BREAK_ON_ERROR(sts);
      }
    }

  // MFX_ERR_MORE_DATA means that file has ended, need to go to buffering loop, exit in case of other errors
  MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
  MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

  // Stage 2: Retrieve the buffered decoded frames
  while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_SURFACE == sts) {
    if (MFX_WRN_DEVICE_BUSY == sts)
      MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call to DecodeFrameAsync

    nIndex = GetFreeSurfaceIndex (pmfxSurfaces, numSurfaces);        // Find free frame surface
    MSDK_CHECK_ERROR (MFX_ERR_NOT_FOUND, nIndex, MFX_ERR_MEMORY_ALLOC);

    // Decode a frame asychronously (returns immediately)
    sts = mfxDEC.DecodeFrameAsync (NULL, pmfxSurfaces[nIndex], &pmfxOutSurface, &syncp);

    // Ignore warnings if output is available,
    // if no output and no action required just repeat the DecodeFrameAsync call
    if (MFX_ERR_NONE < sts && syncp)
      sts = MFX_ERR_NONE;
    if (MFX_ERR_NONE == sts)
      sts = session.SyncOperation (syncp, 60000);      // Synchronize. Waits until decoded frame is ready
    if (MFX_ERR_NONE == sts) {
      ++nFrame;
      sts = mfxAllocator.Lock (mfxAllocator.pthis, pmfxOutSurface->Data.MemId, &(pmfxOutSurface->Data));
      MSDK_BREAK_ON_ERROR (sts);

      //printf ("flush %d memId %d %4.1f\n", pmfxOutSurface->Data.FrameOrder, pmfxOutSurface->Data.MemId, pmfxOutSurface->Data.TimeStamp/3600.0f);
      //sts = WriteRawFrame (pmfxOutSurface, fSink);

      sts = mfxAllocator.Unlock (mfxAllocator.pthis, pmfxOutSurface->Data.MemId, &(pmfxOutSurface->Data));
      MSDK_BREAK_ON_ERROR (sts);
      }
    }

  // MFX_ERR_MORE_DATA indicates that all buffers has been fetched, exit in case of other errors
  MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
  MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

  mfxGetTime(&tEnd);
  double elapsed = TimeDiffMsec(tEnd, tStart) / 1000;
  double fps = ((double)nFrame / elapsed);
  printf("\n%d %d Execution time: %3.2f s (%3.2f fps)\n", nFrame, numSurfaces, elapsed, fps);

  // Clean up resources
  //  - It is recommended to close Media SDK components first, before releasing allocated surfaces, since
  //    some surfaces may still be locked by internal Media SDK resources.
  mfxDEC.Close();

  // session closed automatically on destruction
  for (int i = 0; i < numSurfaces; i++)
    delete pmfxSurfaces[i];
  MSDK_SAFE_DELETE_ARRAY(pmfxSurfaces);
  MSDK_SAFE_DELETE_ARRAY(mfxBS.Data);

  mfxAllocator.Free (mfxAllocator.pthis, &mfxResponse);

  fclose(fSource);

  Release();
  return 0;
  }
