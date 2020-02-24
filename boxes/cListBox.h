// cListBox.h
#pragma once
#include "../common/cD2dWindow.h"

class cListBox : public cD2dWindow::cBox {
public:
  //{{{
  cListBox (cD2dWindow* window, float width, float height, std::vector<std::string>& items,
            std::function<void (cBox* box)> hitCallback)
      : cBox ("list", window, width, height, std::move(hitCallback)), mItems(items) {

    mWindow->getDwriteFactory()->CreateTextFormat (L"FreeSans", NULL,
      DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
      16.0f, L"en-us",
      &mTextFormat);
    }
  //}}}
  //{{{
  virtual ~cListBox() {
    mTextFormat->Release();
    }
  //}}}

  int getIndex() { return mItemIndex; }
  std::string getString() { return mItems[mItemIndex]; }

  //{{{
  bool pick (bool inClient, cPoint pos, bool& change) {

    bool lastPick = mPick;
    mPick = inClient && mBgndRect.inside (pos);
    if (!change && (mPick != lastPick))
      change = true;

    return mPick;
    }
  //}}}
  //{{{
  bool onDown (bool right, cPoint pos)  {

    if (mWindow->getTimedMenuOn()) {
      mMoved = false;
      mMoveInc = 0;
      mScrollInc = 0.f;

      mPressedIndex = int((mScroll + pos.y) / mLineHeight);
      int pressedLine = int(pos.y / mLineHeight);
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
        mHitCallback (this);
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

      dc->FillRectangle (mBgndRect, mWindow->getTransparentBgndBrush());

      int itemIndex = int(mScroll) / (int)mLineHeight;
      float y = mRect.top + 1.f - (int(mScroll) % (int)mLineHeight);

      auto maxWidth = 0.f;
      auto point = cPoint (mRect.left+2, y);

      for (int row = 0; (y < mRect.bottom) && (itemIndex < (int)mItems.size()); row++, itemIndex++, y += mLineHeight) {
        if (row >= (int)mMeasure.size())
          mMeasure.push_back (0);

        std::string str = mItems[itemIndex];
        auto brush = (mTextPressed && !mMoved && (itemIndex == mPressedIndex)) ?
          mWindow->getYellowBrush() : (itemIndex == mItemIndex) ? mWindow->getWhiteBrush() : mWindow->getBlueBrush();

        IDWriteTextLayout* textLayout;
        mWindow->getDwriteFactory()->CreateTextLayout (
          std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(), mTextFormat,
          mRect.getWidth(), mLineHeight, &textLayout);

        struct DWRITE_TEXT_METRICS textMetrics;
        textLayout->GetMetrics (&textMetrics);
        mMeasure[row] = textMetrics.width;
        maxWidth = std::max (textMetrics.width, maxWidth);

        dc->DrawTextLayout (point, textLayout, brush);
        textLayout->Release();

        point.y += mLineHeight;
        }

      mBgndRect = mRect;
      mBgndRect.right = mRect.left + maxWidth + 4.0f;
      mBgndRect.bottom = point.y;
      }
    }
  //}}}

private:
  //{{{
  void incScroll (float inc) {

    mScroll += inc;
    if (mScroll < 0.f)
      mScroll = 0.f;
    else if ((mItems.size() * mLineHeight) < mRect.getHeight())
      mScroll = 0.f;
    else if (mScroll > ((mItems.size() * mLineHeight) - mRect.getHeight()))
      mScroll = float(((int)mItems.size() * mLineHeight) - mRect.getHeight());

    mScrollInc = fabs(inc) < 0.2f ? 0 : inc;
    }
  //}}}

  IDWriteTextFormat* mTextFormat = nullptr;

  std::vector <std::string>& mItems;
  int mItemIndex = -1;

  float mLineHeight = kLineHeight;
  bool mTextPressed = false;
  int mPressedIndex = -1;
  bool mMoved = false;
  float mMoveInc = 0;

  std::vector<float> mMeasure;
  cRect mBgndRect;

  float mScroll = 0.f;
  float mScrollInc = 0.f;
  };
