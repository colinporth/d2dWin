// cStringListBox.h
#pragma once
#include "../common/cD2dWindow.h"

class cStringListBox : public cListBox {
public:
  //{{{
  cStringListBox (cD2dWindow* window, float width, float height, std::vector<std::string>& items,
            std::function<void (cStringListBox* box, const std::string& string)> hitCallback)
      : cListBox (window, width, height), mHitCallback(hitCallback), mItems(items) {

    // allocating matching measure vector
    mMeasureItems.reserve (items.size());
    for (unsigned i = 0; i < items.size(); i++)
      mMeasureItems.push_back (0);
    }
  //}}}

  int getIndex() { return mItemIndex; }
  std::string getIndexString() { return mItems[mItemIndex]; }

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
      if ((pressedLine >= 0) && (pressedLine < mMeasureItems.size())) {
        mTextPressed = pos.x < mMeasureItems[pressedLine];
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
        mHitCallback (this, mItems [mItemIndex]);
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

      auto maxWidth = 0.f;
      auto point = cPoint (mRect.left+2, mRect.top + 1.f - (int(mScroll) % (int)mLineHeight));
      unsigned itemNum = 0;
      for (auto &item : mItems) {
        IDWriteTextLayout* layout;
        mWindow->getDwriteFactory()->CreateTextLayout (
          std::wstring (item.begin(), item.end()).data(), (uint32_t)item.size(), mWindow->getTextFormat(),
          mRect.getWidth(), mLineHeight, &layout);

        if (layout) {
          struct DWRITE_TEXT_METRICS metrics;
          layout->GetMetrics (&metrics);
          mMeasureItems[itemNum] = metrics.width;
          maxWidth = std::max (metrics.width, maxWidth);

          auto brush = (mTextPressed && !mMoved && (itemIndex == mPressedIndex)) ?
            mWindow->getYellowBrush() : (itemIndex == mItemIndex) ?
              mWindow->getWhiteBrush() : mWindow->getBlueBrush();

          dc->DrawTextLayout (point, layout, brush);
          layout->Release();
          }

        point.y += mLineHeight;
        if (point.y > mRect.bottom)
          break;
        itemNum++;
        }

      mBgndRect = { mRect.left, mRect.top, mRect.left + maxWidth + 4.0f, point.y };
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

  std::function<void (cStringListBox* box, const std::string& string)> mHitCallback;
  std::vector <std::string>& mItems;

  std::vector<float> mMeasureItems;
  int mItemIndex = -1;

  float mLineHeight = kLineHeight;

  bool mTextPressed = false;
  int mPressedIndex = -1;
  bool mMoved = false;
  float mMoveInc = 0;

  float mScroll = 0.f;
  float mScrollInc = 0.f;

  cRect mBgndRect;
  };
