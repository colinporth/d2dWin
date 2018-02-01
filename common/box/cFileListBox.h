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

      int row = 0;
      for (auto& item : mRowRectVec) {
        if (item.inside(pos)) {
          mPressedIndex = mFirstRowIndex + row;
          mPressed = true;
          return false;
          }
        row++;
        }

      mPressed = false;
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
      if (mPressed && !mMoved) {
        mFileList->setIndex (mPressedIndex);
        onHit();
        }

      mPressed = false;
      mMoved = false;
      mMoveInc = 0;
      }

    return true;
    }
  //}}}

  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    if (mWindow->getTimedMenuOn()) {
      auto lineHeight = getLineHeight();
      auto textHeight = getLineHeight()*4.f/5.f;

      if (!mPressed && mScrollInc)
        incScroll (mScrollInc * 0.9f);

      mFirstRowIndex = int(mScroll / lineHeight);
      //if (!mMoved && (mFileList->getIndex() < mFirstRowIndex)) {
      //  mScroll -= lineHeight;
      //  mFirstRowIndex = int(mScroll / lineHeight);
      //  }

      dc->FillRectangle (mBgndRect, mWindow->getTransparentBgndBrush());

      auto maxWidth = 0.f;
      auto point = cPoint (mRect.left + 2.f, mRect.top + 1.f - (mScroll - (mFirstRowIndex * lineHeight)));
      auto itemIndex = mFirstRowIndex;
      for (auto row = 0;
           (point.y < mRect.bottom) && (itemIndex < (int)mFileList->size());
           row++, itemIndex++, point.y += lineHeight) {

        auto& fileItem = mFileList->getFileItem (itemIndex);
        auto str = fileItem.getFileName() +
                   " " + fileItem.getFileSizeString() +
                   " " + fileItem.getCreationTimeString();
        auto brush = (mPressed && !mMoved && (itemIndex == mPressedIndex)) ?
          mWindow->getYellowBrush() :
            mFileList->isCurIndex(itemIndex) ? mWindow->getWhiteBrush() : mWindow->getBlueBrush();

        IDWriteTextLayout* textLayout;
        mWindow->getDwriteFactory()->CreateTextLayout (
          wstring (str.begin(), str.end()).data(), (uint32_t)str.size(), getWindow()->getTextFormat(),
          mRect.getWidth(), lineHeight, &textLayout);
        textLayout->SetFontSize (textHeight, {0, (uint32_t)str.size()});
        struct DWRITE_TEXT_METRICS textMetrics;
        textLayout->GetMetrics (&textMetrics);
        maxWidth = max(textMetrics.width, maxWidth);
        dc->DrawTextLayout (point, textLayout, brush);
        textLayout->Release();

        if (row >= (int)mRowRectVec.size())
          mRowRectVec.push_back (cRect (point, point + cPoint(textMetrics.width,lineHeight)));
        else
          mRowRectVec[row] = cRect (point, point + cPoint(textMetrics.width,lineHeight));
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
  float getLineHeight() {
    auto pixPerLine = getHeight() / mFileList->size();
    pixPerLine = min (pixPerLine, kLineHeight);
    pixPerLine = max (pixPerLine, kSmallLineHeight);
    return pixPerLine;
    }
  //}}}
  //{{{
  void incScroll (float inc) {

    auto lineHeight = getLineHeight();
    mScroll += inc;
    if (mScroll < 0.f)
      mScroll = 0.f;
    else if ((mFileList->size() * lineHeight) < mRect.getHeight())
      mScroll = 0.f;
    else if (mScroll > ((mFileList->size() * lineHeight) - mRect.getHeight()))
      mScroll = float(((int)mFileList->size() * lineHeight) - mRect.getHeight());

    mScrollInc = fabs(inc) < 0.2f ? 0 : inc;
    }
  //}}}

  const float kSmallLineHeight = 16.f;

  // vars
  cFileList* mFileList;

  cRect mBgndRect;
  concurrency::concurrent_vector <cRect> mRowRectVec;

  unsigned mFirstRowIndex = 0;
  unsigned mPressedIndex = 0;
  bool mPressed = false;

  bool mMoved = false;
  float mMoveInc = 0;
  float mScroll = 0.f;
  float mScrollInc = 0.f;
  };
