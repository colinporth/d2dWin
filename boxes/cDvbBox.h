// cDvbBox.h
//{{{  includes
#pragma once

#include "../boxes/cTsEpgBox.h"
#include "../../shared/dvb/cWinDvb.h"
//}}}

class cDvbBox : public cTsEpgBox {
public:
  //{{{
  cDvbBox (cD2dWindow* window, float width, float height, cDvb* dvb) :
    cTsEpgBox(window, width, height, dvb), mDvb(dvb) {}
  //}}}
  virtual ~cDvbBox() {}

  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    if (!getTimedOn() || mWindow->getTimedMenuOn()) {

      lock_guard<mutex> lockGuard (mTs->mMutex);
      clear();

      auto r = mRect;
      r.bottom = kLineHeight;
      r.left += kLineHeight / 5.f;

      r.right = r.left + 60.f;
      mBoxItemVec.push_back (new cTuneSelect (this, kLineHeight, r, mDvb, "itv", 650));
      r.left = r.right;

      r.right = r.left + 60.f;
      mBoxItemVec.push_back (new cTuneSelect (this, kLineHeight, r, mDvb, "bbc", 674));
      r.left = r.right;

      r.right = r.left + 60.f;
      mBoxItemVec.push_back (new cTuneSelect (this, kLineHeight, r, mDvb, "hd", 706));
      r.left = r.right;

      r.right = r.left + 60.f;
      mBoxItemVec.push_back (new cSignalValue (this, kLineHeight, r, mDvb, "sig "));
      r.left = r.right;

      r.right = r.left + 60.f;
      mBoxItemVec.push_back (new cErrorValue (this, kLineHeight, r, mDvb, "err "));
      r.left = r.right;

      r = mRect;
      r.top += kLineHeight;
      draw (dc, r);
      }
    }
  //}}}

private:
  //{{{
  class cTuneSelect : public cBoxItem {
  public:
    cTuneSelect (cTsEpgBox* box, float textHeight, cRect r, cDvb* dvb, const string& name, int frequency) :
        cBoxItem(box, nullptr, textHeight, r), mDvb(dvb), mFrequency(frequency)  {
      mStr = name;
      mBrush = mBox->getWindow()->getWhiteBrush();
      }
    virtual ~cTuneSelect() {}

    virtual void onDown() {
      mDvb->stop();
      mDvb->tune (mFrequency * 1000);
      }

    private:
      cDvb* mDvb;
      int mFrequency = 0;
    };
  //}}}
  //{{{
  class cSignalValue : public cBoxItem {
  public:
    cSignalValue (cTsEpgBox* box, float textHeight, cRect r, cDvb* dvb, const string& name) :
        cBoxItem(box, nullptr, textHeight, r), mDvb(dvb), mName(name) {
      mBrush = mBox->getWindow()->getWhiteBrush();
      }
    virtual ~cSignalValue() {}

    virtual void onDraw (ID2D1DeviceContext* dc) {
      mStr = mName + " " + dec(mDvb->mSignal);
      cBoxItem::onDraw (dc);
      }

    private:
      cDvb* mDvb;
      string mName;
    };
  //}}}
  //{{{
  class cErrorValue : public cBoxItem {
  public:
    cErrorValue (cTsEpgBox* box, float textHeight, cRect r, cDvb* dvb, const string& name) :
        cBoxItem(box, nullptr, textHeight, r), mDvb(dvb), mName(name) {
      mBrush = mBox->getWindow()->getWhiteBrush();
      }
    virtual ~cErrorValue() {}

    virtual void onDraw (ID2D1DeviceContext* dc) {
      mStr = mName + " " + dec(mDvb->mDiscontinuity);
      cBoxItem::onDraw (dc);
      }

    private:
      cDvb* mDvb;
      string mName;
    };
  //}}}

  cDvb* mDvb;
  };
