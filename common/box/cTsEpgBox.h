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

    const float kLineHeight = 16.f;
    const float kSmallLineHeight = 13.f;

    if (!getTimedOn() || mWindow->getTimedMenuOn()) {
      if (mTs->mServiceMap.size() > 1) {
        // construct services menu, !!! could check for ts service change here to cull rebuilding menu !!!
        auto curTime = mTs->getCurTime();
        auto curDatePoint = date::floor<date::days>(curTime);
        auto curYearMonthDay = date::year_month_day{curDatePoint};
        auto curToday = curYearMonthDay.day();

        // line heights
        auto lineHeight = (getHeight() / mTs->mServiceMap.size() > kLineHeight) ? kLineHeight : kSmallLineHeight;
        auto bigLineHeight = lineHeight*4.f/3.f;
        auto bgndRect = mRect;
        auto r = mRect;
        r.left += lineHeight / 5.f;

        {
        lock_guard<mutex> lockGuard (mTs->mMutex);
        clear();
        for (auto& service : mTs->mServiceMap) {
          r.bottom = r.top + bigLineHeight;
          mBoxItemVec.push_back (new cServiceName (this, &service.second, bigLineHeight, r));
          r.top = r.bottom;

          if (service.second.getNowEpgItem()) {
            r.bottom = r.top + lineHeight;
            mBoxItemVec.push_back (new cServiceNow (this, &service.second, mTs, lineHeight, r));
            r.top = r.bottom;
            }

          if (service.second.getShowEpg()) {
            for (auto epgItem : service.second.getEpgItemMap()) {
              auto startTime = epgItem.second->getStartTime();
              auto startDatePoint = date::floor<date::days>(startTime);
              auto startYearMonthDay = date::year_month_day{startDatePoint};
              auto startToday = startYearMonthDay.day();
              if ((startToday == curToday) && (startTime > curTime)) { // later today
                r.bottom = r.top + lineHeight;
                mBoxItemVec.push_back (new cServiceEpg (this, &service.second, epgItem.second, lineHeight, r));
                r.top = r.bottom;
                }
              }
            }
          r.top += lineHeight/4.f;
          r.bottom = r.top + lineHeight;
          }
        }

        // draw services boxItems
        bgndRect.bottom = r.bottom;
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
      mBrush = mService->getShowEpg() ? mBox->getWindow()->getWhiteBrush() : mBox->getWindow()->getBlueBrush();
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
      mStr = date::format ("%T", floor<chrono::seconds>(mEpgItem->getStartTime()));
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
