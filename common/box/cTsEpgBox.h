// cTsEpgBox.h
//{{{  includes
#pragma once
#include "../cD2dWindow.h"
#include "../../../shared/dvb/cTransportStream.h"
//}}}

class cTsEpgBox : public cD2dWindow::cBox {
public:
  //{{{
  cTsEpgBox (cD2dWindow* window, float width, float height, cTransportStream* ts)
      : cBox("tsEpg", window, width, height), mTs(ts) {

    mPin = true;

    window->getDwriteFactory()->CreateTextFormat (L"Consolas", NULL,
      DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
      mLineHeight, L"en-us",
      &mTextFormat);
    }
  //}}}
  //{{{
  virtual ~cTsEpgBox() {
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
        return true;
        }

    togglePin();
    return true;
    }
  //}}}
  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    lock_guard<mutex> lockGuard (mTs->mMutex);

    if (mTs->mServiceMap.size() > 1) {
      // construct services menu, !!! could check for ts service change here to cull rebuilding menu !!!
      auto nowTime = mTs->getCurTime();
      struct tm nowTm = *localtime (&nowTime);
      int nowDay = nowTm.tm_mday;

      while (!mBoxItemVec.empty()) {
        auto boxItem = mBoxItemVec.back();
        delete boxItem;
        mBoxItemVec.pop_back();
        }

      mLineHeight = (mTs->mServiceMap.size() >= 10) ? kDefaultLineHeight : kLargeLineHeight;
      auto r = mRect;

      int serviceIndex = 1;
      for (auto& service : mTs->mServiceMap) {
        r.bottom = r.top + mLineHeight;
        mBoxItemVec.push_back (new cServiceName (this, &service.second, serviceIndex++, r));
        r.top = r.bottom + 1.f;

        if (service.second.getNowEpgItem()) {
          r.bottom = r.top + mLineHeight;
          mBoxItemVec.push_back (new cServiceNow (this, &service.second, mTs, r));
          r.top = r.bottom + 1.f;
          }
        if (service.second.getShowEpg()) {
          for (auto epgItem : service.second.getEpgItemMap()) {
            auto timet = epgItem.second->getStartTime();
            struct tm time = *localtime (&timet);
            if ((time.tm_mday == nowDay) && (timet > nowTime)) { // later today
              r.bottom = r.top + mLineHeight;
              mBoxItemVec.push_back (new cServiceEpg (this, &service.second, epgItem.second, r));
              r.top = r.bottom + 1.f;
              }
            }
          }
        }

      // draw services boxItems
      for (auto boxItem : mBoxItemVec)
        boxItem->onDraw (dc);
      }
    }
  //}}}

private:
  //{{{
  class cBoxItem {
  public:
    //{{{
    cBoxItem (cTsEpgBox* box, cService* service, const cRect& r) :
      mBox(box), mService(service), mRect(r) {}
    //}}}
    ~cBoxItem() {}

    bool inside (const cPoint& pos) { return mRect.inside (pos); }
    void setRect (const cRect& r) { mRect = r; }

    virtual void onDown() = 0;
    //{{{
    virtual void onDraw (ID2D1DeviceContext* dc) {
      dc->FillRectangle (mRect, mBox->getWindow()->getTransparentBgndBrush());
      mRect.right = mRect.left + mBox->drawText (dc, mStr, mBox->mTextFormat, mRect, mBrush, mBox->mLineHeight);
      }
    //}}}

  protected:
    cTsEpgBox* mBox;
    cService* mService;
    cRect mRect;

    std::string mStr;
    ID2D1SolidColorBrush* mBgndBrush;
    ID2D1SolidColorBrush* mBrush;
    };
  //}}}
  //{{{
  class cServiceName : public cBoxItem {
  public:
    //{{{
    cServiceName (cTsEpgBox* box, cService* service, int index, const cRect& r) : cBoxItem(box, service, r) {
      mStr = dec(index,2) + " " + service->getNameString();
      mBrush = mService->getShowEpg() ? mBox->getWindow()->getBlueBrush() : mBox->getWindow()->getWhiteBrush();
      }
    //}}}
    ~cServiceName() {}

    virtual void onDown() { mService->toggleShowEpg(); }
    };
  //}}}
  //{{{
  class cServiceNow : public cBoxItem {
  public:
    //{{{
    cServiceNow (cTsEpgBox* box, cService* service, cTransportStream* ts, const cRect& r) :
        cBoxItem(box, service, r), mTs(ts) {
      mStr = "  - now - " + mService->getNowEpgItem()->getStartTimeString() +
             " " + mService->getNowEpgItem()->getDurationString() +
             " " + mService->getNowEpgItem()->getTitleString();
      mBrush = mService->getNowEpgItem()->getRecord() ? mBox->getWindow()->getWhiteBrush() : mBox->getWindow()->getBlueBrush();
      }
    //}}}
    ~cServiceNow() {}

    //{{{
    virtual void onDown() {

      if (mService->getNowEpgItem()->toggleRecord())
        mTs->start (mService, mService->getNowEpgItem()->getTitleString(), mTs->getCurTime(), true);
      else
        mTs->stop (mService);
      }
    //}}}

  private:
    cTransportStream* mTs;
    };
  //}}}
  //{{{
  class cServiceEpg : public cBoxItem {
  public:
    //{{{
    cServiceEpg (cTsEpgBox* box, cService* service, cEpgItem* epgItem, const cRect& r) :
        cBoxItem(box, service,r), mEpgItem(epgItem) {
      auto timet = mEpgItem->getStartTime();
      struct tm time = *localtime (&timet);
      mStr = "        - " + dec(time.tm_hour,2,' ') +
             ":" + dec(time.tm_min,2,'0') +
             " " + mEpgItem->getTitleString();

      mBrush = mEpgItem->getRecord() ? mBox->getWindow()->getWhiteBrush() : mBox->getWindow()->getBlueBrush();
      }
    //}}}
    ~cServiceEpg() {}

    virtual void onDown() { mEpgItem->toggleRecord(); }

  private:
    cEpgItem* mEpgItem;
    };
  //}}}

  // vars
  cTransportStream* mTs;

  const float kLargeLineHeight = 16.f;
  const float kDefaultLineHeight = 13.f;
  float mLineHeight = kDefaultLineHeight;

  IDWriteTextFormat* mTextFormat = nullptr;

  std::vector<cBoxItem*> mBoxItemVec;
  };
