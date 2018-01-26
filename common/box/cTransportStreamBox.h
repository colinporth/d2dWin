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

    auto itemIt = mItemMap.find (int(pos.y / mLineHeight));
    if (itemIt != mItemMap.end())
      itemIt->second->onDown();
    return true;
    }
  //}}}
  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    lock_guard<mutex> lockGuard (mTs->mMutex);

    auto r = cRect (mRect.left, mRect.top, mRect.right, mRect.top + mLineHeight);
    mLineHeight = mTs->mServiceMap.size() >= 10 ? kDefaultLineHeight : kLargeLineHeight;

    auto serviceWidth = 0.f;
    if (mTs->mServiceMap.size() > 1) {
      // draw services
      mItemMap.clear();
      auto now = mTs->getCurTime();
      struct tm nowTime = *localtime (&now);

      int index = 1;
      for (auto &service : mTs->mServiceMap) {
        //{{{  add serviceNameItem
        auto serviceNameItem = new cServiceNameItem (this, &service.second, index++);
        mItemMap.insert (std::map<int,cItem*>::value_type (int((r.top-mRect.top)/mLineHeight), serviceNameItem));
        serviceWidth = max (serviceNameItem->onDraw (dc, r), serviceWidth);
        //}}}
        if (service.second.getNow()) {
          //{{{  add serviceNowItem
          auto serviceNowItem = new cServiceNowItem (this, &service.second, mTs);
          mItemMap.insert (std::map<int,cItem*>::value_type (int((r.top-mRect.top)/mLineHeight), serviceNowItem));
          serviceWidth = max (serviceNowItem->onDraw (dc, r), serviceWidth);
          }
          //}}}
        if (service.second.getShowEpg()) {
          //{{{  add service epgEntryItems
          for (auto &epgItem : service.second.getEpgItemMap()) {
            auto timet = epgItem.second.getStartTime();
            struct tm time = *localtime (&timet);
            if ((time.tm_mday == nowTime.tm_mday) && (timet > now)) {
              auto epgEntryItem = new cEpgEntryItem (this, &service.second, &epgItem.second);
              mItemMap.insert (std::map<int,cItem*>::value_type (int((r.top-mRect.top)/mLineHeight), epgEntryItem));
              serviceWidth = max (epgEntryItem->onDraw (dc, r), serviceWidth);
              }
            }
          //}}}
          }
        }
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
  class cItem {
  public:
    cItem (cTransportStreamBox* box, cService* service) :
      mBox(box), mService(service) {}

    virtual void onDown() = 0;
    virtual float onDraw (ID2D1DeviceContext* dc, cRect& r) = 0;

  protected:
    cTransportStreamBox* mBox;
    cService* mService;
    };
  //}}}
  //{{{
  class cServiceNameItem : public cItem {
  public:
    cServiceNameItem (cTransportStreamBox* box, cService* service, int index) :
      cItem(box, service), mIndex(index) {}

    virtual void onDown() { mService->toggleShowEpg(); }

    virtual float onDraw (ID2D1DeviceContext* dc, cRect& r) {
      auto str = dec(mIndex, 2) + " " + mService->getNameString();
      auto brush = mService->getShowEpg() ? mBox->getWindow()->getBlueBrush() : mBox->getWindow()->getWhiteBrush();
      auto width = mBox->drawText (dc, str, mBox->mTextFormat, r, brush, mBox->mLineHeight);
      r.top = r.bottom;
      r.bottom += mBox->mLineHeight;
      return width;
      }

  private:
    int mIndex = 0;
    };
  //}}}
  //{{{
  class cServiceNowItem : public cItem {
  public:
    cServiceNowItem (cTransportStreamBox* box, cService* service, cTransportStream* ts) :
      cItem(box, service), mTs(ts) {}

    virtual void onDown() {
      mService->getNow()->toggleRecord();
      mTs->startProgram (mService,
                         mService->getNow()->getTitleString(),
                         mService->getNow()->getStartTime(),
                         mService->getNow()->getRecord());
      }

    virtual float onDraw (ID2D1DeviceContext* dc, cRect& r) {
      auto epgItem = mService->getNow();

      auto str = "  - now - " + epgItem->getStartTimeString() +
                 " " + epgItem->getDurationString() +
                 " " + epgItem->getTitleString();
      auto brush = epgItem->getRecord() ? mBox->getWindow()->getWhiteBrush() : mBox->getWindow()->getBlueBrush();
      auto width = mBox->drawText (dc, str, mBox->mTextFormat, r, brush, mBox->mLineHeight);
      r.top = r.bottom;
      r.bottom += mBox->mLineHeight;
      return width;
      }

  private:
    cTransportStream* mTs;
    };
  //}}}
  //{{{
  class cEpgEntryItem : public cItem {
  public:
    cEpgEntryItem (cTransportStreamBox* box, cService* service, cEpgItem* epgItem) :
      cItem(box, service), mEpgItem(epgItem) {}

    virtual void onDown() { mEpgItem->toggleRecord(); }

    virtual float onDraw (ID2D1DeviceContext* dc, cRect& r) {
      auto timet = mEpgItem->getStartTime();
      struct tm time = *localtime (&timet);

      auto str = "        - " + dec(time.tm_hour,2,' ') +
                 ":" + dec(time.tm_min,2,'0') +
                 " "+ mEpgItem->getTitleString();
      auto brush = mEpgItem->getRecord() ? mBox->getWindow()->getWhiteBrush() : mBox->getWindow()->getBlueBrush();
      auto width = mBox->drawText (dc, str, mBox->mTextFormat, r, brush, mBox->mLineHeight);
      r.top = r.bottom;
      r.bottom += mBox->mLineHeight;
      return width;
      }

  private:
    cEpgItem* mEpgItem;
    };
  //}}}

  const float kDefaultLineHeight = 13.f;
  const float kLargeLineHeight = 16.f;

  float mLineHeight = kDefaultLineHeight;

  cTransportStream* mTs;
  IDWriteTextFormat* mTextFormat = nullptr;

  int mContDigits = 0;
  int mPacketDigits = 1;

  std::map<int,cItem*> mItemMap;
  };
