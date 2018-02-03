// cIndexBox.h
#pragma once
#include "../common/cD2dWindow.h"

class cIndexBox : public cD2dWindow::cBox {
public:
  //{{{
  cIndexBox (cD2dWindow* window, float width, float height, vector<string> strings, int& index, bool* changed)
      : cBox("offset", window, width, height), mStrings(strings), mIndex(index), mChanged(changed) {
    mPin = true;
    mChanged = false;
    }
  //}}}
  //{{{
  cIndexBox (cD2dWindow* window, float width, float height, vector<string> strings, int& index, cSemaphore* sem)
      : cBox("offset", window, width, height), mStrings(strings), mIndex(index), mSem(sem) {
    mPin = true;
    mChanged = false;
    }
  //}}}
  virtual ~cIndexBox() {}

  //{{{
  bool onWheel (int delta, cPoint pos)  {
    setIndex (mIndex + delta/120);
    return true;
    }
  //}}}
  //{{{
  bool onDown (bool right, cPoint pos)  {
    setIndex (int(pos.y / kLineHeight));
    return true;
    }
  //}}}
  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    auto r = mRect;
    r.bottom = r.top + kLineHeight;

    for (int index = 0; index < (int)mStrings.size(); index++) {
      string str = mStrings[index];
      dc->FillRectangle (r, index == mIndex ? mWindow->getBlueBrush() : mWindow->getGreyBrush());
      dc->DrawText (wstring (str.begin(), str.end()).data(), (uint32_t)str.size(), mWindow->getTextFormat(),
                    r, index == mIndex ? mWindow->getBlackBrush() : mWindow->getWhiteBrush());
      r.top = r.bottom;
      r.bottom += kLineHeight;
      }
    }
  //}}}

private:
  //{{{
  void setIndex (int index) {
    mIndex = index;
    mIndex = max (mIndex, 0);
    mIndex = min (mIndex, (int)mStrings.size()-1);
    if (mChanged)
      *mChanged = true;
    if (mSem)
      mSem->notify();
    }
  //}}}

  vector<string> mStrings;
  int& mIndex;
  bool* mChanged = nullptr;
  cSemaphore* mSem;
  };
