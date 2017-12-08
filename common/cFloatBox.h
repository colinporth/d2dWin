// cInfoBox.h
#pragma once
#include "cD2dWindow.h"
#include "../../shared/utils/utils.h"
#include "../../shared/utils/cLog.h"

class cFloatBox : public cD2dWindow::cBox {
public:
  //{{{
  cFloatBox (cD2dWindow* window, float width, float height, float& value, int digits = 6, int precision = 2)
      : cBox("float", window, width, height), mValue(value), mDigits(digits), mPrecision(precision) {
    mPin = true;
    }
  //}}}
  //{{{
  cFloatBox (cD2dWindow* window, float width, float height, string title, float& value, int digits = 6, int precision = 3)
      : cBox("float", window, width, height), mTitle(title), mValue(value), mDigits(digits), mPrecision(precision) {
    mPin = true;
    }
  //}}}
  virtual ~cFloatBox() {}

  void onDraw (ID2D1DeviceContext* dc) {

    string str = mTitle + decFrac (mValue, mDigits, mPrecision, '0');

    IDWriteTextLayout* textLayout;
    mWindow->getDwriteFactory()->CreateTextLayout (
      wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
      mWindow->getTextFormat(), getWidth(), getHeight(), &textLayout);

    dc->DrawTextLayout (getTL (2.f), textLayout, mWindow->getBlackBrush());
    dc->DrawTextLayout (getTL(), textLayout, mWindow->getWhiteBrush());

    textLayout->Release();
    }

private:
  string mTitle;
  float& mValue;
  int mDigits;
  int mPrecision;
  };

class cFloatBgndBox : public cFloatBox {
public:
  //{{{
  cFloatBgndBox (cD2dWindow* window, float width, float height, string title, float& value) :
      cFloatBox(window, width, height, title, value) {
    }
  //}}}
  virtual ~cFloatBgndBox() {}

  void onDraw (ID2D1DeviceContext* dc) {

    dc->FillRectangle (mRect, mWindow->getGreyBrush());
    cFloatBox::onDraw (dc);
    }
  };
