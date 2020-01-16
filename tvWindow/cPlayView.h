// cPlayView.h
//{{{  includes
#pragma once

#include "../../shared/utils/cWinAudio.h"

#include "../../shared/dvb/cTransportStream.h"
#include "../common/cVidFrame.h"
#include "../common/cAudFrame.h"

extern "C" {
  #include <libavcodec/avcodec.h>
  #include <libavformat/avformat.h>
  }

#include "mfxvideo++.h"
//}}}

class cPlayView : public cD2dWindow::cView {
public:
  cPlayView (cD2dWindow* window, float width, float height, const std::string& fileName);
  ~cPlayView();

  // cView overrides
  cPoint getSrcSize() { return mBitmap ? mBitmap->GetPixelSize() : cPoint(); }
  bool onKey (int key);
  bool onWheel (int delta, cPoint pos);
  bool onMove (bool right, cPoint pos, cPoint inc);
  bool onUp (bool right, bool mouseMoved, cPoint pos);
  void onDraw (ID2D1DeviceContext* dc);

private:
  //{{{
  class cProgressBox : public cBox {
  public:
    //{{{
    cProgressBox (cD2dWindow* window, float width, float height, cPlayView* playView)
        : cBox("progress", window, width, height), mPlayView(playView) {}
    //}}}
    virtual ~cProgressBox() {}

    //{{{
    bool onDown (bool right, cPoint pos)  {

      mPlayView->setScrub();
      mPlayView->setPlayPts (int64_t ((pos.x / getWidth()) * mPlayView->getLengthPts()));
      mPlayView->getWindow()->changed();

      return true;
      }
    //}}}
    //{{{
    bool onMove (bool right, cPoint pos, cPoint inc) {

      mPlayView->setPlayPts (int64_t ((pos.x / getWidth()) * mPlayView->getLengthPts()));
      mPlayView->getWindow()->changed();
      return true;
      }
    //}}}
    //{{{
    bool onUp (bool right, bool mouseMoved, cPoint pos) {

      mPlayView->setPause();
      return true;
      }
    //}}}
    //{{{
    void onDraw (ID2D1DeviceContext* dc) {

      const float ylen = 2.f;

      // draw analPos blueBar
      auto r = mRect;
      r.left = getWidth() * mPlayView->getStreamPosFrac();
      r.top = r.bottom - 3*ylen;
      dc->FillRectangle (r, mWindow->getBlueBrush());

      // draw vidLastPts yellowBar
      r.left = mRect.left;
      r.right = getWidth() * (float)(mPlayView->mVidTs->getLastLoadedPts() - mPlayView->mAnalTs->getBasePts())
                                     / (float)mPlayView->getLengthPts();
      r.top = r.bottom - ylen;
      dc->FillRectangle (r, mPick ? mWindow->getYellowBrush() :  mWindow->getGreyBrush());

      // draw audLastPts yellowBar
      r.bottom = r.top;
      r.top -= ylen;
      r.right = getWidth() * (float)(mPlayView->mAudTs->getLastLoadedPts() - mPlayView->mAnalTs->getBasePts())
                                    / (float)mPlayView->getLengthPts();
      dc->FillRectangle (r, mPick ? mWindow->getYellowBrush() :  mWindow->getGreyBrush());

      // draw playPts yellowBar
      r.bottom = r.top;
      r.top -= ylen;
      r.right = getWidth() * (float)(mPlayView->getPlayPts() - mPlayView->mAnalTs->getBasePts())
                                    / (float)mPlayView->getLengthPts();
      dc->FillRectangle (r, mPick ? mWindow->getYellowBrush() :  mWindow->getGreyBrush());
      }
    //}}}

  private:
    cPlayView* mPlayView;
    };
  //}}}
  //{{{
  class cTimecodeBox : public cBox {
  public:
    //{{{
    cTimecodeBox (cD2dWindow* window, float width, float height, cPlayView* playView)
        : cBox("timecode", window, width, height), mPlayView(playView) {

      mWindow->getDwriteFactory()->CreateTextFormat (L"Consolas", NULL,
        DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 50.f, L"en-us",
        &mTextFormat);
      mTextFormat->SetTextAlignment (DWRITE_TEXT_ALIGNMENT_TRAILING);
      }
    //}}}
    //{{{
    virtual ~cTimecodeBox() {
      mTextFormat->Release();
      }
    //}}}

    //{{{
    bool onDown (bool right, cPoint pos)  {
      togglePin();
      return true;
      }
    //}}}
    //{{{
    void onDraw (ID2D1DeviceContext* dc) {

      std::string str = getPtsString(mPlayView->mPlayPts) + " " + getPtsString(mPlayView->mAnalTs->mLengthPts);
      std::wstring wstr(str.begin(), str.end());

      IDWriteTextLayout* textLayout;
      mWindow->getDwriteFactory()->CreateTextLayout (wstr.data(), (uint32_t)wstr.size(), mTextFormat,
                                                     getWidth(), getHeight(), &textLayout);

      if (mPin) {
        struct DWRITE_TEXT_METRICS textMetrics;
        textLayout->GetMetrics (&textMetrics);
        auto r = mRect;
        r.left = mRect.right - textMetrics.width - 2.f;
        r.top = mRect.bottom - textMetrics.height + textMetrics.top;
        dc->FillRectangle (r, mWindow->getTransparentBgndBrush());
        }
      else
        dc->DrawTextLayout (getTL (2.f), textLayout, mWindow->getBlackBrush());

      dc->DrawTextLayout (getTL(), textLayout, mWindow->getWhiteBrush());

      textLayout->Release();
      }
    //}}}

  private:
    cPlayView* mPlayView;

    IDWriteTextFormat* mTextFormat = nullptr;
    };
  //}}}
  //{{{
  class cFramesDebugBox : public cBox {
  public:
    //{{{
    cFramesDebugBox (cD2dWindow* window, float width, float height, cPlayView* playView)
        : cBox("frames", window, width, height), mPlayView(playView) {

      mWindow->getDwriteFactory()->CreateTextFormat (L"Consolas", NULL,
        DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 11.f, L"en-us",
        &mTextFormat);
      mTextFormat->SetTextAlignment (DWRITE_TEXT_ALIGNMENT_CENTER);
      }
    //}}}
    //{{{
    virtual ~cFramesDebugBox() {
      mTextFormat->Release();
      }
    //}}}

    //{{{
    bool onWheel (int delta, cPoint pos)  {

      const int kAudSamplesPerAacFrame = 1152;
      auto ptsInc = (pos.y > 40.f) ? (90000/25) : (kAudSamplesPerAacFrame * 90 / 48);

      mPlayView->incPlayPts (-int64_t(delta * ptsInc / 120));
      mPlayView->setPause();
      mPlayView->getWindow()->changed();
      return true;
      }
    //}}}
    //{{{
    bool onMove (bool right, cPoint pos, cPoint inc) {

      const int kAudSamplesPerAacFrame = 1152;
      mPlayView->setPause();
      auto pixPerPts = 18.f * 48 / (kAudSamplesPerAacFrame * 90);
      mPlayView->incPlayPts (int64_t (-inc.x / pixPerPts));
      mWindow->changed();

      return true;
      }
    //}}}
    //{{{
    bool onUp (bool right, bool mouseMoved, cPoint pos) {
      if (!mouseMoved)
        togglePin();
      return true;
      }
    //}}}
    //{{{
    void onDraw (ID2D1DeviceContext* dc) {

      if (mPin)
        dc->FillRectangle (mRect, mWindow->getTransparentBgndBrush());

      float maxY = 0;

      mPlayView->mAudTs->drawFrames (dc, mRect, mTextFormat,
        mWindow->getWhiteBrush(), mWindow->getBlueBrush(), mWindow->getBlackBrush(), mWindow->getYellowBrush(),
        mPlayView->getPlayPts(), maxY);

      mPlayView->mVidTs->drawFrames (dc, mRect, mTextFormat,
        mWindow->getWhiteBrush(), mWindow->getBlueBrush(), mWindow->getBlackBrush(), mWindow->getYellowBrush(),
        mPlayView->getPlayPts(), maxY);

      dc->FillRectangle (RectF((getWidth()/2)-1, 0, (getWidth()/2)+1, maxY),
                         mPin ? mWindow->getYellowBrush() : mWindow->getGreyBrush());

      mLayoutHeight = maxY;
      layout();
      }
    //}}}

  private:
    cPlayView* mPlayView;

    IDWriteTextFormat* mTextFormat = nullptr;
    };
  //}}}

  //{{{
  class cDecodeTransportStream : public cTransportStream {
  public:
    cDecodeTransportStream (int maxFrames) { mFrames.reserve (maxFrames); }
    //{{{
    virtual ~cDecodeTransportStream() {

      for (auto frame : mFrames) {
        delete frame;
        frame = nullptr;
        }
      mFrames.clear();
      }
    //}}}

    int getPid() { return mPid; }
    int64_t getLastLoadedPts() { return mLastLoadedPts; }

    //{{{
    iFrame* findFrame (int64_t& pts) {

      for (auto frame : mFrames)
        if (frame->hasPts (pts)) {
          pts = frame->getPtsEnd();
          return frame;
          }

      return nullptr;
      }
    //}}}
    //{{{
    iFrame* findNearestFrame (int64_t pts) {
    // find first vidFrame on or past pts
    // - returns nullPtr if no frame loaded yet

      iFrame* nearestFrame = nullptr;
      int64_t nearest = 0;

      for (auto frame : mFrames)
        if (frame->hasPts (pts))
          return frame;

        else if (frame->before (pts)) {
          auto dist = frame->beforeDist (pts);
          if (!nearest || (dist < nearest)) {
            nearest = dist;
            nearestFrame = frame;
            }
          }

        else if (frame->after (pts)) {
          auto dist = frame->afterDist (pts);
          if (!nearest || (dist < nearest)) {
            nearest = dist;
            nearestFrame = frame;
            }
          }

      return nearestFrame;
      }
    //}}}
    //{{{
    bool isLoaded (int64_t pts, int frames) {

      for (auto frame = 0; frame < frames; frame++)
        if (!findFrame (pts))
          return false;

      return true;
      }
    //}}}

    //{{{
    void setPid (int pid) {
      mPid = pid;
      invalidateFrames();
      }
    //}}}

    virtual void drawFrames (ID2D1DeviceContext* dc, const cRect& rect, IDWriteTextFormat* textFormat,
                             ID2D1SolidColorBrush* white, ID2D1SolidColorBrush* blue,
                             ID2D1SolidColorBrush* black, ID2D1SolidColorBrush* yellow,
                             int64_t playPts, float& maxY) = 0;
  protected:
    //{{{
    void invalidateFrames() {

      mLoadFrame = 0;
      mLastLoadedPts = -1;

      for (auto frame : mFrames)
        frame->invalidate();
      }
    //}}}

    // const
    const float kPixPerPts = 38.f / 3600.f;
    const float kDrawFramesCentreY = 40.f;
    const float kIndexHeight = 13.f;

    // vars
    int mPid = -1;
    int64_t mLastLoadedPts = -1;
    int mLoadFrame = 0;
    concurrency::concurrent_vector<iFrame*> mFrames;
    };
  //}}}
  //{{{
  class cAudTransportStream : public cDecodeTransportStream {
  public:
    //{{{
    cAudTransportStream (int maxFrames) : cDecodeTransportStream (maxFrames) {

      for (auto i = 0; i < maxFrames; i++)
        mFrames.push_back (new cAudFrame());
      }
    //}}}
    //{{{
    virtual ~cAudTransportStream() {

      if (mAudContext)
        avcodec_close (mAudContext);
      if (mAudParser)
        av_parser_close (mAudParser);
      }
    //}}}

    //{{{
    void drawFrames (ID2D1DeviceContext* dc, const cRect& rect, IDWriteTextFormat* textFormat,
                     ID2D1SolidColorBrush* white, ID2D1SolidColorBrush* blue,
                     ID2D1SolidColorBrush* black, ID2D1SolidColorBrush* yellow,
                     int64_t playPts, float& maxY) {

      auto y = kDrawFramesCentreY;
      auto index = 0;
      for (auto iframe : mFrames) {
        auto frame = (cAudFrame*)iframe;
        if (frame->mNumSamples) {
          const auto audFrameWidth = (90 * frame->mNumSamples / 48) * kPixPerPts;
          int64_t diff = frame->getPts() - playPts;
          auto x = rect.left + (rect.right - rect.left)/2.f + (diff * kPixPerPts);

          if ((x + audFrameWidth > rect.left) && (x < rect.right)) {
            // draw channels
            cRect r (x,0.f, 0.f,y-1.f);
            auto widthPerChannel = (audFrameWidth - 1.f) / frame->mChannels;
            for (auto channel = 0; channel < frame->mChannels; channel++) {
              r.right = r.left + widthPerChannel - 0.5f;
              r.top = y - frame->mPower[channel] / 3.f;
              dc->FillRectangle (r, blue);
              r.left = r.right + 0.5f;
              }

            //  draw index
            r = cRect (x,y, x + audFrameWidth - 1.f, y + kIndexHeight);
            dc->FillRectangle (r, blue);
            auto wstr = wdec (index);
            dc->DrawText (wstr.data(), (uint32_t)wstr.size(), textFormat, r,
                          frame->isFirstPesFrame() ? white : black);
            }
          }
        index++;
        }
      }
    //}}}

  protected:
    //{{{
    bool audDecodePes (cPidInfo* pidInfo, bool skip) {

      bool decoded = false;
      if (pidInfo->mPid == mPid) {
        if (!mAudParser) {
          //{{{  allocate decoder
          AVCodecID streamType;
          if (pidInfo->mStreamType == 17)
            streamType = AV_CODEC_ID_AAC_LATM;
          else if (pidInfo->mStreamType == 15)
            streamType = AV_CODEC_ID_AAC;
          else if (pidInfo->mStreamType == 129)
            streamType = AV_CODEC_ID_AC3;
          else
            streamType = AV_CODEC_ID_MP3;

          mAudParser = av_parser_init (streamType);
          mAudCodec = avcodec_find_decoder (streamType);
          mAudContext = avcodec_alloc_context3 (mAudCodec);
          avcodec_open2 (mAudContext, mAudCodec, NULL);
          }
          //}}}

        cLog::log (LOGINFO2, "audDecodePes PES " + getPtsString (pidInfo->mPts));

        //{{{  init avPacket
        AVPacket avPacket;
        av_init_packet (&avPacket);
        avPacket.data = pidInfo->mBuffer;
        avPacket.size = 0;
        //}}}
        auto avFrame = av_frame_alloc();

        auto interpolatedPts = pidInfo->mPts;
        auto pesPtr = pidInfo->mBuffer;
        auto pesSize = pidInfo->mBufPtr - pidInfo->mBuffer;
        while (pesSize) {
          auto bytesUsed = av_parser_parse2 (mAudParser, mAudContext, &avPacket.data, &avPacket.size,
                                             pesPtr, (int)pesSize, 0, 0, AV_NOPTS_VALUE);
          pesPtr += bytesUsed;
          pesSize -= bytesUsed;
          if (avPacket.size) {
            auto ret = avcodec_send_packet (mAudContext, &avPacket);
            while (ret >= 0) {
              ret = avcodec_receive_frame (mAudContext, avFrame);
              if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF || ret < 0)
                break;
              if (avFrame->nb_samples > 0) {
                auto frame = (cAudFrame*)mFrames[mLoadFrame];
                frame->set (interpolatedPts, avFrame->nb_samples*90/48, pidInfo->mPts, avFrame->channels, avFrame->nb_samples);
                //{{{  covert planar avFrame->data to interleaved float samples
                switch (mAudContext->sample_fmt) {
                  case AV_SAMPLE_FMT_S16P:
                    // 16bit signed planar, copy planar to interleaved
                    for (auto channel = 0; channel < avFrame->channels; channel++) {
                      float power = 0.f;
                      auto srcPtr = (short*)avFrame->data[channel];
                      auto dstPtr = (float*)(frame->mSamples) + channel;
                      for (auto sample = 0; sample < avFrame->nb_samples; sample++) {
                        float value = *srcPtr++ / (float)0x8000;
                        power += value * value;
                        *dstPtr = value;
                        dstPtr += avFrame->channels;
                        }
                      frame->mPower[channel] = sqrtf (power) / avFrame->nb_samples;
                      }
                    break;

                  case AV_SAMPLE_FMT_FLTP:
                    // 32bit float planar, copy planar to interleaved
                    for (auto channel = 0; channel < avFrame->channels; channel++) {
                      float power = 0.f;
                      auto srcPtr = (float*)avFrame->data[channel];
                      auto dstPtr = (float*)(frame->mSamples) + channel;
                      for (auto sample = 0; sample < avFrame->nb_samples; sample++) {
                        power += *srcPtr * *srcPtr;
                        *dstPtr = *srcPtr++;
                        dstPtr += avFrame->channels;
                        }
                      frame->mPower[channel] = sqrtf (power) / avFrame->nb_samples;
                      }
                    break;

                  default:
                    cLog::log (LOGERROR, "audDecodePes - unrecognised sample_fmt " + dec (mAudContext->sample_fmt));
                  }
                //}}}
                mLoadFrame = (mLoadFrame + 1) % mFrames.size();

                mLastLoadedPts = interpolatedPts;
                interpolatedPts += (avFrame->nb_samples * 90) / 48;
                }
              }
            decoded = true;
            }
          }
        av_frame_free (&avFrame);
        }

      return decoded;
      }
    //}}}

  private:
    AVCodec* mAudCodec = nullptr;
    AVCodecContext* mAudContext = nullptr;
    AVCodecParserContext* mAudParser = nullptr;
    };
  //}}}
  //{{{
  class cVidTransportStream : public cDecodeTransportStream {
  public:
    //{{{
    cVidTransportStream (int maxFrames) : cDecodeTransportStream (maxFrames) {

      for (auto i = 0; i < maxFrames; i++)
        mFrames.push_back (new cVidFrame());

      mfxVersion version = { 0,1 };
      mSession.Init (MFX_IMPL_AUTO, &kMfxVersion);
      }
    //}}}
    //{{{
    virtual ~cVidTransportStream() {

      // Clean up resources
      // -  recommended to close Media SDK components first, before releasing allocated surfaces
      //    since //    some surfaces may still be locked by internal Media SDK resources.
      MFXVideoDECODE_Close (mSession);

      // mSession closed automatically on destruction
      for (int i = 0; i < mNumSurfaces; i++)
        delete mSurfaces[i];
      }
    //}}}

    //{{{
    void drawFrames (ID2D1DeviceContext* dc, const cRect& rect, IDWriteTextFormat* textFormat,
                     ID2D1SolidColorBrush* white, ID2D1SolidColorBrush* blue,
                     ID2D1SolidColorBrush* black, ID2D1SolidColorBrush* yellow,
                     int64_t playPts, float& maxY) {

      const auto vidFrameWidth = 3600.f * kPixPerPts; // 25fps

      auto y = kDrawFramesCentreY;

      maxY = y;
      auto index = 0;
      for (auto iframe : mFrames) {
        auto frame = (cVidFrame*)iframe;
        int64_t diff = frame->getPts() - playPts;
        auto x = rect.left + (rect.right - rect.left)/2.f + (diff * kPixPerPts);
        auto y1 = y + kIndexHeight + 1.f;

        // draw index
        dc->FillRectangle (RectF(x,y1, x + vidFrameWidth - 1.f,y1 + kIndexHeight), yellow);
        auto wstr (wdec (index));
        dc->DrawText (wstr.data(), (uint32_t)wstr.size(), textFormat,
                      RectF(x,y1, x + vidFrameWidth - 1.f,y1 + kIndexHeight), black);
        y1 += kIndexHeight + 1.f;

        maxY = std::max (maxY, y1);
        if (frame->isOk()) {
          // draw type
          wstr = frame->mFrameType;
          dc->FillRectangle (RectF(x, y1, x + vidFrameWidth - 1.f, y1 + kIndexHeight), blue);
          dc->DrawText (wstr.data(), (uint32_t)wstr.size(), textFormat,
                        RectF(x,y1, x + vidFrameWidth - 1.f,y1 + kIndexHeight), black);
          y1 += kIndexHeight + 1.f;

          // draw size
          auto l = frame->mPesSize / 1000.f;
          dc->FillRectangle (RectF(x,y1, x + vidFrameWidth - 1.f,y1 + l), white);
          maxY = std::max (maxY,y1+l);

          wstr = (frame->mPesSize >= 1000) ? wdec (frame->mPesSize / 1000) + L"k" : wdec (frame->mPesSize);
          dc->DrawText (wstr.data(), (uint32_t)wstr.size(), textFormat,
                        RectF(x,y1, x + vidFrameWidth - 1.f,y1 + kIndexHeight), black);
          }

        index++;
        }
      }
    //}}}

  protected:
    //{{{
    bool vidDecodePes (cPidInfo* pidInfo, bool skip) {

      const bool kRgba = false;
      bool decoded = false;

      if (pidInfo->mPid == mPid) {
        auto pesSize = int (pidInfo->mBufPtr - pidInfo->mBuffer);
        char frameType = getFrameType (pidInfo->mBuffer, pesSize, pidInfo->mStreamType);
        cLog::log (LOGINFO1, "vPes " + dec (pesSize,5) +
                             " pts:" + getPtsString(pidInfo->mPts) +
                             " " + std::string (&frameType,1) +
                             (skip ? " skip":""));

        mBitstream.Data = pidInfo->mBuffer;
        mBitstream.DataOffset = 0;
        mBitstream.DataLength = pesSize;
        mBitstream.MaxLength = pesSize;
        mBitstream.TimeStamp = pidInfo->mPts;

        if (!mNumSurfaces) {
          //{{{  allocate decoder surfaces, init decoder
          memset (&mVideoParams, 0, sizeof(mVideoParams));
          mVideoParams.mfx.CodecId = (pidInfo->mStreamType == 27) ? MFX_CODEC_AVC : MFX_CODEC_MPEG2;
          mVideoParams.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;

          // decode header
          mfxStatus status = MFXVideoDECODE_DecodeHeader (mSession, &mBitstream, &mVideoParams);
          if (status == MFX_ERR_NONE) {
            //{{{  query surfaces
            mfxFrameAllocRequest frameAllocRequest;
            memset (&frameAllocRequest, 0, sizeof(frameAllocRequest));
            status =  MFXVideoDECODE_QueryIOSurf (mSession, &mVideoParams, &frameAllocRequest);
            mNumSurfaces = frameAllocRequest.NumFrameSuggested;
            //}}}
            auto width = ((mfxU32)((frameAllocRequest.Info.Width)+31)) & (~(mfxU32)31);
            auto height = ((mfxU32)((frameAllocRequest.Info.Height)+31)) & (~(mfxU32)31);

            mfxU16 outWidth;
            mfxU16 outHeight;
            if (kRgba) {
              memset (&mVppParams, 0, sizeof(mVppParams));
              //{{{  VPP Input data
              mVppParams.vpp.In.FourCC = MFX_FOURCC_NV12;
              mVppParams.vpp.In.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
              mVppParams.vpp.In.CropX = 0;
              mVppParams.vpp.In.CropY = 0;
              mVppParams.vpp.In.CropW = mVideoParams.mfx.FrameInfo.CropW;
              mVppParams.vpp.In.CropH = mVideoParams.mfx.FrameInfo.CropH;
              mVppParams.vpp.In.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
              mVppParams.vpp.In.FrameRateExtN = 30;
              mVppParams.vpp.In.FrameRateExtD = 1;
              mVppParams.vpp.In.Width = ((mVppParams.vpp.In.CropW + 15) >> 4) << 4;
              mVppParams.vpp.In.Height =
                (MFX_PICSTRUCT_PROGRESSIVE == mVppParams.vpp.In.PicStruct) ?
                  ((mVppParams.vpp.In.CropH + 15) >> 4) << 4 : ((mfxU32)((mVppParams.vpp.In.CropH)+31)) & (~(mfxU32)31);
              //}}}
              //{{{  VPP Output data
              mVppParams.vpp.Out.FourCC = MFX_FOURCC_RGB4;
              mVppParams.vpp.Out.ChromaFormat = 0;
              mVppParams.vpp.Out.CropX = 0;
              mVppParams.vpp.Out.CropY = 0;
              mVppParams.vpp.Out.CropW = mVideoParams.mfx.FrameInfo.CropW;
              mVppParams.vpp.Out.CropH = mVideoParams.mfx.FrameInfo.CropW;
              mVppParams.vpp.Out.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
              mVppParams.vpp.Out.FrameRateExtN = 30;
              mVppParams.vpp.Out.FrameRateExtD = 1;
              mVppParams.vpp.Out.Width = ((mVppParams.vpp.Out.CropW + 15) >> 4) << 4;
              mVppParams.vpp.Out.Height =
                (MFX_PICSTRUCT_PROGRESSIVE == mVppParams.vpp.Out.PicStruct) ?
                  ((mVppParams.vpp.Out.CropH + 15) >> 4) << 4 : ((mfxU32)((mVppParams.vpp.Out.CropH)+31)) & (~(mfxU32)31);
              //}}}
              mVppParams.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY;

              // Query number of required surfaces for VPP, [0] - in, [1] - out
              mfxFrameAllocRequest vppFrameAllocRequest[2];
              memset (&vppFrameAllocRequest, 0, sizeof (mfxFrameAllocRequest) * 2);
              status = MFXVideoVPP_QueryIOSurf (mSession, &mVppParams, vppFrameAllocRequest);
              mNumVPPInSurfaces = vppFrameAllocRequest[0].NumFrameSuggested;
              mNumVPPOutSurfaces = vppFrameAllocRequest[1].NumFrameSuggested;
              outWidth = (mfxU16)((mfxU32)((vppFrameAllocRequest[1].Info.Width)+31)) & (~(mfxU32)31);
              outHeight = (mfxU16)((mfxU32)((vppFrameAllocRequest[1].Info.Height)+31)) & (~(mfxU32)31);
              }

            cLog::log (LOGNOTICE, "vidDecoder " + dec(width) + "," + dec(height) +
                                  " " + dec(mNumSurfaces) +
                                  "," + dec(mNumVPPInSurfaces) +
                                  "," + dec(mNumVPPOutSurfaces));
            mNumSurfaces += mNumVPPInSurfaces;
            mSurfaces = new mfxFrameSurface1*[mNumSurfaces];
            //{{{  alloc surfaces in system memory
            for (int i = 0; i < mNumSurfaces; i++) {
              mSurfaces[i] = new mfxFrameSurface1;
              memset (mSurfaces[i], 0, sizeof (mfxFrameSurface1));
              memcpy (&mSurfaces[i]->Info, &mVideoParams.mfx.FrameInfo, sizeof(mfxFrameInfo));

              // allocate NV12 followed by planar u, planar v
              mSurfaces[i]->Data.Y = new mfxU8[width * height * 12 / 8];
              mSurfaces[i]->Data.U = mSurfaces[i]->Data.Y + width * height;
              mSurfaces[i]->Data.V = nullptr; // NV12 ignores V pointer
              mSurfaces[i]->Data.Pitch = width;
              }

            if (kRgba) {
              // Allocate surfaces for VPP Out
              // - Width and height of buffer must be aligned, a multiple of 32
              // - Frame surface array keeps pointers all surface planes and general frame info
              mSurfaces2 = new mfxFrameSurface1*[mNumVPPOutSurfaces];
              for (int i = 0; i < mNumVPPOutSurfaces; i++) {
                mSurfaces2[i] = new mfxFrameSurface1;
                memset (mSurfaces2[i], 0, sizeof(mfxFrameSurface1));
                memcpy (&mSurfaces2[i]->Info, &mVppParams.vpp.Out, sizeof(mfxFrameInfo));

                mSurfaces2[i]->Data.Y = new mfxU8[outWidth * outHeight * 32 / 8];
                mSurfaces2[i]->Data.U = mSurfaces2[i]->Data.Y;
                mSurfaces2[i]->Data.V = mSurfaces2[i]->Data.Y;
                mSurfaces2[i]->Data.Pitch = outWidth*4;
                }
              }
            //}}}

            status = MFXVideoDECODE_Init (mSession, &mVideoParams);
            if (kRgba)
              status = MFXVideoVPP_Init (mSession, &mVppParams);
            }
          }
          //}}}

        if (mNumSurfaces) {
          if (skip)
            mfxStatus status = MFXVideoDECODE_Reset (mSession, &mVideoParams);

          // allocate vidFrame for this PES
          ((cVidFrame*)mFrames[mLoadFrame++ % mFrames.size()])->setPes (pidInfo->mPts, 90000/25, pesSize, frameType);

          mfxStatus status = MFX_ERR_NONE;
          while (status >= MFX_ERR_NONE || status == MFX_ERR_MORE_SURFACE) {
            int index = getFreeSurfaceIndex (mSurfaces, mNumSurfaces);
            mfxFrameSurface1* surface = nullptr;
            mfxSyncPoint syncDecode = nullptr;
            cLog::log (LOGINFO3, "vidFrame use surface" + dec (index));
            status = MFXVideoDECODE_DecodeFrameAsync (mSession, &mBitstream, mSurfaces[index], &surface, &syncDecode);
            if (status == MFX_ERR_NONE) {
              if (kRgba) {
                //{{{  vpp rgba
                auto index2 = getFreeSurfaceIndex (mSurfaces2, mNumVPPOutSurfaces);
                mfxSyncPoint syncVpp = nullptr;
                status = MFXVideoVPP_RunFrameVPPAsync (mSession, surface, mSurfaces2[index2], NULL, &syncVpp);

                status = mSession.SyncOperation (syncDecode, 60000);
                status = mSession.SyncOperation (syncVpp, 60000);

                surface = mSurfaces2[index2];
                }
                //}}}
              else
                status = mSession.SyncOperation (syncDecode, 60000);
              if (status == MFX_ERR_NONE) {
                //{{{  got decoded frame, set video of vidFrame
                for (auto iframe : mFrames) {
                  auto frame = (cVidFrame*)iframe;
                  if (frame->getPts() == surface->Data.TimeStamp) {
                    frame->setNv12 (surface->Data.Y, surface->Data.Pitch, surface->Info.Width, surface->Info.Height);
                    cLog::log (LOGINFO2, "-> vidFrame::set - pts:" + getPtsString(surface->Data.TimeStamp));
                    mLastLoadedPts = surface->Data.TimeStamp;
                    break;
                    }
                  }

                decoded = true;
                }
                //}}}
              }
            }
          }
        }

      return decoded;
      }
    //}}}

  private:
    //{{{
    int getFreeSurfaceIndex (mfxFrameSurface1** surfaces, mfxU16 poolSize) {

      if (surfaces)
        for (mfxU16 i = 0; i < poolSize; i++)
          if (0 == surfaces[i]->Data.Locked)
            return i;

      return MFX_ERR_NOT_FOUND;
      }
    //}}}

    // mfx decoder
    MFXVideoSession mSession;
    mfxVideoParam mVideoParams;

    mfxBitstream mBitstream;
    mfxU16 mNumSurfaces = 0;

    mfxVersion kMfxVersion = {0,1};
    mfxVideoParam mVppParams;
    mfxU16 mNumVPPInSurfaces = 0;
    mfxU16 mNumVPPOutSurfaces = 0;

    mfxFrameSurface1** mSurfaces;
    mfxFrameSurface1** mSurfaces2;
    };
  //}}}
  //{{{
  class cAnalTransportStream : public cTransportStream {
  public:
    cAnalTransportStream() {}
    //{{{
    virtual ~cAnalTransportStream() {
      mStreamPosVector.clear();
      }
    //}}}

    int64_t getBasePts() { return mBasePts; }
    int64_t getLengthPts() { return mLengthPts; }
    //{{{
    int64_t getFirstPts (int pid) {
    // return pts of first streamPos matching pid,
    // - return -1 if none

      for (auto &streamPos : mStreamPosVector)
        if (pid == streamPos.mPid)
          return streamPos.mPts;

      return -1;
      }
    //}}}
    //{{{
    cService* getService (int index) {

      int64_t firstPts;
      int64_t lastPts;
      auto service = cTransportStream::getService (index, firstPts, lastPts);
      if (service) {
        mLengthPid = service->getAudPid();
        mBasePts = firstPts;
        mLengthPts = lastPts - firstPts;
        }

      return service;
      }
    //}}}

    //{{{
    int64_t findStreamPos (int pid, int64_t pts) {
    // return streamPos for first streamPos, matching pid, on or after pts
    // - return 0 if none

      for (auto streamPos = mStreamPosVector.rbegin(); streamPos != mStreamPosVector.rend(); ++streamPos)
        if ((pid == streamPos->mPid) && (pts >= streamPos->mPts))
          return streamPos->mPos;

      return 0;
      }
    //}}}
    //{{{
    void clearPosCounts() {
      mStreamPosVector.clear();
      clearPidCounts();
      }
    //}}}
    //{{{
    void clear() {
      mBasePts = -1;
      mLengthPid = -1;
      cTransportStream::clear();
      clearPosCounts();
      }
    //}}}

    // public only for box
    int64_t mLengthPts = -1;

  protected:
    //{{{
    bool audDecodePes (cPidInfo* pidInfo, bool skip) {

      mStreamPosVector.push_back (cStreamPos (pidInfo->mPid, pidInfo->mPts, pidInfo->mStreamPos));
      if (pidInfo->mPid == mLengthPid)
        mLengthPts = pidInfo->mLastPts - pidInfo->mFirstPts;
      //cLog::log (LOGINFO, "anal audPes " + dec(mStreamPosVector.size()) +
      //                    " " + getPtsString (pidInfo->mPts));

      return true;
      }
    //}}}
    //{{{
    bool vidDecodePes (cPidInfo* pidInfo, bool skip) {

      char frameType = getFrameType (pidInfo->mBuffer, pidInfo->mBufPtr - pidInfo->mBuffer, pidInfo->mStreamType);
      if ((frameType == 'I') || (frameType == '?')) {
        mStreamPosVector.push_back (cStreamPos (pidInfo->mPid, pidInfo->mPts, pidInfo->mStreamPos));
        //cLog::log (LOGINFO, "anal vidPes " + dec(mStreamPosVector.size()) +
        //                    " " + getPtsString (pidInfo->mPts));
        }

      return true;
      }
    //}}}

  private:
    int64_t mBasePts = -1;
    int mLengthPid = -1;

    //{{{
    class cStreamPos {
    public:
      cStreamPos (int pid, int64_t pts, int64_t pos) : mPid(pid), mPts(pts), mPos(pos) {}

      int mPid;
      int64_t mPts;
      int64_t mPos;
      };
    //}}}
    concurrency::concurrent_vector<cStreamPos> mStreamPosVector;
    };
  //}}}

  // gets
  int64_t getPlayPts() { return mPlayPts + mAnalTs->getBasePts(); }
  int64_t getLengthPts() { return mAnalTs->getLengthPts(); }
  float getStreamPosFrac() { return float(mStreamPos) / float(mStreamSize); }
  bool getAbort() { return mAbort || mWindow->getExit(); }

  // sets
  void setService (int index);
  void setPlayPts (int64_t playPts);
  void incPlayPts (int64_t incPts) { setPlayPts (mPlayPts + incPts); }
  void setEnd() { setPlayPts (mAnalTs->getLengthPts()); }
  void setPause() { mPlaying = ePause; }
  void setScrub() { mPlaying = eScrub; }
  void togglePlay();

  // threads
  void analyserThread();
  void audThread();
  void vidThread();
  void playThread();

  //{{{  vars
  std::string mFileName;

  cBox* mTimecodeBox;
  cBox* mProgressBox;
  cBox* mAudFrameBox;

  enum ePlaying { ePause, eScrub, ePlay };
  ePlaying mPlaying = ePlay;
  int64_t mPlayPts = 0;

  cAudFrame* mPlayAudFrame = nullptr;

  int64_t mBitmapPts = -1;
  ID2D1Bitmap* mBitmap = nullptr;

  int64_t mStreamPos = 0;
  int64_t mStreamSize = 0;

  cAnalTransportStream* mAnalTs;
  cAudTransportStream* mAudTs;
  cVidTransportStream* mVidTs;

  cSemaphore mPlayPtsSem;
  cSemaphore mFirstVidPtsSem;

  bool mAbort = false;
  bool mAnalAborted = false;
  bool mVidAborted = false;
  bool mAudAborted = false;
  bool mPlayAborted = false;

  iAudio* mAudio = nullptr;
  //}}}
  };
