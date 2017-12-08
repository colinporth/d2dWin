// cBitmap.h
#pragma once
#include "cD2dWindow.h"

class cBitmapBox : public cD2dWindow::cBox {
public:
  //{{{
  cBitmapBox (cD2dWindow* window, float width, float height, ID2D1Bitmap*& bitmap)
      : cBox("bitmap", window, width, height), mBitmap(bitmap) {

    mPin = true;
    }
  //}}}
  virtual ~cBitmapBox() {}

  void onDraw (ID2D1DeviceContext* dc) {
    if (mBitmap)
      dc->DrawBitmap (mBitmap, cRect (mRect.getTL(), mBitmap->GetSize()));
    }

private:
  ID2D1Bitmap*& mBitmap;
  };
