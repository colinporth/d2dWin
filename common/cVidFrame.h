// cVidFrame.h
#pragma once
#include "iFrame.h"

class cVidFrame : public iFrame {
public:
  //{{{
  virtual ~cVidFrame() { 
    _aligned_free (mYbuf);
    _aligned_free (mUbuf);
    _aligned_free (mVbuf);
    _aligned_free (mBgra);
    }
  //}}}

  //{{{
  uint32_t* getBgra() {

    if (mNv12) {
      // unpack NV12 to planar uv
      mUbuf = (uint8_t*)_aligned_realloc (mUbuf, (mHeight/2) * mUVStride, 128);
      mVbuf = (uint8_t*)_aligned_realloc (mVbuf, (mHeight/2) * mUVStride, 128);

      auto uv = mYbuf + (mHeight * mYStride);
      auto u = mUbuf;
      auto v = mVbuf;
      for (auto i = 0; i < mHeight/2 * mUVStride; i++) {
        *u++ = *uv++;
        *v++ = *uv++;
        }
      }

    mBgra = (uint32_t*)_aligned_realloc (mBgra, mWidth * 4 * mHeight, 128);
    auto argbStride = mWidth;

    __m128i y0r0, y0r1, u0, v0;
    __m128i y00r0, y01r0, y00r1, y01r1;
    __m128i u00, u01, v00, v01;
    __m128i rv00, rv01, gu00, gu01, gv00, gv01, bu00, bu01;
    __m128i r00, r01, g00, g01, b00, b01;
    __m128i rgb0123, rgb4567, rgb89ab, rgbcdef;
    __m128i gbgb;
    __m128i ysub, uvsub;
    __m128i zero, facy, facrv, facgu, facgv, facbu;
    __m128i *srcy128r0, *srcy128r1;
    __m128i *dstrgb128r0, *dstrgb128r1;
    __m64   *srcu64, *srcv64;

    int x, y;

    ysub  = _mm_set1_epi32 (0x00100010);
    uvsub = _mm_set1_epi32 (0x00800080);

    facy  = _mm_set1_epi32 (0x004a004a);
    facrv = _mm_set1_epi32 (0x00660066);
    facgu = _mm_set1_epi32 (0x00190019);
    facgv = _mm_set1_epi32 (0x00340034);
    facbu = _mm_set1_epi32 (0x00810081);

    zero  = _mm_set1_epi32( 0x00000000 );

    for (y = 0; y < mHeight; y += 2) {
      srcy128r0 = (__m128i *)(mYbuf + mYStride*y);
      srcy128r1 = (__m128i *)(mYbuf + mYStride*y + mYStride);
      srcu64 = (__m64 *)(mUbuf + mUVStride*(y/2));
      srcv64 = (__m64 *)(mVbuf + mUVStride*(y/2));

      dstrgb128r0 = (__m128i *)(mBgra + argbStride*y);
      dstrgb128r1 = (__m128i *)(mBgra + argbStride*y + argbStride);

      for (x = 0; x < mWidth; x += 16) {
        u0 = _mm_loadl_epi64 ((__m128i *)srcu64 ); srcu64++;
        v0 = _mm_loadl_epi64 ((__m128i *)srcv64 ); srcv64++;

        y0r0 = _mm_load_si128( srcy128r0++ );
        y0r1 = _mm_load_si128( srcy128r1++ );

        // constant y factors
        y00r0 = _mm_mullo_epi16 (_mm_sub_epi16 (_mm_unpacklo_epi8 (y0r0, zero), ysub), facy);
        y01r0 = _mm_mullo_epi16 (_mm_sub_epi16 (_mm_unpackhi_epi8 (y0r0, zero), ysub), facy);
        y00r1 = _mm_mullo_epi16 (_mm_sub_epi16 (_mm_unpacklo_epi8 (y0r1, zero), ysub), facy);
        y01r1 = _mm_mullo_epi16 (_mm_sub_epi16 (_mm_unpackhi_epi8 (y0r1, zero), ysub), facy);

        // expand u and v so they're aligned with y values
        u0  = _mm_unpacklo_epi8 (u0, zero);
        u00 = _mm_sub_epi16 (_mm_unpacklo_epi16 (u0, u0), uvsub);
        u01 = _mm_sub_epi16 (_mm_unpackhi_epi16 (u0, u0), uvsub);

        v0  = _mm_unpacklo_epi8( v0,  zero );
        v00 = _mm_sub_epi16 (_mm_unpacklo_epi16 (v0, v0), uvsub);
        v01 = _mm_sub_epi16 (_mm_unpackhi_epi16 (v0, v0), uvsub);

        // common factors on both rows.
        rv00 = _mm_mullo_epi16 (facrv, v00);
        rv01 = _mm_mullo_epi16 (facrv, v01);
        gu00 = _mm_mullo_epi16 (facgu, u00);
        gu01 = _mm_mullo_epi16 (facgu, u01);
        gv00 = _mm_mullo_epi16 (facgv, v00);
        gv01 = _mm_mullo_epi16 (facgv, v01);
        bu00 = _mm_mullo_epi16 (facbu, u00);
        bu01 = _mm_mullo_epi16 (facbu, u01);

        // row 0
        r00 = _mm_srai_epi16 (_mm_add_epi16 (y00r0, rv00), 6);
        r01 = _mm_srai_epi16 (_mm_add_epi16 (y01r0, rv01), 6);
        g00 = _mm_srai_epi16 (_mm_sub_epi16 (_mm_sub_epi16 (y00r0, gu00), gv00), 6);
        g01 = _mm_srai_epi16 (_mm_sub_epi16 (_mm_sub_epi16 (y01r0, gu01), gv01), 6);
        b00 = _mm_srai_epi16 (_mm_add_epi16 (y00r0, bu00), 6);
        b01 = _mm_srai_epi16 (_mm_add_epi16 (y01r0, bu01), 6);

        r00 = _mm_packus_epi16 (r00, r01);         // rrrr.. saturated
        g00 = _mm_packus_epi16 (g00, g01);         // gggg.. saturated
        b00 = _mm_packus_epi16 (b00, b01);         // bbbb.. saturated

        r01     = _mm_unpacklo_epi8 (r00, zero); // 0r0r..
        gbgb    = _mm_unpacklo_epi8 (b00, g00);  // gbgb..
        rgb0123 = _mm_unpacklo_epi16 (gbgb, r01);  // 0rgb0rgb..
        rgb4567 = _mm_unpackhi_epi16 (gbgb, r01);  // 0rgb0rgb..

        r01     = _mm_unpackhi_epi8 (r00, zero);
        gbgb    = _mm_unpackhi_epi8 (b00, g00 );
        rgb89ab = _mm_unpacklo_epi16 (gbgb, r01);
        rgbcdef = _mm_unpackhi_epi16 (gbgb, r01);

        _mm_stream_si128 (dstrgb128r0++, rgb0123);
        _mm_stream_si128 (dstrgb128r0++, rgb4567);
        _mm_stream_si128 (dstrgb128r0++, rgb89ab);
        _mm_stream_si128 (dstrgb128r0++, rgbcdef);

        // row 1
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
        }
      }

    return mBgra;
    }
  //}}}

  //{{{
  void setPes (int64_t pts, int64_t ptsWidth, int pesSize, char frameType) {

    mOk = false;

    mPts = pts;
    mPtsEnd = pts + ptsWidth;

    mPesSize = pesSize;
    mFrameType = frameType;
    }
  //}}}
  //{{{
  void setYuv (uint8_t** yuv, int* strides, int width, int height) {

    mWidth = width;
    mHeight = height;

    mNv12 = false;
    mYStride = strides[0];
    mUVStride = strides[1];

    // copy
    mYbuf = (uint8_t*)_aligned_realloc (mYbuf, height * mYStride, 128);
    memcpy (mYbuf, yuv[0], height * mYStride);
    mUbuf = (uint8_t*)_aligned_realloc (mUbuf, (height/2) * mUVStride, 128);
    memcpy (mUbuf, yuv[1], (height/2) * mUVStride);
    mVbuf = (uint8_t*)_aligned_realloc (mVbuf, (height/2) * mUVStride, 128);
    memcpy (mVbuf, yuv[2], (height/2) * mUVStride);

    mOk = true;
    }
  //}}}
  //{{{
  void setNv12 (uint8_t* nv12, int stride, int width, int height) {

    mWidth = width;
    mHeight = height;

    mNv12 = true;
    mYStride = stride;
    mUVStride = stride/2;

    // copy all of Nv12 to y buf
    mYbuf = (uint8_t*)_aligned_realloc (mYbuf, height * mYStride * 3 / 2, 128);
    memcpy (mYbuf, nv12, height * mYStride * 3 / 2);

    mOk = true;
    }
  //}}}

  //{{{
  ID2D1Bitmap* makeBitmap (ID2D1DeviceContext* dc, ID2D1Bitmap* bitmap) {

    if (bitmap)  {
      auto pixelSize = bitmap->GetPixelSize();
      if ((pixelSize.width != mWidth) || (pixelSize.height != mHeight)) {
        bitmap->Release();
        bitmap = nullptr;
        }
      }
    if (!bitmap) // create bitmap
      dc->CreateBitmap (SizeU(mWidth, mHeight),
                        { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE, 0,0 },
                        &bitmap);
    bitmap->CopyFromMemory (&RectU(0, 0, mWidth, mHeight), getBgra(), mWidth * 4);

    return bitmap;
    }
  //}}}

  //{{{
  void invalidate() {

    mOk = false;
    mPts = 0;
    mPesSize = 0;
    mFrameType = '?';
    }
  //}}}

  // vars
  int mPesSize = 0;

  bool mNv12 = false;
  char mFrameType = 0;

  int mWidth = 0;
  int mHeight = 0;

  int mYStride = 0;
  int mUVStride= 0;

  uint8_t* mYbuf = nullptr;
  uint8_t* mUbuf = nullptr;
  uint8_t* mVbuf = nullptr;

  uint32_t* mBgra = nullptr;
  };
