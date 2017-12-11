// tvWindow.cpp
//{{{  includes
#include "stdafx.h"

#include <stdio.h>

#include "../../shared/utils/cWinAudio.h"
#include "../../shared/decoders/cTransportStream.h"

#include "../common/cBda.h"
#include "../common/cVidFrame.h"
#include "../common/cAudFrame.h"

#include "../common/cTransportStreamBox.h"
#include "../common/cToggleBox.h"
#include "../common/cIntBox.h"
#include "../common/cValueBox.h"
#include "../common/cLogBox.h"
#include "../common/cWindowBox.h"
#include "../common/cAudFrameBox.h"

extern "C" {
  #include <libavcodec/avcodec.h>
  #include <libavformat/avformat.h>
  }
#pragma comment (lib,"avutil.lib")
#pragma comment (lib,"avcodec.lib")
#pragma comment (lib,"avformat.lib")

#include "mfxvideo++.h"
#ifdef _DEBUG
  #pragma comment (lib,"libmfx_vs2015_d.lib")
#else
  #pragma comment (lib,"libmfx.lib")
#endif

#pragma comment (lib,"dxgi.lib")

using namespace concurrency;
//}}}
//{{{  const
const int kAudMaxFrames = 120; // just over 2secs
const int kVidMaxFrames = 100; // 4secs

const int kAudSamplesPerAacFrame = 1152;
const int kAudSamplesPerUnknownFrame = 1024;

// drawFrames layout const
const float kPixPerPts = 38.f / 3600.f;
const float kDrawFramesCentreY = 40.f;
const float kIndexHeight = 13.f;

#define MSDK_ALIGN16(value)  (((value + 15) >> 4) << 4)
#define MSDK_ALIGN32(X)      (((mfxU32)((X)+31)) & (~ (mfxU32)31))

const bool kRgba = false;
//}}}

class cAppWindow : public cD2dWindow, public cWinAudio {
public:
  //{{{
  void run (string title, int width, int height, const string& param) {

    // init transportStreams
    mAnalTs = new cAnalTransportStream();
    mAudTs = new cAudTransportStream (kAudMaxFrames);
    mVidTs = new cVidTransportStream (kVidMaxFrames);

    // init d2dWindow, boxes
    initialise (title, width, height, false);
    addBox (new cVidFrameView (this, 0.f,0.f));
    addBox (new cTransportStreamBox (this, 0, getHeight()/2.f, mAnalTs))->setPin (false);
    addBox (new cFramesDebugBox (this, 0.f,getHeight()/4.f), 0.f,kTextHeight);
    addBox (new cLogBox (this, 200.f,0.f, true), 0.f,200.f);

    int frequency = param.empty() ? 674 : atoi (param.c_str());
    if (frequency) {
      addBox (new cIntBox (this, 70.f,kTextHeight, "sig ", mSignalStrength), -70.f,0.f);

      mFileName = "C:/videos/tune.ts";
      thread ([=]() { bdaThread (frequency*1000, mFileName); }).detach();
      // wait for file create
      Sleep (2000);
      }
    else
      mFileName = param;

    addBox (new cTimecodeBox (this, 600.f,60.f, mPlayPts, mAnalTs->mLengthPts), -600.f,-60.f)->setPin (true);
    addBox (new cProgressBox (this, 0.f,6.f), 0.f,-6.f);
    addBox (new cAudFrameBox (this, 82.f,240.0f, mPlayAudFrame), -84.f,-240.f-6.0f);
    addBox (new cWindowBox (this, 60.f,24.f), -60.f,0.f);

    // init threads
    auto threadHandle = thread ([=](){ analThread(); });
    SetThreadPriority (threadHandle.native_handle(), THREAD_PRIORITY_BELOW_NORMAL);
    threadHandle.detach();

    thread ([=]() { audThread(); }).detach();
    thread ([=]() { vidThread(); }).detach();

    threadHandle = thread ([=](){ playThread(); });
    SetThreadPriority (threadHandle.native_handle(), THREAD_PRIORITY_HIGHEST);
    threadHandle.detach();

    // loop till quit
    messagePump();
    }
  //}}}

protected:
  //{{{
  bool onKey (int key) {

    switch (key) {
      case 0x00: break;
      case 0x1B: return true;

      case  ' ': togglePlay(); break;

      case 0x21: incPlayPts (-90000*10); break; // page up
      case 0x22: incPlayPts (90000*10); break; // page down
      case 0x23: setPlayPts (mAnalTs->getLengthPts()); break; // end
      case 0x24: setPlayPts (0); break; // home
      case 0x25: incPlayPts (-90000/25); mPlaying = ePause; break; // left arrow
      case 0x26: incPlayPts (-90000); break; // up arrow
      case 0x27: incPlayPts (90000/25); mPlaying = ePause; break; // right arrow
      case 0x28: incPlayPts (90000); break; // down arrow

      case  '0': key += 10; // nasty trick to wrap '0' as channel 10
      case  '1':
      case  '2':
      case  '3':
      case  '4':
      case  '5':
      case  '6':
      case  '7':
      case  '8':
      case  '9': {
        auto service = mAnalTs->getService (key - '1');
        if (service) {
          mAudTs->setPid (service->getAudPid());
          mVidTs->setPid (service->getVidPid());
          changed();
          }
        break;
        }

      case  'F': toggleFullScreen(); break;

      default:   printf ("key %x\n", key);
      }

    return false;
    }
  //}}}

private:
  //{{{
  class cAnalTransportStream : public cTransportStream {
  public:
    cAnalTransportStream () {}
    virtual ~cAnalTransportStream() {}

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
    void clearStreamPos() {
      mStreamPosVector.clear();
      }
    //}}}

    //{{{
    class cStreamPos {
    public:
      cStreamPos (int pid, int64_t pts, int64_t pos) : mPid(pid), mPts(pts), mPos(pos) {}

      int mPid;
      int64_t mPts;
      int64_t mPos;
      };
    //}}}
    concurrent_vector<cStreamPos> mStreamPosVector;

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
    };
  //}}}
  //{{{
  class cAudTransportStream : public cTransportStream {
  public:
    //{{{
    cAudTransportStream (int maxFrames) {

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

    int getPid() { return mPid; }
    int64_t getLastLoadedPts() { return mLastLoadedPts; }
    //{{{
    cAudFrame* findFrame (int64_t& pts) {

      for (auto frame : mFrames)
        if (frame->mLoaded && (pts >= frame->mPts) && (pts < frame->mPtsEnd)) {
          pts = frame->mPtsEnd;
          return frame;
          }

      return nullptr;
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

    //{{{
    void drawFrames (ID2D1DeviceContext* dc, const cRect& rect, IDWriteTextFormat* textFormat,
                    ID2D1SolidColorBrush* white, ID2D1SolidColorBrush* blue,
                    ID2D1SolidColorBrush* black, ID2D1SolidColorBrush* yellow,
                    int64_t playPts) {

      auto y = kDrawFramesCentreY;

      auto index = 0;
      for (auto frame : mFrames) {
        if (frame->mNumSamples) {
          const auto audFrameWidth = (90 * frame->mNumSamples / 48) * kPixPerPts;
          int64_t diff = frame->mPts - playPts;
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
            auto wstr = to_wstring (index);
            dc->DrawText (wstr.data(), (uint32_t)wstr.size(), textFormat, r,
                          frame->mPts == frame->mPesPts ? white : black);
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

        cLog::log (LOGINFO2, "audThread - audDecodePes PES " + getPtsString (pidInfo->mPts));

        //{{{  init avPacket
        AVPacket avPacket;
        av_init_packet (&avPacket);
        avPacket.data = pidInfo->mBuffer;
        avPacket.size = 0;
        //}}}
        auto interpolatedPts = pidInfo->mPts;
        auto pesPtr = pidInfo->mBuffer;
        auto pesSize = pidInfo->mBufPtr - pidInfo->mBuffer;
        auto avFrame = av_frame_alloc();
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

              auto frame = mFrames[mLoadFrame];
              frame->set (interpolatedPts, avFrame->nb_samples*90/48, pidInfo->mPts, avFrame->channels, avFrame->nb_samples);
              switch (mAudContext->sample_fmt) {
                case AV_SAMPLE_FMT_S16P:
                  //{{{  16bit signed planar, copy planar to interleaved, calc channel power
                  for (auto channel = 0; channel < avFrame->channels; channel++) {
                    float power = 0.f;
                    auto srcPtr = (short*)avFrame->data[channel];
                    auto dstPtr = (short*)(frame->mSamples) + channel;
                    for (auto i = 0; i < avFrame->nb_samples; i++) {
                      auto sample = *srcPtr++;
                      power += sample * sample;
                      *dstPtr = sample;
                      dstPtr += avFrame->channels;
                      }
                    frame->mPower[channel] = sqrtf (power) / avFrame->nb_samples;
                    }

                  cLog::log (LOGINFO3, "-> audFrame::set 16bit" + getPtsString(interpolatedPts));
                  break;
                  //}}}
                case AV_SAMPLE_FMT_FLTP:
                  //{{{  32bit float planar, copy planar channel to interleaved, calc channel power
                  for (auto channel = 0; channel < avFrame->channels; channel++) {
                    float power = 0.f;
                    auto srcPtr = (float*)avFrame->data[channel];
                    auto dstPtr = (short*)(frame->mSamples) + channel;
                    for (auto i = 0; i < avFrame->nb_samples; i++) {
                      auto sample = (short)(*srcPtr++ * 0x8000);
                      power += sample * sample;
                      *dstPtr = sample;
                      dstPtr += avFrame->channels;
                      }
                    frame->mPower[channel] = sqrtf (power) / avFrame->nb_samples;
                    }

                  cLog::log (LOGINFO3, "-> audFrame::set float " + getPtsString(interpolatedPts));
                  break;
                  //}}}
                default:
                 cLog::log (LOGERROR, "audDecodePes - unrecognised sample_fmt " + dec (mAudContext->sample_fmt));
                }
              mLoadFrame = (mLoadFrame + 1) % mFrames.size();

              mLastLoadedPts = interpolatedPts;
              interpolatedPts += (avFrame->nb_samples * 90) / 48;
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
    //{{{
    void invalidateFrames() {

      mLoadFrame = 0;
      mLastLoadedPts = -1;

      for (auto frame : mFrames)
        frame->invalidate();
      }
    //}}}

    int mPid = -1;
    int64_t mLastLoadedPts = -1;

    int mLoadFrame = 0;
    concurrent_vector<cAudFrame*> mFrames;

    AVCodec* mAudCodec = nullptr;
    AVCodecContext* mAudContext = nullptr;
    AVCodecParserContext* mAudParser = nullptr;
    };
  //}}}
  //{{{
  class cVidTransportStream : public cTransportStream {
  public:
    //{{{
    cVidTransportStream (int maxFrames) {

      mfxVersion version =  {0,1};
      mSession.Init (MFX_IMPL_AUTO, &kMfxVersion);

      for (auto i = 0; i < maxFrames; i++)
        mFrames.push_back (new cVidFrame());
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

    int getPid() { return mPid; }
    int64_t getLastLoadedPts() { return mLastLoadedPts; }
    //{{{
    cVidFrame* findFrame (int64_t& pts) {

      for (auto frame : mFrames)
        if (frame->mLoaded && (pts >= frame->mPts) && (pts < frame->mPtsEnd)) {
          pts = frame->mPtsEnd;
          return frame;
          }

      return nullptr;
      }
    //}}}
    //{{{
    cVidFrame* findNearestFrame (int64_t pts) {
    // find first vidFrame on or past pts
    // - returns nullPtr if no frame loaded yet

      int64_t nearest = 0;
      cVidFrame* nearestFrame = nullptr;

      for (auto frame : mFrames) {
        if (frame->mLoaded) {
          if ((pts >= frame->mPts) && (pts < frame->mPtsEnd))
            return frame;
          else if (pts < frame->mPts) {
            if (!nearest || ((frame->mPts - pts) < nearest)) {
              nearest = frame->mPts - pts;
              nearestFrame = frame;
              }
            }
          else if (pts > frame->mPtsEnd) {
            if (!nearest || ((pts - frame->mPtsEnd) < nearest)) {
              nearest = pts - frame->mPts;
              nearestFrame = frame;
              }
            }
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

    //{{{
    void drawFrames (ID2D1DeviceContext* dc, const cRect& rect, IDWriteTextFormat* textFormat,
                     ID2D1SolidColorBrush* white, ID2D1SolidColorBrush* blue,
                     ID2D1SolidColorBrush* black, ID2D1SolidColorBrush* yellow,
                     int64_t playPts, float& maxY) {

      const auto vidFrameWidth = 3600.f * kPixPerPts; // 25fps
      auto y = kDrawFramesCentreY;

      maxY = y;
      auto index = 0;
      for (auto frame : mFrames) {
        int64_t diff = frame->mPts - playPts;
        auto x = rect.left + (rect.right - rect.left)/2.f + (diff * kPixPerPts);
        auto y1 = y + kIndexHeight + 1.f;

        // draw index
        dc->FillRectangle (RectF(x,y1, x + vidFrameWidth - 1.f,y1 + kIndexHeight), yellow);
        auto wstr (to_wstring (index));
        dc->DrawText (wstr.data(), (uint32_t)wstr.size(), textFormat,
                      RectF(x,y1, x + vidFrameWidth - 1.f,y1 + kIndexHeight), black);
        y1 += kIndexHeight + 1.f;

        maxY = max (maxY, y1);
        if (frame->mLoaded) {
          // draw type
          wstr = frame->mFrameType;
          dc->FillRectangle (RectF(x, y1, x + vidFrameWidth - 1.f, y1 + kIndexHeight), blue);
          dc->DrawText (wstr.data(), (uint32_t)wstr.size(), textFormat,
                        RectF(x,y1, x + vidFrameWidth - 1.f,y1 + kIndexHeight), black);
          y1 += kIndexHeight + 1.f;

          // draw size
          auto l = frame->mPesSize / 1000.f;
          dc->FillRectangle (RectF(x,y1, x + vidFrameWidth - 1.f,y1 + l), white);
          maxY = max (maxY,y1+l);

          wstr = (frame->mPesSize >= 1000) ? to_wstring (frame->mPesSize / 1000) + L"k" : to_wstring (frame->mPesSize);
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

      bool decoded = false;

      if (pidInfo->mPid == mPid) {
        auto pesSize = int (pidInfo->mBufPtr - pidInfo->mBuffer);
        char frameType = getFrameType (pidInfo->mBuffer, pesSize, pidInfo->mStreamType);
        cLog::log (LOGINFO1, "vPes " + dec (pesSize,5) +
                             " pts:" + getPtsString(pidInfo->mPts) +
                             " " + string (&frameType,1) +
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
            auto width = MSDK_ALIGN32 (frameAllocRequest.Info.Width);
            auto height = MSDK_ALIGN32 (frameAllocRequest.Info.Height);

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
              mVppParams.vpp.In.Width = MSDK_ALIGN16 (mVppParams.vpp.In.CropW);
              mVppParams.vpp.In.Height =
                (MFX_PICSTRUCT_PROGRESSIVE == mVppParams.vpp.In.PicStruct) ?
                  MSDK_ALIGN16(mVppParams.vpp.In.CropH) : MSDK_ALIGN32(mVppParams.vpp.In.CropH);
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
              mVppParams.vpp.Out.Width = MSDK_ALIGN16 (mVppParams.vpp.Out.CropW);
              mVppParams.vpp.Out.Height =
                (MFX_PICSTRUCT_PROGRESSIVE == mVppParams.vpp.Out.PicStruct) ?
                  MSDK_ALIGN16(mVppParams.vpp.Out.CropH) : MSDK_ALIGN32(mVppParams.vpp.Out.CropH);
              //}}}
              mVppParams.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY;

              // Query number of required surfaces for VPP, [0] - in, [1] - out
              mfxFrameAllocRequest vppFrameAllocRequest[2];
              memset (&vppFrameAllocRequest, 0, sizeof (mfxFrameAllocRequest) * 2);
              status = MFXVideoVPP_QueryIOSurf (mSession, &mVppParams, vppFrameAllocRequest);
              mNumVPPInSurfaces = vppFrameAllocRequest[0].NumFrameSuggested;
              mNumVPPOutSurfaces = vppFrameAllocRequest[1].NumFrameSuggested;

              outWidth = (mfxU16)MSDK_ALIGN32(vppFrameAllocRequest[1].Info.Width);
              outHeight = (mfxU16)MSDK_ALIGN32(vppFrameAllocRequest[1].Info.Height);
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
          mFrames[mLoadFrame++ % mFrames.size()]->setPes (pidInfo->mPts, 90000/25, pesSize, frameType);

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
                for (auto frame : mFrames)
                  if (frame->mPts == surface->Data.TimeStamp) {
                    frame->setNv12 (surface->Data.Y, surface->Data.Pitch, surface->Info.Width, surface->Info.Height);
                    cLog::log (LOGINFO2, "-> vidFrame::set - pts:" + getPtsString(surface->Data.TimeStamp));
                    mLastLoadedPts = surface->Data.TimeStamp;
                    break;
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
    //{{{
    void invalidateFrames() {

      mLastLoadedPts = -1;
      mLoadFrame = 0;
      for (auto frame : mFrames)
        frame->invalidate();
      }
    //}}}

    // vidFrames
    int mPid = -1;
    concurrent_vector<cVidFrame*> mFrames;
    int mLoadFrame = 0;

    int64_t mLastLoadedPts = -1;

    // mfx decoder
    MFXVideoSession mSession;
    mfxFrameAllocator mAllocator;
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
  class cVidFrameView : public cView {
  public:
    //{{{
    cVidFrameView (cD2dWindow* window, float width, float height)
        : cView("vidFrame", window, width, height) {
      mPin = true;
      }
    //}}}
    virtual ~cVidFrameView() {}

    //{{{
    cPoint getSrcSize() {
      auto appWindow = dynamic_cast<cAppWindow*>(mWindow);
      return appWindow->mBitmap ? appWindow->mBitmap->GetPixelSize() : cPoint();
      }
    //}}}
    //{{{
    bool onWheel (int delta, cPoint pos) {

      auto appWindow = dynamic_cast<cAppWindow*>(mWindow);

      if (mWindow->getControl())
        return cView::onWheel (delta, pos);
      else {
        appWindow->mPlaying = ePause;
        appWindow->incPlayPts (-int64_t(delta * (90000/25) / 120));
        }

      return true;
      }
    //}}}
    //{{{
    bool onMove (bool right, cPoint pos, cPoint inc) {

      if (mWindow->getControl())
        return cView::onMove (right, pos, inc);
      else {
        auto appWindow = dynamic_cast<cAppWindow*>(mWindow);
        appWindow->mPlaying = ePause;
        appWindow->incPlayPts (int64_t (-inc.x * kAudSamplesPerAacFrame * 48 / 90 / 8));
        return true;
        }
      }
    //}}}
    //{{{
    bool onUp (bool right, bool mouseMoved, cPoint pos) {

      auto appWindow = dynamic_cast<cAppWindow*>(mWindow);
      if (!mouseMoved)
        appWindow->togglePlay();

      return false;
      }
    //}}}

    //{{{
    void onDraw (ID2D1DeviceContext* dc) {

      auto appWindow = dynamic_cast<cAppWindow*>(mWindow);

      auto vidframe = appWindow->mVidTs->findNearestFrame (appWindow->getPlayPts());
      if (vidframe && (vidframe->mPts != appWindow->mBitmapPts)) {
        appWindow->mBitmap = vidframe->makeBitmap (dc, appWindow->mBitmap);
        appWindow->mBitmapPts = vidframe->mPts;
        }

      dc->SetTransform (mView2d.mTransform);
      if (appWindow->mBitmap)
        dc->DrawBitmap (appWindow->mBitmap, cRect(mWindow->getSize()));

      dc->SetTransform (Matrix3x2F::Identity());
      }
    //}}}
    };
  //}}}
  //{{{
  class cFramesDebugBox : public cBox {
  public:
    //{{{
    cFramesDebugBox (cD2dWindow* window, float width, float height)
        : cBox("frames", window, width, height) {

      mWindow->getDwriteFactory()->CreateTextFormat (L"Consolas", NULL,
        DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 11.f, L"en-us",
        &mTextFormat);
      mTextFormat->SetTextAlignment (DWRITE_TEXT_ALIGNMENT_CENTER);

      mWindow->getDc()->CreateSolidColorBrush (kTransparentBlack, &mBrush);
      }
    //}}}
    //{{{
    virtual ~cFramesDebugBox() {
      mTextFormat->Release();
      mBrush->Release();
      }
    //}}}

    //{{{
    bool onWheel (int delta, cPoint pos)  {

      auto ptsInc = (pos.y > 40.f) ? (90000/25) : (kAudSamplesPerAacFrame * 90 / 48);

      auto appWindow = dynamic_cast<cAppWindow*>(mWindow);
      appWindow->incPlayPts (-int64_t(delta * ptsInc / 120));
      appWindow->setPlaying (cAppWindow::ePause);

      return true;
      }
    //}}}
    //{{{
    bool onMove (bool right, cPoint pos, cPoint inc) {

      auto appWindow = dynamic_cast<cAppWindow*>(mWindow);

      appWindow->setPlaying (cAppWindow::ePause);
      auto pixPerPts = 18.f * 48 / (kAudSamplesPerAacFrame * 90);
      appWindow->incPlayPts (int64_t (-inc.x / pixPerPts));

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
        dc->FillRectangle (mRect, mBrush);

      auto appWindow = dynamic_cast<cAppWindow*>(mWindow);
      appWindow->mAudTs->drawFrames (dc, mRect, mTextFormat,
        mWindow->getWhiteBrush(), mWindow->getBlueBrush(), mWindow->getBlackBrush(), mWindow->getYellowBrush(),
        appWindow->getPlayPts());

      float maxY = 0;
      appWindow->mVidTs->drawFrames (dc, mRect, mTextFormat,
        mWindow->getWhiteBrush(), mWindow->getBlueBrush(), mWindow->getBlackBrush(), mWindow->getYellowBrush(),
        appWindow->getPlayPts(), maxY);

      dc->FillRectangle (RectF((getWidth()/2)-1, 0, (getWidth()/2)+1, maxY),
                         mPin ? mWindow->getYellowBrush() : mWindow->getGreyBrush());

      mLayoutHeight = maxY;
      layout();
      }
    //}}}

  private:
    IDWriteTextFormat* mTextFormat = nullptr;
    ID2D1SolidColorBrush* mBrush = nullptr;
    };
  //}}}
  //{{{
  class cTimecodeBox : public cBox {
  public:
    //{{{
    cTimecodeBox (cD2dWindow* window, float width, float height, int64_t& playPts, int64_t& lengthPts)
        : cBox("timecode", window, width, height), mPlayPts(playPts), mLengthPts(lengthPts) {

      mWindow->getDwriteFactory()->CreateTextFormat (L"Consolas", NULL,
        DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 50.f, L"en-us",
        &mTextFormat);
      mTextFormat->SetTextAlignment (DWRITE_TEXT_ALIGNMENT_TRAILING);

      mWindow->getDc()->CreateSolidColorBrush (kTransparentBlack, &mBrush);
      }
    //}}}
    //{{{
    virtual ~cTimecodeBox() {
      mTextFormat->Release();
      mBrush->Release();
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

      string str = getPtsString(mPlayPts) + " " + getPtsString(mLengthPts);
      wstring wstr(str.begin(), str.end());

      IDWriteTextLayout* textLayout;
      mWindow->getDwriteFactory()->CreateTextLayout (wstr.data(), (uint32_t)wstr.size(), mTextFormat,
                                                     getWidth(), getHeight(), &textLayout);

      if (mPin) {
        struct DWRITE_TEXT_METRICS textMetrics;
        textLayout->GetMetrics (&textMetrics);
        auto r = mRect;
        r.left = mRect.right - textMetrics.width - 2.f;
        r.top = mRect.bottom - textMetrics.height + textMetrics.top;
        dc->FillRectangle (r, mBrush);
        }
      else
        dc->DrawTextLayout (getTL (2.f), textLayout, mWindow->getBlackBrush());

      dc->DrawTextLayout (getTL(), textLayout, mWindow->getWhiteBrush());

      textLayout->Release();
      }
    //}}}

  private:
    int64_t& mPlayPts;
    int64_t& mLengthPts;

    IDWriteTextFormat* mTextFormat = nullptr;
    ID2D1SolidColorBrush* mBrush = nullptr;
    };
  //}}}
  //{{{
  class cProgressBox : public cBox {
  public:
    //{{{
    cProgressBox (cD2dWindow* window, float width, float height)
        : cBox("progress", window, width, height) {
      mPin = true;
      }
    //}}}
    virtual ~cProgressBox() {}

    //{{{
    bool onDown (bool right, cPoint pos)  {

      auto appWindow = dynamic_cast<cAppWindow*>(mWindow);

      appWindow->setPlaying (cAppWindow::eScrub);
      appWindow->setPlayPts (int64_t ((pos.x / getWidth()) * appWindow->getLengthPts()));

      return true;
      }
    //}}}
    //{{{
    bool onMove (bool right, cPoint pos, cPoint inc) {

      auto appWindow = dynamic_cast<cAppWindow*>(mWindow);

      appWindow->setPlayPts (int64_t ((pos.x / getWidth()) * appWindow->getLengthPts()));
      return true;
      }
    //}}}
    //{{{
    bool onUp (bool right, bool mouseMoved, cPoint pos) {

      auto appWindow = dynamic_cast<cAppWindow*>(mWindow);

      appWindow->setPlaying (cAppWindow::ePause);
      return true;
      }
    //}}}
    //{{{
    void onDraw (ID2D1DeviceContext* dc) {

      auto appWindow = dynamic_cast<cAppWindow*>(mWindow);

      const float ylen = 2.f;

      // draw analPos blueBar
      auto r = mRect;
      r.left = getWidth() * appWindow->getStreamPosFrac();
      r.top = r.bottom - 3*ylen;
      dc->FillRectangle (r, mWindow->getBlueBrush());

      // draw vidLastPts yellowBar
      r.left = mRect.left;
      r.right = getWidth() * (float)(appWindow->mVidTs->getLastLoadedPts() - appWindow->mAnalTs->getBasePts())
                                     / (float)appWindow->getLengthPts();
      r.top = r.bottom - ylen;
      dc->FillRectangle (r, mPick ? mWindow->getYellowBrush() :  mWindow->getGreyBrush());

      // draw audLastPts yellowBar
      r.bottom = r.top;
      r.top -= ylen;
      r.right = getWidth() * (float)(appWindow->mAudTs->getLastLoadedPts() - appWindow->mAnalTs->getBasePts())
                                    / (float)appWindow->getLengthPts();
      dc->FillRectangle (r, mPick ? mWindow->getYellowBrush() :  mWindow->getGreyBrush());

      // draw playPts yellowBar
      r.bottom = r.top;
      r.top -= ylen;
      r.right = getWidth() * (float)(appWindow->getPlayPts() - appWindow->mAnalTs->getBasePts())
                                    / (float)appWindow->getLengthPts();
      dc->FillRectangle (r, mPick ? mWindow->getYellowBrush() :  mWindow->getGreyBrush());
      }
    //}}}
    };
  //}}}

  enum ePlaying { ePause, eScrub, ePlay };
  //{{{  gets
  ePlaying getPlaying() { return mPlaying; }

  int64_t getPlayPts() { return mPlayPts + mAnalTs->getBasePts(); }
  int64_t getLengthPts() { return mAnalTs->getLengthPts(); }

  float getStreamPosFrac() { return float(mStreamPos) / float(mStreamSize); }
  //}}}
  //{{{  sets
  void setPlaying (ePlaying playing) { mPlaying = playing; }

  //{{{
  void setPlayPts (int64_t playPts) {
  // mPlayPts is offset from mBasePts

    if (playPts < 0)
      mPlayPts = 0;
    else if (playPts > getLengthPts())
      mPlayPts = getLengthPts();
    else
      mPlayPts = playPts;

    mPlayPtsSem.notifyAll();

    changed();
    }
  //}}}
  void incPlayPts (int64_t incPts) { setPlayPts (mPlayPts + incPts); }
  //}}}
  //{{{
  void togglePlay() {

    switch (mPlaying) {
      case ePause: mPlaying = ePlay; break;
      case eScrub: mPlaying = ePlay; break;
      case ePlay:  mPlaying = ePause; break;
      }
    }
  //}}}

  //{{{
  void bdaThread (int frequency, const string& fileName) {

    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::log (LOGNOTICE, "bdaThread - start");

    auto bda = new cBda();
    bda->createGraph (frequency, fileName);

    while (!getExit()) {
      mSignalStrength = bda->getSignalStrength();
      Sleep (200);
      }

    cLog::log (LOGNOTICE, "bdaThread - exit");
    CoUninitialize();
    }
  //}}}
  //{{{
  void analThread() {

    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::log (LOGNOTICE, "analThread - start");

    auto file = _open (mFileName.c_str(), _O_RDONLY | _O_BINARY);
    //{{{  get streamSize
    struct _stat64 buf;
    _fstati64 (file, &buf);
    mStreamSize = buf.st_size ;
    //}}}

    const auto kChunkSize = 2048*188;
    const auto chunkBuf = (uint8_t*)malloc (kChunkSize);
    //{{{  parse front of stream until first service
    int64_t streamPos = 0;

    cService* service = nullptr;
    while (!service) {
      auto chunkBytesLeft = _read (file, chunkBuf, kChunkSize);
      auto chunkPtr = chunkBuf;
      while (chunkBytesLeft >= 188) {
        auto bytesUsed = mAnalTs->demux (chunkPtr, chunkBytesLeft, streamPos, false, -1);
        streamPos += bytesUsed;
        chunkPtr += bytesUsed;
        chunkBytesLeft -= (int)bytesUsed;
        }

      service = mAnalTs->getService (0);
      }

    // select first service
    mVidTs->setPid (service->getVidPid());
    mAudTs->setPid (service->getAudPid());
    cLog::log (LOGNOTICE, "analThread - first service - vidPid:" + dec(service->getVidPid()) +
                          " audPid:" + dec(service->getAudPid()));

    mAnalTs->clearStreamPos();
    //}}}
    //{{{  parse end of stream for initial lastPts
    streamPos = mStreamSize - (kChunkSize * 8);
    _lseeki64 (file, streamPos, SEEK_SET);

    while (streamPos < mStreamSize) {
      auto chunkBytesLeft = _read (file, chunkBuf, kChunkSize);
      auto chunkPtr = chunkBuf;
      while (chunkBytesLeft >= 188) {
        auto bytesUsed = mAnalTs->demux (chunkPtr, chunkBytesLeft, streamPos, chunkPtr == chunkBuf, -1);
        streamPos += bytesUsed;
        chunkPtr += bytesUsed;
        chunkBytesLeft -= (int)bytesUsed;
        }
      }

    mAnalTs->clearStreamPos();
    //}}}

    // analyse stream from start, chase tail, update progress bar
    mStreamPos = 0;
    _lseeki64 (file, mStreamPos, SEEK_SET);

    int firstVidSignalCount = 0;
    int fileDoneCount = 0;
    while (fileDoneCount < 50) {
      int64_t bytesToRead = mStreamSize - mStreamPos;
      if (bytesToRead > kChunkSize) // trim to kChunkSize
        bytesToRead = kChunkSize;
      if (bytesToRead >= 188) {
        // at least 1 packet to read, trim read to packet boundary
        bytesToRead -= bytesToRead % 188;
        auto chunkBytesLeft = _read (file, chunkBuf, (unsigned int)bytesToRead);
        fileDoneCount = 0;
        if (chunkBytesLeft >= 188) {
          auto chunkPtr = chunkBuf;
          while (chunkBytesLeft >= 188) {
            auto bytesUsed = mAnalTs->demux (chunkPtr, chunkBytesLeft, mStreamPos, mStreamPos == 0, -1);
            mStreamPos += bytesUsed;
            chunkPtr += bytesUsed;
            chunkBytesLeft -= (int)bytesUsed;
            changed();

            if ((firstVidSignalCount < 3) &&
                (mAnalTs->getFirstPts (service->getAudPid()) != -1) &&
                (mAnalTs->getFirstPts (service->getVidPid()) != -1)) {
              cLog::log (LOGNOTICE, "analThread - signal " + dec(firstVidSignalCount++));
              mFirstVidPtsSem.notifyAll();
              }
            }
          }
        }
      else {
        //{{{  check if fileSize changed, 50 goes 100ms second apart before we give up
        _fstati64 (file, &buf);
        if (mStreamSize == buf.st_size) {
          Sleep (100);
          fileDoneCount++;
          }
        else
          mStreamSize = buf.st_size;
        }
        //}}}
      }

    _close (file);
    free (chunkBuf);

    cLog::log (LOGNOTICE, "analThread - exit - mStreamPosVector.size:" + dec (mAnalTs->mStreamPosVector.size()));
    CoUninitialize();
    }
  //}}}
  //{{{
  void audThread() {

    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::log (LOGNOTICE, "audThread - start");
    av_register_all();

    const int kChunkSize = 2048*188;
    const auto chunkBuf = (uint8_t*)malloc (kChunkSize);

    cLog::log (LOGNOTICE, "audThread - waiting");
    mFirstVidPtsSem.wait();
    cLog::log (LOGNOTICE, "audThread - signalled");

    bool skip = false;
    int64_t lastJumpStreamPos = -1;

    int64_t streamPos = 0;
    auto file = _open (mFileName.c_str(), _O_RDONLY | _O_BINARY);
    while (true) {
      auto chunkPtr = chunkBuf;
      auto chunkBytesLeft = _read (file, chunkBuf, kChunkSize);
      if (chunkBytesLeft < 188) {
        // end of file
        while (mAudTs->isLoaded (getPlayPts(), 1))
          mPlayPtsSem.wait();
        //{{{  jump to pts in stream
        auto jumpStreamPos = mAnalTs->findStreamPos (mAudTs->getPid(), getPlayPts());
        streamPos = jumpStreamPos;
        _lseeki64 (file, streamPos, SEEK_SET);
        lastJumpStreamPos = jumpStreamPos;

        chunkBytesLeft = 0;
        skip = true;
        //}}}
        }
      else {
        while (chunkBytesLeft >= 188) {
          //{{{  demux up to a frame
          // decode a frame
          auto bytesUsed = mAudTs->demux (chunkPtr, chunkBytesLeft, streamPos, skip, mAudTs->getPid());
          streamPos += bytesUsed;
          chunkPtr += bytesUsed;
          chunkBytesLeft -= (int)bytesUsed;
          skip = false;
          //}}}
          changed();

          while (mAudTs->isLoaded (getPlayPts(), kAudMaxFrames/2))
            mPlayPtsSem.wait();

          bool loaded = mAudTs->isLoaded (getPlayPts(), 1);
          if (!loaded || (getPlayPts() > mAudTs->getLastLoadedPts() + 100000)) {
            auto jumpStreamPos = mAnalTs->findStreamPos (mAudTs->getPid(), getPlayPts());
            if (jumpStreamPos != lastJumpStreamPos) {
              //{{{  jump to jumpStreamPos, unless same as last, wait for rest of GOP or chunk to demux
              cLog::log (LOGINFO, "audThread - jump playPts:" + getPtsString(getPlayPts()) +
                         (loaded ? " after ":" notLoaded ") + getPtsString(mAudTs->getLastLoadedPts()));
              streamPos = jumpStreamPos;
              _lseeki64 (file, streamPos, SEEK_SET);
              lastJumpStreamPos = jumpStreamPos;
              chunkBytesLeft = 0;
              skip = true;
              break;
              }
              //}}}
            }
          }
        }
      }

    _close (file);
    free (chunkBuf);

    cLog::log (LOGNOTICE, "audThread - exit");
    CoUninitialize();
    }
  //}}}
  //{{{
  void vidThread() {

    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::log (LOGNOTICE, "vidThread - start");

    const int kChunkSize = 2048*188;
    const auto chunkBuf = (uint8_t*)malloc (kChunkSize);

    cLog::log (LOGNOTICE, "vidThread - waiting");
    mFirstVidPtsSem.wait();
    cLog::log (LOGNOTICE, "vidThread - signalled");

    bool skip = false;
    int64_t lastJumpStreamPos = -1;

    int64_t streamPos = 0;
    auto file = _open (mFileName.c_str(), _O_RDONLY | _O_BINARY);
    while (true) {
      auto chunkPtr = chunkBuf;
      auto chunkBytesLeft = _read (file, chunkBuf, kChunkSize);
      if (chunkBytesLeft < 188) {
        // end of file
        while (mVidTs->isLoaded (getPlayPts(), 1))
          mPlayPtsSem.wait();
        //{{{  jump to pts in stream
        auto jumpStreamPos = mAnalTs->findStreamPos (mVidTs->getPid(), getPlayPts());
        streamPos = jumpStreamPos;
        _lseeki64 (file, streamPos, SEEK_SET);
        lastJumpStreamPos = jumpStreamPos;

        chunkBytesLeft = 0;
        skip = true;
        //}}}
        }
      else {
        while (chunkBytesLeft >= 188) {
          //{{{  demux up to a frame
          auto bytesUsed = mVidTs->demux (chunkPtr, chunkBytesLeft, streamPos, skip, mVidTs->getPid());
          streamPos += bytesUsed;
          chunkPtr += bytesUsed;
          chunkBytesLeft -= (int)bytesUsed;
          skip = false;
          //}}}
          changed();

          while (mVidTs->isLoaded (getPlayPts(), 4))
            mPlayPtsSem.wait();

          bool loaded = mVidTs->isLoaded (getPlayPts(), 1);
          if (!loaded || (getPlayPts() > mVidTs->getLastLoadedPts() + 100000)) {
            auto jumpStreamPos = mAnalTs->findStreamPos (mVidTs->getPid(), getPlayPts());
            if (jumpStreamPos != lastJumpStreamPos) {
              //{{{  jump to jumpStreamPos, unless same as last, wait for rest of GOP or chunk to demux
              cLog::log (LOGINFO, "vidThread - jump playPts:" + getPtsString(getPlayPts()) +
                         (loaded ? " after ":" notLoaded ") + getPtsString(mVidTs->getLastLoadedPts()));
              _lseeki64 (file, jumpStreamPos, SEEK_SET);
              lastJumpStreamPos = jumpStreamPos;
              streamPos = jumpStreamPos;
              chunkBytesLeft = 0;
              skip = true;
              break;
              }
              //}}}
            }
          }
        }
      }

    _close (file);
    free (chunkBuf);

    cLog::log (LOGNOTICE, "vidThread - exit");
    CoUninitialize();
    }
  //}}}
  //{{{
  void playThread() {

    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::log (LOGNOTICE, "playThread - start");

    audOpen (2, 48000);

    cLog::log (LOGNOTICE, "playThread - waiting");
    mFirstVidPtsSem.wait();
    mPlayPts = mAnalTs->getFirstPts (mVidTs->getPid()) - mAnalTs->getBasePts();
    cLog::log (LOGNOTICE, "playThread - signalled " + getFullPtsString (mPlayPts));

    while (true) {
      auto pts = getPlayPts();
      mPlayAudFrame = mAudTs->findFrame (pts);

      // play using frame where available, else play silence
      audPlay (mPlayAudFrame ? mPlayAudFrame->mChannels : getSrcChannels(),
               mPlayAudFrame && (mPlaying != ePause) ?  mPlayAudFrame->mSamples : nullptr,
               mPlayAudFrame ? mPlayAudFrame->mNumSamples : kAudSamplesPerUnknownFrame,
               1.f);

      if ((mPlaying == ePlay) && (mPlayPts < getLengthPts()))
        incPlayPts (int64_t (((mPlayAudFrame ? mPlayAudFrame->mNumSamples : kAudSamplesPerUnknownFrame) * 90) / 48));
      if (mPlayPts > getLengthPts())
        mPlayPts = getLengthPts();

      mPlayPtsSem.notifyAll();
      }

    audClose();

    cLog::log (LOGNOTICE, "playThread - exit");
    CoUninitialize();
    }
  //}}}

  //{{{  vars
  string mFileName;

  cBda* mBda = nullptr;
  int mSignalStrength = 0;

  ePlaying mPlaying = ePlay;
  int64_t mPlayPts = 0;

  int64_t mStreamPos = 0;
  int64_t mStreamSize = 0;

  cAnalTransportStream* mAnalTs;
  cAudTransportStream* mAudTs;
  cVidTransportStream* mVidTs;

  int64_t mBitmapPts = -1;
  ID2D1Bitmap* mBitmap = nullptr;

  cAudFrame* mPlayAudFrame;

  cSemaphore mFirstVidPtsSem;
  cSemaphore mPlayPtsSem;
  //}}}
  };

//{{{
int __stdcall WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

  auto res = CoInitializeEx (NULL, COINIT_MULTITHREADED);

  //cLog::init ("tvWindow", LOGINFO, true);
  cLog::init ("tvWindow", LOGINFO, false, "C:/Users/colin/Desktop");

  string param;

  int numArgs;
  auto args = CommandLineToArgvW (GetCommandLineW(), &numArgs);
  if (numArgs > 1) {
    // get fileName from commandLine
    wstring wstr (args[1]);
    param = string (wstr.begin(), wstr.end());
    }

  cAppWindow appWindow;
  appWindow.run ("tvWindow", 1920/2, 1080/2, param);

  CoUninitialize();
  }
//}}}
