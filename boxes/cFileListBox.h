// cFileListBox.h
//{{{  includes
#pragma once
#include "../common/cD2dWindow.h"
//}}}

class cFileListBox : public cD2dWindow::cBox {
public:
  //{{{
  cFileListBox (cD2dWindow* window, float width, float height, cFileList* fileList) :
      cBox ("fileList", window, width, height), mFileList(fileList) {}
  //}}}
  virtual ~cFileListBox() {}

  //{{{
  bool pick (bool inClient, cPoint pos, bool& change) {

    lock_guard<mutex> lockGuard (mMutex);

    bool lastPick = mPick;
    mPick = inClient && mBgndRect.inside (pos);
    if (!change && (mPick != lastPick))
      change = true;

    return mPick;
    }
  //}}}
  //{{{
  bool onProx (bool inClient, cPoint pos) {

    if (mWindow->getTimedMenuOn()) {
      lock_guard<mutex> lockGuard (mMutex);
      unsigned rowIndex = 0;
      for (auto& row : mRowVec) {
        if (row.mRect.inside (pos)) {
          mProxIndex = mFirstRowIndex + rowIndex;
          mProxed = true;
          return false;
          }
        rowIndex++;
        }
      mProxed = false;
      }

    return cBox::onProx (inClient, pos);
    }
  //}}}
  //{{{
  bool onProxExit() {
    mProxed = false;
    return false;
    }
  //}}}
  //{{{
  bool onDown (bool right, cPoint pos)  {

    if (mWindow->getTimedMenuOn()) {
      mMoved = false;
      mMoveInc = 0;
      mScrollInc = 0.f;

      if (mProxed)
        mPressed = true;
      else {
        mPressed = false;
        mFileList->nextSort();
        }

      mWindow->changed();
      return true;
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
        mFileList->setIndex (mProxIndex);
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
  bool onWheel (int delta, cPoint pos)  {
    if (mWindow->getTimedMenuOn()) 
      incScroll (-delta / 30.f);
    return true;
    }
  //}}}
  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    if (mWindow->getTimedMenuOn()) {
      mLineHeight = getLineHeight();
      auto textHeight = mLineHeight*5.f/6.f;

      if (!mPressed && mScrollInc)
        incScroll (mScrollInc * 0.9f);

      mFirstRowIndex = int(mScroll / mLineHeight);
      //{{{  tricky sync of scroll to curIndex
      //if (!mMoved && (mFileList->getIndex() < mFirstRowIndex)) {
      //  mScroll -= mLineHeight;
      //  mFirstRowIndex = int(mScroll / mLineHeight);
      //  }
      //}}}

      float maxColumnWidth[cFileItem::kFields] = { 0.f };

      // layout visible rows
      lock_guard<mutex> lockGuard (mMutex);
      mRowVec.clear();
      auto index = mFirstRowIndex;
      auto point = cPoint (0.f, 1.f - (mScroll - (mFirstRowIndex * mLineHeight)));
      while ((index < mFileList->size()) && (point.y < mRect.bottom)) {
        // layout row
        cRow row;
        auto& fileItem = mFileList->getFileItem (index);
        for (auto field = 0u; field < cFileItem::kFields; field++) {
          auto str = fileItem.getFieldString (field);
          mWindow->getDwriteFactory()->CreateTextLayout (
            wstring (str.begin(), str.end()).data(), (uint32_t)str.size(), getWindow()->getTextFormat(),
            mRect.getWidth(), mLineHeight, &row.mTextLayout[field]);
          row.mTextLayout[field]->SetFontSize (textHeight, {0, (uint32_t)str.size()});
          row.mTextLayout[field]->GetMetrics (&row.mTextMetrics[field]);
          maxColumnWidth[field] = max (row.mTextMetrics[field].width, maxColumnWidth[field]);
          }
        row.mRect = cRect(point, point + cPoint(row.mTextMetrics[0].width, mLineHeight));
        row.mBrush = (mProxed && !mMoved && (index == mProxIndex)) ?
          mWindow->getYellowBrush() :
            mFileList->isCurIndex (index) ? mWindow->getWhiteBrush() : mWindow->getBlueBrush();
        mRowVec.push_back (row);

        point.y += mLineHeight;
        index++;
        }

      //{{{  layout and draw rows
      // layout fieldStops
      mColumnsWidth = 0.f;
      for (auto field = 0u; field < cFileItem::kFields; field++) {
        mColumnsWidth += maxColumnWidth[field] + textHeight/2.f;
        mColumn[field] = mColumnsWidth - 2.f;
        }

      // layout,draw bgnd
      mBgndRect = cRect (mColumnsWidth + mLineHeight/2.f, point.y);
      dc->FillRectangle (mBgndRect + mRect.getTL(), mWindow->getTransparentBgndBrush());

      // layout,draw fields
      for (auto& row : mRowVec) {
        auto p = mRect.getTL() + row.mRect.getTL();
        for (auto field = 0u; field < cFileItem::kFields; field++) {
          p.x = mRect.left + row.mRect.left + (field ? mColumn[field]-row.mTextMetrics[field].width : 2.f);
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
  const float kMinLineHeight = 16.f;
  const float kMaxLineHeight = 24.f;

  float getLineHeight() { return min (max (getHeight() / mFileList->size(), kMinLineHeight), kMaxLineHeight); }
  //{{{
  void incScroll (float inc) {

    mScroll += inc;
    if (mScroll < 0.f)
      mScroll = 0.f;
    else if ((mFileList->size() * mLineHeight) < mRect.getHeight())
      mScroll = 0.f;
    else if (mScroll > ((mFileList->size() * mLineHeight) - mRect.getHeight()))
      mScroll = float(((int)mFileList->size() * mLineHeight) - mRect.getHeight());

    mScrollInc = fabs(inc) < 0.2f ? 0 : inc;
    }
  //}}}

  // vars
  cFileList* mFileList;

  mutex mMutex; // guard mRowVec - pick,prox,down against draw
  //{{{
  class cRow {
  public:
    cRect mRect;
    IDWriteTextLayout* mTextLayout[cFileItem::kFields];
    struct DWRITE_TEXT_METRICS mTextMetrics[cFileItem::kFields];
    ID2D1SolidColorBrush* mBrush;
    };
  //}}}
  concurrency::concurrent_vector <cRow> mRowVec;

  float mColumn[cFileItem::kFields] = { 0.f };
  float mColumnsWidth = 0.f;
  float mLineHeight = 0.f;
  cRect mBgndRect;

  bool mProxed = false;
  bool mPressed = false;
  bool mMoved = false;
  float mMoveInc = 0;
  float mScroll = 0.f;
  float mScrollInc = 0.f;

  unsigned mFirstRowIndex = 0;
  unsigned mProxIndex = 0;
  };
