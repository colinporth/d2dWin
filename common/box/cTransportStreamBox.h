// cTransportStreamPidBox.h
#pragma once
#include "../cD2dWindow.h"
#include "../../../shared/dvb/cTransportStream.h"

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

    auto epgItemIt = mEpgItemMap.find (int(pos.y/ mLineHeight));
    if (epgItemIt == mEpgItemMap.end()) {
      auto serviceNameIt = mServiceNameMap.find (int(pos.y/ mLineHeight));
      if (serviceNameIt == mServiceNameMap.end())
        togglePin();
      else
        serviceNameIt->second->toggleShowEpg();
      }
    else
      epgItemIt->second->toggleRecord();

    return true;
    }
  //}}}
  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    auto r = cRect (mRect.left, mRect.top, mRect.right, mRect.top + mLineHeight);
    auto serviceWidth = 0.f;

    {
    lock_guard<mutex> lockGuard (mTs->mMutex);
    mLineHeight = mTs->mServiceMap.size() >= 10 ? kDefaultLineHeight : kLargeLineHeight;
    if (mTs->mServiceMap.size() > 1) {
      //{{{  draw services
      mServiceNameMap.clear();
      mEpgItemMap.clear();
      auto now = mTs->getCurTime();
      struct tm nowTime = *localtime (&now);

      int i = 1;
      for (auto &service : mTs->mServiceMap) {
        auto str = dec(i, mTs->mServiceMap.size() >= 10 ? 2:1) + " " + service.second.getNameString();
        serviceWidth = max (drawText (dc, str, mTextFormat, r, mWindow->getWhiteBrush(), mLineHeight), serviceWidth);
        mServiceNameMap.insert (std::map<int,cService*>::value_type (int((r.top-mRect.top)/mLineHeight), &service.second));
        r.top = r.bottom;
        r.bottom += mLineHeight;

        auto epgItem = service.second.getNow();
        if (epgItem) {
          str = "  - now - " + epgItem->getStartTimeString() +
                " " + epgItem->getDurationString() +
                " " + epgItem->getTitleString();
          auto brush = epgItem->getRecord() ? mWindow->getWhiteBrush() : mWindow->getBlueBrush();
          serviceWidth = max (drawText (dc, str, mTextFormat, r, brush, mLineHeight), serviceWidth);
          mEpgItemMap.insert (std::map<int,cEpgItem*>::value_type (int((r.top-mRect.top)/mLineHeight), epgItem));
          r.top = r.bottom;
          r.bottom += mLineHeight;
          }

        if (service.second.getShowEpg()) {
          for (auto &epgItem : service.second.getEpgItemMap()) {
            auto timet = epgItem.second.getStartTime();
            struct tm time = *localtime (&timet);
            if ((time.tm_mday == nowTime.tm_mday) && (timet > now)) {
              std::string str = "        - " + dec(time.tm_hour,2,' ') +
                                ":" + dec(time.tm_min,2,'0') +
                                " "+ epgItem.second.getTitleString();
              auto brush = epgItem.second.getRecord() ? mWindow->getWhiteBrush() : mWindow->getBlueBrush();
              serviceWidth = max (drawText (dc, str, mTextFormat, r, brush, mLineHeight), serviceWidth);
              mEpgItemMap.insert (std::map<int,cEpgItem*>::value_type (int((r.top-mRect.top)/mLineHeight), &epgItem.second));
              r.top = r.bottom;
              r.bottom += mLineHeight;
              }
            }
          }

        i++;
        }
      serviceWidth += mLineHeight;
      }
      //}}}
    }

    {
    lock_guard<mutex> lockGuard (mTs->mMutex);
    if (mTs->mPidInfoMap.size()) {
      //{{{  draw pids
      auto maxPidPackets = 10000;
      for (auto &pidInfo : mTs->mPidInfoMap)
        maxPidPackets = max (maxPidPackets, pidInfo.second.mPackets);

      auto r = cRect (mRect.left + serviceWidth, mRect.top,
                      mRect.right, mRect.top + mLineHeight);
      for (auto &pidInfo : mTs->mPidInfoMap) {
        auto str = wdec (pidInfo.second.mPackets,mPacketDigits) +
                   (mContDigits ? (L":" + wdec(pidInfo.second.mDisContinuity, mContDigits)) : L"") +
                   L" " + wdec(pidInfo.first, 4) +
                   L" " + getFullPtsWstring (pidInfo.second.mPts) +
                   L" " + pidInfo.second.getTypeWstring();
        auto width = drawText (dc, str, mTextFormat, r, mWindow->getWhiteBrush(), mLineHeight) + mLineHeight/2.f;

        dc->FillRectangle (
          cRect (r.left + width, r.top+4.f,
                 r.left + width + (r.getWidth() - width)*pidInfo.second.mPackets/maxPidPackets, r.top+mLineHeight),
          mWindow->getOrangeBrush());

        auto rInfo = r;
        rInfo.left += width;
        drawText (dc, pidInfo.second.getInfoString(), mTextFormat, rInfo, mWindow->getWhiteBrush(), mLineHeight);
        r.top = r.bottom;
        r.bottom += mLineHeight;

        if (pidInfo.second.mPackets > pow (10, mPacketDigits))
          mPacketDigits++;
        }

      if (mTs->getDiscontinuity() > pow (10, mContDigits))
        mContDigits++;
      }
      //}}}
    }
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

  std::map<int,cEpgItem*> mEpgItemMap;
  std::map<int,cService*> mServiceNameMap;
  };
