 // hlsWindow.cpp
//{{{  includes
#include "stdafx.h"
#include <shellapi.h>

#include "../../shared/utils/cSong.h"

#include "../../shared/decoders/cAudioDecode.h"
#include "../../shared/audio/audioWASAPI.h"

#include "../../shared/net/cWinSockHttp.h"

#include "../common/cD2dWindow.h"
#include "../boxes/cClockBox.h"
#include "../boxes/cSongBox.h"
#include "../boxes/cLogBox.h"
#include "../boxes/cWindowBox.h"

#include "../mfx/include/mfxvideo++.h"
#ifdef _DEBUG
  #pragma comment (lib,"libmfx_d.lib")
#else
  #pragma comment (lib,"libmfx.lib")
#endif

using namespace std;
using namespace chrono;
//}}}
constexpr int kDefaultChannelNum = 3;
constexpr int kAudBitrate = 128000; //  96000  128000
constexpr int kVidBitrate = 827008; // 827008 1604032 2812032 5070016

const string kHost = "vs-hls-uk-live.akamaized.net";
const vector <string> kChannels = { "bbc_one_hd",          "bbc_two_hd",          "bbc_four_hd", // pa4
                                    "bbc_news_channel_hd", "bbc_one_scotland_hd", "s4cpbs",      // pa4
                                    "bbc_one_south_west",  "bbc_parliament" };                   // pa3

//{{{
class cVideoDecode {
public:
  //{{{
  class cFrame {
  public:
    cFrame (uint64_t pts) : mPts(pts), mOk(false) {}
    //{{{
    virtual ~cFrame() {
      _aligned_free (mYbuf);
      _aligned_free (mUbuf);
      _aligned_free (mVbuf);
      _aligned_free (mBgra);
      }
    //}}}

    bool ok() { return mOk; }
    uint64_t getPts() { return mPts; }

    int getWidth() { return mWidth; }
    int getHeight() { return mHeight; }
    uint32_t* getBgra() { return mBgra; }

    //{{{
    void set (uint64_t pts) {
      mOk = false;
      mPts = pts;
      }
    //}}}
    //{{{
    void setNv12 (uint8_t* buffer, int width, int height, int stride) {

      mOk = false;
      mYStride = stride;
      mUVStride = stride/2;

      mWidth = width;
      mHeight = height;

      // copy all of nv12 to yBuf
      mYbuf = (uint8_t*)_aligned_realloc (mYbuf, height * mYStride * 3 / 2, 128);
      memcpy (mYbuf, buffer, height * mYStride * 3 / 2);

      // unpack nv12 to planar uv
      mUbuf = (uint8_t*)_aligned_realloc (mUbuf, (mHeight/2) * mUVStride, 128);
      mVbuf = (uint8_t*)_aligned_realloc (mVbuf, (mHeight/2) * mUVStride, 128);

      uint8_t* uv = mYbuf + (mHeight * mYStride);
      uint8_t* u = mUbuf;
      uint8_t* v = mVbuf;
      for (int i = 0; i < mHeight/2 * mUVStride; i++) {
        *u++ = *uv++;
        *v++ = *uv++;
        }

      int argbStride = mWidth;
      mBgra = (uint32_t*)_aligned_realloc (mBgra, mWidth * 4 * mHeight, 128);

      __m128i ysub  = _mm_set1_epi32 (0x00100010);
      __m128i uvsub = _mm_set1_epi32 (0x00800080);
      __m128i facy  = _mm_set1_epi32 (0x004a004a);
      __m128i facrv = _mm_set1_epi32 (0x00660066);
      __m128i facgu = _mm_set1_epi32 (0x00190019);
      __m128i facgv = _mm_set1_epi32 (0x00340034);
      __m128i facbu = _mm_set1_epi32 (0x00810081);
      __m128i zero  = _mm_set1_epi32 (0x00000000);

      for (int y = 0; y < mHeight; y += 2) {
        __m128i* srcy128r0 = (__m128i*)(mYbuf + mYStride*y);
        __m128i* srcy128r1 = (__m128i*)(mYbuf + mYStride*y + mYStride);
        __m64* srcu64 = (__m64*)(mUbuf + mUVStride * (y/2));
        __m64* srcv64 = (__m64*)(mVbuf + mUVStride * (y/2));
        __m128i* dstrgb128r0 = (__m128i*)(mBgra + argbStride * y);
        __m128i* dstrgb128r1 = (__m128i*)(mBgra + argbStride * y + argbStride);

        for (int x = 0; x < mWidth; x += 16) {
          __m128i u0 = _mm_loadl_epi64 ((__m128i *)srcu64 );
          srcu64++;
          __m128i v0 = _mm_loadl_epi64 ((__m128i *)srcv64 );
          srcv64++;
          __m128i y0r0 = _mm_load_si128 (srcy128r0++);
          __m128i y0r1 = _mm_load_si128 (srcy128r1++);
          //{{{  constant y factors
          __m128i y00r0 = _mm_mullo_epi16 (_mm_sub_epi16 (_mm_unpacklo_epi8 (y0r0, zero), ysub), facy);
          __m128i y01r0 = _mm_mullo_epi16 (_mm_sub_epi16 (_mm_unpackhi_epi8 (y0r0, zero), ysub), facy);
          __m128i y00r1 = _mm_mullo_epi16 (_mm_sub_epi16 (_mm_unpacklo_epi8 (y0r1, zero), ysub), facy);
          __m128i y01r1 = _mm_mullo_epi16 (_mm_sub_epi16 (_mm_unpackhi_epi8 (y0r1, zero), ysub), facy);
          //}}}
          //{{{  expand u and v so they're aligned with y values
          u0 = _mm_unpacklo_epi8 (u0, zero);
          __m128i u00 = _mm_sub_epi16 (_mm_unpacklo_epi16 (u0, u0), uvsub);
          __m128i u01 = _mm_sub_epi16 (_mm_unpackhi_epi16 (u0, u0), uvsub);

          v0 = _mm_unpacklo_epi8( v0,  zero );
          __m128i v00 = _mm_sub_epi16 (_mm_unpacklo_epi16 (v0, v0), uvsub);
          __m128i v01 = _mm_sub_epi16 (_mm_unpackhi_epi16 (v0, v0), uvsub);
          //}}}
          //{{{  common factors on both rows.
          __m128i rv00 = _mm_mullo_epi16 (facrv, v00);
          __m128i rv01 = _mm_mullo_epi16 (facrv, v01);
          __m128i gu00 = _mm_mullo_epi16 (facgu, u00);
          __m128i gu01 = _mm_mullo_epi16 (facgu, u01);
          __m128i gv00 = _mm_mullo_epi16 (facgv, v00);
          __m128i gv01 = _mm_mullo_epi16 (facgv, v01);
          __m128i bu00 = _mm_mullo_epi16 (facbu, u00);
          __m128i bu01 = _mm_mullo_epi16 (facbu, u01);
          //}}}
          //{{{  row 0
          __m128i r00 = _mm_srai_epi16 (_mm_add_epi16 (y00r0, rv00), 6);
          __m128i r01 = _mm_srai_epi16 (_mm_add_epi16 (y01r0, rv01), 6);
          __m128i g00 = _mm_srai_epi16 (_mm_sub_epi16 (_mm_sub_epi16 (y00r0, gu00), gv00), 6);
          __m128i g01 = _mm_srai_epi16 (_mm_sub_epi16 (_mm_sub_epi16 (y01r0, gu01), gv01), 6);
          __m128i b00 = _mm_srai_epi16 (_mm_add_epi16 (y00r0, bu00), 6);
          __m128i b01 = _mm_srai_epi16 (_mm_add_epi16 (y01r0, bu01), 6);

          r00 = _mm_packus_epi16 (r00, r01);                // rrrr.. saturated
          g00 = _mm_packus_epi16 (g00, g01);                // gggg.. saturated
          b00 = _mm_packus_epi16 (b00, b01);                // bbbb.. saturated

          r01 = _mm_unpacklo_epi8 (r00, zero);              // 0r0r..
          __m128i gbgb    = _mm_unpacklo_epi8 (b00, g00);   // gbgb..
          __m128i rgb0123 = _mm_unpacklo_epi16 (gbgb, r01); // 0rgb0rgb..
          __m128i rgb4567 = _mm_unpackhi_epi16 (gbgb, r01); // 0rgb0rgb..

          r01 = _mm_unpackhi_epi8 (r00, zero);
          gbgb = _mm_unpackhi_epi8 (b00, g00);
          __m128i rgb89ab = _mm_unpacklo_epi16 (gbgb, r01);
          __m128i rgbcdef = _mm_unpackhi_epi16 (gbgb, r01);

          _mm_stream_si128 (dstrgb128r0++, rgb0123);
          _mm_stream_si128 (dstrgb128r0++, rgb4567);
          _mm_stream_si128 (dstrgb128r0++, rgb89ab);
          _mm_stream_si128 (dstrgb128r0++, rgbcdef);
          //}}}
          //{{{  row 1
          r00 = _mm_srai_epi16 (_mm_add_epi16 (y00r1, rv00), 6);
          r01 = _mm_srai_epi16 (_mm_add_epi16 (y01r1, rv01), 6);
          g00 = _mm_srai_epi16 (_mm_sub_epi16 (_mm_sub_epi16 (y00r1, gu00), gv00), 6);
          g01 = _mm_srai_epi16 (_mm_sub_epi16 (_mm_sub_epi16 (y01r1, gu01), gv01), 6);
          b00 = _mm_srai_epi16 (_mm_add_epi16 (y00r1, bu00), 6);
          b01 = _mm_srai_epi16 (_mm_add_epi16 (y01r1, bu01), 6);

          r00 = _mm_packus_epi16 (r00, r01);        // rrrr.. saturated
          g00 = _mm_packus_epi16 (g00, g01);        // gggg.. saturated
          b00 = _mm_packus_epi16 (b00, b01);        // bbbb.. saturated

          r01     = _mm_unpacklo_epi8 (r00, zero);  // 0r0r..
          gbgb    = _mm_unpacklo_epi8 (b00, g00);   // gbgb..
          rgb0123 = _mm_unpacklo_epi16 (gbgb, r01); // 0rgb0rgb..
          rgb4567 = _mm_unpackhi_epi16 (gbgb, r01); // 0rgb0rgb..

          r01     = _mm_unpackhi_epi8 (r00, zero);
          gbgb    = _mm_unpackhi_epi8 (b00, g00);
          rgb89ab = _mm_unpacklo_epi16 (gbgb, r01);
          rgbcdef = _mm_unpackhi_epi16 (gbgb, r01);

          _mm_stream_si128 (dstrgb128r1++, rgb0123);
          _mm_stream_si128 (dstrgb128r1++, rgb4567);
          _mm_stream_si128 (dstrgb128r1++, rgb89ab);
          _mm_stream_si128 (dstrgb128r1++, rgbcdef);
          //}}}
          }
        }
      mOk = true;
      }
    //}}}

    //{{{
    void clear() {
      mOk = false;
      mPts = 0;
      }
    //}}}

  private:
    bool mOk = false;
    uint64_t mPts = 0;

    int mWidth = 0;
    int mHeight = 0;
    int mYStride = 0;
    int mUVStride = 0;

    uint8_t* mYbuf = nullptr;
    uint8_t* mUbuf = nullptr;
    uint8_t* mVbuf = nullptr;

    uint32_t* mBgra = nullptr;
    };
  //}}}

  cVideoDecode() {}
  //{{{
  virtual ~cVideoDecode() {

    for (auto frame : mFramePool)
      delete frame;
    }
  //}}}

  int getWidth() { return mWidth; }
  int getHeight() { return mHeight; }
  int getFramePoolSize() { return (int)mFramePool.size(); }
  virtual int getSurfacePoolSize() = 0;

  void setPlayPts (uint64_t playPts) { mPlayPts = playPts; }
  //{{{
  cFrame* findPlayFrame() {
  // returns nearest frame within a 25fps frame of mPlayPts, nullptr if none

    uint64_t nearDist = 90000 / 25;

    cFrame* nearFrame = nullptr;
    for (auto frame : mFramePool) {
      if (frame->ok()) {
        uint64_t dist = frame->getPts() > mPlayPts ? frame->getPts() - mPlayPts : mPlayPts - frame->getPts();
        if (dist < nearDist) {
          nearDist = dist;
          nearFrame = frame;
          }
        }
      }

    return nearFrame;
    }
  //}}}
  //{{{
  void clear() {
  // returns nearest frame within a 25fps frame of mPlayPts, nullptr if none

    for (auto frame : mFramePool)
      frame->clear();
    }
  //}}}

  virtual void decode (uint64_t pts, uint8_t* pesBuffer, unsigned int pesBufferLen) = 0;

protected:
  static constexpr int kMaxVideoFramePoolSize = 200;
  //{{{
  cFrame* getFreeFrame (uint64_t pts) {
  // return first frame older than mPlayPts, otherwise add new frame

    while (true) {
      for (auto frame : mFramePool) {
        if (frame->ok() && (frame->getPts() < mPlayPts)) {
          // reuse frame
          frame->set (pts);
          return frame;
          }
        }

      if (mFramePool.size() < kMaxVideoFramePoolSize) {
        // allocate new frame
        mFramePool.push_back (new cFrame (pts));
        cLog::log (LOGINFO1, "allocated newFrame %d for %u at play:%u", mFramePool.size(), pts, mPlayPts);
        return mFramePool.back();
        }
      else
        this_thread::sleep_for (40ms);
      }

    // cannot reach here
    return nullptr;
    }
  //}}}

  int mWidth = 0;
  int mHeight = 0;

  uint64_t mPlayPts = 0;
  vector <cFrame*> mFramePool;
  };
//}}}
//{{{
class cVideoDecodeBox : public cD2dWindow::cView {
public:
  cVideoDecodeBox (cD2dWindow* window, float width, float height, cVideoDecode* videoDecode)
    : cView("videoDecode", window, width, height), mVideoDecode(videoDecode) {}
  virtual ~cVideoDecodeBox() {}

  void onDraw (ID2D1DeviceContext* dc) {

    auto frame = mVideoDecode->findPlayFrame();
    if (frame) {
      if (frame->getPts() != mPts) { // new Frame, update bitmap
        mPts = frame->getPts();
        if (mBitmap)  {
          auto pixelSize = mBitmap->GetPixelSize();
          if ((pixelSize.width != frame->getWidth()) || (pixelSize.height != frame->getHeight())) {
            // bitmap size changed, remove and recreate
            mBitmap->Release();
            mBitmap = nullptr;
            }
          }

        if (!mBitmap)
          dc->CreateBitmap (D2D1::SizeU(frame->getWidth(), frame->getHeight()),
                            { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE, 0,0 },
                            &mBitmap);
        mBitmap->CopyFromMemory (&D2D1::RectU(0,0, frame->getWidth(),frame->getHeight()),
                                 frame->getBgra(), frame->getWidth() * 4);
        }
      }

    if (mBitmap) {
      dc->SetTransform (mView2d.mTransform);
      dc->DrawBitmap (mBitmap, cRect(getSize()));
      dc->SetTransform (D2D1::Matrix3x2F::Identity());
      }

    // info string
    string str = dec(mVideoDecode->getFramePoolSize()) +
                 " " + dec(mVideoDecode->getSurfacePoolSize()) +
                 " " + dec(mVideoDecode->getWidth()) +
                 "x" + dec(mVideoDecode->getHeight());
    IDWriteTextLayout* textLayout;
    mWindow->getDwriteFactory()->CreateTextLayout (
      std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
      mWindow->getTextFormat(), getWidth(), getHeight(), &textLayout);
    dc->DrawTextLayout (getTL(), textLayout, mWindow->getWhiteBrush());
    textLayout->Release();
    }

private:
  cVideoDecode* mVideoDecode;

  ID2D1Bitmap* mBitmap = nullptr;
  uint64_t mPts = 0;
  };
//}}}
//{{{
class cMfxVideoDecode : public cVideoDecode {
public:
  //{{{
  cMfxVideoDecode() : cVideoDecode() {

    mfxVersion kMfxVersion = { 0,1 };
    mSession.Init (MFX_IMPL_AUTO, &kMfxVersion);
    }
  //}}}
  //{{{
  virtual ~cMfxVideoDecode() {

    MFXVideoDECODE_Close (mSession);

    for (auto surface : mSurfacePool)
      delete surface;
    }
  //}}}

  int getSurfacePoolSize() { return (int)mSurfacePool.size(); }
  //{{{
  void decode (uint64_t pts, uint8_t* pesBuffer, unsigned int pesBufferLen) {

    mBitstream.Data = pesBuffer;
    mBitstream.DataOffset = 0;
    mBitstream.DataLength = pesBufferLen;
    mBitstream.MaxLength = pesBufferLen;
    mBitstream.TimeStamp = pts;

    if (!mWidth) {
      // firstTime, decode header, init decoder
      memset (&mVideoParams, 0, sizeof(mVideoParams));
      mVideoParams.mfx.CodecId = MFX_CODEC_AVC;
      mVideoParams.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
      if (MFXVideoDECODE_DecodeHeader (mSession, &mBitstream, &mVideoParams) != MFX_ERR_NONE) {
        cLog::log (LOGERROR, "MFXVideoDECODE_DecodeHeader failed");
        return;
        }

      //  query surface for mWidth,mHeight
      mfxFrameAllocRequest frameAllocRequest;
      memset (&frameAllocRequest, 0, sizeof(frameAllocRequest));
      if (MFXVideoDECODE_QueryIOSurf (mSession, &mVideoParams, &frameAllocRequest) != MFX_ERR_NONE) {
        cLog::log (LOGERROR, "MFXVideoDECODE_QueryIOSurf failed");
        return;
        }
      mWidth = ((mfxU32)((frameAllocRequest.Info.Width)+31)) & (~(mfxU32)31);
      mHeight = frameAllocRequest.Info.Height;
      // unsure why this was done ??? trace it back for height as well as width ???
      //mHeight = ((mfxU32)((frameAllocRequest.Info.Height)+31)) & (~(mfxU32)31);

      if (MFXVideoDECODE_Init (mSession, &mVideoParams) != MFX_ERR_NONE) {
        cLog::log (LOGERROR, "MFXVideoDECODE_Init failed");
        return;
        }
      }

    // reset decoder on skip
    //mfxStatus status = MFXVideoDECODE_Reset (mSession, &mVideoParams);

    // decode video pes
    // - could be none or multiple frames
    // - returned by decode order, not presentation order
    mfxStatus status = MFX_ERR_NONE;
    while ((status >= MFX_ERR_NONE) || (status == MFX_ERR_MORE_SURFACE)) {
      mfxFrameSurface1* surface = nullptr;
      mfxSyncPoint syncDecode = nullptr;
      status = MFXVideoDECODE_DecodeFrameAsync (mSession, &mBitstream, getFreeSurface(), &surface, &syncDecode);
      if (status == MFX_ERR_NONE) {
        status = mSession.SyncOperation (syncDecode, 60000);
        if (status == MFX_ERR_NONE) {
          cLog::log (LOGINFO1, "decoded pts:%u %dx%d:%d",
                               surface->Data.TimeStamp, surface->Info.Width, surface->Info.Height, surface->Data.Pitch);
          auto frame = getFreeFrame (surface->Data.TimeStamp);
          frame->setNv12 (surface->Data.Y, surface->Info.Width, surface->Info.Height, surface->Data.Pitch);
          }
        }
      }
    }
  //}}}

private:
  //{{{
  mfxFrameSurface1* getFreeSurface() {
  // return first unlocked surface, allocate new if none

    // reuse any unlocked surface
    for (auto surface : mSurfacePool)
      if (!surface->Data.Locked)
        return surface;

    // allocate new surface
    auto surface = new mfxFrameSurface1;
    memset (surface, 0, sizeof (mfxFrameSurface1));
    memcpy (&surface->Info, &mVideoParams.mfx.FrameInfo, sizeof(mfxFrameInfo));
    surface->Data.Y = new mfxU8[mWidth * mHeight * 12 / 8];
    surface->Data.U = surface->Data.Y + mWidth * mHeight;
    surface->Data.V = nullptr; // NV12 ignores V pointer
    surface->Data.Pitch = mWidth;
    mSurfacePool.push_back (surface);

    cLog::log (LOGINFO1, "allocating new mfxFrameSurface1");

    return nullptr;
    }
  //}}}

  MFXVideoSession mSession;
  mfxVideoParam mVideoParams;
  mfxBitstream mBitstream;
  vector <mfxFrameSurface1*> mSurfacePool;
  };
//}}}
//{{{
class cFFmpegVideoDecode : public cVideoDecode {
public:
  cFFmpegVideoDecode() : cVideoDecode() { }
  virtual ~cFFmpegVideoDecode() { }

  int getSurfacePoolSize() { return 0; }
  void decode (uint64_t pts, uint8_t* pesBuffer, unsigned int pesBufferLen) {}

private:
  };
//}}}

class cAppWindow : public cD2dWindow {
public:
  //{{{
  void run (const string& title, int width, int height, int channelNum, int audBitrate, int vidBitrate) {

    init (title, width, height, false);

    mVideoDecode = new cMfxVideoDecode();
    add (new cVideoDecodeBox (this, 0.f,0.f, mVideoDecode), 0.f,0.f);

    add (new cClockBox (this, 40.f), -135.f,35.f);
    add (new cSongBox (this, 0.f,0.f, mSong));
    mLogBox = add (new cLogBox (this, 20.f));
    add (new cWindowBox (this, 60.f,24.f), -60.f,0.f)->setPin (false);

    // startup
    thread ([=](){ hlsThread (kHost, kChannels[channelNum], audBitrate, vidBitrate); }).detach();

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

      case 'F' : toggleFullScreen(); break;
      case 'L' : mLogBox->togglePin(); break;
      case ' ' : mPlaying = !mPlaying; break;

      //{{{
      case 'M':  // mark
        mSong.getSelect().addMark (mSong.getPlayFrame());
        changed();
        break;
      //}}}
      //{{{
      case 0x21: // page up - back one hour
        mSong.incPlaySec (-60*60, false);
        //mVideoDecode.setPlayPts (framePtr->getPts());
        mVideoDecode->clear();
        changed();
        break;
      //}}}
      //{{{
      case 0x22: // page down - forward one hour
        mSong.incPlaySec (60*60, false);
        //mVideoDecode.setPlayPts (framePtr->getPts());
        mVideoDecode->clear();
        changed();
        break;
      //}}}
      //{{{
      case 0x25: // left  arrow -1s, ctrl -10s, shift -5m
        mSong.incPlaySec (-(getShift() ? 300 : getControl() ? 10 : 1), false);
        //mVideoDecode.setPlayPts (framePtr->getPts());
        mVideoDecode->clear();
        changed();
        break;
      //}}}
      //{{{
      case 0x27: // right arrow +1s, ctrl +10s, shift +5m
        mSong.incPlaySec (getShift() ? 300 : getControl() ?  10 :  1, false);
        if (getShift() || getControl())
          mVideoDecode->clear();
        //mVideoDecode.setPlayPts (framePtr->getPts());
        changed();
        break;
      //}}}
      //{{{
      case 0x24: // home
        mSong.setPlayFrame (
        mSong.getSelect().empty() ? mSong.getFirstFrame() : mSong.getSelect().getFirstFrame());
        changed();
        break;
      //}}}
      //{{{
      case 0x23: // end
        mSong.setPlayFrame (
        mSong.getSelect().empty() ? mSong.getLastFrame() : mSong.getSelect().getLastFrame());
        changed();
        break;
      //}}}
      //{{{
      case 0x2e: // delete select
        mSong.getSelect().clearAll(); changed();
        break;
      //}}}
      //{{{
      default:
        cLog::log (LOGINFO, "key %x", key);
        changed();
        break;
      //}}}
      }

    return false;
    }
  //}}}

private:
  //{{{
  static uint64_t getPts (uint8_t* ts) {
  // return 33 bits of pts,dts

    if ((ts[0] & 0x01) && (ts[2] & 0x01) && (ts[4] & 0x01)) {
      // valid marker bits
      uint64_t pts = ts[0] & 0x0E;
      pts = (pts << 7) | ts[1];
      pts = (pts << 8) | (ts[2] & 0xFE);
      pts = (pts << 7) | ts[3];
      pts = (pts << 7) | (ts[4] >> 1);
      return pts;
      }
    else {
      cLog::log (LOGERROR, "getPts marker bits - %02x %02x %02x %02x 0x02", ts[0], ts[1], ts[2], ts[3], ts[4]);
      return 0;
      }
    }
  //}}}
  //{{{
  static string getTagValue (uint8_t* buffer, char* tag) {

    const char* tagPtr = strstr ((char*)buffer, tag);
    const char* valuePtr = tagPtr + strlen (tag);
    const char* endPtr = strchr (valuePtr, '\n');

    return string (valuePtr, endPtr - valuePtr);
    }
  //}}}

  //{{{
  void hlsThread (const string& host, const string& channel, int audBitrate, int vidBitrate) {
  // hls http chunk load and decode thread

    cLog::setThreadName ("hls ");

    constexpr int kHlsPreload = 2;

    uint8_t* pesBuffer = nullptr;
    int pesBufferLen = 0;

    mSong.setChannel (channel);
    mSong.setBitrate (audBitrate, audBitrate < 128000 ? 180 : 360); // audBitrate, audioFrames per chunk

    while (!getExit()) {
      const string path = "pool_902/live/uk/" + mSong.getChannel() +
                          "/" + mSong.getChannel() +
                          ".isml/" + mSong.getChannel() +
                          (mSong.getBitrate() < 128000 ? "-pa3=" : "-pa4=") + dec(mSong.getBitrate()) +
                          "-video=" + dec(vidBitrate);
      cPlatformHttp http;
      string redirectedHost = http.getRedirect (host, path + ".m3u8");
      if (http.getContent()) {
        //{{{  got .m3u8, parse for mediaSequence, programDateTimePoint
        int mediaSequence = stoi (getTagValue (http.getContent(), "#EXT-X-MEDIA-SEQUENCE:"));

        istringstream inputStream (getTagValue (http.getContent(), "#EXT-X-PROGRAM-DATE-TIME:"));
        system_clock::time_point programDateTimePoint;
        inputStream >> date::parse ("%FT%T", programDateTimePoint);

        http.freeContent();
        //}}}

        mSong.init (cAudioDecode::eAac, 2, 48000, mSong.getBitrate() < 128000 ? 2048 : 1024); // samplesPerFrame
        mSong.setHlsBase (mediaSequence, programDateTimePoint, -37s, (2*60*60) - 30);
        cAudioDecode audioDecode (cAudioDecode::eAac);

        thread player;
        bool firstTime = true;
        mSongChanged = false;
        while (!getExit() && !mSongChanged) {
          int chunkNum = mSong.getHlsLoadChunkNum (getNowRaw(), 12s, kHlsPreload);
          if (chunkNum) {
            // get hls chunkNum chunk
            mSong.setHlsLoad (cSong::eHlsLoading, chunkNum);
            if (http.get (redirectedHost, path + '-' + dec(chunkNum) + ".ts") == 200) {
              //{{{  process audio first
              int seqFrameNum = mSong.getHlsFrameFromChunkNum (chunkNum);

              uint64_t pts = 0;

              // parse ts packets
              uint8_t* ts = http.getContent();
              uint8_t* tsEnd = ts + http.getContentSize();
              while ((ts < tsEnd) && (*ts++ == 0x47)) {
                auto payStart = ts[0] & 0x40;
                auto pid = ((ts[0] & 0x1F) << 8) | ts[1];
                auto headerBytes = (ts[2] & 0x20) ? 4 + ts[3] : 3;
                ts += headerBytes;
                auto tsBodyBytes = 187 - headerBytes;

                if (pid == 34) {
                  // audio pid
                  if (payStart && !ts[0] && !ts[1] && (ts[2] == 1) && (ts[3] == 0xC0)) {
                    if (pesBufferLen) {
                      //{{{  process prev audioPes
                      uint8_t* pesBufferPtr = pesBuffer;
                      uint8_t* pesBufferEnd = pesBuffer + pesBufferLen;

                      while (audioDecode.parseFrame (pesBufferPtr, pesBufferEnd)) {
                        // several aacFrames per audio pes
                        float* samples = audioDecode.decodeFrame (seqFrameNum);
                        if (samples) {
                          if (firstTime)
                            mSong.setFixups (audioDecode.getNumChannels(), audioDecode.getSampleRate(), audioDecode.getNumSamples());
                          mSong.addAudioFrame (seqFrameNum++, samples, true, mSong.getNumFrames(), nullptr, pts);
                          pts += (audioDecode.getNumSamples() * 90) / 48;
                          changed();

                          if (firstTime) {
                            // launch player
                            firstTime = false;
                            player = thread ([=](){ playThread (true); });
                            }
                          }

                        pesBufferPtr += audioDecode.getNextFrameOffset();
                        }

                      pesBufferLen = 0;
                      }
                      //}}}

                    if (ts[7] & 0x80)
                      pts = getPts (ts+9);

                    // skip header
                    int pesHeaderBytes = 9 + ts[8];
                    ts += pesHeaderBytes;
                    tsBodyBytes -= pesHeaderBytes;
                    }

                  // copy ts payload into pesBuffer
                  pesBuffer = (uint8_t*)realloc (pesBuffer, pesBufferLen + tsBodyBytes);
                  memcpy (pesBuffer + pesBufferLen, ts, tsBodyBytes);
                  pesBufferLen += tsBodyBytes;
                  }

                ts += tsBodyBytes;
                }

              if (pesBufferLen) {
                //{{{  process last audioPes
                uint8_t* pesBufferPtr = pesBuffer;
                uint8_t* pesBufferEnd = pesBuffer + pesBufferLen;

                while (audioDecode.parseFrame (pesBufferPtr, pesBufferEnd)) {
                  // several aacFrames per audio pes
                  float* samples = audioDecode.decodeFrame (seqFrameNum);
                  if (samples) {
                    mSong.addAudioFrame (seqFrameNum++, samples, true, mSong.getNumFrames(), nullptr, pts);
                    pts += (audioDecode.getNumSamples() * 90) / 48;
                    changed();
                    }

                  pesBufferPtr += audioDecode.getNextFrameOffset();
                  }

                pesBufferLen = 0;
                }
                //}}}
              //}}}
              mSong.setHlsLoad (cSong::eHlsIdle, chunkNum);
              //{{{  process video second, may block waiting for free videoFrames
              // parse ts packets
              ts = http.getContent();
              while ((ts < tsEnd) && (*ts++ == 0x47)) {
                auto payStart = ts[0] & 0x40;
                auto pid = ((ts[0] & 0x1F) << 8) | ts[1];
                auto headerBytes = (ts[2] & 0x20) ? 4 + ts[3] : 3;
                ts += headerBytes;
                auto tsBodyBytes = 187 - headerBytes;

                if (pid == 33) {
                  //  video pid
                  if (payStart && !ts[0] && !ts[1] && (ts[2] == 1) && (ts[3] == 0xe0)) {
                    if (pesBufferLen) {
                      // process prev videoPes
                      mVideoDecode->decode (pts, pesBuffer, pesBufferLen);
                      pesBufferLen = 0;
                      }

                    if (ts[7] & 0x80)
                      pts = getPts (ts+9);

                    // skip header
                    int pesHeaderBytes = 9 + ts[8];
                    ts += pesHeaderBytes;
                    tsBodyBytes -= pesHeaderBytes;
                    }

                  // copy ts payload into pesBuffer
                  pesBuffer = (uint8_t*)realloc (pesBuffer, pesBufferLen + tsBodyBytes);
                  memcpy (pesBuffer + pesBufferLen, ts, tsBodyBytes);
                  pesBufferLen += tsBodyBytes;
                  }

                ts += tsBodyBytes;
                }

              if (pesBufferLen) {
                // process last videoPes
                mVideoDecode->decode (pts, pesBuffer, pesBufferLen);
                pesBufferLen = 0;
                }
              //}}}
              http.freeContent();
              }
            else {
              //{{{  failed to load expected available chunk, back off for 250ms
              mSong.setHlsLoad (cSong::eHlsFailed, chunkNum);
              changed();

              cLog::log (LOGERROR, "late " + dec(chunkNum));
              this_thread::sleep_for (250ms);
              }
              //}}}
            }
          else // no chunk available, back off for 100ms
            this_thread::sleep_for (100ms);
          }
        player.join();
        }
      }

    free (pesBuffer);
    cLog::log (LOGINFO, "exit");
    }
  //}}}
  //{{{
  void playThread (bool streaming) {
  // audio player thread, video just follows play pts

    cLog::setThreadName ("play");
    SetThreadPriority (GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    float silence [2048*2] = { 0.f };
    float samples [2048*2] = { 0.f };

    auto device = getDefaultAudioOutputDevice();
    if (device) {
      cLog::log (LOGINFO, "device @ %d", mSong.getSampleRate());
      device->setSampleRate (mSong.getSampleRate());
      cAudioDecode decode (mSong.getFrameType());

      device->start();
      while (!getExit() && !mSongChanged) {
        device->process ([&](float*& srcSamples, int& numSrcSamples) mutable noexcept {
          // lambda callback - load srcSamples
          shared_lock<shared_mutex> lock (mSong.getSharedMutex());

          auto framePtr = mSong.getAudioFramePtr (mSong.getPlayFrame());
          if (mPlaying && framePtr && framePtr->getSamples()) {
            if (mSong.getNumChannels() == 1) {
              //{{{  mono to stereo
              auto src = framePtr->getSamples();
              auto dst = samples;
              for (int i = 0; i < mSong.getSamplesPerFrame(); i++) {
                *dst++ = *src;
                *dst++ = *src++;
                }
              }
              //}}}
            else
              memcpy (samples, framePtr->getSamples(), mSong.getSamplesPerFrame() * mSong.getNumChannels() * sizeof(float));
            srcSamples = samples;
            }
          else
            srcSamples = silence;
          numSrcSamples = mSong.getSamplesPerFrame();

          if (mPlaying && framePtr) {
            mVideoDecode->setPlayPts (framePtr->getPts());
            mSong.incPlayFrame (1, true);
            changed();
            }
          });

        if (!streaming && (mSong.getPlayFrame() > mSong.getLastFrame()))
          break;
        }

      device->stop();
      }

    cLog::log (LOGINFO, "exit");
    }
  //}}}

  //{{{  vars
  cSong mSong;
  bool mSongChanged = false;
  bool mPlaying = true;
  cBox* mLogBox = nullptr;

  cVideoDecode* mVideoDecode = nullptr;
  //}}}
  };

// main
int WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

  cLog::init (LOGINFO, true, "", "hlsWindow");

  int numArgs;
  auto args = CommandLineToArgvW (GetCommandLineW(), &numArgs);
  vector<string> names;
  for (int i = 1; i < numArgs; i++)
    names.push_back (wcharToString (args[i]));

  int channelNum = names.empty() ? kDefaultChannelNum : stoi (names[0]);

  cAppWindow appWindow;
  appWindow.run ("hlsWindow", 800, 420, channelNum, kAudBitrate, kVidBitrate);
  return 0;
  }
