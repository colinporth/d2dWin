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
      for (auto& item : mRowRectVec) {
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
      auto textHeight = getLineHeight()*5.f/6.f;

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

      mRowRectVec.clear();

      float maxFieldWidth[3] = { 0.f };
      auto point = cPoint (mRect.left + 2.f, mRect.top + 1.f - (mScroll - (mFirstRowIndex * lineHeight)));
      auto index = mFirstRowIndex;
      for (auto row = 0u;
           (point.y < mRect.bottom) && (index < mFileList->size());
           row++, index++, point.y += lineHeight) {

        auto brush = (mPressed && !mMoved && (index == mPressedIndex)) ?
          mWindow->getYellowBrush() :
            mFileList->isCurIndex (index) ? mWindow->getWhiteBrush() : mWindow->getBlueBrush();

        auto& fileItem = mFileList->getFileItem (index);
        string str[3];
        str[0] = fileItem.getFileName();
        str[1] = fileItem.getFileSizeString();
        str[2] = fileItem.getCreationString();

        IDWriteTextLayout* textLayout[3];
        struct DWRITE_TEXT_METRICS textMetrics[3];
        for (auto i = 0; i < 3; i++) {
          mWindow->getDwriteFactory()->CreateTextLayout (
            wstring (str[i].begin(), str[i].end()).data(), (uint32_t)str[i].size(), getWindow()->getTextFormat(),
            mRect.getWidth(), lineHeight, &textLayout[i]);
          textLayout[i]->SetFontSize (textHeight, {0, (uint32_t)str[i].size()});
          textLayout[i]->GetMetrics (&textMetrics[i]);
          maxFieldWidth[i] = max (textMetrics[i].width, maxFieldWidth[i]);
          }

        auto p = point;
        dc->DrawTextLayout (p, textLayout[0], brush);
        p.x = mMaxWidth - textMetrics[2].width;
        dc->DrawTextLayout (p, textLayout[2], brush);
        p.x = mMaxWidth - mMaxFieldWidth[2] - textHeight - textMetrics[1].width;
        dc->DrawTextLayout (p, textLayout[1], brush);

        for (auto i = 0; i < 3; i++)
          textLayout[i]->Release();

        mRowRectVec.push_back (cRect (point.x, point.y, p.x, point.y + lineHeight));
        }

      mBgndRect = mRect;
      mBgndRect.right = mRect.left + mMaxWidth + getLineHeight()/4.f;
      mBgndRect.bottom = point.y;

      mMaxWidth = 2.f * textHeight;
      for (auto i = 0; i < 3; i++) {
        mMaxWidth += maxFieldWidth[i];
        mMaxFieldWidth[i] = maxFieldWidth[i];
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

  cRect mBgndRect;
  float mMaxWidth = 0.f;
  float mMaxFieldWidth[3] = { 0.f };
  concurrency::concurrent_vector <cRect> mRowRectVec;

  unsigned mFirstRowIndex = 0;
  unsigned mPressedIndex = 0;
  bool mPressed = false;

  bool mMoved = false;
  float mMoveInc = 0;
  float mScroll = 0.f;
  float mScrollInc = 0.f;

  };
