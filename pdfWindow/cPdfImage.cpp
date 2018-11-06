// cPdfImage.cpp - windows only jpeg file decoder
//{{{  includes
#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX

#include <vector>

#include "cPdfImage.h"

#include "../../shared/utils/cLog.h"

using namespace std;
using namespace chrono;
//}}}

// public
cPdfImage::~cPdfImage() { }

//{{{
string cPdfImage::getDebugString() {
  return "cPdfImage debugStr";
  }
//}}}

//{{{
uint32_t cPdfImage::loadImage (ID2D1DeviceContext* dc, int scale) {

  auto time = system_clock::now();

  //ID2D1Bitmap* bitmap;
  //dc->CreateBitmap (D2D1::SizeU (cinfo.output_width, cinfo.output_height), bgraBuf, pitch,
  //                  { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE, 0,0 }, &bitmap);

  //  switch bitmap, bgraBuf, set sizes
  //if (mBgraBuf)
  //  free (mBgraBuf);
  //mBgraBuf = bgraBuf;
  //if (mBitmap) {
  //  auto bitmapSize = mBitmap->GetPixelSize();
  //  allocSize -= bitmapSize.width * bitmapSize.height * 4;
  //  mBitmap->Release();
  //  }
  //mBitmap = bitmap;

  //mSize.width = cinfo.output_width;
  //mSize.height = cinfo.output_height;
  //mImageSize.width = cinfo.image_width;
  //mImageSize.height = cinfo.image_height;
  //
  //allocSize += cinfo.output_width * cinfo.output_height * 4;

  //mLoadTime = (float)duration_cast<milliseconds>(system_clock::now() - time).count();
  //cLog::log (LOGINFO3, "cPdfImage::loadImage - scale " + dec(scale) + " took " + dec (mLoadTime) + "ms " + getFileName());

  return 0;
  }
//}}}
//{{{
void cPdfImage::releaseImage() {

  if (mBitmap) {
    mBitmap->Release();
    mBitmap = nullptr;
    }

  free (mBgraBuf);
  mBgraBuf = nullptr;
  }
//}}}
