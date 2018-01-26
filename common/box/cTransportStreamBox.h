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

    auto itemIt = mBoxItemMap.find (int(pos.y / mLineHeight));
    if (itemIt != mBoxItemMap.end()) {
      itemIt->second->onDown();
      getWindow()->changed();
      }

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
      // construct services menu
      mBoxItemMap.clear();
      auto now = mTs->getCurTime();
      struct tm nowTime = *localtime (&now);

      int lineIndex = 0;
      int serviceIndex = 1;
      for (auto &service : mTs->mServiceMap) {
        //{{{  add serviceNameItem
        auto serviceNameItem = new cServiceNameItem (this, &service.second, serviceIndex++);
        mBoxItemMap.insert (std::map<int,cBoxItem*>::value_type (lineIndex++, serviceNameItem));
        //}}}
        if (service.second.getNow()) {
          //{{{  add serviceNowItem
          auto serviceNowItem = new cServiceNowItem (this, &service.second, mTs);
          mBoxItemMap.insert (std::map<int, cBoxItem*>::value_type (lineIndex++, serviceNowItem));
          }
          //}}}
        if (service.second.getShowEpg()) {
          //{{{  add service epgEntryItems
          for (auto &epgItem : service.second.getEpgItemMap()) {
            auto timet = epgItem.second.getStartTime();
            struct tm time = *localtime (&timet);
            if ((time.tm_mday == nowTime.tm_mday) && (timet > now)) {
              auto epgEntryItem = new cEpgEntryItem (this, &service.second, &epgItem.second);
              mBoxItemMap.insert (std::map<int, cBoxItem*>::value_type (lineIndex++, epgEntryItem));
              }
            }
          //}}}
          }
        }

      // draw services
      for (auto boxItem : mBoxItemMap)
        serviceWidth = max (boxItem.second->onDraw (dc, r), serviceWidth);
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
    cBoxItem (cTransportStreamBox* box, cService* service) :
      mBox(box), mService(service) {}

    virtual void onDown() = 0;
    //{{{
    virtual float onDraw (ID2D1DeviceContext* dc, cRect& r) {
      auto width = mBox->drawText (dc, mStr, mBox->mTextFormat, r, mBrush, mBox->mLineHeight);
      mRect = r;
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
  class cServiceNameItem : public cBoxItem {
  public:
    cServiceNameItem (cTransportStreamBox* box, cService* service, int index) :
       cBoxItem(box, service) {
      mStr = dec(index, 2) + " " + service->getNameString();
      mBrush = mService->getShowEpg() ? mBox->getWindow()->getBlueBrush() : mBox->getWindow()->getWhiteBrush();
      }

    virtual void onDown() { mService->toggleShowEpg(); }
    };
  //}}}
  //{{{
  class cServiceNowItem : public cBoxItem {
  public:
    cServiceNowItem (cTransportStreamBox* box, cService* service, cTransportStream* ts) :
        cBoxItem(box, service), mTs(ts) {
      mStr = "  - now - " + mService->getNow()->getStartTimeString() +
             " " + mService->getNow()->getDurationString() +
             " " + mService->getNow()->getTitleString();
      mBrush = mService->getNow()->getRecord() ? mBox->getWindow()->getWhiteBrush() : mBox->getWindow()->getBlueBrush();
      }

    virtual void onDown() {
      mService->getNow()->toggleRecord();
      mTs->startProgram (mService,
                         mService->getNow()->getTitleString(),
                         mService->getNow()->getStartTime(),
                         mService->getNow()->getRecord());
      }

  private:
    cTransportStream* mTs;
    };
  //}}}
  //{{{
  class cEpgEntryItem : public cBoxItem {
  public:
    cEpgEntryItem (cTransportStreamBox* box, cService* service, cEpgItem* epgItem) :
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

  const float kDefaultLineHeight = 13.f;
  const float kLargeLineHeight = 16.f;

  float mLineHeight = kDefaultLineHeight;

  cTransportStream* mTs;
  IDWriteTextFormat* mTextFormat = nullptr;

  int mContDigits = 0;
  int mPacketDigits = 1;

  std::map<int,cBoxItem*> mBoxItemMap;
  };
