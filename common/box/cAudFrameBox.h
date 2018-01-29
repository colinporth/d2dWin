// cAudBox.h
#pragma once
#include "../cD2dWindow.h"
#include "../cAudFrame.h"
#include "../../../shared/utils/iAudio.h"

const float kBarsScale = 3.0f;  // need to work out power scaling derivation

class cAudFrameBox : public cD2dWindow::cBox {
public:
  //{{{
  cAudFrameBox (cD2dWindow* window, float width, float height, cAudFrame*& audFrame, iAudio* audio)
      : cBox("audioBox", window, width, height), mAudFrame(audFrame), mAudio(audio) {
    mPin = true;

    mWindow->getDc()->CreateSolidColorBrush (ColorF (0.8f, 0.1f, 0.1f, 0.5f), &mBrush);
    }
  //}}}
  //{{{
  virtual ~cAudFrameBox() {
    mBrush->Release();
    }
  //}}}

  //{{{
  bool onProx (bool inClient, cPoint pos) {

    if (mAudFrame) {
      if (mAudFrame->mChannels == 6) {
        auto audio = dynamic_cast<iAudio*>(mWindow);
        if (!audio)
          audio = dynamic_cast<iAudio*>(this);
        if (!audio)
          return false;

        if ((1.f - (pos.y / getHeight())) > (audio->getVolume() / audio->getMaxVolume()))
          audio->setMixDown (iAudio::eBestMix);
        else {
          const iAudio::eMixDown kLookup[5] = { iAudio::eBLBR, iAudio::eFLFR,
                                                iAudio::eCentre,
                                                iAudio::eFLFR, iAudio::eBLBR };
          auto barIndex = int(5.f * pos.x / getWidth());
          audio->setMixDown (kLookup[barIndex]);
          }
        }
      }

    return cBox::onProx (inClient, pos);
    }
  //}}}
  //{{{
  bool onProxExit() {

    auto audio = dynamic_cast<iAudio*>(mWindow);
    if (!audio)
      audio = dynamic_cast<iAudio*>(this);
    if (!audio)
      return false;

    audio->setMixDown (iAudio::eBestMix);
    return true;
    }
  //}}}
  //{{{
  bool onWheel (int delta, cPoint posy)  {

    auto audio = dynamic_cast<iAudio*>(mWindow);
    if (!audio)
      audio = dynamic_cast<iAudio*>(this);
    if (!audio)
      return false;

    audio->setVolume (audio->getVolume() + delta/1200.f);
    return true;
    }
  //}}}
  bool onDown (bool right, cPoint pos)  { return setFromPos (pos); }
  bool onMove (bool right, cPoint pos, cPoint inc) { return setFromPos (pos); }

  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    if (mPick) {
      //{{{  show out chans
      auto str = dec(mAudio->getDstChannels()) + " out chans";

      IDWriteTextLayout* textLayout;
      mWindow->getDwriteFactory()->CreateTextLayout (
        wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
        mWindow->getTextFormat(), getWidth(), getHeight(), &textLayout);

      dc->DrawTextLayout (getBL() + cPoint(2.f, -kTextHeight), textLayout, mWindow->getBlackBrush());
      dc->DrawTextLayout (getBL() + cPoint(0.f, -kTextHeight), textLayout, mWindow->getWhiteBrush());
      textLayout->Release();
      }
      //}}}

    if (mAudFrame) {
      if (mAudFrame->mChannels == 2) {
        //{{{  2 chan display
        auto r = mRect;

        r.left += 1.f;
        r.right = mRect.getCentreX()- 0.5f;
        drawChanBar (dc, r, true, mAudFrame->mPower[0]);

        r.left = r.right + 0.5f;
        r.right = mRect.right - 1.f;
        drawChanBar (dc, r, true, mAudFrame->mPower[1]);
        }
        //}}}
      else {
        //{{{  5.1 chan display
        auto r = mRect;
        auto widthPerChannel = mRect.getWidth() / 5.f;

        // BL
        r.left = mRect.left + 1.f;
        r.right = r.left + widthPerChannel;
        drawChanBar (dc, r, mAudio->getMixedBL(), mAudFrame->mPower[4]);

        // FL
        r.left = r.right;
        r.right += widthPerChannel;
        drawChanBar (dc, r, mAudio->getMixedFL(), mAudFrame->mPower[0]);

        // C
        r.left = r.right;
        r.right += widthPerChannel;
        drawChanBar (dc, r, mAudio->getMixedC(), mAudFrame->mPower[2]);

        // FR
        r.left = r.right;
        r.right += widthPerChannel;
        drawChanBar (dc, r, mAudio->getMixedFR(), mAudFrame->mPower[1]);

        // BR
        r.left = r.right;
        r.right = mRect.right - 1.0f;
        drawChanBar (dc, r, mAudio->getMixedBL(), mAudFrame->mPower[5]);

        // W overlay
        r.left = mRect.left + 1.f;
        r.right = mRect.right - 1.f;
        drawWooferBar (dc, r, mAudio->getMixedW(), mAudFrame->mPower[3]);
        }
        //}}}
      }

    if (mPick) {
      auto r = mRect;
      r.top = r.bottom - (getHeight() * mAudio->getVolume() / mAudio->getMaxVolume());
      dc->DrawRectangle (r, mWindow->getYellowBrush());
      }
    }
  //}}}

private:
  //{{{
  bool setFromPos (cPoint pos) {
    mAudio->setVolume ((1.f - (pos.y / getHeight())) * mAudio->getMaxVolume());
    return true;
    }
  //}}}
  //{{{
  void drawChanBar (ID2D1DeviceContext* dc, cRect& r, bool selected, float value) {

    r.top = r.bottom - value / kBarsScale;
    selected ? dc->FillRectangle (r, mWindow->getGreenBrush()) : dc->DrawRectangle (r, mWindow->getGreenBrush());
    }
  //}}}
  //{{{
  void drawWooferBar (ID2D1DeviceContext* dc, cRect& r, bool selected, float value) {

    r.top = r.bottom - value / kBarsScale;
    dc->FillRectangle (r, selected ? mBrush : mWindow->getGreyBrush());
    }
  //}}}

  cAudFrame*& mAudFrame;
  iAudio* mAudio;

  ID2D1SolidColorBrush* mBrush = nullptr;
  };
