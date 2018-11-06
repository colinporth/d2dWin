// cPdfImage.h - windows only turbo jpeg decoder
#pragma once
//{{{  includes
#include "iImage.h"

#include "../../shared/utils/utils.h"
#include "../../shared/utils/date.h"

#include "../common/cView2d.h"
//}}}

class cPdfImage : public iImage {
public:
   virtual ~cPdfImage();

  //{{{  gets
  // iImage
  bool isOk() { return true; }
  bool isLoaded() { return mBitmap != nullptr; }

  int getImageLen() { return mImageLen; }

  cPoint getSize() { return mSize; }
  int getWidth() { return mSize.width; }
  int getHeight() { return mSize.height; }
  int getNumComponents() { return 4; }
  int getScale() { return mLoadScale; }
  std::string getDebugString();

  cPoint getImageSize() { return mImageSize; }
  int getImageWidth() { return mImageSize.width; }
  int getImageHeight() { return mImageSize.height; }

  ID2D1Bitmap* getBitmap() { return mBitmap; }

  uint8_t* getBgraBuf() { return mBgraBuf; }
  uint8_t getRed (cPoint pos) { return 0; }
  uint8_t getGreen (cPoint pos) { return 0; }
  uint8_t getBlue (cPoint pos) { return 0; }
  //}}}

  // iImage
  uint32_t loadImage (ID2D1DeviceContext* dc, int scale);
  void releaseImage();

private:
  uint8_t* mBuf = nullptr;
  int mBufLen = 0;
  int mImageLen = 0;
  D2D1_SIZE_U mImageSize = {0,0};
  D2D1_SIZE_U mThumbSize = {0,0};
  D2D1_SIZE_U mSize = {0,0};
  int mLoadScale = 0;
  ID2D1Bitmap* mBitmap = nullptr;
  uint8_t* mBgraBuf = nullptr;
  };
