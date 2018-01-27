// cTsPidBox.h
//{{{  includes
#pragma once
#include "../cD2dWindow.h"
#include "../../../shared/dvb/cTransportStream.h"
//}}}

class cTsPidBox : public cD2dWindow::cBox {
public:
  //{{{
  cTsPidBox (cD2dWindow* window, float width, float height, cTransportStream* ts)
      : cBox("tsPid", window, width, height), mTs(ts) {

    mPin = true;

    window->getDwriteFactory()->CreateTextFormat (L"Consolas", NULL,
      DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
      mLineHeight, L"en-us",
      &mTextFormat);
    }
  //}}}
  //{{{
  virtual ~cTsPidBox() {
    mTextFormat->Release();
    }
  //}}}

  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    if (mTs->mPidInfoMap.size()) {

      lock_guard<mutex> lockGuard (mTs->mMutex);

      mLineHeight = (mTs->mServiceMap.size() >= 10) ? kDefaultLineHeight : kLargeLineHeight;
      auto r = mRect;

      // draw pids
      auto maxPidPackets = 10000;
      for (auto& pidInfo : mTs->mPidInfoMap)
        maxPidPackets = max (maxPidPackets, pidInfo.second.mPackets);

      for (auto &pidInfo : mTs->mPidInfoMap) {
        auto str = wdec (pidInfo.second.mPackets,mPacketDigits) +
                   (mContDigits ? (L":" + wdec(pidInfo.second.mDisContinuity, mContDigits)) : L"") +
                   L" " + wdec(pidInfo.first, 4) +
                   L" " + getFullPtsWstring (pidInfo.second.mPts) +
                   L" " + pidInfo.second.getTypeWstring();

        r.left = mRect.left;
        r.bottom = r.top + mLineHeight;
        auto width = drawText (dc, str, mTextFormat, r, mWindow->getWhiteBrush(), mLineHeight) + mLineHeight/2.f;

        r.left = mRect.left + width;
        r.right = r.left + r.getWidth() * pidInfo.second.mPackets/maxPidPackets;
        dc->FillRectangle (r, mWindow->getOrangeBrush());

        r.right = mRect.right;
        drawText (dc, pidInfo.second.getInfoString(), mTextFormat, r, mWindow->getWhiteBrush(), mLineHeight);

        r.top = r.bottom;
        r.bottom += mLineHeight;

        if (pidInfo.second.mPackets > pow (10, mPacketDigits))
          mPacketDigits++;
        }

      if (mTs->getDiscontinuity() > pow (10, mContDigits))
        mContDigits++;
      }
    }
  //}}}

private:
  // vars
  cTransportStream* mTs;

  const float kLargeLineHeight = 16.f;
  const float kDefaultLineHeight = 13.f;
  float mLineHeight = kDefaultLineHeight;

  int mContDigits = 0;
  int mPacketDigits = 1;

  IDWriteTextFormat* mTextFormat = nullptr;
  };
