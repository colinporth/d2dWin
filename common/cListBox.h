// cListBox.h
#pragma once
#include "cD2dWindow.h"
#include "../../shared/utils/utils.h"
#include "../../shared/utils/cLog.h"

class cListBox : public cD2dWindow::cBox {
public:
  //{{{
  cListBox (cD2dWindow* window, float width, float height, vector<string>& names, int& index, bool& indexChanged)
      : cBox ("list", window, width, height), mNames(names), mIndex(index), mIndexChanged(indexChanged) {
    mPin = true;
    }
  //}}}
  virtual ~cListBox() {}

  //{{{
  bool onDown (bool right, cPoint pos)  {

    mMoved = false;
    mMoveInc = 0;
    mScrollInc = 0.f;

    mPressedIndex = int((mScroll + pos.y) / kTextHeight);
    int pressedLine = int(pos.y / kTextHeight);
    if (pressedLine >= 0 && pressedLine < mMeasure.size()) {
      mTextPressed = pos.x < mMeasure[pressedLine];
      return true;
      }
    else
      return false;
    }
  //}}}
  //{{{
  bool onMove (bool right, cPoint pos, cPoint inc) {

    mMoveInc += inc.y;

    if (abs(mMoveInc) > 2)
      mMoved = true;

    if (mMoved)
      incScroll (-(float)inc.y);
    return true;
    }
  //}}}
  //{{{
  bool onUp (bool right, bool mouseMoved, cPoint pos) {

    if (mTextPressed && !mMoved) {
      mIndex = mPressedIndex;
      mIndexChanged = true;
      }

    mTextPressed = false;
    mPressedIndex = -1;
    mMoved = false;
    mMoveInc = 0;

    return true;
    }
  //}}}

  void onDraw (ID2D1DeviceContext* dc) {
    if (!mTextPressed && mScrollInc)
      incScroll (mScrollInc * 0.9f);

    int index = int(mScroll) / (int)kTextHeight;
    float y = mRect.top - (int(mScroll) % (int)kTextHeight);
    for (int i = 0; (y < mRect.bottom) && (index < (int)mNames.size()); i++, index++, y += kTextHeight) {
      if (i >= (int)mMeasure.size())
        mMeasure.push_back (0);

      mMeasure[i] = 200;
      string str = mNames[index];
      auto brush = mTextPressed && !mMoved && (index == mPressedIndex) ?
                     mWindow->getYellowBrush() : (index == mIndex) ?
                       mWindow->getWhiteBrush() : mWindow->getLightGreyBrush();

      dc->DrawText (wstring (str.begin(), str.end()).data(), (uint32_t)str.size(), mWindow->getTextFormat(),
                    cRect(mRect.left+2, mRect.top+y+1.f, mRect.right, mRect.top+y+1.f + kTextHeight),
                    brush);
      }
    }

private:
  //{{{
  void incScroll (float inc) {

    mScroll += inc;

    if (mScroll < 0.f)
      mScroll = 0.f;
    else if (mScroll > ((mNames.size() * kTextHeight) - (mRect.bottom - mRect.top)))
      mScroll = float(((int)mNames.size() * kTextHeight) - (mRect.bottom - mRect.top));

    mScrollInc = fabs(inc) < 0.2f ? 0 : inc;
    }
  //}}}

  vector <string>& mNames;
  int& mIndex;
  bool& mIndexChanged;

  bool mTextPressed = false;
  int mPressedIndex = -1;
  bool mMoved = false;
  float mMoveInc = 0;

  vector <int> mMeasure;

  float mScroll = 0.f;
  float mScrollInc = 0.f;
  };
