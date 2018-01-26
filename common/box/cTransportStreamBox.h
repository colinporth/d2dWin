// cTransportStreamBox.h
//{{{  includes
#pragma once
#include "../cD2dWindow.h"
#include "../../../shared/dvb/cTransportStream.h"
//}}}

class cTransportStreamBox : public cD2dWindow::cBox {
public:
  //{{{
  cTransportStreamBox (cD2dWindow* window, float width, float height, cTransportStream* ts)
      : cBox("tsPid", window, width, height), mTs(ts) {

    mPin = true;

    window->getDwriteFactory()->CreateTextFormat (L"Consolas", NULL,
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

    pos += getTL();
    for (auto boxItem : mBoxItemVec)
      if (boxItem->inside (pos)) {
        boxItem->onDown();
        getWindow()->changed();
        break;
        }

    return true;
    }
  //}}}
  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    lock_guard<mutex> lockGuard (mTs->mMutex);

    mLineHeight = (mTs->mServiceMap.size() >= 10) ? kDefaultLineHeight : kLargeLineHeight;
    auto r = cRect (mRect.left, mRect.top, mRect.right, mRect.top + mLineHeight);

    auto serviceWidth = 0.f;
    if (mTs->mServiceMap.size() > 1) {
      // construct services menu, !!! could check for ts service change here to cull rebuilding menu !!!
      auto nowTime = mTs->getCurTime();
      struct tm nowTm = *localtime (&nowTime);
      int nowDay = nowTm.tm_mday;

      mBoxItemVec.clear();

      int serviceIndex = 1;
      for (auto& service : mTs->mServiceMap) {
        mBoxItemVec.push_back (new cServiceName (this, &service.second, serviceIndex++));
        if (service.second.getNow())
          mBoxItemVec.push_back (new cServiceNow (this, &service.second, mTs));
        if (service.second.getShowEpg()) {
          for (auto &epgItem : service.second.getEpgItemMap()) {
            auto timet = epgItem.second.getStartTime();
            struct tm time = *localtime (&timet);
            if ((time.tm_mday == nowDay) && (timet > nowTime)) // later today
              mBoxItemVec.push_back (new cServiceEpg (this, &service.second, &epgItem.second));
            }
          }
        }

      // draw services boxItems
      for (auto boxItem : mBoxItemVec)
        serviceWidth = max (boxItem->onDraw (dc, r), serviceWidth);
      serviceWidth += mLineHeight;
      }

    if (mTs->mPidInfoMap.size()) {
      //{{{  draw pids
      auto maxPidPackets = 10000;
      for (auto &pidInfo : mTs->mPidInfoMap)
        maxPidPackets = max (maxPidPackets, pidInfo.second.mPackets);

      auto r = cRect (mRect.left + serviceWidth, mRect.top, mRect.right, mRect.top + mLineHeight);
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
  //}}}

private:
  //{{{
  class cBoxItem {
  public:
    cBoxItem (cTransportStreamBox* box, cService* service) : mBox(box), mService(service) {}

    bool inside (const cPoint& pos) { return mRect.inside (pos); }

    virtual void onDown() = 0;
    //{{{
    virtual float onDraw (ID2D1DeviceContext* dc, cRect& r) {

      auto width = mBox->drawText (dc, mStr, mBox->mTextFormat, r, mBrush, mBox->mLineHeight);

      mRect = r;
      mRect.right = r.left + width;

      r.top = r.bottom;
      r.bottom += mBox->mLineHeight;
      return width;
      }
    //}}}

  protected:
    cTransportStreamBox* mBox;
    cService* mService;

    std::string mStr;
    ID2D1SolidColorBrush* mBrush;
    cRect mRect;
    };
  //}}}
  //{{{
  class cServiceName : public cBoxItem {
  public:
    cServiceName (cTransportStreamBox* box, cService* service, int index) : cBoxItem(box, service) {
      mStr = dec(index,2) + " " + service->getNameString();
      mBrush = mService->getShowEpg() ? mBox->getWindow()->getBlueBrush() : mBox->getWindow()->getWhiteBrush();
      }

    virtual void onDown() { mService->toggleShowEpg(); }
    };
  //}}}
  //{{{
  class cServiceNow : public cBoxItem {
  public:
    cServiceNow (cTransportStreamBox* box, cService* service, cTransportStream* ts) :
        cBoxItem(box, service), mTs(ts) {
      mStr = "  - now - " + mService->getNow()->getStartTimeString() +
             " " + mService->getNow()->getDurationString() +
             " " + mService->getNow()->getTitleString();
      mBrush = mService->getNow()->getRecord() ? mBox->getWindow()->getWhiteBrush() : mBox->getWindow()->getBlueBrush();
      }

    virtual void onDown() {
      mService->getNow()->toggleRecord();
      mTs->start (mService,
                  mService->getNow()->getTitleString(),
                  mService->getNow()->getStartTime(),
                  mService->getNow()->getRecord());
      }

  private:
    cTransportStream* mTs;
    };
  //}}}
  //{{{
  class cServiceEpg : public cBoxItem {
  public:
    cServiceEpg (cTransportStreamBox* box, cService* service, cEpgItem* epgItem) :
        cBoxItem(box, service), mEpgItem(epgItem) {
      auto timet = mEpgItem->getStartTime();
      struct tm time = *localtime (&timet);
      mStr = "        - " + dec(time.tm_hour,2,' ') + ":" + dec(time.tm_min,2,'0') +
             " " + mEpgItem->getTitleString();

      mBrush = mEpgItem->getRecord() ? mBox->getWindow()->getWhiteBrush() : mBox->getWindow()->getBlueBrush();
      }

    virtual void onDown() { mEpgItem->toggleRecord(); }

  private:
    cEpgItem* mEpgItem;
    };
  //}}}

  cTransportStream* mTs;

  const float kLargeLineHeight = 16.f;
  const float kDefaultLineHeight = 13.f;
  float mLineHeight = kDefaultLineHeight;

  IDWriteTextFormat* mTextFormat = nullptr;

  int mContDigits = 0;
  int mPacketDigits = 1;

  std::vector<cBoxItem*> mBoxItemVec;
  };
