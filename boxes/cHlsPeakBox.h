// hlsPeakBox.h
//{{{  includes
#pragma once
#include "../common/cD2dWindow.h"
#include "../../shared/utils/date.h"
#include "../../shared/hls/hls.h"
//}}}

const int kScrubFrames = 3;

class cHlsPeakBox : public cD2dWindow::cBox {
public:
  //{{{
  cHlsPeakBox (cD2dWindow* window, float width, float height, cHls* hls) :
      cBox("hlsPeak", window, width, height), mHls(hls) {

    mWindow->getDwriteFactory()->CreateTextFormat (L"Consolas", NULL,
      DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.f, L"en-us",
      &mTextFormat);
    mTextFormat->SetTextAlignment (DWRITE_TEXT_ALIGNMENT_TRAILING);

    mWindow->getDc()->CreateSolidColorBrush ({0.f, 0.f, 0.5f, 1.f}, &mDarkBlueBrush);
    mWindow->getDc()->CreateSolidColorBrush ({0.f, 0.5f, 0.f, 1.f}, &mDarkGreenBrush);

    D2D1_GRADIENT_STOP gradientStops[2];
    gradientStops[0].color = D2D1::ColorF (D2D1::ColorF::Black, 1);
    gradientStops[0].position = 0;
    gradientStops[1].color = D2D1::ColorF (D2D1::ColorF::Gray, 1);
    gradientStops[1].position = 1.f;
    mWindow->getDc()->CreateGradientStopCollection (
      gradientStops, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &mGradientStopCollectionSeconds);

    mWindow->getDc()->CreateLinearGradientBrush (
      D2D1::LinearGradientBrushProperties(D2D1::Point2F(0, 0), D2D1::Point2F(100.f, 10.f)),
      mGradientStopCollectionSeconds, &mLinearGradientBrush);

    gradientStops[0].color = D2D1::ColorF (D2D1::ColorF::Black, 1);
    gradientStops[0].position = 0;
    gradientStops[1].color = D2D1::ColorF (D2D1::ColorF::Yellow, 1);
    gradientStops[1].position = 1.f;

    mWindow->getDc()->CreateGradientStopCollection (
      gradientStops, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &mGradientStopCollection);

    mWindow->getDc()->CreateRadialGradientBrush (
      D2D1::RadialGradientBrushProperties(D2D1::Point2F(100, 100), D2D1::Point2F(0, 0), 100, 100),
      mGradientStopCollection, &mRadialGradientBrush);
    }
  //}}}
  //{{{
  virtual ~cHlsPeakBox() {
    mTextFormat->Release();

    mDarkBlueBrush->Release();
    mDarkGreenBrush->Release();

    mGradientStopCollection->Release();
    mRadialGradientBrush->Release();

    mGradientStopCollectionSeconds->Release();
    mLinearGradientBrush->Release();
    }
  //}}}

  // overrides
  //{{{
  bool onDown (bool right, cPoint pos)  {

    mMove = 0;

    mPressInc = pos.x - (getWidth()/2);
    mHls->setScrub();
    setZoomAnim (1.f + ((getWidth()/2) - abs(pos.x - (getWidth()/2))) / (getWidth()/6), 4);

    return true;
    }
  //}}}
  //{{{
  bool onMove (bool right, cPoint pos, cPoint inc) {

    mMove += abs(inc.x) + abs(inc.y);
    mHls->incPlaySample ((-inc.x * kSamplesPerFrame) / mZoom);

    return true;
    }
  //}}}
  //{{{
  bool onUp (bool right, bool mouseMoved, cPoint pos) {

    if (mMove < kLineHeight /2) {
      mAnimation = mPressInc;
      mHls->incPlaySample (mPressInc * kSamplesPerFrame / kNormalZoom);
      mHls->setPlay();
      }

    mHls->setPause();
    setZoomAnim (kNormalZoom, 3);

    return true;
    }
  //}}}
  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    //{{{  animate
    mAnimation /= 2.f;

    if (mZoomInc > 0) {
      if (mZoom + mZoomInc > mTargetZoom)
        mZoom = mTargetZoom;
      else
        mZoom += mZoomInc;
      }
    else  {
      if (mZoom + mZoomInc < mTargetZoom)
        mZoom = mTargetZoom;
      else
        mZoom += mZoomInc;
      }
    //}}}
    float samplesPerPix = kSamplesPerFrame / mZoom;
    float pixPerSecond = kSamplesPerSecond / samplesPerPix;

    double leftSample = mHls->getPlaySample() - (getCentreX() + mAnimation)*samplesPerPix;
    //{{{  draw clock
    //auto timePointTz = date::make_zoned (date::current_zone(), mTimePoint);
    auto timePoint = mHls->mPlayTimePoint + std::chrono::seconds (mWindow->getDayLightSeconds());
    auto datePoint = floor<date::days>(timePoint);
    auto timeOfDay = date::make_time (std::chrono::duration_cast<std::chrono::milliseconds>(timePoint - datePoint));

    float radius = getHeight() / 8.f;
    auto centre = cPoint (getCentreX(), mRect.bottom - radius - 12.f);

    //{{{  draw hour
    auto hourRadius = radius * 0.6f;
    auto h = timeOfDay.hours().count() + (timeOfDay.minutes().count() / 60.f);
    auto hourAngle = (1.f - (h / 6.f)) * kPi;
    dc->DrawLine (centre, centre + cPoint(hourRadius * sin (hourAngle), hourRadius * cos (hourAngle)),
                  mWindow->getWhiteBrush(), 2.f);
    //}}}
    //{{{  draw minute
    auto minRadius = radius * 0.75f;
    auto m = timeOfDay.minutes().count() + (timeOfDay.seconds().count() / 60.f);
    auto minAngle = (1.f - (m/30.f)) * kPi;
    dc->DrawLine (centre, centre + cPoint (minRadius * sin (minAngle), minRadius * cos (minAngle)),
                  mWindow->getWhiteBrush(), 2.f);
    //}}}
    //{{{  draw seconds
    auto secRadius = radius * 0.85f;
    auto s = timeOfDay.seconds().count();
    auto secAngle = (1.f - (s /30.f)) * kPi;
    dc->DrawLine (centre, centre + cPoint (secRadius * sin (secAngle), secRadius * cos (secAngle)),
                  mWindow->getRedBrush(), 2.f);
    //}}}
    //{{{  draw subSec
    //auto subSecRadius = radius * 0.8f;
    //auto subSec = timeOfDay.subseconds().count();
    //auto subSecAngle = (1.f - (subSec / 500.f)) * kPi;
    //dc->DrawLine (centre, centre + cPoint (subSecRadius * sin (subSecAngle), secRadius * cos (subSecAngle)),
                  //mWindow->getDarkGreyBrush(), 2.f);
    //}}}

    auto timeOfDay1 = date::make_time (std::chrono::duration_cast<std::chrono::milliseconds>(mWindow->getNow() - datePoint));
    auto timeDiff = std::chrono::duration_cast<std::chrono::seconds>(mWindow->getNow() - timePoint).count();
    if (timeDiff < 60) {
      //{{{  draw clock seconds
      auto secRadius = radius * 0.85f;
      auto s = timeOfDay1.seconds().count();
      auto secAngle = (1.f - (s /30.f)) * kPi;
      dc->DrawLine (centre, centre + cPoint (secRadius * sin (secAngle), secRadius * cos (secAngle)),
                    mWindow->getOrangeBrush(), 2.f);
      }
      //}}}
    else if (timeDiff < 3600) {
      //{{{  draw clock minutes
      auto minRadius = radius * 0.75f;
      auto m = timeOfDay1.minutes().count() + (timeOfDay1.seconds().count() / 60.f);
      auto minAngle = (1.f - (m/30.f)) * kPi;
      dc->DrawLine (centre, centre + cPoint (minRadius * sin (minAngle), minRadius * cos (minAngle)),
                    mWindow->getOrangeBrush(), 2.f);
      }
      //}}}

    //mRadialGradientBrush->SetCenter (centre);
    //mRadialGradientBrush->SetGradientOriginOffset (cPoint(0,0));
    //mRadialGradientBrush->SetRadiusX (radius);
    //mRadialGradientBrush->SetRadiusY (radius);
    //dc->FillEllipse (Ellipse (centre, radius,radius), mRadialGradientBrush);
    dc->DrawEllipse (Ellipse (centre, radius,radius), mWindow->getGreyBrush(), 2.f);
    //}}}
    //{{{  draw samples
    centre = getCentre();
    auto midWidth = centre.x + int(mHls->getPlaying() == cHls::eScrub ? kScrubFrames*mZoom : mZoom);
    auto scale = getHeight() / 0x100;

    ID2D1SolidColorBrush* brush = mDarkBlueBrush;
    uint8_t* samples = nullptr;
    uint32_t numSamples = 0;
    auto sample = leftSample;
    for (auto x = mRect.left; x < mRect.right; x++) {
      if (!numSamples) {
        samples = mHls->getPeakSamples (sample, numSamples, mZoom);
        if (samples)
          sample += numSamples * samplesPerPix;
        }
      if (samples) {
        if (x >= midWidth)
          brush = mWindow->getDarkGreyBrush();
        else if (x >= centre.x)
          brush = mDarkGreenBrush;
        auto left = *samples++ * scale;
        dc->FillRectangle (cRect(x-1.f, centre.y-left, x+1.f, centre.y + (*samples++ * scale)), brush);
        numSamples--;
        }
      else
        sample += samplesPerPix;
      }
    //}}}
    //{{{  draw secondsStrip
    auto seconds = int(leftSample / kSamplesPerSecondD);
    auto subSeconds = (float)fmod (leftSample, kSamplesPerSecondD);

    auto r = mRect;
    r.top = r.bottom - 11.f;
    r.right = r.left + ((kSamplesPerSecondF - subSeconds) / samplesPerPix) + pixPerSecond;

    const float kTextWidth = 70.f;
    for (r.left = mRect.left; r.left  < mRect.right + kTextWidth;) {
      mLinearGradientBrush->SetStartPoint (cPoint(r.left,0.f));
      mLinearGradientBrush->SetEndPoint (cPoint(r.right,0.f));
      dc->FillRectangle (r, mLinearGradientBrush);

      if (seconds % 60 == 0)
        dc->FillRectangle (cRect (r.left-2.f, r.top, r.left, r.bottom), mWindow->getWhiteBrush());
      else if (seconds % 5 == 0)
        dc->FillRectangle (cRect (r.left-1.f, r.top, r.left, r.bottom), mWindow->getWhiteBrush());

      if (seconds % 5 == 0) {
        auto str = getTimeString (seconds);
        dc->DrawText (std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(), mTextFormat,
                      cRect (r.left-kTextWidth, r.top-3.f, r.left-3.f, r.bottom),
                      mWindow->getWhiteBrush());
        }

      r.left = r.right;
      r.right += pixPerSecond;
      seconds++;
      }
    //}}}
    }
  //}}}

private:
  //{{{
  void setZoomAnim (float zoom, int16_t frames) {

    mTargetZoom = zoom;
    mZoomInc = (mTargetZoom - mZoom) / frames;
    }
  //}}}

  // vars
  const float kPi = 3.141592f;

  cHls* mHls;

  float mMove = 0.f;
  bool mMoved = false;
  float mPressInc = 0.f;
  float mAnimation = 0.f;

  float mZoom = kNormalZoom;
  float mZoomInc = 0.f;
  float mTargetZoom = kNormalZoom;

  IDWriteTextFormat* mTextFormat = nullptr;
  ID2D1SolidColorBrush* mDarkBlueBrush = nullptr;
  ID2D1SolidColorBrush* mDarkGreenBrush = nullptr;

  ID2D1GradientStopCollection* mGradientStopCollection = nullptr;
  ID2D1RadialGradientBrush* mRadialGradientBrush = nullptr;

  ID2D1GradientStopCollection* mGradientStopCollectionSeconds = nullptr;
  ID2D1LinearGradientBrush* mLinearGradientBrush = nullptr;
  };
