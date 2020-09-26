// hlsWindow.cpp
//{{{  includes
#include "stdafx.h"

#include "../../shared/audio/audioWASAPI.h"
#include "../../shared/utils/cSong.h"
#include "../../shared/decoders/cAudioDecode.h"
#include "../../shared/net/cWinSockHttp.h"

#include "../common/cD2dWindow.h"
#include "../boxes/cLogBox.h"
#include "../boxes/cWindowBox.h"
#include "../boxes/cClockBox.h"
#include "../boxes/cCalendarBox.h"
#include "../boxes/cSongBox.h"

#include "../mfx/include/mfxvideo++.h"

#ifdef _DEBUG
  #pragma comment (lib,"libmfx_d.lib")
#else
  #pragma comment (lib,"libmfx.lib")
#endif

using namespace std;
using namespace chrono;
//}}}
//{{{  urls
//vs-hls-uk-live.akamaized.net/pool_902/live/uk/bbc_one_hd/bbc_one_hd.isml/bbc_one_hd-pa4%3d128000-video%3d827008.m3u8
//vs-hls-uk-live.akamaized.net/pool_902/live/uk/bbc_one_hd/bbc_one_hd.isml/bbc_one_hd-pa4%3d128000-video%3d1604032.m3u8
//vs-hls-uk-live.akamaized.net/pool_902/live/uk/bbc_one_hd/bbc_one_hd.isml/bbc_one_hd-pa4%3d128000-video%3d2812032.m3u8
//vs-hls-uk-live.akamaized.net/pool_902/live/uk/bbc_one_hd/bbc_one_hd.isml/bbc_one_hd-pa4%3d128000-video%3d5070016.m3u8

//vs-hls-uk-live.akamaized.net/pool_902/live/uk/bbc_four_hd/bbc_four_hd.isml/bbc_four_hd-pa4%3d128000-video%3d5070016.m3u8
//vs-hls-uk-live.akamaized.net/pool_902/live/uk/bbc_one_south_west/bbc_one_south_west.isml/bbc_one_south_west-pa3%3d96000-video%3d1604032.m3u8
//}}}

const string kHost = "vs-hls-uk-live.akamaized.net";
const vector <string> kChannels = { "bbc_one_hd", "bbc_four_hd", "bbc_one_south_west" };
constexpr int kBitRate = 128000;

//constexpr int kVidBitrate = 827008;
constexpr int kVidBitrate = 1604032;
//constexpr int kVidBitrate = 2812032;
//constexpr int kVidBitrate = 5070016;
constexpr int kMaxVideoFrames = 400;

//{{{
class cMfxVideoDecode {
public:
  //{{{
  class cFrame {
  public:
    //{{{
    virtual ~cFrame() {
      _aligned_free (mYbuf);
      _aligned_free (mUbuf);
      _aligned_free (mVbuf);
      _aligned_free (mBgra);
      }
    //}}}

    bool ok() { return mOk; }
    int getWidth() { return mWidth; }
    int getHeight() { return mHeight; }
    uint32_t* getBgra() { return mBgra; }
    uint64_t getTimestamp() { return mTimestamp; }

    //{{{
    void setNv12 (uint8_t* buffer, int stride, int width, int height, uint64_t timestamp) {

      mOk = false;

      mYStride = stride;
      mUVStride = stride/2;

      mWidth = width;
      mHeight = height;
      mTimestamp = timestamp;

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

      mBgra = (uint32_t*)_aligned_realloc (mBgra, mWidth * 4 * mHeight, 128);
      int argbStride = mWidth;

      __m128i ysub  = _mm_set1_epi32 (0x00100010);
      __m128i uvsub = _mm_set1_epi32 (0x00800080);

      __m128i facy  = _mm_set1_epi32 (0x004a004a);
      __m128i facrv = _mm_set1_epi32 (0x00660066);
      __m128i facgu = _mm_set1_epi32 (0x00190019);
      __m128i facgv = _mm_set1_epi32 (0x00340034);
      __m128i facbu = _mm_set1_epi32 (0x00810081);

      __m128i zero  = _mm_set1_epi32( 0x00000000 );

      for (int y = 0; y < mHeight; y += 2) {
        __m128i* srcy128r0 = (__m128i *)(mYbuf + mYStride*y);
        __m128i* srcy128r1 = (__m128i *)(mYbuf + mYStride*y + mYStride);
        __m64* srcu64 = (__m64 *)(mUbuf + mUVStride*(y/2));
        __m64* srcv64 = (__m64 *)(mVbuf + mUVStride*(y/2));

        __m128i* dstrgb128r0 = (__m128i *)(mBgra + argbStride*y);
        __m128i* dstrgb128r1 = (__m128i *)(mBgra + argbStride*y + argbStride);

        for (int x = 0; x < mWidth; x += 16) {
          __m128i u0 = _mm_loadl_epi64 ((__m128i *)srcu64 ); srcu64++;
          __m128i v0 = _mm_loadl_epi64 ((__m128i *)srcv64 ); srcv64++;

          __m128i y0r0 = _mm_load_si128( srcy128r0++ );
          __m128i y0r1 = _mm_load_si128( srcy128r1++ );
          //{{{  constant y factors
          __m128i y00r0 = _mm_mullo_epi16 (_mm_sub_epi16 (_mm_unpacklo_epi8 (y0r0, zero), ysub), facy);
          __m128i y01r0 = _mm_mullo_epi16 (_mm_sub_epi16 (_mm_unpackhi_epi8 (y0r0, zero), ysub), facy);
          __m128i y00r1 = _mm_mullo_epi16 (_mm_sub_epi16 (_mm_unpacklo_epi8 (y0r1, zero), ysub), facy);
          __m128i y01r1 = _mm_mullo_epi16 (_mm_sub_epi16 (_mm_unpackhi_epi8 (y0r1, zero), ysub), facy);
          //}}}
          //{{{  expand u and v so they're aligned with y values
          u0  = _mm_unpacklo_epi8 (u0, zero);
          __m128i u00 = _mm_sub_epi16 (_mm_unpacklo_epi16 (u0, u0), uvsub);
          __m128i u01 = _mm_sub_epi16 (_mm_unpackhi_epi16 (u0, u0), uvsub);

          v0  = _mm_unpacklo_epi8( v0,  zero );
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

          r00 = _mm_packus_epi16 (r00, r01);         // rrrr.. saturated
          g00 = _mm_packus_epi16 (g00, g01);         // gggg.. saturated
          b00 = _mm_packus_epi16 (b00, b01);         // bbbb.. saturated

          r01     = _mm_unpacklo_epi8 (r00, zero); // 0r0r..
          __m128i gbgb    = _mm_unpacklo_epi8 (b00, g00);  // gbgb..
          __m128i rgb0123 = _mm_unpacklo_epi16 (gbgb, r01);  // 0rgb0rgb..
          __m128i rgb4567 = _mm_unpackhi_epi16 (gbgb, r01);  // 0rgb0rgb..

          r01     = _mm_unpackhi_epi8 (r00, zero);
          gbgb    = _mm_unpackhi_epi8 (b00, g00 );
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

          r00 = _mm_packus_epi16 (r00, r01);         // rrrr.. saturated
          g00 = _mm_packus_epi16 (g00, g01);         // gggg.. saturated
          b00 = _mm_packus_epi16 (b00, b01);         // bbbb.. saturated

          r01     = _mm_unpacklo_epi8 (r00,  zero); // 0r0r..
          gbgb    = _mm_unpacklo_epi8 (b00,  g00);  // gbgb..
          rgb0123 = _mm_unpacklo_epi16 (gbgb, r01);  // 0rgb0rgb..
          rgb4567 = _mm_unpackhi_epi16 (gbgb, r01);  // 0rgb0rgb..

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

  private:
    bool mOk = false;
    int mYStride = 0;
    int mUVStride = 0;

    int mWidth = 0;
    int mHeight = 0;
    uint64_t mTimestamp = 0xFFFFFFFFFFFFFFFF;

    uint8_t* mYbuf = nullptr;
    uint8_t* mUbuf = nullptr;
    uint8_t* mVbuf = nullptr;

    uint32_t* mBgra = nullptr;
    };
  //}}}

  //{{{
  cMfxVideoDecode() {

    mfxVersion kMfxVersion = { 0,1 };
    mSession.Init (MFX_IMPL_AUTO, &kMfxVersion);
    }
  //}}}
  //{{{
  ~cMfxVideoDecode() {

    // Clean up resources
    // -  recommended to close Media SDK components first, before releasing allocated surfaces
    //    since //    some surfaces may still be locked by internal Media SDK resources.
    MFXVideoDECODE_Close (mSession);

    // mSession closed automatically on destruction
    for (int i = 0; i < mNumSurfaces; i++)
      delete mSurfaces[i];

    for (auto frame : mFrames)
      delete frame;
    }
  //}}}

  //{{{
  cFrame* getCurFrame() {

    cFrame* nearestFrame = nullptr;

    double nearest = 0.0;
    for (auto frame : mFrames)
      if (frame->ok())
        if (!nearestFrame || (fabs(frame->getTimestamp() - mPlayFrame)) < nearest) {
          nearestFrame = frame;
          nearest = fabs(frame->getTimestamp() - mPlayFrame);
          }

    return nearestFrame;
    }
  //}}}
  //{{{
  void setPlayFrame (int playFrame) {

    mPlayFrame = playFrame;
    }
  //}}}
  //{{{
  void decode (uint8_t* pes, int pesSize, uint64_t timestamp) {

    mBitstream.Data = pes;
    mBitstream.DataOffset = 0;
    mBitstream.DataLength = pesSize;
    mBitstream.MaxLength = pesSize;
    mBitstream.TimeStamp = timestamp;

    if (!mNumSurfaces) {
      // allocate decoder surfaces, init decoder, decode header
      memset (&mVideoParams, 0, sizeof(mVideoParams));
      mVideoParams.mfx.CodecId = MFX_CODEC_AVC;
      mVideoParams.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
      if (MFXVideoDECODE_DecodeHeader (mSession, &mBitstream, &mVideoParams) != MFX_ERR_NONE) {
        cLog::log (LOGERROR, "MFXVideoDECODE_DecodeHeader failed");
        return;
        }

      //  query surfaces
      mfxFrameAllocRequest frameAllocRequest;
      memset (&frameAllocRequest, 0, sizeof(frameAllocRequest));
      if (MFXVideoDECODE_QueryIOSurf (mSession, &mVideoParams, &frameAllocRequest) != MFX_ERR_NONE) {
        cLog::log (LOGERROR, "MFXVideoDECODE_QueryIOSurf failed");
        return;
        }
      mNumSurfaces = frameAllocRequest.NumFrameSuggested;
      mWidth = ((mfxU32)((frameAllocRequest.Info.Width)+31)) & (~(mfxU32)31);
      mHeight = ((mfxU32)((frameAllocRequest.Info.Height)+31)) & (~(mfxU32)31);

      // alloc surfaces in system memory
      mSurfaces = new mfxFrameSurface1*[mNumSurfaces];
      for (int i = 0; i < mNumSurfaces; i++) {
        mSurfaces[i] = new mfxFrameSurface1;
        memset (mSurfaces[i], 0, sizeof (mfxFrameSurface1));
        memcpy (&mSurfaces[i]->Info, &mVideoParams.mfx.FrameInfo, sizeof(mfxFrameInfo));

        // allocate nv12 followed by planar u, planar v
        mSurfaces[i]->Data.Y = new mfxU8[mWidth * mHeight * 12 / 8];
        mSurfaces[i]->Data.U = mSurfaces[i]->Data.Y + mWidth * mHeight;
        mSurfaces[i]->Data.V = nullptr; // NV12 ignores V pointer
        mSurfaces[i]->Data.Pitch = mWidth;
        }

      if (MFXVideoDECODE_Init (mSession, &mVideoParams) != MFX_ERR_NONE) {
        cLog::log (LOGERROR, "MFXVideoDECODE_Init failed");
        return;
        }
      }

    //mfxStatus status = MFXVideoDECODE_Reset (mSession, &mVideoParams);
    mfxStatus status = MFX_ERR_NONE;
    while (status >= MFX_ERR_NONE || status == MFX_ERR_MORE_SURFACE) {
      mfxFrameSurface1* surface = nullptr;
      mfxSyncPoint syncDecode = nullptr;
      //cLog::log (LOGINFO, "decode surface" + dec (index));
      status = MFXVideoDECODE_DecodeFrameAsync (mSession, &mBitstream, getFreeSurface(), &surface, &syncDecode);
      if (status == MFX_ERR_NONE) {
        status = mSession.SyncOperation (syncDecode, 60000);
        if (status == MFX_ERR_NONE) {
          //cLog::log (LOGINFO, "decode %d, %d %d %d",
          //                     surface->Data.TimeStamp,
          //                     surface->Data.Pitch, surface->Info.Width, surface->Info.Height);
          getOldestFrame()->setNv12 (surface->Data.Y,
                                     surface->Data.Pitch, surface->Info.Width, surface->Info.Height,
                                     surface->Data.TimeStamp);
          }
        }
      }
    }
  //}}}

private:
  //{{{
  cFrame* getOldestFrame() {
  // return free, or oldest frame

    if (mFrames.size() < kMaxVideoFrames) {
      mFrames.push_back (new cFrame);
      return mFrames.back();
      }

    cFrame* oldestFrame = nullptr;
    for (auto frame : mFrames)
      if (!oldestFrame)
        oldestFrame = frame;
      else if (frame->getTimestamp() < oldestFrame->getTimestamp())
        oldestFrame = frame;

    return oldestFrame;
    }
  //}}}
  //{{{
  mfxFrameSurface1* getFreeSurface() {

    for (mfxU16 i = 0; i < mNumSurfaces; i++)
      if (!mSurfaces[i]->Data.Locked)
        return mSurfaces[i];

    return nullptr;
    }
  //}}}

  MFXVideoSession mSession;

  mfxVideoParam mVideoParams;
  mfxU16 mNumSurfaces = 0;
  int mWidth = 0;
  int mHeight = 0;

  mfxBitstream mBitstream;
  mfxFrameSurface1** mSurfaces;

  vector <cFrame*> mFrames;
  int mPlayFrame = 0;
  };
//}}}
//{{{
class cMfxVideoDecodeBox : public cD2dWindow::cView {
public:
  cMfxVideoDecodeBox (cD2dWindow* window, float width, float height, cMfxVideoDecode& videoDecode)
      : cView("videoDecode", window, width, height), mVideoDecode(videoDecode) {}
  virtual ~cMfxVideoDecodeBox() {}

  void onDraw (ID2D1DeviceContext* dc) {

    auto frame = mVideoDecode.getCurFrame();
    if (frame) {
      cLog::log (LOGINFO, "onDraw:%d was:%d", frame->getTimestamp(), mTimestamp);
      if (frame->getTimestamp() != mTimestamp) {
        // make new bitmap from frame
        if (mBitmap)  {
          auto pixelSize = mBitmap->GetPixelSize();
          if ((pixelSize.width != frame->getWidth()) || (pixelSize.height != frame->getHeight())) {
            // bitmap size changed
            mBitmap->Release();
            mBitmap = nullptr;
            }
          }

        if (!mBitmap) // create/recreate bitmap
          dc->CreateBitmap (D2D1::SizeU(frame->getWidth(), frame->getHeight()),
                            { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE, 0,0 },
                            &mBitmap);

        mBitmap->CopyFromMemory (&D2D1::RectU(0,0, frame->getWidth(),frame->getHeight()),
                                 frame->getBgra(), frame->getWidth() * 4);

        mTimestamp = frame->getTimestamp();
        }
      }

    if (mBitmap) {
      dc->SetTransform (mView2d.mTransform);
      dc->DrawBitmap (mBitmap, cRect(getSize()));
      dc->SetTransform (D2D1::Matrix3x2F::Identity());
      }
    }

private:
  cMfxVideoDecode& mVideoDecode;

  ID2D1Bitmap* mBitmap = nullptr;
  uint64_t mTimestamp = 0;
  };
//}}}

class cAppWindow : public cD2dWindow {
public:
  //{{{
  void run (const string& title, int width, int height, const vector<string>& names) {

    init (title, width, height, false);
    add (new cMfxVideoDecodeBox (this, 0.f,0.f, mVideoDecode), 0.f,0.f);
    //add (new cCalendarBox (this, 190.f,150.f), -190.f,0.f);
    add (new cClockBox (this, 40.f), -135.f,35.f);
    add (new cSongBox (this, 0.f,0.f, mSong));

    // startup
    thread ([=](){ hlsThread (kHost, kChannels[0], kBitRate); }).detach();

    mLogBox = add (new cLogBox (this, 20.f));
    add (new cWindowBox (this, 60.f,24.f), -60.f,0.f)->setPin (false);

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

      case 'M' : mSong.getSelect().addMark (mSong.getPlayFrame()); changed(); break;

      case ' ' : mPlaying = !mPlaying; break;

      case 0x21: mSong.prevSilencePlayFrame(); changed(); break;; // page up
      case 0x22: mSong.nextSilencePlayFrame(); changed(); break;; // page down

      case 0x25: mSong.incPlaySec (getShift() ? -300 : getControl() ? -10 : -1, false);  changed(); break; // left arrow  - 1 sec
      case 0x27: mSong.incPlaySec (getShift() ? 300 : getControl() ?  10 :  1, false);  changed(); break; // right arrow  + 1 sec

      case 0x24: mSong.setPlayFrame (
        mSong.getSelect().empty() ? mSong.getFirstFrame() : mSong.getSelect().getFirstFrame()); changed(); break; // home
      case 0x23: mSong.setPlayFrame (
        mSong.getSelect().empty() ? mSong.getLastFrame() : mSong.getSelect().getLastFrame()); changed(); break; // end

      case 0x2e: mSong.getSelect().clearAll(); changed(); break;; // delete select

      default  : cLog::log (LOGINFO, "key %x", key); changed(); break;
      }

    return false;
    }
  //}}}

private:
  //{{{
  static string getTaggedValue (uint8_t* buffer, char* tag) {

    const char* tagPtr = strstr ((char*)buffer, tag);
    const char* valuePtr = tagPtr + strlen (tag);
    const char* endPtr = strchr (valuePtr, '\n');
    return string (valuePtr, endPtr - valuePtr);
    }
  //}}}
  //{{{
  void hlsThread (const string& host, const string& chan, int bitrate) {
  // hls chunk http load and analyse thread, single thread helps chan change and jumping backwards

    //mFile = fopen ("C:/Users/colin/Desktop/hls.ts", "wb");
    int vidFrameNum = 0;

    constexpr int kHlsPreload = 2;
    cLog::setThreadName ("hls ");

    mSong.setChan (chan);
    mSong.setBitrate (bitrate, 360);
    while (!getExit()) {
      const string path = "pool_902/live/uk/" + mSong.getChan() + "/" + mSong.getChan() + ".isml/" + mSong.getChan() +
                          "-pa4=" + dec(mSong.getBitrate()) + "-video=" + dec(kVidBitrate);
      cPlatformHttp http;
      string redirectedHost = http.getRedirect (host, path + ".m3u8");
      if (http.getContent()) {
        //{{{  hls m3u8 ok, parse it for baseChunkNum, baseTimePoint
        int mediaSequence = stoi (getTaggedValue (http.getContent(), "#EXT-X-MEDIA-SEQUENCE:"));

        istringstream inputStream (getTaggedValue (http.getContent(), "#EXT-X-PROGRAM-DATE-TIME:"));
        system_clock::time_point programDateTimePoint;
        inputStream >> date::parse ("%FT%T", programDateTimePoint);

        http.freeContent();
        //}}}
        mSong.init (cAudioDecode::eAac, 2, 48000, 1024);
        mSong.setHlsBase (mediaSequence, programDateTimePoint, -37s);
        cAudioDecode decode (cAudioDecode::eAac);

        thread player;
        bool firstTime = true;
        mSongChanged = false;
        while (!getExit() && !mSongChanged) {
          int chunkNum = mSong.getHlsLoadChunkNum (getNowRaw(), 12s, kHlsPreload);
          if (chunkNum) {
            // get hls chunkNum chunk
            mSong.setHlsLoad (cSong::eHlsLoading, chunkNum);
            if (http.get (redirectedHost, path + '-' + dec(chunkNum) + ".ts") == 200) {
              //{{{  chunk loaded, process it
              cLog::log (LOGINFO, "chunk " + dec(chunkNum) +
                                  " at " + date::format ("%T", floor<seconds>(getNow())) +
                                  " " + dec(http.getContentSize()));
              //fwrite (http.getContent(), 1, http.getContentSize(), mFile);

              int seqFrameNum = mSong.getHlsFrameFromChunkNum (chunkNum);

              int patNum = 0;
              int pgmNum = 0;
              int vidPesNum = 0;
              int audPesNum = 0;

              uint8_t* vidPes = nullptr;
              int vidPesLen = 0;
              auto aacFrames = http.getContent();
              auto aacFramesPtr = aacFrames;

              uint8_t* ts = http.getContent();
              uint8_t* tsEnd = ts + http.getContentSize();
              while ((ts < tsEnd) && (*ts++ == 0x47)) {
                // ts packet start, dumb ts parser
                auto payStart = ts[0] & 0x40;
                auto pid = ((ts[0] & 0x1F) << 8) | ts[1];
                auto headerBytes = (ts[2] & 0x20) ? 4 + ts[3] : 3;
                ts += headerBytes;
                auto tsBodyBytes = 187 - headerBytes;

                if (pid == 33) {
                  //{{{  vid pes
                  if (payStart && !ts[0] && !ts[1] && (ts[2] == 1) && (ts[3] == 0xe0)) {
                    int pesHeaderBytes = 9 + ts[8];
                    ts += pesHeaderBytes;
                    tsBodyBytes -= pesHeaderBytes;

                    if (vidPes) {
                      // last vidPes at vidFrameNum
                      //mSong.addVideoFrame (vidFrameNum, vidPes, vidPesLen);
                      mVideoDecode.decode (vidPes, vidPesLen, vidFrameNum);

                      vidPes = nullptr;
                      vidPesLen = 0;
                      vidFrameNum++;
                      }

                    vidPesNum++;
                    }

                  // copy ts payload into vidPes buffer, !!!! expensive way !!!!!!
                  vidPes = (uint8_t*)realloc (vidPes, vidPesLen + tsBodyBytes);
                  memcpy (vidPes + vidPesLen, ts, tsBodyBytes);
                  vidPesLen += tsBodyBytes;
                  }
                  //}}}
                else if (pid == 34) {
                  //{{{  aud pes
                  if (payStart && !ts[0] && !ts[1] && (ts[2] == 1) && (ts[3] == 0xC0)) {
                    int pesHeaderBytes = 9 + ts[8];
                    ts += pesHeaderBytes;
                    tsBodyBytes -= pesHeaderBytes;

                    audPesNum++;
                    }

                  // copy ts payload aacFrames back into buffer, for later decode
                  memcpy (aacFramesPtr, ts, tsBodyBytes);
                  aacFramesPtr += tsBodyBytes;
                  }
                  //}}}
                else if (pid == 0) {
                  //{{{  pat
                  if (payStart) {
                    //cLog::log (LOGINFO, "pat");
                    patNum++;
                    }
                  }
                  //}}}
                else if (pid == 32) {
                  //{{{  pgm
                  if (payStart) {
                    //cLog::log (LOGINFO, "pgm");
                    pgmNum++;
                    }
                  }
                  //}}}
                else {
                  //{{{  other
                  // other pid
                  if (payStart)
                    cLog::log (LOGINFO, "other pid:%d header %x %x %x %x headerBytes:%d",
                                        pid, int(ts[0]), int(ts[1]), int(ts[2]), int(ts[3]), headerBytes);
                  }
                  //}}}
                ts += tsBodyBytes;
                }
              cLog::log (LOGINFO, "- pat:" + dec(patNum) + " pgm:" + dec(pgmNum) +
                                  " videoPes:" + dec(vidPesNum) + " audioPes:" + dec(audPesNum));

              while (decode.parseFrame (aacFrames, aacFramesPtr)) {
                float* samples = decode.decodeFrame (seqFrameNum);
                if (samples) {
                  mSong.setFixups (decode.getNumChannels(), decode.getSampleRate(), decode.getNumSamples());
                  mSong.addAudioFrame (seqFrameNum++, samples, true, mSong.getNumFrames());
                  changed();
                  if (firstTime) {
                    firstTime = false;
                    player = thread ([=](){ playThread (true); });
                    }
                  }
                aacFrames += decode.getNextFrameOffset();
                }

              http.freeContent();
              mSong.setHlsLoad (cSong::eHlsIdle, chunkNum);
              }
              //}}}
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

    //fclose (mFile);
    cLog::log (LOGINFO, "exit");
    }
  //}}}
  //{{{
  void playThread (bool streaming) {

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
            mSong.incPlayFrame (1, true);
            mVideoDecode.setPlayFrame (mSong.getBasePlayFrame());
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

  //FILE* mFile = nullptr;
  cMfxVideoDecode mVideoDecode;
  //}}}
  };

int main (int argc, char** argv) {

  cLog::init (LOGINFO, false, "", "hlsWindow");

  cAppWindow appWindow;
  appWindow.run ("hlsWindow", 800, 420, {});

  return 0;
  }
