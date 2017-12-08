// console.cpp
//{{{  includes
#include "stdafx.h"

#include <iomanip>
#include <iostream>
#include <locale>
#include <ostream>
#include <stdexcept>

#include <winsock2.h>
#include <WS2tcpip.h>
#pragma comment(lib,"ws2_32.lib")

#include "../date/tz.h"

#include "../../shared/net/cWinSockHttp.h"
#include "../../shared/utils/cWinAudio.h"
#include "../../shared/hls/hls.h"

#include "../common/cIntBox.h"
//}}}
//{{{  const
const int kScrubFrames = 3;
//}}}

//{{{
//const int GEOMETRY_COUNT = 2;
//ID2D1Factory*           g_pD2DFactory   = NULL; // Direct2D factory
//ID2D1HwndRenderTarget*  g_pRenderTarget = NULL; // Render target
//ID2D1SolidColorBrush*   g_pBlackBrush   = NULL; // Outline brush
//ID2D1RadialGradientBrush* g_pRadialGradientBrush = NULL ; // Radial gradient brush

//// 2 circle to build up a geometry group. this is the outline of the progress bar
//D2D1_ELLIPSE g_Ellipse0 = D2D1::Ellipse(D2D1::Point2F(300, 300), 150, 150);
//D2D1_ELLIPSE g_Ellipse1 = D2D1::Ellipse(D2D1::Point2F(300, 300), 200, 200);

//D2D1_ELLIPSE g_Ellipse[GEOMETRY_COUNT] = {
  //g_Ellipse0,
  //g_Ellipse1,
  //};

//ID2D1EllipseGeometry* g_pEllipseArray[GEOMETRY_COUNT] = { NULL };
//ID2D1GeometryGroup* g_pGeometryGroup = NULL;

//VOID CreateD2DResource(HWND hWnd) {

    //if (!g_pRenderTarget)
    //{
        //HRESULT hr ;

        //hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_pD2DFactory) ;
        //if (FAILED(hr))
        //{
            //MessageBox(hWnd, "Create D2D factory failed!", "Error", 0) ;
            //return ;
        //}

        //// Obtain the size of the drawing area
        //RECT rc ;
        //GetClientRect(hWnd, &rc) ;

        //// Create a Direct2D render target
        //hr = g_pD2DFactory->CreateHwndRenderTarget(
            //D2D1::RenderTargetProperties(),
            //D2D1::HwndRenderTargetProperties(
            //hWnd,
            //D2D1::SizeU(rc.right - rc.left,rc.bottom - rc.top)
            //),
            //&g_pRenderTarget
            //) ;
        //if (FAILED(hr))
        //{
            //MessageBox(hWnd, "Create render target failed!", "Error", 0) ;
            //return ;
        //}

        //// Create the outline brush(black)
        //hr = g_pRenderTarget->CreateSolidColorBrush(
            //D2D1::ColorF(D2D1::ColorF::Black),
            //&g_pBlackBrush
            //) ;
        //if (FAILED(hr))
        //{
            //MessageBox(hWnd, "Create outline brush(black) failed!", "Error", 0) ;
            //return ;
        //}

        //// Define gradient stops
        //D2D1_GRADIENT_STOP gradientStops[2] ;
        //gradientStops[0].color = D2D1::ColorF(D2D1::ColorF::Blue) ;
        //gradientStops[0].position = 0.f ;
        //gradientStops[1].color = D2D1::ColorF(D2D1::ColorF::White) ;
        //gradientStops[1].position = 1.f ;

        //// Create gradient stops collection
        //ID2D1GradientStopCollection* pGradientStops = NULL ;
        //hr = g_pRenderTarget->CreateGradientStopCollection(
            //gradientStops,
            //2,
            //D2D1_GAMMA_2_2,
            //D2D1_EXTEND_MODE_CLAMP,
            //&pGradientStops
            //) ;
        //if (FAILED(hr))
        //{
            //MessageBox(NULL, "Create gradient stops collection failed!", "Error", 0);
        //}

        //// Create a linear gradient brush to fill in the ellipse
        //hr = g_pRenderTarget->CreateRadialGradientBrush(
            //D2D1::RadialGradientBrushProperties(
            //D2D1::Point2F(170, 170),
            //D2D1::Point2F(0, 0),
            //150,
            //150),
            //pGradientStops,
            //&g_pRadialGradientBrush
            //) ;

        //if (FAILED(hr))
        //{
            //MessageBox(hWnd, "Create linear gradient brush failed!", "Error", 0) ;
            //return ;
        //}

        //// Create the 2 ellipse.
        //for (int i = 0; i < GEOMETRY_COUNT; ++i)
        //{
            //hr = g_pD2DFactory->CreateEllipseGeometry(g_Ellipse[i], &g_pEllipseArray[i]);
            //if (FAILED(hr))
            //{
                //MessageBox(hWnd, "Create Ellipse Geometry failed!", "Error", 0);
                //return;
            //}
        //}

        //// Create the geometry group, the 2 circles make up a group.
        //hr = g_pD2DFactory->CreateGeometryGroup(
            //D2D1_FILL_MODE_ALTERNATE,
            //(ID2D1Geometry**)&g_pEllipseArray,
            //ARRAYSIZE(g_pEllipseArray),
            //&g_pGeometryGroup
        //);
    //}
//}

//VOID Render(HWND hwnd)
//{
    //// total angle to rotate
    //static float totalAngle = 0.0f;

    //// Get last time
    //static DWORD lastTime = timeGetTime();

    //// Get current time
    //DWORD currentTime = timeGetTime();

    //// Calculate time elapsed in current frame.
    //float timeDelta = (float)(currentTime - lastTime) * 0.1;

    //// Increase the totalAngle by the time elapsed in current frame.
    //totalAngle += timeDelta;

    //CreateD2DResource(hwnd) ;

    //g_pRenderTarget->BeginDraw() ;

    //// Clear background color to White
    //g_pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

    //// Draw geometry group
    //g_pRenderTarget->DrawGeometry(g_pGeometryGroup, g_pBlackBrush);

    //// Roatate the gradient brush based on the total elapsed time
    //D2D1_MATRIX_3X2_F rotMatrix = D2D1::Matrix3x2F::Rotation(totalAngle, D2D1::Point2F(300, 300));
    //g_pRadialGradientBrush->SetTransform(rotMatrix);

    //// Fill geometry group with the transformed brush
    //g_pRenderTarget->FillGeometry(g_pGeometryGroup, g_pRadialGradientBrush);

    //HRESULT hr = g_pRenderTarget->EndDraw() ;
    //if (FAILED(hr))
    //{
        //MessageBox(NULL, "Draw failed!", "Error", 0) ;
        //return ;
    //}

    //// Update last time to current time for next loop
    //lastTime = currentTime;
//}

    //SAFE_RELEASE(g_pRenderTarget) ;
    //SAFE_RELEASE(g_pBlackBrush) ;
    //SAFE_RELEASE(g_pGeometryGroup);
    //SAFE_RELEASE(g_pRadialGradientBrush);

    //for (int i = 0; i < GEOMETRY_COUNT; ++i)
    //{
        //SAFE_RELEASE(g_pEllipseArray[i]);
        //g_pEllipseArray[i] = NULL;
    //}

    //SAFE_RELEASE(g_pD2DFactory) ;
//}}}

//{{{
void reportMonth (date::year_month const yearMonth) {

  auto weekDay1 = date::weekday{ yearMonth / 1};
  auto lastDay1 = (yearMonth / date::last).day();
  cout << "first day of week " << format (" %a ", weekDay1)
       << " lastDay " << format (" %e ", lastDay1) << endl;

  // print year month line
  cout << format (cout.getloc(), " %B %Y", yearMonth) << endl;

  // print days of week line
  auto weekDay = date::sun;
  do {
    auto dayStr = format (cout.getloc(), "%a", weekDay);
    dayStr.resize (3);
    cout << ' ' << dayStr;
    } while (++weekDay != date::sun);
  cout << endl;

  // print first line
  weekDay = date::weekday{ yearMonth / 1};
  cout << string (static_cast<unsigned>((weekDay - date::sun).count())*4, ' ');

  using date::operator""_d;
  auto curDay = 1_d;
  do {
    cout << format (" %e ", curDay);
    ++curDay;
    } while (++weekDay != date::sun);
  cout << endl;

  // print other lines
  for (auto line = 1u; line < 6; ++line) {
    unsigned index = line;
    auto sysDay = date::sys_days{yearMonth/1};
    if (date::weekday{sysDay} == date::sun)
      ++index;

    auto yearMonthDayWeek = yearMonth / date::sun[index];
    if (yearMonthDayWeek.ok()) {
      auto curDay = date::year_month_day{ yearMonthDayWeek }.day();
      auto const lastDay = (yearMonth / date::last).day();
      auto weekDay = date::sun;
      do {
        cout << format (" %e ", curDay);
        } while ((++weekDay != date::sun) && (++curDay <= lastDay));
      }
    cout << endl;
    }
  }
//}}}
//{{{
void reportYearCalendar() {

  date::year_month_day yearMonthDay = floor<date::days>(chrono::system_clock::now());

  for (auto month = 1u; month <= 12; ++month)
    reportMonth (yearMonthDay.year() / date::month{month});
  }
//}}}
//{{{
void reportDateTimes() {

  auto timepoint = chrono::system_clock::now();

  // covert to chrono::timepoint to C style time_t,tm
  auto time = chrono::system_clock::to_time_t (timepoint);
  auto localTm = *localtime (&time);
  cLog::log(LOGNOTICE, "localTm %02d-%02d-%02d %02d:%02d:%02d - weekDay:%d - yearDay:%d - bst:%d",
                       localTm.tm_year + 1900, localTm.tm_mon + 1, localTm.tm_mday,
                       localTm.tm_hour, localTm.tm_min, localTm.tm_sec,
                       localTm.tm_wday, localTm.tm_yday + 1, localTm.tm_isdst);

  // timepoint to stream
  {
  using namespace date;
  stringstream timepointStr;
  timepointStr << "timepoint " << timepoint << " str";
  cLog::log (LOGNOTICE, timepointStr.str());
  }

  // timepoint to unzoned date
  auto datepoint = date::floor<date::days>(timepoint);
  auto yearMonthDay = date::year_month_day {datepoint};
  auto timeOfDay = date::make_time (chrono::duration_cast<chrono::milliseconds>(timepoint - datepoint));
  cLog::log(LOGNOTICE, "timepoint %02d-%02d-%02d %02d:%02d:%02d.%03d",
                       yearMonthDay.year(), yearMonthDay.month(), yearMonthDay.day(),
                       timeOfDay.hours().count(), timeOfDay.minutes().count(),
                       timeOfDay.seconds().count(), timeOfDay.subseconds().count());

  // zoned
  stringstream version;
  version << date::get_tzdb().version;
  cLog::log (LOGNOTICE, "tz Version " + version.str());

  // timepoint to zoned
  auto timepointTz = date::make_zoned (date::current_zone(), timepoint);
  stringstream timepointTzStr;
  timepointTzStr << "timepointTz " << timepointTz << " str";
  cLog::log (LOGNOTICE, timepointTzStr.str());
  };
//}}}
//{{{
void reportTimezone() {
// UTC = local time + bias

  TIME_ZONE_INFORMATION timeZoneInfo;
  auto dwRet = GetTimeZoneInformation (&timeZoneInfo);

  if (dwRet == TIME_ZONE_ID_STANDARD || dwRet == TIME_ZONE_ID_UNKNOWN)
    wprintf (L"%s\n", timeZoneInfo.StandardName);

  else if (dwRet == TIME_ZONE_ID_DAYLIGHT ) {
    wprintf (L"%s\n", timeZoneInfo.DaylightName);
    printf ("bais %d standardBias %d dayLightBias %d \n",
             timeZoneInfo.Bias, timeZoneInfo.StandardBias, timeZoneInfo.DaylightBias);
    }

  else {
    printf ("GetTimeZoneInformation error%d\n", GetLastError());
    return;
    }
  }
//}}}

class cAppWindow : public cD2dWindow, public cWinAudio {
public:
  //{{{
  void run (string title, int width, int height, int chan) {

    CoInitialize (NULL); // for winAudio
    audOpen (48000, 16, 2);

    initialise (title, width, height, false);
    addBox (new cGeometryBox (this, 500.f,300.f));

    //for (auto y = 1; y < 5; y++)
    //  for (auto x = 1; x < 5; x++)
    //    addBox (new cRadialGradBox (this, x * 20.f, y * 20.f), x*x * 20.f, y*y * 20.f);
    //for (auto y = 1; y < 5; y++)
    //  for (auto x = 1; x < 5; x++)
    //    addBox (new cLinearGradBox (this, x * 20.0f, y * 20.f), 500.f + x*x * 20.f, y*y * 20.f);
    //addBox (new cInfoBox (this, 50.f,20.f, false, mRenderTime), 0,-20.f);

    //cLog::log (LOGINFO, "hlsWindow %d", chan);
    //mHls = new cHls (chan, true);
    // launch loaderThread
    //thread ([=]() { hlsLoaderThread(); } ).detach();
    // launch playerThread, high priority
    //auto playerThread = thread ([=]() { hlsPlayerThread(); });
    //SetThreadPriority (playerThread.native_handle(), THREAD_PRIORITY_HIGHEST);
    //playerThread.detach();

    messagePump();

    // cleanup
    audClose();
    CoUninitialize();
    }
  //}}}

protected:
  //{{{
  bool onKey (int key) {

    switch (key) {
      case 0x00: break;
      case 0x1B: return true;

      case  ' ': if (mHls) mHls->togglePlay(); break;

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
  class cLinearGradBox : public cBox {
  public:
    //{{{
    cLinearGradBox (cAppWindow* window, float width, float height)
        : cBox("progress", window, width, height) {

      mPin = true;

      D2D1_GRADIENT_STOP gradientStops[2];
      gradientStops[0].color = D2D1::ColorF (D2D1::ColorF::Red, 1);
      gradientStops[0].position = 0.f;
      gradientStops[1].color = D2D1::ColorF (D2D1::ColorF::Blue, 1);
      gradientStops[1].position = 1.f;

      mWindow->getDc()->CreateGradientStopCollection (
        gradientStops, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &mGradientStopCollection);

      mWindow->getDc()->CreateLinearGradientBrush (
        D2D1::LinearGradientBrushProperties(D2D1::Point2F(0, 0), D2D1::Point2F(100.f, 10.f)),
        mGradientStopCollection, &mLinearGradientBrush);
      }
    //}}}
    //{{{
    virtual ~cLinearGradBox() {
      mGradientStopCollection->Release();
      mLinearGradientBrush->Release();
      }
    //}}}

    void onDraw (ID2D1DeviceContext* dc) {

      mLinearGradientBrush->SetStartPoint (mRect.getTL());
      mLinearGradientBrush->SetEndPoint (mRect.getBR());
      dc->FillRectangle (mRect, mLinearGradientBrush);
      }

  private:
    ID2D1GradientStopCollection* mGradientStopCollection = nullptr;
    ID2D1LinearGradientBrush* mLinearGradientBrush = nullptr;
    };
  //}}}
  //{{{
  class cRadialGradBox : public cBox {
  public:
    //{{{
    cRadialGradBox (cAppWindow* window, float width, float height)
        : cBox("progress", window, width, height) {

      mPin = true;

      D2D1_GRADIENT_STOP gradientStops[2];
      gradientStops[0].color = D2D1::ColorF (D2D1::ColorF::Red, 1);
      gradientStops[0].position = 0.f;
      gradientStops[1].color = D2D1::ColorF (D2D1::ColorF::Blue, 1);
      gradientStops[1].position = 1.f;

      mWindow->getDc()->CreateGradientStopCollection (
        gradientStops, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &mGradientStopCollection);

      mWindow->getDc()->CreateRadialGradientBrush (
        D2D1::RadialGradientBrushProperties(D2D1::Point2F(100, 100), D2D1::Point2F(0, 0), 100, 100),
        mGradientStopCollection, &mRadialGradientBrush);
      }
    //}}}
    //{{{
    virtual ~cRadialGradBox() {
      mGradientStopCollection->Release();
      mRadialGradientBrush->Release();
      }
    //}}}

    void onDraw (ID2D1DeviceContext* dc) {

      mRadialGradientBrush->SetCenter (mRect.getCentre());
      mRadialGradientBrush->SetGradientOriginOffset (cPoint(0,0));
      mRadialGradientBrush->SetRadiusX (mRect.getWidth()/2.f);
      mRadialGradientBrush->SetRadiusY (mRect.getHeight()/2.f);
      dc->FillEllipse (Ellipse (mRect.getCentre(), getWidth()/2.f, getHeight()/2.f), mRadialGradientBrush);
      }

  private:
    ID2D1GradientStopCollection* mGradientStopCollection = nullptr;
    ID2D1RadialGradientBrush* mRadialGradientBrush = nullptr;
    };
  //}}}
  //{{{
  class cGeometryBox : public cBox {
  public:
    //{{{
    cGeometryBox (cAppWindow* window, float width, float height)
        : cBox("progress", window, width, height) {

      mPin = true;

      D2D1_GRADIENT_STOP gradientStops[2];
      gradientStops[0].color = D2D1::ColorF (D2D1::ColorF::Red, 1);
      gradientStops[0].position = 0.f;
      gradientStops[1].color = D2D1::ColorF (D2D1::ColorF::Blue, 1);
      gradientStops[1].position = 1.f;

      mWindow->getDc()->CreateGradientStopCollection (
        gradientStops, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &mGradientStopCollection);

      mWindow->getDc()->CreateLinearGradientBrush (
        D2D1::LinearGradientBrushProperties(D2D1::Point2F(0, 0), D2D1::Point2F(100.f, 10.f)),
        mGradientStopCollection, &mLinearGradientBrush);

      window->getD2d1Factory()->CreatePathGeometry (&mSunGeometry);

      ID2D1GeometrySink* sink = NULL;
      mSunGeometry->Open (&sink);
      sink->SetFillMode (D2D1_FILL_MODE_WINDING);
      sink->BeginFigure (D2D1::Point2F(270, 255), D2D1_FIGURE_BEGIN_FILLED);
      sink->AddArc (D2D1::ArcSegment (D2D1::Point2F(440, 255), // end point
                                       D2D1::SizeF(85, 85), 0, // rotation angle
                                       D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
      sink->EndFigure (D2D1_FIGURE_END_CLOSED);
      sink->Close();
      sink->Release();
      }
    //}}}
    //{{{
    virtual ~cGeometryBox() {
      mGradientStopCollection->Release();
      mLinearGradientBrush->Release();
      mSunGeometry->Release();
      }
    //}}}

    void onDraw (ID2D1DeviceContext* dc) {

      mLinearGradientBrush->SetStartPoint (mRect.getTL());
      mLinearGradientBrush->SetEndPoint (mRect.getBR());
      dc->FillGeometry (mSunGeometry, mLinearGradientBrush);
      dc->DrawGeometry (mSunGeometry, mWindow->getWhiteBrush(), 2.f);

      //mLinearGradientBrush->SetStartPoint (mRect.getTL());
      //mLinearGradientBrush->SetEndPoint (mRect.getBR());
      //dc->FillRectangle (mRect, mLinearGradientBrush);
      }

  private:
    ID2D1GradientStopCollection* mGradientStopCollection = nullptr;
    ID2D1LinearGradientBrush* mLinearGradientBrush = nullptr;
    ID2D1PathGeometry* mSunGeometry = nullptr;
    };
  //}}}

  //{{{
  void hlsLoaderThread() {

    cWinSockHttp http;
    http.initialise();

    mHls->mChanChanged = true;
    while (true) {
      if (mHls->mChanChanged)
        mHls->setChan (http, mHls->mChan);

      //mHls->loadPicAtPlayFrame (http);
      if (!mHls->loadAtPlayFrame (http))
        Sleep (1000);

      mHlsSem.wait();
      }
    }
  //}}}
  //{{{
  void hlsPlayerThread() {

    uint32_t seqNum = 0;
    uint32_t numSamples = 0;
    uint32_t lastSeqNum = 0;
    uint16_t scrubCount = 0;
    double scrubSample = 0;
    while (true) {
      if (mHls->getPlaying() == cHls::eScrub) {
        if (scrubCount == 0)
          scrubSample = mHls->getPlaySample();
        if (scrubCount < 3) {
          auto sample = mHls->getPlaySamples (scrubSample + (scrubCount * kSamplesPerFrame), seqNum, numSamples);
          audPlay (sample, 4096, 1.f);
          }
        else
          audPlay (nullptr, 4096, 1.f);
        if (scrubCount++ > 3)
          scrubCount = 0;
        //int srcSamplesConsumed = mHls->getReSamples (mHls->getPlaySample(), seqNum, numSamples, mReSamples, mHls->mSpeed);
        //audPlay (mReSamples, 4096, 1.f);
        //mHls->incPlaySample (srcSamplesConsumed);
        }
      else if (mHls->getPlaying()) {
        auto sample = mHls->getPlaySamples (mHls->getPlaySample(), seqNum, numSamples);
        audPlay (sample, 4096, 1.f);
        if (sample)
          mHls->incPlayFrame();
        }
      else
        audPlay (nullptr, 4096, 1.f);

      if (mHls->mChanChanged || !seqNum || (seqNum != lastSeqNum)) {
        lastSeqNum = seqNum;
        mHlsSem.notify();
        }

      if (mHls->mVolumeChanged) {
        setVolume (mHls->mVolume);
        mHls->mVolumeChanged = false;
        }
      }
    }
  //}}}
  cHls* mHls = nullptr;
  cSemaphore mHlsSem;
  };

//{{{
int main (int argc, char** argv) {

  cLog::init ("console", LOGINFO3, false);

  reportDateTimes();
  reportTimezone();
  //reportYearCalendar();

  WSADATA wsaData;
  if (WSAStartup (MAKEWORD(2,2), &wsaData)) {
    //{{{  error exit
    cLog::log (LOGERROR, "WSAStartup failed");
    exit (0);
    }
    //}}}

  cAppWindow appWindow;
  appWindow.run ("console", 1920/2, 1080/2, 4);
  }
//}}}
