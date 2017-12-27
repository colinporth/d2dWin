// cBmpBox.h
#pragma once
#include "../cD2dWindow.h"

class cBmpBox : public cD2dWindow::cBox {
public:
  //{{{
  cBmpBox (cD2dWindow* window, float width, float height, const uint8_t* bmp,
           int myValue, int& value, bool& valueChanged)
      : cBox("bmp", window, width, height), mBmp(bmp),
        mMyValue(myValue), mValue(value), mValueChanged(valueChanged) {

    mPin = true;

    mSizeX = *(bmp + 0x12);
    mSizeY = *(bmp + 0x16);

    window->getDc()->CreateBitmap (SizeU(mSizeX, mSizeY),
                                   { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE, 0,0 },
                                   &mBitmap);

    auto bmpPtr = (uint8_t*)mBmp+54;
    auto line = (uint8_t*)malloc (mSizeX*4);
    for (auto y = 0; y < mSizeY; y++) {
      auto linePtr = line;
      for (auto x = 0; x < mSizeX; x++) {
        *linePtr++ = *bmpPtr++;
        *linePtr++ = *bmpPtr++;
        *linePtr++ = *bmpPtr++;
        *linePtr++ = 255;
        }
      mBitmap->CopyFromMemory (&RectU (0, y, mSizeX, y+1), line, mSizeX*4);
      }
    free (line);
    }
  //}}}
  virtual ~cBmpBox() {}

  //{{{
  bool onDown (bool right, cPoint pos)  {

    mValue = mMyValue;
    mValueChanged = true;
    return true;
    }
  //}}}

  void onDraw (ID2D1DeviceContext* dc) {
    dc->DrawBitmap (mBitmap, mRect);
    }

private:
  const uint8_t* mBmp;
  int mMyValue;

  int& mValue;
  bool& mValueChanged;

  ID2D1Bitmap* mBitmap = nullptr;
  int mSizeX = 0;
  int mSizeY = 0;
  };
