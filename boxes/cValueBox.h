// cValueBox.h
//{{{  includes
#pragma once

#include "../common/cD2dWindow.h"
#include "../../shared/utils/utils.h"
#include "../../shared/utils/cLog.h"
//}}}

class cValueBox : public cD2dWindow::cBox {
public:
  //{{{
  cValueBox (cD2dWindow* window, float width, float height, const string& title, float min, float max, float& value, bool& changed)
      : cBox("offset", window, width, height), mTitle(title), mMin(min), mMax(max),
        mValue(value), mChanged(changed) {
    mPin = true;
    mChanged = false;
    }
  //}}}
  virtual ~cValueBox() {}

  //{{{
  bool onWheel (int delta, cPoint pos)  {
    setValue (mValue + delta/120);
    return true;
    }
  //}}}
  //{{{
  bool onMove (bool right, cPoint pos, cPoint inc) {
    setValue (mValue + inc.x - inc.y);
    return true;
    }
  //}}}

  void onDraw (ID2D1DeviceContext* dc) {
    string str = mTitle + " " + dec (mValue);
    dc->FillRectangle (mRect, mPick ? mWindow->getYellowBrush() : mWindow->getGreyBrush());
    dc->DrawText (wstring (str.begin(), str.end()).data(), (uint32_t)str.size(), mWindow->getTextFormat(),
                  mRect, mPick ? mWindow->getBlackBrush() : mWindow->getWhiteBrush());
    }

private:
  //{{{
  void setValue (float value) {
    mValue = value;
    mValue = max (mValue, mMin);
    mValue = min (mValue, mMax);
    mChanged = true;
    }
  //}}}

  string mTitle;
  bool& mChanged;
  float& mValue;
  float mMin = 0.f;
  float mMax = 100.f;
  };
