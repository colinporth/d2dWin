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
    }
  //}}}
  virtual ~cTsEpgBox() {}

  //{{{
  bool onDown (bool right, cPoint pos)  {

    if (!getTimedOn() || mWindow->getTimedMenuOn()) {
      pos += getTL();

      for (auto boxItem : mBoxItemVec)
        if (boxItem->inside (pos)) {
          boxItem->onDown();
          getWindow()->changed();
          return true;
          }
      togglePin();
      }

    return true;
    }
  //}}}
  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    const float kDefaultLineHeight = 16.f;
    const float kSmallLineHeight = 13.f;

    if (!getTimedOn() || mWindow->getTimedMenuOn()) {
      lock_guard<mutex> lockGuard (mTs->mMutex);

      if (mTs->mServiceMap.size() > 1) {
        // construct services menu, !!! could check for ts service change here to cull rebuilding menu !!!
        auto nowTime = mTs->getCurTime();
        struct tm nowTm = *localtime (&nowTime);
        int nowDay = nowTm.tm_mday;

        // notch line height
        auto lineHeight = (getHeight() / mTs->mServiceMap.size() > kDefaultLineHeight) ? kDefaultLineHeight : kSmallLineHeight;
        auto r = mRect;
        r.left += lineHeight / 5.f;

        clear();
        for (auto& service : mTs->mServiceMap) {
          r.bottom = r.top + lineHeight;
          mBoxItemVec.push_back (new cServiceName (this, &service.second, lineHeight, r));
          r.top = r.bottom;

          if (service.second.getNowEpgItem()) {
            r.bottom = r.top + lineHeight;
            mBoxItemVec.push_back (new cServiceNow (this, &service.second, mTs, lineHeight, r));
            r.top = r.bottom;
            }
          if (service.second.getShowEpg()) {
            for (auto epgItem : service.second.getEpgItemMap()) {
              auto timet = epgItem.second->getStartTime();
              struct tm time = *localtime (&timet);
              if ((time.tm_mday == nowDay) && (timet > nowTime)) { // later today
                r.bottom = r.top + lineHeight;
                mBoxItemVec.push_back (new cServiceEpg (this, &service.second, epgItem.second, lineHeight, r));
                r.top = r.bottom;
                }
              }
            }

          r.top = r.bottom + lineHeight/4.f;
          }

        // draw services boxItems
        auto bgndRect = r;
        bgndRect.top = mRect.top;
        dc->FillRectangle (bgndRect, mWindow->getTransparentBgndBrush());
        for (auto boxItem : mBoxItemVec)
          boxItem->onDraw (dc);
        }
      }
    }
  //}}}

private:
  //{{{
  class cBoxItem {
  public:
    cBoxItem (cTsEpgBox* box, cService* service, float textHeight, const cRect& r) :
      mBox(box), mService(service), mRect(r), mTextHeight(textHeight) {}
    ~cBoxItem() {}

    bool inside (const cPoint& pos) { return mRect.inside (pos); }
    void setRect (const cRect& r) { mRect = r; }

    virtual void onDown() = 0;

    virtual void onDraw (ID2D1DeviceContext* dc) {
      mRect.right = mRect.left + mBox->drawText (
        dc, mStr, mBox->getWindow()->getTextFormat(), mRect, mBrush, mTextHeight);
      }

  protected:
    cTsEpgBox* mBox;
    cService* mService;
    cRect mRect;
    const float mTextHeight;

    std::string mStr;
    ID2D1SolidColorBrush* mBgndBrush = nullptr;
    ID2D1SolidColorBrush* mBrush = nullptr;
    };
  //}}}
  //{{{
  class cServiceName : public cBoxItem {
  public:
    cServiceName (cTsEpgBox* box, cService* service, float textHeight, const cRect& r) :
        cBoxItem(box, service, textHeight, r) {
      mStr = service->getNameString();
      mBrush = mService->getShowEpg() ? mBox->getWindow()->getBlueBrush() : mBox->getWindow()->getWhiteBrush();
      }
    virtual ~cServiceName() {}

    virtual void onDown() { mService->toggleShowEpg(); }
    };
  //}}}
  //{{{
  class cServiceNow : public cBoxItem {
  public:
    cServiceNow (cTsEpgBox* box, cService* service, cTransportStream* ts, float textHeight, const cRect& r) :
        cBoxItem(box, service, textHeight,  r), mTs(ts) {
      mStr = "- " + mService->getNowEpgItem()->getStartTimeString() +
             " " + mService->getNowEpgItem()->getDurationString() +
             " " + mService->getNowEpgItem()->getTitleString();
      mBrush = mService->getNowEpgItem()->getRecord() ? mBox->getWindow()->getWhiteBrush() : mBox->getWindow()->getBlueBrush();
      }
    virtual ~cServiceNow() {}

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
    cServiceEpg (cTsEpgBox* box, cService* service, cEpgItem* epgItem, float textHeight, const cRect& r) :
        cBoxItem(box, service, textHeight, r), mEpgItem(epgItem) {
      auto timet = mEpgItem->getStartTime();
      struct tm time = *localtime (&timet);
      mStr = "- " + dec(time.tm_hour,2,' ') +
             ":" + dec(time.tm_min,2,'0') +
             " " + mEpgItem->getTitleString();

      mBrush = mEpgItem->getRecord() ? mBox->getWindow()->getWhiteBrush() : mBox->getWindow()->getBlueBrush();
      }
    virtual ~cServiceEpg() {}

    virtual void onDown() { mEpgItem->toggleRecord(); }

  private:
    cEpgItem* mEpgItem;
    };
  //}}}

  //{{{
  void clear() {
    while (!mBoxItemVec.empty()) {
      auto boxItem = mBoxItemVec.back();
      delete boxItem;
      mBoxItemVec.pop_back();
      }
    }
  //}}}

  // vars
  cTransportStream* mTs;
  std::vector<cBoxItem*> mBoxItemVec;
  };
