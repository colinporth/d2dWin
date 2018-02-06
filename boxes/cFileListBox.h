// cFileListBox.h
//{{{  includes
#pragma once
#include "../common/cD2dWindow.h"
#include "../../shared/utils/utils.h"
#include "../../shared/utils/cLog.h"
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
      pos += getTL();
      for (auto& item : mRowVec) {
        if (item.inside (pos)) {
          mPressedIndex = mFirstRowIndex + row;
          mPressed = true;
          return false;
          }
        row++;
        }

      mFileList->nextSort();
      mWindow->changed();
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
      auto textHeight = lineHeight*5.f/6.f;

      if (!mPressed && mScrollInc)
        incScroll (mScrollInc * 0.9f);

      mFirstRowIndex = int(mScroll / lineHeight);
      //{{{  tricky sync of scroll to curIndex
      //if (!mMoved && (mFileList->getIndex() < mFirstRowIndex)) {
      //  mScroll -= lineHeight;
      //  mFirstRowIndex = int(mScroll / lineHeight);
      //  }
      //}}}

      dc->FillRectangle (mBgndRect, mWindow->getTransparentBgndBrush());

      mRowVec.clear();

      float maxFieldWidth[cFileItem::kFields] = { 0.f };

      auto index = mFirstRowIndex;
      auto point = cPoint (0.f, mRect.top + 1.f - (mScroll - (mFirstRowIndex * lineHeight)));
      while ((index < mFileList->size()) && (point.y < mRect.bottom)) {
        auto brush = (mPressed && !mMoved && (index == mPressedIndex)) ?
          mWindow->getYellowBrush() :
            mFileList->isCurIndex (index) ? mWindow->getWhiteBrush() : mWindow->getBlueBrush();

        auto& fileItem = mFileList->getFileItem (index);

        IDWriteTextLayout* textLayout[cFileItem::kFields];
        struct DWRITE_TEXT_METRICS textMetrics[cFileItem::kFields];
        for (auto i = 0u; i < cFileItem::kFields; i++) {
          auto str = fileItem.getFieldString (i);
          mWindow->getDwriteFactory()->CreateTextLayout (
            wstring (str.begin(), str.end()).data(), (uint32_t)str.size(), getWindow()->getTextFormat(),
            mRect.getWidth(), lineHeight, &textLayout[i]);
          textLayout[i]->SetFontSize (textHeight, {0, (uint32_t)str.size()});
          textLayout[i]->GetMetrics (&textMetrics[i]);
          maxFieldWidth[i] = max (textMetrics[i].width, maxFieldWidth[i]);
          }

        auto p = point;
        for (auto i = 0u; i < cFileItem::kFields; i++) {
          p.x = i ? mFieldStop[i] - textMetrics[i].width : textHeight/2.f;
          dc->DrawTextLayout (p, textLayout[i], brush);
          textLayout[i]->Release();
          }

        mRowVec.push_back (cRect (point, point + cPoint(textMetrics[0].width, lineHeight)));
        point.y += lineHeight;
        index++;
        }

      mBgndRect = mRect;
      mBgndRect.right = mRect.left + mMaxWidth + lineHeight/4.f;
      mBgndRect.bottom = point.y;

      mMaxWidth = 0.f;
      for (auto i = 0u; i < cFileItem::kFields; i++) {
        mMaxWidth += maxFieldWidth[i] + textHeight/2.f;
        mFieldStop[i] = mMaxWidth - textHeight/4.f;
        }
      }
    }
  //}}}

protected:
  virtual void onHit() = 0;

private:
  //{{{
  float getLineHeight() {
    auto pixPerLine = getHeight() / mFileList->size();
    pixPerLine = min (pixPerLine, kLineHeight - 2.f);
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

  concurrency::concurrent_vector <cRect> mRowVec;
  float mFieldStop[cFileItem::kFields] = { 0.f };
  float mMaxWidth = 0.f;
  cRect mBgndRect;

  unsigned mFirstRowIndex = 0;
  unsigned mPressedIndex = 0;
  bool mPressed = false;

  bool mMoved = false;
  float mMoveInc = 0;
  float mScroll = 0.f;
  float mScrollInc = 0.f;

  };
