// hlsWindow.cpp
//{{{  includes
#include "stdafx.h"

#include <winsock2.h>
#include <WS2tcpip.h>
#pragma comment(lib,"ws2_32.lib")

#include "../../shared/net/cWinSockHttp.h"
#include "../../shared/utils/cWinAudio.h"
#include "../../shared/hls/hls.h"

#include "../../shared/hls/r1x80.h"
#include "../../shared/hls/r2x80.h"
#include "../../shared/hls/r3x80.h"
#include "../../shared/hls/r4x80.h"
#include "../../shared/hls/r5x80.h"
#include "../../shared/hls/r6x80.h"

#include "../common/cValueBox.h"
#include "../common/cFloatBox.h"
#include "../common/cLogBox.h"
#include "../common/cWindowBox.h"
#include "../common/cVolumeBox.h"
#include "../common/cBmpBox.h"
#include "../common/cClockBox.h"
#include "../common/cDateBox.h"
#include "../common/cCalendarBox.h"

using namespace chrono;
//}}}
const int kScrubFrames = 3;

class cAppWindow : public cD2dWindow, public cWinAudio {
public:
  cAppWindow() : mLoadSem("load") {}
  //{{{
  void run (const string& title, int width, int height) {

    int chan = kDefaultChan;
    cLog::log (LOGINFO, "hlsWindow %d %d", chan, 384000);

    initialise (title, width, height, false);
    mHls = new cHls (chan, kDefaultBitrate, getDaylightSeconds());
    addBox (new cCalendarBox (this, 190.f,160.f, mTimePoint), -190.f - 24.f,0);
    addBox (new cHlsDotsBox (this, 18.f,60.f, mHls), -24.f, 0);
    addBox (new cHlsPeakBox (this, 0,0, mHls));
    addBox (new cLogBox (this, 200.f,0, true), 0,200.f);
    addBox (new cBmpBox (this, 60.f, 60.f, r1x80, 1, mHls->mChan, mHls->mChanChanged));
    addBox (new cBmpBox (this, 60.f, 60.f, r2x80, 2, mHls->mChan, mHls->mChanChanged), 61.f,0);
    addBox (new cBmpBox (this, 60.f, 60.f, r3x80, 3, mHls->mChan, mHls->mChanChanged), 122.f,0);
    addBox (new cBmpBox (this, 60.f, 60.f, r4x80, 4, mHls->mChan, mHls->mChanChanged), 183.f,0);
    addBox (new cBmpBox (this, 60.f, 60.f, r5x80, 5, mHls->mChan, mHls->mChanChanged), 244.f,0);
    addBox (new cBmpBox (this, 60.f, 60.f, r6x80, 6, mHls->mChan, mHls->mChanChanged), 305.f,0);
    //addBox (new cClockBox (this, 40.f, mTimePoint), -82.f,-82.f);
    addBox (new cVolumeBox (this, 12.f,0), -12.f,0);
    addBox (new cFloatBox (this, 50.f,20.f, mRenderTime), 0,-20.f);
    addBox (new cWindowBox (this, 60.f,24.f), -60.f,0);

    // launch loaderThread
    thread ([=]() { hlsLoaderThread(); } ).detach();

    // launch playerThread, high priority
    auto playerThread = thread ([=]() { hlsPlayerThread(); });
    SetThreadPriority (playerThread.native_handle(), THREAD_PRIORITY_HIGHEST);
    playerThread.detach();

    // loop till quit
    messagePump();
    }
  //}}}

protected:
  //{{{
  bool onKey (int key) {

    switch (key) {
      case 0x00: break;
      case 0x1B: return true;

      case  ' ': if (mHls) mHls->togglePlay(); break;

      case 0x21:
        //{{{  page up
        if (mHls) {
          mHls->incPlaySeconds (-60);
          mLoadSem.notify();
          changed();
          }
        break;
        //}}}
      case 0x22:
        //{{{  page down
        if (mHls) {
          mHls->incPlaySeconds (60);
          mLoadSem.notify();
          changed();
          }
        break;
        //}}}
      case 0x25:
       //{{{  left arrow
       if (mHls) {
         mHls->incPlaySeconds (-1);
         mLoadSem.notify();
         changed();
         }
       break;
       //}}}
      case 0x27:
        //{{{  right arrow
        if (mHls) {
          mHls->incPlaySeconds (1);
          mLoadSem.notify();
          changed();
          }
        break;
        //}}}
      case 0x26:
        //{{{  up arrow
        if (mHls && mHls->mChan > 1) {
          mHls->mChan--;
          mHls->mChanChanged = true;
          }
        break;
        //}}}
      case 0x28:
        //{{{  down arrow
        if (mHls && mHls->mChan < 6) {
          mHls->mChan++;
          mHls->mChanChanged = true;
          }
        break;
        //}}}
      case 0x23: break; // home
      case 0x24: break; // end

      case  '1':
      case  '2':
      case  '3':
      case  '4':
      case  '5':
      case  '6': mHls->mChan = key - '0'; mHls->mChanChanged = true; break;

      case  'F': toggleFullScreen(); break;

      default  : printf ("key %x\n", key);
      }

    return false;
    }
  //}}}

private:
  //{{{
  class cHlsPeakBox : public cBox {
  public:
    //{{{
    cHlsPeakBox (cD2dWindow* window, float width, float height, cHls* hls) :
        cBox("hlsPeak", window, width, height), mHls(hls) {
      mPin = true;

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

      if (mMove < kTextHeight/2) {
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
      auto timePoint = mHls->mPlayTimePoint + seconds (mWindow->getDaylightSeconds());
      auto datePoint = floor<date::days>(timePoint);
      auto timeOfDay = date::make_time (duration_cast<milliseconds>(timePoint - datePoint));

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

      auto timeOfDay1 = date::make_time (duration_cast<milliseconds>(mWindow->mTimePoint - datePoint));
      auto timeDiff = duration_cast<seconds>(mWindow->mTimePoint - timePoint).count();
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
          auto str = getTimeString(seconds);
          dc->DrawText (wstring (str.begin(), str.end()).data(), (uint32_t)str.size(), mTextFormat,
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
  //}}}
  //{{{
  class cHlsDotsBox : public cBox {
  public:
    //{{{
    cHlsDotsBox (cD2dWindow* window, float width, float height, cHls* hls) :
        cBox("hlsDots", window, width, height), mHls(hls) {

      mPin = true;

      mWindow->getDwriteFactory()->CreateTextFormat (L"FreeSans", NULL,
        DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.f, L"en-us",
        &mTextFormat);
      mTextFormat->SetTextAlignment (DWRITE_TEXT_ALIGNMENT_CENTER);
      }
    //}}}
    //{{{
    virtual ~cHlsDotsBox() {
      mTextFormat->Release();
      }
    //}}}

    // overrides
    //{{{
    void onDraw (ID2D1DeviceContext* dc) {

      for (auto chunk = 0; chunk < 3; chunk++) {
        bool loaded;
        bool loading;
        int offset;
        mHls->getChunkLoad (chunk, loaded, loading, offset);
        auto brush = loading ? mWindow->getOrangeBrush() :
                       loaded ? mWindow->getGreenBrush() : mWindow->getRedBrush();

        auto centre = cPoint(getCentre().x, mRect.top + (chunk + 0.5f) * (getHeight() / 3.f));
        auto radius = getWidth() / 2.f;
        dc->FillEllipse (Ellipse (centre, radius, radius), brush);

        if (loaded || loading) {
          auto str = dec(offset);
          dc->DrawText (wstring (str.begin(), str.end()).data(), (uint32_t)str.size(), mTextFormat,
                        cRect (mRect.left, centre.y-8.f, mRect.right, centre.y +4.f),
                        mWindow->getLightGreyBrush());
          }
        }
      }
    //}}}

  private:
    // vars
    cHls* mHls;
    IDWriteTextFormat* mTextFormat = nullptr;
    };
  //}}}

  //{{{
  void hlsLoaderThread() {
  // make sure chuinks around playframe loaded, only thread using http

    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::setThreadName ("load");

    cWinSockHttp http;
    http.initialise();

    mHls->mChanChanged = true;
    while (true) {
      if (mHls->mChanChanged)
        mHls->setChan (http, mHls->mChan);
      if (!mHls->loadAtPlayFrame (http))
        Sleep (1000);

      // wait for change to run again
      mLoadSem.wait();
      }

    cLog::log (LOGNOTICE, "exit");
    CoUninitialize();
    }
  //}}}
  //{{{
  void hlsPlayerThread() {

    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::setThreadName ("play");

    audOpen (2, 48000);

    uint16_t scrubCount = 0;
    double scrubSample = 0;

    uint32_t seqNum = 0;
    uint32_t lastSeqNum = 0;

    while (true) {
      switch (mHls->getPlaying()) {
        case cHls::ePause:
          audPlay (2, nullptr, 1024, 1.f);
          break;

        case cHls::eScrub: {
          if (scrubCount == 0)
            scrubSample = mHls->getPlaySample();
          if (scrubCount < 3) {
            uint32_t numSamples = 0;
            auto sample = mHls->getPlaySamples (scrubSample + scrubCount*kSamplesPerFrame, seqNum, numSamples);
            audPlay (2, sample, 1024, 1.f);
            }
          else
            audPlay (2, nullptr, 1024, 1.f);
          if (scrubCount++ > 3)
            scrubCount = 0;
          }
          break;

        case cHls::ePlay: {
          uint32_t numSamples = 0;
          auto sample = mHls->getPlaySamples (mHls->getPlaySample(), seqNum, numSamples);
          audPlay (2, sample, 1024, 1.f);
          if (sample)
            mHls->incPlayFrame();
          changed();
          }
          break;
        }

      if (mHls->mChanChanged || !seqNum || (seqNum != lastSeqNum)) {
        lastSeqNum = seqNum;
        mLoadSem.notify();
        }
      }

    // cleanup
    audClose();

    cLog::log (LOGNOTICE, "exit");
    CoUninitialize();
    }
  //}}}

  // vars
  cHls* mHls = nullptr;
  cSemaphore mLoadSem;
  };

//{{{
int __stdcall WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::init (LOGINFO1, true);

  int numArgs;
  auto args = CommandLineToArgvW (GetCommandLineW(), &numArgs);
  if (numArgs > 1) {
    // get fileName from commandLine
    wstring wstr (args[1]);
    auto fileName = string (wstr.begin(), wstr.end());
    }

  WSADATA wsaData;
  if (WSAStartup (MAKEWORD(2,2), &wsaData)) {
    //{{{  error exit
    cLog::log (LOGERROR, "WSAStartup failed");
    exit (0);
    }
    //}}}

  cAppWindow appWindow;
  appWindow.run ("hlsWindow", 600, 340);

  CoUninitialize();
  }
//}}}
