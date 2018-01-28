// cListBox.h
//{{{  includes
#pragma once

#include "../cD2dWindow.h"
#include "../../../shared/utils/utils.h"
#include "../../../shared/utils/cLog.h"
//}}}

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

    if (mWindow->getTimedMenuOn()) {
      mMoved = false;
      mMoveInc = 0;
      mScrollInc = 0.f;

      mPressedIndex = int((mScroll + pos.y) / kTextHeight);
      int pressedLine = int(pos.y / kTextHeight);
      if (pressedLine >= 0 && pressedLine < mMeasure.size()) {
        mTextPressed = pos.x < mMeasure[pressedLine];
        return true;
        }
      }
    return false;
    }
  //}}}
  //{{{
  bool onMove (bool right, cPoint pos, cPoint inc) {

    if (mWindow->getTimedMenuOn()) {
      mMoveInc += inc.y;

      if (abs(mMoveInc) > 2)
        mMoved = true;

      if (mMoved)
        incScroll (-(float)inc.y);
      }

    return true;
    }
  //}}}
  //{{{
  bool onUp (bool right, bool mouseMoved, cPoint pos) {

    if (mWindow->getTimedMenuOn()) {
      if (mTextPressed && !mMoved) {
        mIndex = mPressedIndex;
        mIndexChanged = true;
        //mWindow->setTimedMenuOff();
        }

      mTextPressed = false;
      mPressedIndex = -1;
      mMoved = false;
      mMoveInc = 0;
      }

    return true;
    }
  //}}}

  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    if (mWindow->getTimedMenuOn()) {
      if (!mTextPressed && mScrollInc)
        incScroll (mScrollInc * 0.9f);

      int nameIndex = int(mScroll) / (int)kTextHeight;
      float y = mRect.top + 1.f - (int(mScroll) % (int)kTextHeight);

      for (int row = 0; (y < mRect.bottom) && (nameIndex < (int)mNames.size()); row++, nameIndex++, y += kTextHeight) {
        if (row >= (int)mMeasure.size())
          mMeasure.push_back (0);
        mMeasure[row] = 200;

        std::string str = mNames[nameIndex];
        auto brush = mTextPressed && !mMoved && (nameIndex == mPressedIndex) ?
                       mWindow->getYellowBrush() : (nameIndex == mIndex) ?
                         mWindow->getWhiteBrush() : mWindow->getLightGreyBrush();
        auto r = cRect(mRect.left+2, y, mRect.right, y + kTextHeight);
        dc->FillRectangle (r, mWindow->getTransparentBgndBrush());
        dc->DrawText (wstring (str.begin(), str.end()).data(), (uint32_t)str.size(), mWindow->getTextFormat(),
                      r, brush);
        }
      }
    }
  //}}}

private:
  //{{{
  void incScroll (float inc) {

    mScroll += inc;

    if (mScroll < 0.f)
      mScroll = 0.f;
    else if ((mNames.size() * kTextHeight) < mRect.getHeight())
      mScroll = 0.f;
    else if (mScroll > ((mNames.size() * kTextHeight) - mRect.getHeight()))
      mScroll = float(((int)mNames.size() * kTextHeight) - mRect.getHeight());

    mScrollInc = fabs(inc) < 0.2f ? 0 : inc;
    }
  //}}}

  std::vector <std::string>& mNames;
  int& mIndex;
  bool& mIndexChanged;

  bool mTextPressed = false;
  int mPressedIndex = -1;
  bool mMoved = false;
  float mMoveInc = 0;

  std::vector <int> mMeasure;

  float mScroll = 0.f;
  float mScrollInc = 0.f;
  };
