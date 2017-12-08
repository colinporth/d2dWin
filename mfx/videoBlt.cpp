ID3D11VideoDevice* d3dVideoDevice = ...;
ID3D11VideoProcessor* d3dVideoProc = ...;
ID3D11VideoProcessorEnumerator* d3dVideoProcEnum = ...;
ID3D11Texture2D* srcTextureNV12Fmt = ...;
ID3D11Texture2D* dstTextureRGBFmt = ...;

ID3D11VideoProcessorInputView* videoProcInputView;
D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inputViewDesc = { 0 };
inputViewDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
inputViewDesc.Texture2D.ArraySlice = arraySliceIdx;
inputViewDesc.Texture2D.MipSlice = 0;
hr = d3dVideoDevice->CreateVideoProcessorInputView (
  srcTextureNV12Fmt, d3dVideoProcEnum, &inputViewDesc, &videoProcInputView);

ID3D11VideoProcessorOutputView * videoProcOutputView;
D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outputViewDesc = { D3D11_VPOV_DIMENSION_TEXTURE2D };
outputViewDesc.Texture2D.MipSlice = 0;
hr = d3dVideoDevice->CreateVideoProcessorOutputView (
  dstTextureRGBFmt, d3dVideoProcEnum, &outputViewDesc, &videoProcOutputView);

// Setup streams
D3D11_VIDEO_PROCESSOR_STREAM streams = { 0 };
streams.Enable = TRUE;
streams.pInputSurface = videoProcInputView.get();

RECT srcRect = { /* source rectangle in pixels*/ };
RECT dstRect = { /* destination rectangle in pixels*/ };

hr = videoCtx_->VideoProcessorBlt (d3dVideoProc, videoProcOutputView.get(), 0, 1, &streams);

//{{{
mfxStatus CD3D11Device::CreateVideoProcessor(mfxFrameSurface1 * pSrf) {

  if (mVideoProcessorEnum.p || NULL == pSrf)
    return MFX_ERR_NONE;

  D3D11_VIDEO_PROCESSOR_CONTENT_DESC ContentDesc;
  MSDK_ZERO_MEMORY (ContentDesc);
  ContentDesc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
  ContentDesc.InputFrameRate.Numerator = 30000;
  ContentDesc.InputFrameRate.Denominator = 1000;
  ContentDesc.InputWidth  = pSrf->Info.CropW;
  ContentDesc.InputHeight = pSrf->Info.CropH;
  ContentDesc.OutputWidth = pSrf->Info.CropW;
  ContentDesc.OutputHeight = pSrf->Info.CropH;
  ContentDesc.OutputFrameRate.Numerator = 30000;
  ContentDesc.OutputFrameRate.Denominator = 1000;
  ContentDesc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

  if (FAILED (m_pDX11VideoDevice->CreateVideoProcessorEnumerator (&ContentDesc, &mVideoProcessorEnum))
    return MFX_ERR_DEVICE_FAILED;
  if (FAILED (m_pDX11VideoDevice->CreateVideoProcessor (mVideoProcessorEnum, 0, &mVideoProcessor))
    return MFX_ERR_DEVICE_FAILED;

  return MFX_ERR_NONE;
  }
//}}}

------------------------------------------------------------------------------
mfxFrameSurface1 * pSrf, mfxFrameAllocator * pAlloc

  CComPtr<ID3D11VideoProcessorInputView>  mInputView;
  CComPtr<ID3D11VideoProcessorOutputView> mOutputView;

  mfxStatus sts = CreateVideoProcessor (pSrf);
  MSDK_CHECK_STATUS (sts, "CreateVideoProcessor failed");

  HRESULT hres = m_pSwapChain->GetBuffer(0, __uuidof( ID3D11Texture2D ), (void**)&m_pDXGIBackBuffer.p);
  if (FAILED(hres))
    return MFX_ERR_DEVICE_FAILED;

  D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC OutputViewDesc;
  OutputViewDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
  OutputViewDesc.Texture2D.MipSlice = 0;
  hres = mDX11VideoDevice->CreateVideoProcessorOutputView (
    mDXGIBackBuffer, mVideoProcessorEnum, &OutputViewDesc, &mOutputView.p);

  D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC InputViewDesc;
  InputViewDesc.FourCC = 0;
  InputViewDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
  InputViewDesc.Texture2D.MipSlice = 0;
  InputViewDesc.Texture2D.ArraySlice = 0;

  mfxHDLPair pair = {NULL};
  sts = pAlloc->GetHDL (pAlloc->pthis, pSrf->Data.MemId, (mfxHDL*)&pair);
  MSDK_CHECK_STATUS(sts, "pAlloc->GetHDL failed");

  ID3D11Texture2D* texture2D = reinterpret_cast<ID3D11Texture2D*>(pair.first);
  //D3D11_TEXTURE2D_DESC Texture2DDesc;
  hres = m_pDX11VideoDevice->CreateVideoProcessorInputView (
   texture2D, m_VideoProcessorEnum, &InputViewDesc, &mInputView.p);
  if (FAILED(hres))
    return MFX_ERR_DEVICE_FAILED;

  //  NV12 surface to RGB backbuffer
  RECT rect = {0};
  rect.right  = pSrf->Info.CropW;
  rect.bottom = pSrf->Info.CropH;

  D3D11_VIDEO_PROCESSOR_STREAM StreamData;
  StreamData.Enable = TRUE;
  StreamData.OutputIndex = 0;
  StreamData.InputFrameOrField = 0;
  StreamData.PastFrames = 0;
  StreamData.FutureFrames = 0;
  StreamData.ppPastSurfaces = NULL;
  StreamData.ppFutureSurfaces = NULL;
  StreamData.pInputSurface = mInputView;
  StreamData.ppPastSurfacesRight = NULL;
  StreamData.ppFutureSurfacesRight = NULL;
  StreamData.pInputSurfaceRight = NULL;

  m_pVideoContext->VideoProcessorSetStreamSourceRect(m_pVideoProcessor, 0, true, &rect);
  m_pVideoContext->VideoProcessorSetStreamFrameFormat( m_pVideoProcessor, 0, D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE);
  hres = m_pVideoContext->VideoProcessorBlt( m_pVideoProcessor, m_pOutputView, 0, 1, &StreamData );
  if (FAILED(hres))
      return MFX_ERR_DEVICE_FAILED;

  DXGI_PRESENT_PARAMETERS parameters = {0};
  hres = m_pSwapChain->Present1 (0, 0, &parameters);
  if (FAILED(hres))
    return MFX_ERR_DEVICE_FAILED;

  return MFX_ERR_NONE;
