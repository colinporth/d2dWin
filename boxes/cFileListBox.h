// cFileListBox.h
#pragma once
#include "../common/cD2dWindow.h"
#include "../boxes/cListBox.h"
#include "../../shared/utils/cFileList.h"

class cFileListBox : public cListBox {
public:
  //{{{
  cFileListBox (cD2dWindow* window, float width, float height, cFileList* fileList,
                std::function<void (cFileListBox* box, int index)> hitCallback) :
      cListBox (window, width, height), mFileList(fileList), mHitCallback(hitCallback) {}
  //}}}
  virtual ~cFileListBox() {}

  //{{{
  bool pick (bool inClient, cPoint pos, bool& change) {

    std::lock_guard<std::mutex> lockGuard (mMutex);

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
      std::lock_guard<std::mutex> lockGuard (mMutex);
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
        mHitCallback (this, mProxIndex);
        }

      mPressed = false;
      mMoved = false;
      mMoveInc = 0;
      }

    mProxIndex = -1;
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
      auto countDown = mWindow->getCountDown();
      float opacity = 1.f;
      if (countDown < 5) {
        opacity = countDown / 5.f;
        mWindow->changed();
        }

      mLineHeight = getLineHeight();
      auto textHeight = mLineHeight * 5.f / 6.f;

      if (!mPressed && mScrollInc)
        incScroll (mScrollInc * 0.9f);

      // calc first row index, ensure curItemIndex is visible
      mFirstRowIndex = int(mScroll / mLineHeight);

      if (mFileList->ensureItemVisible()) {
        //{{{  curItem probaly changed, show it
        // strange interaction between itemIndex change and its visibility
        if (mFileList->getIndex() < mFirstRowIndex) {
          mFirstRowIndex = mFileList->getIndex();
          mScroll = mFirstRowIndex * mLineHeight;
          }
        else if ((mLastRowIndex > 0) && (mFileList->getIndex() >= mLastRowIndex)) {
          mFirstRowIndex += mFileList->getIndex() - mLastRowIndex;
          mScroll = mFirstRowIndex * mLineHeight;
          }

        setTimedOn();
        mProxed = false;
        }
        //}}}

      float maxColumnWidth[cFileList::cFileItem::kFields] = { 0.f };

      // layout visible rows
      std::lock_guard<std::mutex> lockGuard (mMutex);
      mRowVec.clear();
      auto index = mFirstRowIndex;
      auto point = cPoint (0.f, 1.f - (mScroll - (mFirstRowIndex * mLineHeight)));
      while ((index < mFileList->size()) && (point.y < mRect.bottom)) {
        // layout row
        cRow row;
        auto& fileItem = mFileList->getFileItem (index);
        for (auto field = 0u; field < cFileList::cFileItem::kFields; field++) {
          auto str = fileItem.getFieldString (field);
          mWindow->getDwriteFactory()->CreateTextLayout (
            std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(), getWindow()->getTextFormat(),
            mRect.getWidth(), mLineHeight, &row.mTextLayout[field]);
          row.mTextLayout[field]->SetFontSize (textHeight, {0, (uint32_t)str.size()});
          row.mTextLayout[field]->GetMetrics (&row.mTextMetrics[field]);
          maxColumnWidth[field] = std::max (row.mTextMetrics[field].width, maxColumnWidth[field]);
          }
        row.mRect = cRect(point, point + cPoint(row.mTextMetrics[0].width, mLineHeight));
        row.mBrush = (mProxed && !mMoved && (index == mProxIndex)) ?
          mWindow->getYellowBrush() :
            mFileList->isCurIndex (index) ? mWindow->getWhiteBrush() : mWindow->getBlueBrush();
        mRowVec.push_back (row);
        mLastRowIndex = index;

        point.y += mLineHeight;
        index++;
        }

      //{{{  layout fieldStops
      mColumnsWidth = 0.f;
      for (auto field = 0u; field < cFileList::cFileItem::kFields; field++) {
        mColumnsWidth += maxColumnWidth[field] + textHeight/2.f;
        mColumn[field] = mColumnsWidth - 2.f;
        }
      //}}}
      //{{{  layout, draw bgnd
      mBgndRect = cRect (mColumnsWidth + mLineHeight/2.f, point.y);

      auto brush = mWindow->getTransparentBgndBrush();
      auto oldOpacity = brush->GetOpacity();

      brush->SetOpacity (brush->GetOpacity() * opacity);
      dc->FillRectangle (mBgndRect + mRect.getTL(), mWindow->getTransparentBgndBrush());
      brush->SetOpacity (oldOpacity);
      //}}}
      //{{{  layout, draw fields
      for (auto& row : mRowVec) {
        auto point = mRect.getTL() + row.mRect.getTL();
        for (auto field = 0u; field < cFileList::cFileItem::kFields; field++) {
          point.x = mRect.left + row.mRect.left + (field ? mColumn[field]-row.mTextMetrics[field].width : 2.f);

          row.mBrush->SetOpacity (opacity);
          dc->DrawTextLayout (point, row.mTextLayout[field], row.mBrush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
          row.mBrush->SetOpacity (1.f);

          row.mTextLayout[field]->Release();
          }
        }
      //}}}
      }
    }
  //}}}

private:
  const float kMinLineHeight = 16.f;
  const float kMaxLineHeight = 24.f;

  float getLineHeight() { return std::min (std::max (getHeight() / mFileList->size(), kMinLineHeight), kMaxLineHeight); }
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
  std::function<void (cFileListBox* box, int index)> mHitCallback;

  //{{{
  class cRow {
  public:
    cRect mRect;
    IDWriteTextLayout* mTextLayout[cFileList::cFileItem::kFields];
    struct DWRITE_TEXT_METRICS mTextMetrics[cFileList::cFileItem::kFields];
    ID2D1SolidColorBrush* mBrush;
    };
  //}}}
  concurrency::concurrent_vector <cRow> mRowVec;
  std::mutex mMutex; // guard mRowVec - pick,prox,down against draw

  float mColumn [cFileList::cFileItem::kFields] = { 0.f };
  float mColumnsWidth = 0.f;
  float mLineHeight = 0.f;
  cRect mBgndRect;

  bool mProxed = false;
  bool mPressed = false;
  bool mMoved = false;
  float mMoveInc = 0;
  float mScroll = 0.f;
  float mScrollInc = 0.f;

  unsigned mProxIndex = 0;
  unsigned mFirstRowIndex = 0;
  unsigned mLastRowIndex = 0;
  };
