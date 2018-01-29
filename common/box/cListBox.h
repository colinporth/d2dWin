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
  cListBox (cD2dWindow* window, float width, float height,
            vector<string>& items, int& itemIndex, bool& itemIndexChanged)
      : cBox ("list", window, width, height),
        mItems(items), mItemIndex(itemIndex), mItemIndexChanged(itemIndexChanged) {

    mWindow->getDwriteFactory()->CreateTextFormat (L"FreeSans", NULL,
      DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
      16.0f, L"en-us",
      &mTextFormat);

    mPin = true;
    }
  //}}}
  //{{{
  virtual ~cListBox() {
    mTextFormat->Release();
    }
  //}}}

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
        mItemIndex = mPressedIndex;
        mItemIndexChanged = true;
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

      dc->FillRectangle (mRect, mWindow->getTransparentBgndBrush());

      int itemIndex = int(mScroll) / (int)kTextHeight;
      float y = mRect.top + 1.f - (int(mScroll) % (int)kTextHeight);

      auto maxWidth = 0.f;
      auto point = cPoint (mRect.left+2, y);

      for (int row = 0; (y < mRect.bottom) && (itemIndex < (int)mItems.size()); row++, itemIndex++, y += kTextHeight) {
        if (row >= (int)mMeasure.size())
          mMeasure.push_back (0);

        std::string str = mItems[itemIndex];
        auto brush = (mTextPressed && !mMoved && (itemIndex == mPressedIndex)) ?
          mWindow->getYellowBrush() : (itemIndex == mItemIndex) ? mWindow->getWhiteBrush() : mWindow->getBlueBrush();

        IDWriteTextLayout* textLayout;
        mWindow->getDwriteFactory()->CreateTextLayout (
          wstring (str.begin(), str.end()).data(), (uint32_t)str.size(), mTextFormat,
          mRect.getWidth(), kTextHeight, &textLayout);

        struct DWRITE_TEXT_METRICS textMetrics;
        textLayout->GetMetrics (&textMetrics);
        mMeasure[row] = textMetrics.width;
        maxWidth = max (textMetrics.width, maxWidth);

        dc->DrawTextLayout (point, textLayout, brush);
        textLayout->Release();

        point.y += kTextHeight;
        }

      mRect.right = maxWidth + 4.0f;
      mRect.bottom = point.y;
      }
    }
  //}}}

private:
  //{{{
  void incScroll (float inc) {

    mScroll += inc;
    if (mScroll < 0.f)
      mScroll = 0.f;
    else if ((mItems.size() * kTextHeight) < mRect.getHeight())
      mScroll = 0.f;
    else if (mScroll > ((mItems.size() * kTextHeight) - mRect.getHeight()))
      mScroll = float(((int)mItems.size() * kTextHeight) - mRect.getHeight());

    mScrollInc = fabs(inc) < 0.2f ? 0 : inc;
    }
  //}}}

  IDWriteTextFormat* mTextFormat = nullptr;

  std::vector <std::string>& mItems;
  int& mItemIndex;
  bool& mItemIndexChanged;

  bool mTextPressed = false;
  int mPressedIndex = -1;
  bool mMoved = false;
  float mMoveInc = 0;

  std::vector<float> mMeasure;
  cRect mBgndRect;

  float mScroll = 0.f;
  float mScrollInc = 0.f;
  };
