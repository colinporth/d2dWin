// cToggleBox.h
//{{{  includes
#pragma once
#include "../cD2dWindow.h"
//}}}

class cToggleBox : public cD2dWindow::cBox {
public:
  //{{{
  cToggleBox (cD2dWindow* window, float width, float height, const string& title, bool& value, bool& changed)
      : cBox("offset", window, width, height), mTitle(title), mValue(value), mChanged(changed) {
    mPin = true;
    mChanged = false;
    }
  //}}}
  virtual ~cToggleBox() {}

  //{{{
  bool onDown (bool right, cPoint pos)  {
    mValue = !mValue;
    mChanged = true;
    return true;
    }
  //}}}

  void onDraw (ID2D1DeviceContext* dc) {
    string str = mTitle;
    dc->FillRectangle (mRect, mValue ? mWindow->getBlueBrush() : mWindow->getGreyBrush());
    dc->DrawText (wstring (str.begin(), str.end()).data(), (uint32_t)str.size(), mWindow->getTextFormat(),
                  mRect, mValue ? mWindow->getBlackBrush() : mWindow->getWhiteBrush());
    }

private:
  string mTitle;
  bool& mChanged;
  bool& mValue;
  };
