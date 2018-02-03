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

      auto textHeight = kLineHeight*4.f/5.f;
      auto r = mRect;
      r.bottom = kLineHeight;
      r.left += kLineHeight / 5.f;

      r.right = r.left + 40.f;
      mBoxItemVec.push_back (new cTune (this, textHeight, r, mDvb, "itv", 650));
      r.left = r.right;

      r.right = r.left + 40.f;
      mBoxItemVec.push_back (new cTune (this, textHeight, r, mDvb, "bbc", 674));
      r.left = r.right;

      r.right = r.left + 40.f;
      mBoxItemVec.push_back (new cTune (this, textHeight, r, mDvb, "hd", 706));

      r.left = mRect.right - 200.f;
      r.right = r.left + 100.f;
      mBoxItemVec.push_back (new cValue (this, textHeight, r, "sig " + dec(mDvb->getSignal())));
      r.left = r.right;

      r.right = mRect.right + 100.f;
      mBoxItemVec.push_back (new cValue (this, textHeight, r, "err " + dec(mDvb->getDiscontinuity())));

      r = mRect;
      r.top += kLineHeight;
      draw (dc, r);
      }
    }
  //}}}

private:
  //{{{
  class cTune : public cBoxItem {
  public:
    cTune (cTsEpgBox* box, float textHeight, cRect r, cDvb* dvb, const string& name, int frequency) :
        cBoxItem(box, nullptr, textHeight, r), mDvb(dvb), mFrequency(frequency)  {
      mStr = name;
      mBrush = (mDvb->getFrequency() == frequency) ? mBox->getWindow()->getWhiteBrush() : mBox->getWindow()->getGreyBrush();
      }
    virtual ~cTune() {}

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
  class cValue : public cBoxItem {
  public:
    cValue (cTsEpgBox* box, float textHeight, cRect r, const string& name) : 
        cBoxItem(box, nullptr, textHeight, r) {
      mStr = name;
      mBrush = mBox->getWindow()->getWhiteBrush();
      }

    virtual ~cValue() {}
    };
  //}}}

  cDvb* mDvb;
  };
