// cFileListBox.h
//{{{  includes
#pragma once
#include "../cD2dWindow.h"
#include "../../../shared/utils/utils.h"
#include "../../../shared/utils/cLog.h"
//}}}

class cFileListBox : public cD2dWindow::cBox {
public:
  //{{{
  cFileListBox (cD2dWindow* window, float width, float height, cFileList* fileList) :
      cBox ("fileList", window, width, height), mFileList(fileList) {

    mPin = true;
    }
  //}}}
  virtual ~cFileListBox() {}

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
        mFileList->setIndex (mPressedIndex);
        onHit();
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

      auto itemIndex = int(mScroll) / (int)kTextHeight;
      float y = mRect.top + 1.f - (int(mScroll) % (int)kTextHeight);

      auto maxWidth = 0.f;
      auto point = cPoint (mRect.left + 2.f, y);

      for (auto row = 0;
           (y < mRect.bottom) && (itemIndex < (int)mFileList->size());
           row++, itemIndex++, y += kTextHeight) {
        if (row >= (int)mMeasure.size())
          mMeasure.push_back (0);

        auto& fileItem = mFileList->getFileItem (itemIndex);
        auto str = fileItem.getFileName() + 
                   " " + fileItem.getFileSizeString() + 
                   " " + fileItem.getCreationTimeString();
        auto brush = (mTextPressed && !mMoved && (itemIndex == mPressedIndex)) ?
          mWindow->getYellowBrush() :
            mFileList->isCurIndex(itemIndex) ? mWindow->getWhiteBrush() : mWindow->getBlueBrush();

        IDWriteTextLayout* textLayout;
        mWindow->getDwriteFactory()->CreateTextLayout (
          wstring (str.begin(), str.end()).data(), (uint32_t)str.size(), getWindow()->getTextFormat(),
          mRect.getWidth(), kTextHeight, &textLayout);

        struct DWRITE_TEXT_METRICS textMetrics;
        textLayout->GetMetrics (&textMetrics);
        mMeasure[row] = textMetrics.width;
        maxWidth = max (textMetrics.width, maxWidth);

        dc->DrawTextLayout (point, textLayout, brush);
        textLayout->Release();

        point.y += kTextHeight;
        }

      mBgndRect = mRect;
      mBgndRect.right = mRect.left + maxWidth + 4.0f;
      mBgndRect.bottom = point.y;
      }
    }
  //}}}

protected:
  virtual void onHit() = 0;

private:
  //{{{
  void incScroll (float inc) {

    mScroll += inc;
    if (mScroll < 0.f)
      mScroll = 0.f;
    else if ((mFileList->size() * kTextHeight) < mRect.getHeight())
      mScroll = 0.f;
    else if (mScroll > ((mFileList->size() * kTextHeight) - mRect.getHeight()))
      mScroll = float(((int)mFileList->size() * kTextHeight) - mRect.getHeight());

    mScrollInc = fabs(inc) < 0.2f ? 0 : inc;
    }
  //}}}

  cFileList* mFileList;

  cRect mBgndRect;
  concurrency::concurrent_vector <float> mMeasure;

  bool mTextPressed = false;
  int mPressedIndex = -1;
  bool mMoved = false;
  float mMoveInc = 0;
  float mScroll = 0.f;
  float mScrollInc = 0.f;
  };
