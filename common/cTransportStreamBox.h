// cTransportStreamPidBox.h
#pragma once
//{{{  includes
#include "cD2dWindow.h"
#include "../../shared/decoders/cTransportStream.h"
//}}}

class cTransportStreamBox : public cD2dWindow::cBox {
public:
  //{{{
  cTransportStreamBox (cD2dWindow* window, float width, float height, cTransportStream* ts)
      : cBox("tsPid", window, width, height), mTs(ts) {

    mPin = true;

    mWindow->getDwriteFactory()->CreateTextFormat (L"Consolas", NULL,
      DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
      mLineHeight, L"en-us",
      &mTextFormat);
    }
  //}}}
  //{{{
  virtual ~cTransportStreamBox() {
    mTextFormat->Release();
    }
  //}}}

  //{{{
  bool onDown (bool right, cPoint pos)  {
    togglePin();
    return true;
    }
  //}}}
  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    auto r = cRect (mRect.left, mRect.top, mRect.right, mRect.top + mLineHeight);
    auto serviceWidth = 0.f;

    mLineHeight = mTs->mServiceMap.size() >= 10 ? kDefaultLineHeight : kLargeLineHeight;
    if (mTs->mServiceMap.size() > 1) {
      //{{{  draw services
      int i = 1;
      for (auto &service : mTs->mServiceMap) {
        auto str = dec(i, mTs->mServiceMap.size() >= 10 ? 2:1) + " " + service.second.getNameString();
        serviceWidth = max (drawText (dc, str, mTextFormat, r, mWindow->getWhiteBrush(), mLineHeight), serviceWidth);
        r.top = r.bottom;
        r.bottom += mLineHeight;

        for (auto epgItem : service.second.getNowVec()) {
          str = "  - " + epgItem->getStartTimeString() +
                " " + epgItem->getDurationString() +
                " " + epgItem->getTitleString();
          serviceWidth = max (drawText (dc, str, mTextFormat, r, mWindow->getWhiteBrush(), mLineHeight), serviceWidth);
          r.top = r.bottom;
          r.bottom += mLineHeight;
          }

        i++;
        }
      }
      //}}}

    if (mTs->mPidInfoMap.size()) {
      //{{{  draw pids
      auto maxPidPackets = 10000;
      for (auto &pidInfo : mTs->mPidInfoMap)
        maxPidPackets = max (maxPidPackets, pidInfo.second.mTotal);

      auto r = cRect (mRect.left + serviceWidth + mLineHeight, mRect.top,
                      mRect.right, mRect.top + mLineHeight);
      for (auto &pidInfo : mTs->mPidInfoMap) {
        auto str = (mContDigits ? dec (pidInfo.second.mDisContinuity,mContDigits) + ":" : "") +
                   dec (pidInfo.second.mTotal,mPacketDigits) +
                   " " + dec (pidInfo.first, 4) +
                   " " + getFullPtsString (pidInfo.second.mPts) +
                   " " + pidInfo.second.getTypeString();
        auto width = drawText (dc, str, mTextFormat, r, mWindow->getWhiteBrush(), mLineHeight) + mLineHeight/2.f;

        dc->FillRectangle (
          cRect (r.left + width, r.top+4.f,
                 r.left + width + (r.getWidth() - width)*pidInfo.second.mTotal/maxPidPackets, r.top+mLineHeight),
          mWindow->getOrangeBrush());

        auto rInfo = r;
        rInfo.left += width;
        drawText (dc, pidInfo.second.getInfoString(), mTextFormat, rInfo, mWindow->getWhiteBrush(), mLineHeight);
        r.top = r.bottom;
        r.bottom += mLineHeight;

        if (pidInfo.second.mTotal > pow (10, mPacketDigits))
          mPacketDigits++;
        }

      if (mTs->getDiscontinuity() > pow (10, mContDigits))
        mContDigits++;
      }
      //}}}
    }
  //}}}

private:
  const float kDefaultLineHeight = 13.f;
  const float kLargeLineHeight = 16.f;

  float mLineHeight = kDefaultLineHeight;

  cTransportStream* mTs;
  IDWriteTextFormat* mTextFormat = nullptr;

  int mContDigits = 0;
  int mPacketDigits = 1;
  };
