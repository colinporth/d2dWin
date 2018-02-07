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
        if (item.mRect.inside (pos)) {
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

      float maxFieldWidth[cFileItem::kFields] = { 0.f };

      // layout visible rows
      mRowVec.clear();
      auto index = mFirstRowIndex;
      auto point = mRect.getTL() + cPoint (0.f, 1.f - (mScroll - (mFirstRowIndex * lineHeight)));
      while ((index < mFileList->size()) && (point.y < mRect.bottom)) {
        // layout row
        cRow row;
        auto& fileItem = mFileList->getFileItem (index);
        for (auto field = 0u; field < cFileItem::kFields; field++) {
          auto str = fileItem.getFieldString (field);
          mWindow->getDwriteFactory()->CreateTextLayout (
            wstring (str.begin(), str.end()).data(), (uint32_t)str.size(), getWindow()->getTextFormat(),
            mRect.getWidth(), lineHeight, &row.mTextLayout[field]);
          row.mTextLayout[field]->SetFontSize (textHeight, {0, (uint32_t)str.size()});
          row.mTextLayout[field]->GetMetrics (&row.mTextMetrics[field]);
          maxFieldWidth[field] = max (row.mTextMetrics[field].width, maxFieldWidth[field]);
          }
        row.mRect = cRect(point, point + cPoint(row.mTextMetrics[0].width, lineHeight));
        row.mBrush = (mPressed && !mMoved && (index == mPressedIndex)) ?
          mWindow->getYellowBrush() :
            mFileList->isCurIndex (index) ? mWindow->getWhiteBrush() : mWindow->getBlueBrush();
        mRowVec.push_back (row);

        point.y += lineHeight;
        index++;
        }

      //{{{  layout fieldStops
      mMaxWidth = 0.f;
      for (auto field = 0u; field < cFileItem::kFields; field++) {
        mMaxWidth += maxFieldWidth[field] + textHeight/2.f;
        mFieldStop[field] = mMaxWidth - textHeight/4.f;
        }
      //}}}
      //{{{  layout,draw bgnd
      mBgndRect = mRect;
      mBgndRect.right = mRect.left + mMaxWidth + lineHeight/4.f;
      mBgndRect.bottom = point.y;

      dc->FillRectangle (mBgndRect, mWindow->getTransparentBgndBrush());
      //}}}
      //{{{  layout,draw fields
      for (auto& row : mRowVec) {
        auto p = row.mRect.getTL();
        for (auto field = 0u; field < cFileItem::kFields; field++) {
          p.x = mRect.left + (field ? mFieldStop[field] - row.mTextMetrics[field].width : textHeight/2.f);
          dc->DrawTextLayout (p, row.mTextLayout[field], row.mBrush);
          row.mTextLayout[field]->Release();
          }
        }
      //}}}
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

  //{{{
  class cRow {
  public:
    cRow() {}
    ~cRow() {}

    IDWriteTextLayout* mTextLayout[cFileItem::kFields];
    struct DWRITE_TEXT_METRICS mTextMetrics[cFileItem::kFields];

    cRect mRect;
    ID2D1SolidColorBrush* mBrush;
    };
  //}}}
  concurrency::concurrent_vector <cRow> mRowVec;

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
