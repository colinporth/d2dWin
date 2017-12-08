// cDoubleBox.h
#pragma once
#include "cD2dWindow.h"
#include "../../shared/utils/utils.h"
#include "../../shared/utils/cLog.h"

class cDoubleBox : public cD2dWindow::cBox {
public:
  //{{{
  cDoubleBox (cD2dWindow* window, float width, float height, double& value, int digits = 6, int precision = 2) :
      cBox("double", window, width, height), mValue(value), mDigits(digits), mPrecision(precision) {
    mPin = true;
    }
  //}}}
  //{{{
  cDoubleBox (cD2dWindow* window, float width, float height, const string& title, double& value, int digits = 6, int precision = 3) :
      cBox("double", window, width, height), mTitle(title), mValue(value), mDigits(digits), mPrecision(precision) {
    mPin = true;
    }
  //}}}
  virtual ~cDoubleBox() {}

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
  double& mValue;
  string mTitle;
  int mDigits;
  int mPrecision;
  };

class cDoubleBgndBox : public cDoubleBox {
public:
  //{{{
  cDoubleBgndBox (cD2dWindow* window, float width, float height, string title, double& value) :
      cDoubleBox(window, width, height, title, value) {
    }
  //}}}
  virtual ~cDoubleBgndBox() {}

  void onDraw (ID2D1DeviceContext* dc) {

    dc->FillRectangle (mRect, mWindow->getGreyBrush());
    cDoubleBox::onDraw (dc);
    }
  };
