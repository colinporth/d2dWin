// tvWindow.cpp
//{{{  includes
#include "stdafx.h"

#include <stdio.h>

#include "../../shared/utils/cWinAudio.h"
#include "../../shared/dvb/cDumpTransportStream.h"

#include "../common/cDvb.h"
#include "../common/cVidFrame.h"
#include "../common/cAudFrame.h"

#include "../common/box/cTsEpgBox.h"
#include "../common/box/cToggleBox.h"
#include "../common/box/cIntBox.h"
#include "../common/box/cValueBox.h"
#include "../common/box/cLogBox.h"
#include "../common/box/cWindowBox.h"
#include "../common/box/cAudFrameBox.h"
#include "../common/box/cListBox.h"

extern "C" {
  #include <libavcodec/avcodec.h>
  #include <libavformat/avformat.h>
  }
#pragma comment (lib,"avutil.lib")
#pragma comment (lib,"avcodec.lib")
#pragma comment (lib,"avformat.lib")

#include "mfxvideo++.h"
#ifdef _DEBUG
  #pragma comment (lib,"libmfx_d.lib")
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

const int kChunkSize = 2048*188;
//}}}

class cAppWindow : public cD2dWindow, public cWinAudio {
public:
  //{{{
  void run (string title, int width, int height, const string& param) {

    // init d2dWindow, boxes
    initialise (title, width, height, false);
    add (new cLogBox (this, 200.f,0.f, true), 0.f,0.f);
    //add (new cFramesDebugBox (this, 0.f,getHeight()/4.f), 0.f,kTextHeight);

    // launch file player
    thread threadHandle;
    int frequency = atoi (param.c_str());
    if (param.empty() || frequency) {
      updateFileList();
      cLog::log (LOGNOTICE, "getFiles %d files", mFileList.size());
      add (new cListBox (this, getWidth()/2.f, 0.f, mFileList, mFileIndex, mFileIndexChanged), 0.f,kTextHeight);
      threadHandle = thread ([=](){ filesPlayerThread(); });
      }
    else
      threadHandle = thread ([=](){ filePlayerThread (param); });
    SetThreadPriority (threadHandle.native_handle(), THREAD_PRIORITY_BELOW_NORMAL);
    threadHandle.detach();

    if (frequency) {
      // launch dvb
      mDvb = new cDvb();
      if (mDvb->createGraph (frequency * 1000)) {
        mDvbTs = new cDumpTransportStream ("C:/tv", false);

        add (new cIntBgndBox (this, 120.f, kTextHeight, "signal ", mSignal), 0.f,0.f);
        add (new cUInt64BgndBox (this, 120.f, kTextHeight, "pkt ", mDvbTs->mPackets), 122.f,0.f);
        add (new cUInt64BgndBox (this, 120.f, kTextHeight, "dis ", mDvbTs->mDiscontinuity), 224.f,0.f);
        add (new cTsEpgBox (this, getWidth()/2.f,0.f, mDvbTs), getWidth()/2.f,kTextHeight);

        thread ([=]() { dvbGrabThread (mDvb); }).detach();
        thread ([=]() { dvbSignalThread (mDvb); }).detach();
        }
      }

    // exit, maximise box
    add (new cWindowBox (this, 60.f,24.f), -60.f,0.f);

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

      case  ' ': mPlayContext.togglePlay(); break;

      case 0x24: mPlayContext.setPlayPts (0); changed(); break; // home
      case 0x23: mPlayContext.setPlayPts (mPlayContext.mAnalTs->getLengthPts()); changed(); break; // end
      case 0x21: mPlayContext.incPlayPts (-90000*10); changed(); break; // page up
      case 0x22: mPlayContext.incPlayPts (90000*10); changed(); break; // page down
      case 0x25: mPlayContext.incPlayPts (-90000/25); mPlayContext.mPlaying = ePause; changed(); break; // left arrow
      case 0x27: mPlayContext.incPlayPts (90000/25); mPlayContext.mPlaying = ePause;  changed();break; // right arrow
      case 0x26: // up arrow
        //{{{  prev file
        updateFileList();
        if (!mFileList.empty() && (mFileIndex > 0)) {
          mFileIndex = mFileIndex--;
          changed();
          }
        break;
        //}}}
      case 0x28: // down arrow
        //{{{  next file
        updateFileList();
        if (!mFileList.empty() && mFileIndex < mFileList.size()-1) {
          mFileIndex = mFileIndex++;
          changed();
          }
        break;
        //}}}
      case 0x0d: // enter
        //{{{  enter
        if (!mFileList.empty()) {
          cLog::log (LOGINFO, "enter key");
          mFileIndexChanged = true;
          }
        break;
        //}}}

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
        auto service = mPlayContext.mAnalTs->getService (key - '1');
        if (service) {
          mPlayContext.mAudTs->setPid (service->getAudPid());
          mPlayContext.mVidTs->setPid (service->getVidPid());
          changed();
          }
        break;
        }

      case  'F': toggleFullScreen(); break;

      default: cLog::log (LOGINFO, "key %x", key);
      }

    return false;
    }
  //}}}

private:
  enum ePlaying { ePause, eScrub, ePlay };
  //{{{
  class cAnalTransportStream : public cTransportStream {
  public:
    cAnalTransportStream () {}
    //{{{
    virtual ~cAnalTransportStream() {
      mBasePts = -1;
      mLengthPid = -1;
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
      clearCounts();
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
    concurrent_vector<cStreamPos> mStreamPosVector;
    };
  //}}}
  //{{{
  class cDecodeTransportStream : public cTransportStream {
  public:
    //{{{
    virtual ~cDecodeTransportStream() {
      mFrames.clear();
      mPid = -1;
      mLastLoadedPts = -1;
      mLoadFrame = 0;
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

    int mPid = -1;
    int64_t mLastLoadedPts = -1;
    int mLoadFrame = 0;

    concurrent_vector<iFrame*> mFrames;
    };
  //}}}
  //{{{
  class cAudTransportStream : public cDecodeTransportStream {
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
            auto wstr = to_wstring (index);
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

              auto frame = (cAudFrame*)mFrames[mLoadFrame];
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
    AVCodec* mAudCodec = nullptr;
    AVCodecContext* mAudContext = nullptr;
    AVCodecParserContext* mAudParser = nullptr;
    };
  //}}}
  //{{{
  class cVidTransportStream : public cDecodeTransportStream {
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
        auto wstr (to_wstring (index));
        dc->DrawText (wstr.data(), (uint32_t)wstr.size(), textFormat,
                      RectF(x,y1, x + vidFrameWidth - 1.f,y1 + kIndexHeight), black);
        y1 += kIndexHeight + 1.f;

        maxY = max (maxY, y1);
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
  class cPlayContext {
  public:
    //{{{
    cPlayContext() :
      mFirstVidPtsSem("firstVidPts"), mPlayStoppedSem("playStopped"),
      mAudStoppedSem("audStopped"), mVidStoppedSem("vidStopped") {}
    //}}}

    // gets
    ePlaying getPlaying() { return mPlaying; }
    int64_t getPlayPts() { return mPlayPts + mAnalTs->getBasePts(); }
    int64_t getLengthPts() { return mAnalTs->getLengthPts(); }
    float getStreamPosFrac() { return float(mStreamPos) / float(mStreamSize); }

    // sets
    void setPlaying (ePlaying playing) { mPlaying = playing; }
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
    void setPlayPts (int64_t playPts) {
    // mPlayPts is offset from mBasePts

      if (playPts < 0)
        mPlayPts = 0;
      else if (playPts > getLengthPts())
        mPlayPts = getLengthPts();
      else
        mPlayPts = playPts;

      mPlayPtsSem.notifyAll();
      }
    //}}}
    void incPlayPts (int64_t incPts) { setPlayPts (mPlayPts + incPts); }

    // vars
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
    cSemaphore mPlayStoppedSem;
    cSemaphore mAudStoppedSem;
    cSemaphore mVidStoppedSem;
    };
  //}}}

  //{{{
  class cVidFrameView : public cView {
  public:
    //{{{
    cVidFrameView (cD2dWindow* window, float width, float height, cPlayContext* playContext)
        : cView("vidFrame", window, width, height), mPlayContext(playContext) {
      mPin = true;
      }
    //}}}
    virtual ~cVidFrameView() {}

    //{{{
    cPoint getSrcSize() {
      return mPlayContext->mBitmap ? mPlayContext->mBitmap->GetPixelSize() : cPoint();
      }
    //}}}
    //{{{
    bool onWheel (int delta, cPoint pos) {

      if (mWindow->getControl())
        return cView::onWheel (delta, pos);
      else {
        mPlayContext->mPlaying = ePause;
        mPlayContext->incPlayPts (-int64_t(delta * (90000/25) / 120));
        mWindow->changed();
        }

      return true;
      }
    //}}}
    //{{{
    bool onMove (bool right, cPoint pos, cPoint inc) {

      if (mWindow->getControl())
        return cView::onMove (right, pos, inc);
      else {
        mPlayContext->mPlaying = ePause;
        mPlayContext->incPlayPts (int64_t (-inc.x * kAudSamplesPerAacFrame * 48 / 90 / 8));
        mWindow->changed();
        return true;
        }
      }
    //}}}
    //{{{
    bool onUp (bool right, bool mouseMoved, cPoint pos) {

      if (!mouseMoved)
        mPlayContext->togglePlay();

      return false;
      }
    //}}}

    //{{{
    void onDraw (ID2D1DeviceContext* dc) {

      auto vidTs = mPlayContext->mVidTs;
      //if (vidTs) {
        auto vidframe = (cVidFrame*)vidTs->findNearestFrame (mPlayContext->getPlayPts());
        if (vidframe && (vidframe->getPts() != mPlayContext->mBitmapPts)) {
          mPlayContext->mBitmap = vidframe->makeBitmap (dc, mPlayContext->mBitmap);
          mPlayContext->mBitmapPts = vidframe->getPts();
          }

        dc->SetTransform (mView2d.mTransform);
        if (mPlayContext->mBitmap)
          dc->DrawBitmap (mPlayContext->mBitmap, cRect(mWindow->getSize()));

        dc->SetTransform (Matrix3x2F::Identity());
      //  }
      }
    //}}}

  private:
    cPlayContext* mPlayContext;
    };
  //}}}
  //{{{
  class cFramesDebugBox : public cBox {
  public:
    //{{{
    cFramesDebugBox (cD2dWindow* window, float width, float height, cPlayContext* playContext)
        : cBox("frames", window, width, height), mPlayContext(playContext) {

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

      mPlayContext->incPlayPts (-int64_t(delta * ptsInc / 120));
      mPlayContext->setPlaying (cAppWindow::ePause);
      mWindow->changed();
      return true;
      }
    //}}}
    //{{{
    bool onMove (bool right, cPoint pos, cPoint inc) {

      mPlayContext->setPlaying (cAppWindow::ePause);
      auto pixPerPts = 18.f * 48 / (kAudSamplesPerAacFrame * 90);
      mPlayContext->incPlayPts (int64_t (-inc.x / pixPerPts));
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
        dc->FillRectangle (mRect, mBrush);

      float maxY = 0;

      mPlayContext->mAudTs->drawFrames (dc, mRect, mTextFormat,
        mWindow->getWhiteBrush(), mWindow->getBlueBrush(), mWindow->getBlackBrush(), mWindow->getYellowBrush(),
        mPlayContext->getPlayPts(), maxY);

      mPlayContext->mVidTs->drawFrames (dc, mRect, mTextFormat,
        mWindow->getWhiteBrush(), mWindow->getBlueBrush(), mWindow->getBlackBrush(), mWindow->getYellowBrush(),
        mPlayContext->getPlayPts(), maxY);

      dc->FillRectangle (RectF((getWidth()/2)-1, 0, (getWidth()/2)+1, maxY),
                         mPin ? mWindow->getYellowBrush() : mWindow->getGreyBrush());

      mLayoutHeight = maxY;
      layout();
      }
    //}}}

  private:
    cPlayContext* mPlayContext;

    IDWriteTextFormat* mTextFormat = nullptr;
    ID2D1SolidColorBrush* mBrush = nullptr;
    };
  //}}}
  //{{{
  class cProgressBox : public cBox {
  public:
    //{{{
    cProgressBox (cD2dWindow* window, float width, float height, cPlayContext* playContext)
        : cBox("progress", window, width, height), mPlayContext(playContext) {
      mPin = true;
      }
    //}}}
    virtual ~cProgressBox() {}

    //{{{
    bool onDown (bool right, cPoint pos)  {

      auto appWindow = dynamic_cast<cAppWindow*>(mWindow);

      mPlayContext->setPlaying (cAppWindow::eScrub);
      mPlayContext->setPlayPts (int64_t ((pos.x / getWidth()) * mPlayContext->getLengthPts()));
      mWindow->changed();

      return true;
      }
    //}}}
    //{{{
    bool onMove (bool right, cPoint pos, cPoint inc) {

      auto appWindow = dynamic_cast<cAppWindow*>(mWindow);

      mPlayContext->setPlayPts (int64_t ((pos.x / getWidth()) * mPlayContext->getLengthPts()));
      mWindow->changed();
      return true;
      }
    //}}}
    //{{{
    bool onUp (bool right, bool mouseMoved, cPoint pos) {

      auto appWindow = dynamic_cast<cAppWindow*>(mWindow);

      mPlayContext->setPlaying (cAppWindow::ePause);
      return true;
      }
    //}}}
    //{{{
    void onDraw (ID2D1DeviceContext* dc) {

      auto appWindow = dynamic_cast<cAppWindow*>(mWindow);

      const float ylen = 2.f;

      // draw analPos blueBar
      auto r = mRect;
      r.left = getWidth() * mPlayContext->getStreamPosFrac();
      r.top = r.bottom - 3*ylen;
      dc->FillRectangle (r, mWindow->getBlueBrush());

      // draw vidLastPts yellowBar
      r.left = mRect.left;
      r.right = getWidth() * (float)(mPlayContext->mVidTs->getLastLoadedPts() - mPlayContext->mAnalTs->getBasePts())
                                     / (float)mPlayContext->getLengthPts();
      r.top = r.bottom - ylen;
      dc->FillRectangle (r, mPick ? mWindow->getYellowBrush() :  mWindow->getGreyBrush());

      // draw audLastPts yellowBar
      r.bottom = r.top;
      r.top -= ylen;
      r.right = getWidth() * (float)(mPlayContext->mAudTs->getLastLoadedPts() - mPlayContext->mAnalTs->getBasePts())
                                    / (float)mPlayContext->getLengthPts();
      dc->FillRectangle (r, mPick ? mWindow->getYellowBrush() :  mWindow->getGreyBrush());

      // draw playPts yellowBar
      r.bottom = r.top;
      r.top -= ylen;
      r.right = getWidth() * (float)(mPlayContext->getPlayPts() - mPlayContext->mAnalTs->getBasePts())
                                    / (float)mPlayContext->getLengthPts();
      dc->FillRectangle (r, mPick ? mWindow->getYellowBrush() :  mWindow->getGreyBrush());
      }
    //}}}

  private:
    cPlayContext* mPlayContext;
    };
  //}}}
  //{{{
  class cTimecodeBox : public cBox {
  public:
    //{{{
    cTimecodeBox (cD2dWindow* window, float width, float height, cPlayContext* playContext)
        : cBox("timecode", window, width, height), mPlayContext(playContext) {

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

      string str = getPtsString(mPlayContext->mPlayPts) + " " + getPtsString(mPlayContext->mAnalTs->mLengthPts);
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
    cPlayContext* mPlayContext;

    IDWriteTextFormat* mTextFormat = nullptr;
    ID2D1SolidColorBrush* mBrush = nullptr;
    };
  //}}}

  //{{{
  void updateFileList() {
    mFileList = getFiles ("C:/tv", "*.ts");
    }
  //}}}
  //{{{
  void playFile (const string& fileName, cPlayContext* playContext) {

    //{{{  create transportStreams
    playContext->mAnalTs = new cAnalTransportStream();
    playContext->mAudTs = new cAudTransportStream (kAudMaxFrames);
    playContext->mVidTs = new cVidTransportStream (kVidMaxFrames);
    //}}}
    //{{{  create boxes
    auto vidFrameView = addFront (new cVidFrameView (this, 0.f,0.f, playContext));
    auto timecodeBox = add (new cTimecodeBox (this, 600.f,60.f, playContext), -600.f,-60.f);
    timecodeBox->setPin (true);
    auto progressBox = add (new cProgressBox (this, 0.f,6.f, playContext), 0.f,-6.f);
    auto audFrameBox = add (new cAudFrameBox (this, 82.f,240.0f, playContext->mPlayAudFrame), -84.f,-240.f-6.0f);
    //}}}
    //{{{  launch threads
    thread ([=]() { audThread (fileName, playContext); }).detach();
    thread ([=]() { vidThread (fileName, playContext); }).detach();
    auto threadHandle = thread ([=](){ playThread (playContext); });
    SetThreadPriority (threadHandle.native_handle(), THREAD_PRIORITY_HIGHEST);
    threadHandle.detach();
    //}}}

    const auto fileChunkBuffer = (uint8_t*)malloc (kChunkSize);
    auto file = _open (fileName.c_str(), _O_RDONLY | _O_BINARY);
    //{{{  get streamSize
    struct _stat64 buf;
    _fstat64 (file, &buf);
    playContext->mStreamSize = buf.st_size ;
    //}}}
    //{{{  parse front of stream until first service
    int64_t streamPos = 0;

    cService* service = nullptr;
    while (!service) {
      auto chunkBytesLeft = _read (file, fileChunkBuffer, kChunkSize);
      auto chunkPtr = fileChunkBuffer;
      while (chunkBytesLeft >= 188) {
        auto bytesUsed = playContext->mAnalTs->demux (chunkPtr, chunkBytesLeft, streamPos, false, -1);
        streamPos += bytesUsed;
        chunkPtr += bytesUsed;
        chunkBytesLeft -= (int)bytesUsed;
        }

      service = playContext->mAnalTs->getService (0);
      }

    // select first service
    playContext->mVidTs->setPid (service->getVidPid());
    playContext->mAudTs->setPid (service->getAudPid());
    cLog::log (LOGNOTICE, "first service - vidPid:" + dec(service->getVidPid()) +
                          " audPid:" + dec(service->getAudPid()));
    playContext->mAnalTs->clearPosCounts();
    //}}}
    //{{{  parse end of stream for initial lastPts
    streamPos = playContext->mStreamSize - (kChunkSize * 8);
    _lseeki64 (file, streamPos, SEEK_SET);

    while (streamPos < playContext->mStreamSize) {
      auto chunkBytesLeft = _read (file, fileChunkBuffer, kChunkSize);
      auto chunkPtr = fileChunkBuffer;
      while (chunkBytesLeft >= 188) {
        auto bytesUsed = playContext->mAnalTs->demux (chunkPtr, chunkBytesLeft, streamPos, chunkPtr == fileChunkBuffer, -1);
        streamPos += bytesUsed;
        chunkPtr += bytesUsed;
        chunkBytesLeft -= (int)bytesUsed;
        }
      }

    playContext->mAnalTs->clearPosCounts();
    //}}}

    // analyse stream from start, chase tail, update progress bar
    playContext->mStreamPos = 0;
    _lseeki64 (file, playContext->mStreamPos, SEEK_SET);

    int sameCount = 0;
    int firstVidSignalCount = 0;
    while (!mFileIndexChanged && (sameCount < 10)) {
      int64_t bytesToRead = playContext->mStreamSize - playContext->mStreamPos;
      if (bytesToRead > kChunkSize) // trim to kChunkSize
        bytesToRead = kChunkSize;
      if (bytesToRead >= 188) { // at least 1 packet to read, trim read to packet boundary
        bytesToRead -= bytesToRead % 188;
        auto chunkBytesLeft = _read (file, fileChunkBuffer, (unsigned int)bytesToRead);
        if (chunkBytesLeft >= 188) {
          auto chunkPtr = fileChunkBuffer;
          while (chunkBytesLeft >= 188) {
            auto bytesUsed = playContext->mAnalTs->demux (chunkPtr, chunkBytesLeft, playContext->mStreamPos, playContext->mStreamPos == 0, -1);
            playContext->mStreamPos += bytesUsed;
            chunkPtr += bytesUsed;
            chunkBytesLeft -= (int)bytesUsed;
            changed();

            if ((firstVidSignalCount < 3) &&
                (playContext->mAnalTs->getFirstPts (service->getAudPid()) != -1) &&
                (playContext->mAnalTs->getFirstPts (service->getVidPid()) != -1)) {
              firstVidSignalCount++;
              playContext->mFirstVidPtsSem.notifyAll();
              cLog::log (LOGERROR, "mFirstVidPtsSem.notifyAll");
              }
            }
          }
        }
      else {
        //{{{  check fileSize changed
        while (!mFileIndexChanged) {
          Sleep (1000);
          _fstat64 (file, &buf);
          if (buf.st_size > playContext->mStreamSize) {
            cLog::log (LOGINFO, "fileSize now " + dec(playContext->mStreamSize));
            playContext->mStreamSize = buf.st_size;
            sameCount = 0;
            break;
            }
          else {
            sameCount++;
            if (sameCount >= 10) {
              cLog::log (LOGINFO, "fileSize sameCount expired " + dec(sameCount));
              break;
              }
            else
              cLog::log (LOGINFO, "fileSize same " + dec(sameCount));
            }
          }
        }
        //}}}
      }
    _close (file);
    free (fileChunkBuffer);

    // analyse finished, wait for fileChange or exit
    while (!mFileIndexChanged)
      Sleep (10);
    //{{{  wait for threads to close down
    playContext->mPlayStoppedSem.wait();
    playContext->mPlayPtsSem.notifyAll();
    playContext->mPlayPtsSem.notifyAll();
    playContext->mAudStoppedSem.wait();
    playContext->mVidStoppedSem.wait();
    //}}}
    //{{{  remove boxes
    removeBox (timecodeBox);
    removeBox (progressBox);
    removeBox (audFrameBox);
    removeBox (vidFrameView);
    //}}}
    //{{{  delete resources
    delete playContext->mAudTs;
    delete playContext->mVidTs;
    delete playContext->mAnalTs;
    //}}}
    }
  //}}}

  //{{{
  void filePlayerThread (const string& fileName) {

    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::setThreadName ("file");

    playFile (fileName, &mPlayContext);

    cLog::log (LOGNOTICE, "exit");
    CoUninitialize();
    }
  //}}}
  //{{{
  void filesPlayerThread() {

    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::setThreadName ("fils");

    while (true) {
      mFileIndexChanged = false;
      playFile (mFileList[mFileIndex], &mPlayContext);
      if (!mFileIndexChanged) {
        if (mFileIndex < mFileList.size()-1) // play next file
          mFileIndex++;
        else
          break;
        }
      }

    cLog::log (LOGNOTICE, "exit");
    CoUninitialize();
    }
  //}}}
  //{{{
  void audThread (const string& fileName, cPlayContext* playContext) {

    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::setThreadName ("aud ");
    av_register_all();

    const auto fileChunkBuffer = (uint8_t*)malloc (kChunkSize);
    playContext->mFirstVidPtsSem.wait();

    bool skip = false;
    int64_t lastJumpStreamPos = -1;

    int64_t streamPos = 0;
    auto file = _open (fileName.c_str(), _O_RDONLY | _O_BINARY);
    while (!mFileIndexChanged) {
      auto chunkPtr = fileChunkBuffer;
      auto chunkBytesLeft = _read (file, fileChunkBuffer, kChunkSize);
      if (chunkBytesLeft < 188) {
        // end of file
        while (!mFileIndexChanged && playContext->mAudTs->isLoaded (playContext->getPlayPts(), 1))
          playContext->mPlayPtsSem.wait();
        //{{{  jump to pts in stream
        auto jumpStreamPos = playContext->mAnalTs->findStreamPos (playContext->mAudTs->getPid(), playContext->getPlayPts());
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
          auto bytesUsed = playContext->mAudTs->demux (chunkPtr, chunkBytesLeft, streamPos, skip, playContext->mAudTs->getPid());
          streamPos += bytesUsed;
          chunkPtr += bytesUsed;
          chunkBytesLeft -= (int)bytesUsed;
          skip = false;
          //}}}
          changed();

          while (!mFileIndexChanged && playContext->mAudTs->isLoaded (playContext->getPlayPts(), kAudMaxFrames/2))
            playContext->mPlayPtsSem.wait();

          bool loaded = playContext->mAudTs->isLoaded (playContext->getPlayPts(), 1);
          if (!loaded || (playContext->getPlayPts() > playContext->mAudTs->getLastLoadedPts() + 100000)) {
            auto jumpStreamPos = playContext->mAnalTs->findStreamPos (playContext->mAudTs->getPid(), playContext->getPlayPts());
            if (jumpStreamPos != lastJumpStreamPos) {
              //{{{  jump to jumpStreamPos, unless same as last, wait for rest of GOP or chunk to demux
              cLog::log (LOGINFO, "jump playPts:" + getPtsString(playContext->getPlayPts()) +
                         (loaded ? " after ":" notLoaded ") + getPtsString(playContext->mAudTs->getLastLoadedPts()));

              _lseeki64 (file, jumpStreamPos, SEEK_SET);

              streamPos = jumpStreamPos;
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
    free (fileChunkBuffer);
    playContext->mAudStoppedSem.notifyAll();

    cLog::log (LOGNOTICE, "exit");
    CoUninitialize();
    }
  //}}}
  //{{{
  void vidThread (const string& fileName, cPlayContext* playContext) {

    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::setThreadName ("vid ");

    const auto fileChunkBuffer = (uint8_t*)malloc (kChunkSize);
    playContext->mFirstVidPtsSem.wait();

    bool skip = false;
    int64_t lastJumpStreamPos = -1;

    int64_t streamPos = 0;
    auto file = _open (fileName.c_str(), _O_RDONLY | _O_BINARY);
    while (!mFileIndexChanged) {
      auto chunkPtr = fileChunkBuffer;
      auto chunkBytesLeft = _read (file, fileChunkBuffer, kChunkSize);
      if (chunkBytesLeft < 188) {
        // end of file
        while (!mFileIndexChanged && playContext->mVidTs->isLoaded (playContext->getPlayPts(), 1))
          playContext->mPlayPtsSem.wait();
        //{{{  jump to pts in stream
        auto jumpStreamPos = playContext->mAnalTs->findStreamPos (playContext->mVidTs->getPid(), playContext->getPlayPts());
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
          auto bytesUsed = playContext->mVidTs->demux (chunkPtr, chunkBytesLeft, streamPos, skip, playContext->mVidTs->getPid());
          streamPos += bytesUsed;
          chunkPtr += bytesUsed;
          chunkBytesLeft -= (int)bytesUsed;
          skip = false;
          //}}}
          changed();

          while (!mFileIndexChanged && playContext->mVidTs->isLoaded (playContext->getPlayPts(), 4))
            playContext->mPlayPtsSem.wait();

          bool loaded = playContext->mVidTs->isLoaded (playContext->getPlayPts(), 1);
          if (!loaded || (playContext->getPlayPts() > playContext->mVidTs->getLastLoadedPts() + 100000)) {
            auto jumpStreamPos = playContext->mAnalTs->findStreamPos (playContext->mVidTs->getPid(), playContext->getPlayPts());
            if (jumpStreamPos != lastJumpStreamPos) {
              //{{{  jump to jumpStreamPos, unless same as last, wait for rest of GOP or chunk to demux
              cLog::log (LOGINFO, "jump playPts:" + getPtsString(playContext->getPlayPts()) +
                         (loaded ? " after ":" notLoaded ") + getPtsString(playContext->mVidTs->getLastLoadedPts()));

              _lseeki64 (file, jumpStreamPos, SEEK_SET);

              streamPos = jumpStreamPos;
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
    free (fileChunkBuffer);
    playContext->mVidStoppedSem.notifyAll();

    cLog::log (LOGNOTICE, "exit");
    CoUninitialize();
    }
  //}}}
  //{{{
  void playThread (cPlayContext* playContext) {

    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::setThreadName ("play");

    audOpen (2, 48000);

    playContext->mFirstVidPtsSem.wait();
    playContext->mPlayPts = playContext->mAnalTs->getFirstPts (playContext->mVidTs->getPid()) - playContext->mAnalTs->getBasePts();

    while (!mFileIndexChanged) {
      auto pts = playContext->getPlayPts();
      playContext->mPlayAudFrame = (cAudFrame*)playContext->mAudTs->findFrame (pts);

      // play using frame where available, else play silence
      audPlay (playContext->mPlayAudFrame ? playContext->mPlayAudFrame->mChannels : getSrcChannels(),
        playContext->mPlayAudFrame && (playContext->mPlaying != ePause) ? playContext->mPlayAudFrame->mSamples : nullptr,
        playContext->mPlayAudFrame ? playContext->mPlayAudFrame->mNumSamples : kAudSamplesPerUnknownFrame,
               1.f);

      if ((playContext->mPlaying == ePlay) && (playContext->mPlayPts < playContext->getLengthPts()))
        playContext->incPlayPts (int64_t (((playContext->mPlayAudFrame ? playContext->mPlayAudFrame->mNumSamples : kAudSamplesPerUnknownFrame) * 90) / 48));
      if (playContext->mPlayPts > playContext->getLengthPts())
        playContext->mPlayPts = playContext->getLengthPts();
      changed();

      playContext->mPlayPtsSem.notifyAll();
      }

    audClose();
    playContext->mPlayStoppedSem.notifyAll();

    cLog::log (LOGNOTICE, "exit");
    CoUninitialize();
    }
  //}}}

  //{{{
  void dvbGrabThread (cDvb* dvb) {

    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::setThreadName ("grab");

    dvb->run();

    int64_t streamPos = 0;
    auto blockSize = 0;
    while (true) {
      auto ptr = dvb->getBlock (blockSize);
      if (blockSize) {
        streamPos += mDvbTs->demux (ptr, blockSize, streamPos, false, -1);
        dvb->releaseBlock (blockSize);
        changed();
        }
      else
        Sleep (1);
      }

    cLog::log (LOGNOTICE, "exit");
    CoUninitialize();
    }
  //}}}
  //{{{
  void dvbSignalThread (cDvb* dvb) {

    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::setThreadName ("sign");

    while (true) {
      mSignal = dvb->getSignal();
      Sleep (100);
      }

    cLog::log (LOGNOTICE, "exit");
    CoUninitialize();
    }
  //}}}

  //{{{  vars
  cPlayContext mPlayContext;

  vector <string> mFileList;
  int mFileIndex = 0;
  bool mFileIndexChanged = false;

  cDumpTransportStream* mDvbTs;
  cDvb* mDvb = nullptr;
  int mSignal = 0;
  //}}}
  };

//{{{
int __stdcall WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);

  cLog::init (LOGINFO, true);
  //cLog::init (LOGINFO, false, "C:/Users/colin/Desktop");

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
