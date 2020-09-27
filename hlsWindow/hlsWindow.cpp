// hlsWindow.cpp
//{{{  includes
#include "stdafx.h"

#include "../../shared/audio/audioWASAPI.h"
#include "../../shared/utils/cSong.h"
#include "../../shared/decoders/cAudioDecode.h"
#include "../../shared/net/cWinSockHttp.h"

#include "../common/cD2dWindow.h"
#include "../boxes/cWindowBox.h"
#include "../boxes/cClockBox.h"
#include "../boxes/cSongBox.h"
#include "../boxes/cLogBox.h"

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
constexpr int kVidBitrate = 1604032; // 827008 1604032 2812032 5070016

//{{{
class cMfxVideoDecode {
public:
  //{{{
  class cFrame {
  public:
    cFrame (uint64_t pts, char type) : mPts(pts), mType(type), mOk(false) {}
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
    void set (uint64_t pts, char type) {
      mOk = false;
      mPts = pts;
      mType = type;
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
        __m64* srcu64 = (__m64*)(mUbuf + mUVStride*(y/2));
        __m64* srcv64 = (__m64*)(mVbuf + mUVStride*(y/2));
        __m128i* dstrgb128r0 = (__m128i*)(mBgra + argbStride*y);
        __m128i* dstrgb128r1 = (__m128i*)(mBgra + argbStride*y + argbStride);

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
          gbgb = _mm_unpackhi_epi8 (b00, g00 );
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

          r01     = _mm_unpacklo_epi8 (r00,  zero); // 0r0r..
          gbgb    = _mm_unpacklo_epi8 (b00,  g00);  // gbgb..
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

  private:
    bool mOk = false;
    uint64_t mPts = 0;
    char mType = ' ';

    int mYStride = 0;
    int mUVStride = 0;

    int mWidth = 0;
    int mHeight = 0;

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

    MFXVideoDECODE_Close (mSession);
    for (auto surface : mSurfaces)
      delete surface;
    for (auto frame : mFrames)
      delete frame;
    }
  //}}}

  //{{{
  cFrame* findPlayFrame() {

    for (auto frame : mFrames)
      if (frame->getPts()/3600 == mPlayPts/3600)
        return frame;

    return nullptr;
    }
  //}}}
  void setPlayPts (uint64_t playPts) { mPlayPts = playPts; }
  //{{{
  void decode (uint64_t seqNum, uint64_t pts, uint8_t* pes, int pesSize) {

    allocateFrame (pts, getType (pes, pesSize));

    mBitstream.Data = pes;
    mBitstream.DataOffset = 0;
    mBitstream.DataLength = pesSize;
    mBitstream.MaxLength = pesSize;
    mBitstream.TimeStamp = pts;

    if (mSurfaces.empty()) {
      //{{{  allocate decoder surfaces, init decoder, decode header
      mfxVideoParam mVideoParams;
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
      mWidth = ((mfxU32)((frameAllocRequest.Info.Width)+31)) & (~(mfxU32)31);
      mHeight = ((mfxU32)((frameAllocRequest.Info.Height)+31)) & (~(mfxU32)31);

      // alloc surfaces in system memory
      for (int i = 0; i < frameAllocRequest.NumFrameSuggested; i++) {
        auto surface = new mfxFrameSurface1;
        memset (surface, 0, sizeof (mfxFrameSurface1));
        memcpy (&surface->Info, &mVideoParams.mfx.FrameInfo, sizeof(mfxFrameInfo));
        surface->Data.Y = new mfxU8[mWidth * mHeight * 12 / 8];
        surface->Data.U = surface->Data.Y + mWidth * mHeight;
        surface->Data.V = nullptr; // NV12 ignores V pointer
        surface->Data.Pitch = mWidth;
        mSurfaces.push_back (surface);
        }

      if (MFXVideoDECODE_Init (mSession, &mVideoParams) != MFX_ERR_NONE) {
        cLog::log (LOGERROR, "MFXVideoDECODE_Init failed");
        return;
        }
      }
      //}}}

    //mfxStatus status = MFXVideoDECODE_Reset (mSession, &mVideoParams);
    mfxStatus status = MFX_ERR_NONE;
    while ((status >= MFX_ERR_NONE) || (status == MFX_ERR_MORE_SURFACE)) {
      mfxFrameSurface1* surface = nullptr;
      mfxSyncPoint syncDecode = nullptr;
      status = MFXVideoDECODE_DecodeFrameAsync (mSession, &mBitstream, getFreeSurface(), &surface, &syncDecode);
      if (status == MFX_ERR_NONE) {
        status = mSession.SyncOperation (syncDecode, 60000);
        if (status == MFX_ERR_NONE) {
          //cLog::log (LOGINFO, "-> frame pts:%u %dx%d:%d",
          //                    surface->Data.TimeStamp/3600,
          //                    surface->Info.Width, surface->Info.Height, surface->Data.Pitch);
          auto frame = findAllocatedFrame (surface->Data.TimeStamp);
          frame->setNv12 (surface->Data.Y, surface->Info.Width, surface->Info.Height, surface->Data.Pitch);
          }
        }
      }
    }
  //}}}

private:
  //{{{
  static char getType (uint8_t* pes, int64_t pesSize) {
  // return frameType of video pes

    //{{{
    class cBitstream {
    // used to parse H264 stream to find I frames
    public:
      cBitstream (const uint8_t* buffer, uint32_t bit_len) :
        mDecBuffer(buffer), mDecBufferSize(bit_len), mNumOfBitsInBuffer(0), mBookmarkOn(false) {}

      //{{{
      uint32_t peekBits (uint32_t bits) {

        bookmark (true);
        uint32_t ret = getBits (bits);
        bookmark (false);
        return ret;
        }
      //}}}
      //{{{
      uint32_t getBits (uint32_t numBits) {

        //{{{
        static const uint32_t msk[33] = {
          0x00000000, 0x00000001, 0x00000003, 0x00000007,
          0x0000000f, 0x0000001f, 0x0000003f, 0x0000007f,
          0x000000ff, 0x000001ff, 0x000003ff, 0x000007ff,
          0x00000fff, 0x00001fff, 0x00003fff, 0x00007fff,
          0x0000ffff, 0x0001ffff, 0x0003ffff, 0x0007ffff,
          0x000fffff, 0x001fffff, 0x003fffff, 0x007fffff,
          0x00ffffff, 0x01ffffff, 0x03ffffff, 0x07ffffff,
          0x0fffffff, 0x1fffffff, 0x3fffffff, 0x7fffffff,
          0xffffffff
          };
        //}}}

        if (numBits == 0)
          return 0;

        uint32_t retData;
        if (mNumOfBitsInBuffer >= numBits) {  // don't need to read from FILE
          mNumOfBitsInBuffer -= numBits;
          retData = mDecData >> mNumOfBitsInBuffer;
          // wmay - this gets done below...retData &= msk[numBits];
          }
        else {
          uint32_t nbits;
          nbits = numBits - mNumOfBitsInBuffer;
          if (nbits == 32)
            retData = 0;
          else
            retData = mDecData << nbits;

          switch ((nbits - 1) / 8) {
            case 3:
              nbits -= 8;
              if (mDecBufferSize < 8)
                return 0;
              retData |= *mDecBuffer++ << nbits;
              mDecBufferSize -= 8;
              // fall through
            case 2:
              nbits -= 8;
              if (mDecBufferSize < 8)
                return 0;
              retData |= *mDecBuffer++ << nbits;
              mDecBufferSize -= 8;
            case 1:
              nbits -= 8;
              if (mDecBufferSize < 8)
                return 0;
              retData |= *mDecBuffer++ << nbits;
              mDecBufferSize -= 8;
            case 0:
              break;
            }
          if (mDecBufferSize < nbits)
            return 0;

          mDecData = *mDecBuffer++;
          mNumOfBitsInBuffer = min(8u, mDecBufferSize) - nbits;
          mDecBufferSize -= min(8u, mDecBufferSize);
          retData |= (mDecData >> mNumOfBitsInBuffer) & msk[nbits];
          }

        return (retData & msk[numBits]);
        };
      //}}}

      //{{{
      uint32_t getUe() {

        uint32_t bits;
        uint32_t read;
        int bits_left;
        bool done = false;
        bits = 0;

        // we want to read 8 bits at a time - if we don't have 8 bits,
        // read what's left, and shift.  The exp_golomb_bits calc remains the same.
        while (!done) {
          bits_left = bits_remain();
          if (bits_left < 8) {
            read = peekBits (bits_left) << (8 - bits_left);
            done = true;
            }
          else {
            read = peekBits (8);
            if (read == 0) {
              getBits (8);
              bits += 8;
              }
            else
             done = true;
            }
          }

        uint8_t coded = exp_golomb_bits[read];
        getBits (coded);
        bits += coded;

        return getBits (bits + 1) - 1;
        }
      //}}}
      //{{{
      int32_t getSe() {

        uint32_t ret;
        ret = getUe();
        if ((ret & 0x1) == 0) {
          ret >>= 1;
          int32_t temp = 0 - ret;
          return temp;
          }

        return (ret + 1) >> 1;
        }
      //}}}

      //{{{
      void check_0s (int count) {

        uint32_t val = getBits (count);
        if (val != 0)
          cLog::log (LOGERROR, "field error - %d bits should be 0 is %x", count, val);
        }
      //}}}
      //{{{
      int bits_remain() {
        return mDecBufferSize + mNumOfBitsInBuffer;
        };
      //}}}
      //{{{
      int byte_align() {

        int temp = 0;
        if (mNumOfBitsInBuffer != 0)
          temp = getBits (mNumOfBitsInBuffer);
        else {
          // if we are byte aligned, check for 0x7f value - this will indicate
          // we need to skip those bits
          uint8_t readval = peekBits (8);
          if (readval == 0x7f)
            readval = getBits (8);
          }

        return temp;
        };
      //}}}

    private:
      //{{{
      const uint8_t exp_golomb_bits[256] = {
        8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 3,
        3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2,
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0,
        };
      //}}}

      //{{{
      void bookmark (bool on) {

        if (on) {
          mNumOfBitsInBuffer_bookmark = mNumOfBitsInBuffer;
          mDecBuffer_bookmark = mDecBuffer;
          mDecBufferSize_bookmark = mDecBufferSize;
          mBookmarkOn = 1;
          mDecData_bookmark = mDecData;
          }

        else {
          mNumOfBitsInBuffer = mNumOfBitsInBuffer_bookmark;
          mDecBuffer = mDecBuffer_bookmark;
          mDecBufferSize = mDecBufferSize_bookmark;
          mDecData = mDecData_bookmark;
          mBookmarkOn = 0;
          }

        };
      //}}}

      const uint8_t* mDecBuffer;
      uint32_t mDecBufferSize;
      uint32_t mNumOfBitsInBuffer;
      bool mBookmarkOn;

      uint8_t mDecData_bookmark = 0;
      uint8_t mDecData = 0;

      uint32_t mNumOfBitsInBuffer_bookmark = 0;
      const uint8_t* mDecBuffer_bookmark = 0;
      uint32_t mDecBufferSize_bookmark = 0;
      };
    //}}}

    // h264 minimal parser
    auto pesEnd = pes + pesSize;
    while (pes < pesEnd) {
      //{{{  skip past startcode, find next startcode
      auto buf = pes;
      auto bufSize = pesSize;

      uint32_t startOffset = 0;
      if (!buf[0] && !buf[1]) {
        if (!buf[2] && buf[3] == 1) {
          buf += 4;
          startOffset = 4;
          }
        else if (buf[2] == 1) {
          buf += 3;
          startOffset = 3;
          }
        }

      // find next start code
      auto offset = startOffset;
      uint32_t nalSize = offset;
      uint32_t val = 0xffffffff;
      while (offset++ < bufSize - 3) {
        val = (val << 8) | *buf++;
        if (val == 0x0000001) {
          nalSize = offset - 4;
          break;
          }
        if ((val & 0x00ffffff) == 0x0000001) {
          nalSize = offset - 3;
          break;
          }

        nalSize = (uint32_t)bufSize;
        }
      //}}}

      if (nalSize > 3) {
        // parse NAL bitStream
        cBitstream bitstream (buf, (nalSize - startOffset) * 8);
        bitstream.check_0s (1);
        bitstream.getBits (2);
        switch (bitstream.getBits (5)) {
          case 1:
          case 5:
            bitstream.getUe();
            switch (bitstream.getUe()) {
              case 5: return 'P';
              case 6: return 'B';
              case 7: return 'I';
              default:return '?';
              }
            break;
          //case 6: cLog::log(LOGINFO, ("SEI"); break;
          //case 7: cLog::log(LOGINFO, ("SPS"); break;
          //case 8: cLog::log(LOGINFO, ("PPS"); break;
          //case 9: cLog::log(LOGINFO,  ("AUD"); break;
          //case 0x0d: cLog::log(LOGINFO, ("SEQEXT"); break;
          }
        }

      pes += nalSize;
      }

    return '?';
    }
  //}}}

  //{{{
  mfxFrameSurface1* getFreeSurface() {
  // return first unlocked surface;

    for (auto surface : mSurfaces)
      if (!surface->Data.Locked)
        return surface;

    return nullptr;
    }
  //}}}
  //{{{
  cFrame* allocateFrame (uint64_t pts, char type) {
  // return first frame older than mPlayPts, otherwise add new frame

    for (auto frame : mFrames)
      if (frame->ok() && (frame->getPts() < mPlayPts)) {
        frame->set (pts, type);
        return frame;
        }

    // allocate new frame
    mFrames.push_back (new cFrame (pts, type));

    //cLog::log (LOGINFO, "allocating new frame %d for %u at play:%u", mFrames.size(), pts, mPlayPts);
    return mFrames.back();
    }
  //}}}
  //{{{
  cFrame* findAllocatedFrame (uint64_t pts) {

    for (auto frame : mFrames)
      if (frame->getPts() == pts)
        return frame;

    return nullptr;
    }
  //}}}

  MFXVideoSession mSession;
  mfxBitstream mBitstream;
  vector <mfxFrameSurface1*> mSurfaces;
  int mWidth = 0;
  int mHeight = 0;

  vector <cFrame*> mFrames;
  uint64_t mPlayPts = 0;
  };
//}}}
//{{{
class cVideoDecodeBox : public cD2dWindow::cView {
public:
  cVideoDecodeBox (cD2dWindow* window, float width, float height, cMfxVideoDecode& videoDecode)
    : cView("videoDecode", window, width, height), mVideoDecode(videoDecode) {}
  virtual ~cVideoDecodeBox() {}

  void onDraw (ID2D1DeviceContext* dc) {

    auto frame = mVideoDecode.findPlayFrame();
    if (frame) {
      if (frame->getPts() != mPts) {
        //cLog::log (LOGINFO, "onDraw show:%u was:%u", frame->getPts(), mPts);
        // new Frame, update bitmap
        if (mBitmap)  {
          auto pixelSize = mBitmap->GetPixelSize();
          if ((pixelSize.width != frame->getWidth()) || (pixelSize.height != frame->getHeight())) {
            // bitmap size changed, remove, then recreate
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

        mPts = frame->getPts();
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
  uint64_t mPts = 0;
  };
//}}}

class cAppWindow : public cD2dWindow {
public:
  //{{{
  void run (const string& title, int width, int height) {

    init (title, width, height, false);
    add (new cVideoDecodeBox (this, 0.f,0.f, mVideoDecode), 0.f,0.f);
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
  void hlsThread (const string& host, const string& chan, int bitrate) {
  // hls chunk http load and analyse thread, single thread helps chan change and jumping backwards

    cLog::setThreadName ("hls ");
    int vidFrameNum = 0;

    //mFile = fopen ("C:/Users/colin/Desktop/hls.ts", "wb");
    constexpr int kHlsPreload = 2;

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
              //{{{  chunk loaded, process it
              //cLog::log (LOGINFO, "chunk " + dec(chunkNum) +
              //                    " at " + date::format ("%T", floor<seconds>(getNow())) +
              //                    " " + dec(http.getContentSize()));
              //fwrite (http.getContent(), 1, http.getContentSize(), mFile);

              int seqFrameNum = mSong.getHlsFrameFromChunkNum (chunkNum);

              int vidPesNum = 0;
              int audPesNum = 0;

              uint8_t* vidPes = nullptr;
              int vidPesLen = 0;
              uint64_t vidPts = 0;

              auto aacFrames = http.getContent();
              auto aacFramesPtr = aacFrames;
              uint64_t audPts = 0;

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
                    // new vidPes start
                    if (vidPes) // process last vidPes
                      mVideoDecode.decode (vidFrameNum++, vidPts, vidPes, vidPesLen);
                      //mSong.addVideoFrame (vidPes, vidPesLen, vidPts);

                    if (ts[7] & 0x80) // has vid pts
                      vidPts = getPts (ts+9);

                    int pesHeaderBytes = 9 + ts[8];
                    ts += pesHeaderBytes;
                    tsBodyBytes -= pesHeaderBytes;

                    vidPes = nullptr;
                    vidPesLen = 0;
                    vidPesNum++;
                    }

                  // copy ts payload into vidPes buffer, !!!! expensive way !!!!!!
                  vidPes = (uint8_t*)realloc (vidPes, vidPesLen + tsBodyBytes);
                  memcpy (vidPes + vidPesLen, ts, tsBodyBytes);
                  vidPesLen += tsBodyBytes;
                  }
                  //}}}
                else if (pid == 34) {
                  //{{{  audio pes
                  if (payStart && !ts[0] && !ts[1] && (ts[2] == 1) && (ts[3] == 0xC0)) {
                    // aud pes start
                    if (ts[7] & 0x80) // has aud pts
                      if (!audPts) // ony need first of chunk
                        audPts = getPts (ts+9);

                    //cLog::log (LOGINFO, "audioPes %u", audPts);

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
                ts += tsBodyBytes;
                }

              // process any outstanding vidPes
              if (vidPes)
                mVideoDecode.decode (vidFrameNum++, vidPts, vidPes, vidPesLen);

              // process whole chunk of audFrames, !!!! could do this above for each pes like vid !!!!
              while (audioDecode.parseFrame (aacFrames, aacFramesPtr)) {
                float* samples = audioDecode.decodeFrame (seqFrameNum);
                if (samples) {
                  mSong.setFixups (audioDecode.getNumChannels(), audioDecode.getSampleRate(), audioDecode.getNumSamples());
                  mSong.addAudioFrame (seqFrameNum++, samples, true, mSong.getNumFrames(), nullptr, audPts);
                  audPts += (audioDecode.getNumSamples() * 90) / 48;
                  changed();
                  if (firstTime) {
                    firstTime = false;
                    player = thread ([=](){ playThread (true); });
                    }
                  }
                aacFrames += audioDecode.getNextFrameOffset();
                }

              cLog::log (LOGINFO, "chunk:%d vidPes:%d audPes:%d vidPts:%u audPts:%u",
                                  chunkNum, vidPesNum, audPesNum, vidPts, audPts);
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
            mVideoDecode.setPlayPts (framePtr->getPts());
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

  //FILE* mFile = nullptr;
  cMfxVideoDecode mVideoDecode;
  //}}}
  };

// main
int main (int argc, char** argv) {
  cLog::init (LOGINFO, false, "", "hlsWindow");
  cAppWindow appWindow;
  appWindow.run ("hlsWindow", 800, 420);
  return 0;
  }
