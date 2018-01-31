// cD2dWindow.cpp
//{{{  includes
#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX

#include <windows.h>
#include <wrl.h>
#define _USE_MATH_DEFINES

#include <shobjidl.h>
#include <shlguid.h>
#include <shellapi.h>
#include <shlwapi.h>
#pragma comment(lib,"shlwapi.lib")

#include <algorithm>
#include <thread>

#include <d3d11.h>
#include <d3d11_1.h>
#include <d2d1.h>
#include <d2d1_1.h>
#include <DXGI1_2.h>
#include <d2d1helper.h>
#include <dwrite.h>
#pragma comment(lib,"d3d11")
#pragma comment(lib,"d2d1.lib")
#pragma comment(lib,"dxguid.lib")
#pragma comment(lib,"dwrite.lib")

using namespace Microsoft::WRL;
using namespace D2D1;
using namespace std;
#include "cD2dWindow.h"

using namespace chrono;
//}}}

// static var init
cD2dWindow* cD2dWindow::mD2dWindow = NULL;

// local procedure
//{{{
LRESULT CALLBACK WndProc (HWND hWnd, unsigned int msg, WPARAM wparam, LPARAM lparam) {

  return cD2dWindow::mD2dWindow->wndProc (hWnd, msg, wparam, lparam);
  }
//}}}

// public
//{{{
void cD2dWindow::initialise (string title, int width, int height, bool fullScreen) {

  mD2dWindow = this;

  WNDCLASSEX wndclass = { sizeof (WNDCLASSEX),
                          CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW,
                          WndProc,
                          0, 0,
                          GetModuleHandle (0),
                          LoadIcon (0,IDI_APPLICATION),
                          LoadCursor (0,IDC_ARROW),
                          NULL,
                          0,
                          "windowClass",
                          LoadIcon (0,IDI_APPLICATION) };

  if (RegisterClassEx (&wndclass))
    mHWND = CreateWindowEx (0,
                            "windowClass",
                            title.data(),
                            WS_OVERLAPPEDWINDOW,
                            20, 20, width+4, height+32,
                            0, 0,
                            GetModuleHandle(0), 0);

  if (mHWND) {
    //SetWindowLong (hWnd, GWL_STYLE, 0);
    if (fullScreen)
      toggleFullScreen();
    ShowWindow (mHWND, SW_SHOWDEFAULT);
    UpdateWindow (mHWND);

    createDirect2d();

    TIME_ZONE_INFORMATION timeZoneInfo;
    if (GetTimeZoneInformation (&timeZoneInfo) == TIME_ZONE_ID_DAYLIGHT)
      mDaylightSeconds = -timeZoneInfo.DaylightBias * 60;

    mRenderThread = thread ([=]() { renderThread(); } );
    mRenderThread.detach();
    }
  }
//}}}
//{{{
cD2dWindow::cBox* cD2dWindow::add (cBox* box, cPoint pos) {

  mBoxes.push_back (box);
  box->setPos (pos);
  return box;
  }
//}}}
//{{{
cD2dWindow::cBox* cD2dWindow::add (cBox* box, float x, float y) {
  return add (box, cPoint(x,y));
  }
//}}}
//{{{
cD2dWindow::cBox* cD2dWindow::add (cBox* box) {
  return add (box, cPoint());
  }
//}}}
//{{{
cD2dWindow::cBox* cD2dWindow::addBelow (cBox* box) {

  mBoxes.push_back (box);
  auto lastBox = mBoxes.back();
  box->setPos (lastBox->getBL());
  return box;
  }
//}}}
//{{{
cD2dWindow::cBox* cD2dWindow::addFront (cBox* box, float x, float y) {

  mBoxes.push_front (box);
  box->setPos (cPoint(x,y));
  return box;
  }
//}}}
//{{{
void cD2dWindow::removeBox (cBox* box) {

  for (auto boxIt = mBoxes.begin(); boxIt != mBoxes.end(); ++boxIt)
    if (*boxIt == box) {
      mBoxes.erase (boxIt);
      changed();
      return;
      }
  }
//}}}

//{{{
void cD2dWindow::toggleFullScreen() {

  mFullScreen = !mFullScreen;

  if (mFullScreen) {
    MONITORINFOEX monitorInfo;
    monitorInfo.cbSize = sizeof(monitorInfo);

    HMONITOR hMonitor = MonitorFromWindow (mHWND, MONITOR_DEFAULTTONEAREST);
    GetMonitorInfo (hMonitor, &monitorInfo);

    WINDOWINFO wndInfo;
    wndInfo.cbSize = sizeof(WINDOWINFO);
    GetWindowInfo (mHWND, &wndInfo);

    mScreenRect = wndInfo.rcWindow;
    mScreenStyle = wndInfo.dwStyle;

    SetWindowLong (mHWND, GWL_STYLE, WS_POPUP);
    SetWindowPos (mHWND, HWND_NOTOPMOST,
                  monitorInfo.rcMonitor.left , monitorInfo.rcMonitor.top,
                  abs(monitorInfo.rcMonitor.left - monitorInfo.rcMonitor.right),
                  abs(monitorInfo.rcMonitor.top - monitorInfo.rcMonitor.bottom),
                  SWP_SHOWWINDOW);
    }

  else {
    AdjustWindowRectEx (&mScreenRect, 0, 0, 0);
    SetWindowLong (mHWND, GWL_STYLE, mScreenStyle);
    SetWindowPos (mHWND, HWND_NOTOPMOST,
                  mScreenRect.left , mScreenRect.top ,
                  abs(mScreenRect.right - mScreenRect.left), abs(mScreenRect.bottom - mScreenRect.top),
                  SWP_SHOWWINDOW);
    }

  }
//}}}
//{{{
void cD2dWindow::onResize() {

  RECT rect;
  GetClientRect (mHWND, &rect);

  if (((rect.right - rect.left) != mClient.height) || ((rect.bottom - rect.top) != mClient.width)) {
    if (mDeviceContext) {
      mDeviceContext->SetTarget (nullptr);
      mD2dTargetBitmap = nullptr;
      createSizedResources();

      onResize (mDeviceContext.Get());
      changed();
      }
    }
  }
//}}}

//{{{
LRESULT cD2dWindow::wndProc (HWND hWnd, unsigned int msg, WPARAM wparam, LPARAM lparam) {

  switch (msg) {
    //{{{
    case WM_KEYDOWN:
      mKeyDown++;

      if (wparam == 0x10)
        mShiftKeyDown = true;
      if (wparam == 0x11)
        mControlKeyDown = true;

      if (onKey ((int)wparam))
        PostQuitMessage (0) ;

      keyChanged();
      return 0;
    //}}}
    //{{{
    case WM_KEYUP:
      mKeyDown--;

      if (wparam == 0x10)
        mShiftKeyDown = false;
      if (wparam == 0x11)
        mControlKeyDown = false;
      onKeyUp ((int)wparam);

      keyChanged();
      return 0;
    //}}}

    case WM_LBUTTONDOWN:
    //{{{
    case WM_RBUTTONDOWN: {

      if (!mMouseDown)
        SetCapture (hWnd);

      mMouseDown = true;
      mMouseMoved = false;

      mDownMouse = cPoint (LOWORD(lparam), HIWORD(lparam));
      mRightDown = (msg == WM_RBUTTONDOWN);
      mLastMouse = mDownMouse;

      if (onMouseDown (mRightDown, mDownMouse))
        changed();

      cursorChanged();
      return 0;
      }
    //}}}

    case WM_LBUTTONUP:
    //{{{
    case WM_RBUTTONUP: {

      mLastMouse = cPoint (LOWORD(lparam), HIWORD(lparam));
      if (onMouseUp (mRightDown, mMouseMoved, mLastMouse))
        changed();

      if (mMouseDown)
        ReleaseCapture();
      mMouseDown = false;

      cursorChanged();
      return 0;
      }
    //}}}

    //{{{
    case WM_MOUSEMOVE: {

      mProxMouse = cPoint (LOWORD(lparam), HIWORD(lparam));

      if (mMouseDown) {
        mMouseMoved = true;
        if (onMouseMove (mRightDown, mProxMouse, mProxMouse - mLastMouse))
          changed();
        mLastMouse = mProxMouse;
        }
      else if (onMouseProx (true, mProxMouse))
        changed();

      if (!mMouseTracking) {
        TRACKMOUSEEVENT trackMouseEvent;
        trackMouseEvent.cbSize = sizeof(TRACKMOUSEEVENT);
        trackMouseEvent.dwFlags = TME_LEAVE;
        trackMouseEvent.hwndTrack = hWnd;
        if (TrackMouseEvent (&trackMouseEvent))
          mMouseTracking = true;
        }

      cursorChanged();
      return 0;
      }
    //}}}
    //{{{
    case WM_MOUSEWHEEL: {
      if (onMouseWheel (GET_WHEEL_DELTA_WPARAM(wparam), mProxMouse))
        changed();

      cursorChanged();
      return 0;
      }
    //}}}
    //{{{
    case WM_MOUSELEAVE: {

      if (onMouseProx (false, cPoint()))
        changed();

      mMouseTracking =  false;
      return 0;
      }
    //}}}

    //{{{
    case WM_SIZE: {
      onResize();
      return 0;
      }
    //}}}
    //{{{
    case WM_DESTROY: {
      PostQuitMessage(0) ;
      return 0;
      }
    //}}}

    case WM_TIMER:
      //{{{  cursor hide logic
      if (mCursorDown > 0) {
        mCursorDown--;
        if (!mCursorOn) {
          mCursorOn = true;
          mTimedMenuOn = true;
          ShowCursor (mCursorOn);
          }
        }
      else if (mCursorOn) {
        mCursorOn = false;
        mTimedMenuOn = false;
        ShowCursor (mCursorOn);
        }
      return 0;
      //}}}

    case WM_SYSCOMMAND:
      switch (wparam) {
        //{{{
        case SC_MAXIMIZE:
          cLog::log (LOGINFO, "SC_MAXIMIZE");
          toggleFullScreen();
          break;
        //}}}
        //{{{
        case SC_SCREENSAVE:
          cLog::log (LOGINFO, "SC_SCREENSAVE");
          return 0;
        //}}}
        //{{{
        case SC_MONITORPOWER:
          cLog::log (LOGINFO, "SC_MONITORPOWER");
          return 0;
        //}}}
        default: ; // not handled, fall thru
        }
    }

  return DefWindowProc (hWnd, msg, wparam, lparam);
  }
//}}}
//{{{
void cD2dWindow::messagePump() {

  UINT_PTR timer = SetTimer (mHWND, 0x1234, 1000/25, NULL);

  MSG msg;
  while (!mExit && GetMessage (&msg, NULL, 0, 0)) {
    TranslateMessage (&msg);
    DispatchMessage (&msg);
    }

  KillTimer (NULL, timer);
  }
//}}}

// private
//{{{
void cD2dWindow::createDirect2d() {

  // create D2D1Factory
  #ifdef _DEBUG
    D2D1CreateFactory (D2D1_FACTORY_TYPE_MULTI_THREADED, {D2D1_DEBUG_LEVEL_INFORMATION},
                       mD2D1Factory.GetAddressOf());
  #else
    D2D1CreateFactory (D2D1_FACTORY_TYPE_MULTI_THREADED, {D2D1_DEBUG_LEVEL_NONE},
                       mD2D1Factory.GetAddressOf());
  #endif

  // create DWriteFactory
  DWriteCreateFactory (DWRITE_FACTORY_TYPE_SHARED,
                       __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(mDWriteFactory.GetAddressOf()));

  createDeviceResources();
  createSizedResources();

  // create a textFormat
  mDWriteFactory->CreateTextFormat (L"FreeSans", NULL,
                                    DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                    16.f, L"en-us",
                                    &mTextFormat);
  mTextFormat->SetWordWrapping (DWRITE_WORD_WRAPPING_EMERGENCY_BREAK);

  // create solid brushes
  mDeviceContext->CreateSolidColorBrush (ColorF (ColorF::Black), &mBlackBrush);
  mDeviceContext->CreateSolidColorBrush (ColorF (ColorF::DimGray), &mDarkGreyBrush);
  mDeviceContext->CreateSolidColorBrush (ColorF (ColorF::Gray), &mGreyBrush);
  mDeviceContext->CreateSolidColorBrush (ColorF (ColorF::LightGray), &mLightGreyBrush);
  mDeviceContext->CreateSolidColorBrush (ColorF (ColorF::White), &mWhiteBrush);
  mDeviceContext->CreateSolidColorBrush (ColorF (ColorF::CornflowerBlue), &mBlueBrush);
  mDeviceContext->CreateSolidColorBrush (ColorF (ColorF::Green), &mGreenBrush);
  mDeviceContext->CreateSolidColorBrush (ColorF (ColorF::Yellow), &mYellowBrush);
  mDeviceContext->CreateSolidColorBrush (ColorF (ColorF::Orange), &mOrangeBrush);
  mDeviceContext->CreateSolidColorBrush (ColorF (ColorF::Red), &mRedBrush);
  mDeviceContext->CreateSolidColorBrush (ColorF (0.f, 0.f, 0.f, 0.5f), &mTransparentBgndBrush);
  }
//}}}
//{{{
void cD2dWindow::createDeviceResources() {
// create DX11 API device object, get D3Dcontext

  D3D_FEATURE_LEVEL featureLevels[] = {
    D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
    D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
    D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_1 };

  D3D_FEATURE_LEVEL featureLevel;

  ComPtr<ID3D11DeviceContext> d3d11deviceContext;
  if (D3D11CreateDevice (nullptr,                  // specify null to use the default adapter
                         D3D_DRIVER_TYPE_HARDWARE,
                         0,
                         D3D11_CREATE_DEVICE_BGRA_SUPPORT, // optionally set debug and Direct2D compatibility flags
                         featureLevels, ARRAYSIZE(featureLevels), // list of feature levels this app can support
                         D3D11_SDK_VERSION,
                         &mD3device,               // returns the Direct3D device created
                         &featureLevel,            // returns feature level of device created
                         &d3d11deviceContext) != S_OK) {             // returns the device immediate context
    cLog::log (LOGERROR, "D3D11CreateDevice - failed");
    }
  cLog::log (LOGNOTICE, "D3D11CreateDevice - featureLevel %d.%d",
                         (featureLevel >> 12) & 0xF, (featureLevel >> 8) & 0xF);

  mD3device.As (&mD3dDevice1);
  d3d11deviceContext.As(&mD3dContext1);
  mD3device.As(&mDxgiDevice);
  mD3dDevice1.As (&mDxgiDevice1);

  // create D2D1device from DXGIdevice for D2Drendering.
  ComPtr<ID2D1Device> d2d1Device;
  mD2D1Factory->CreateDevice (mDxgiDevice.Get(), &d2d1Device);

  // create D2Ddevice_context from D2D1device
  d2d1Device->CreateDeviceContext (D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &mDeviceContext);
  }
//}}}
//{{{
void cD2dWindow::createSizedResources() {

  RECT rect;
  GetClientRect (mHWND, &rect);
  mClient.width = rect.right - rect.left;
  mClient.height = rect.bottom - rect.top;
  mClientF.x = float(mClient.width);
  mClientF.y = float(mClient.height);

  if (!mSwapChain) {
    // get DXGIAdapter from DXGIdevice
    ComPtr<IDXGIAdapter> dxgiAdapter;
    mDxgiDevice1->GetAdapter (&dxgiAdapter);

    // get DXGIFactory2 from DXGIadapter
    ComPtr<IDXGIFactory2> dxgiFactory2;
    dxgiAdapter->GetParent (IID_PPV_ARGS (&dxgiFactory2));

    // create swapChain
    //{{{  setup swapChainDesc
    DXGI_SWAP_CHAIN_DESC1 dxgiSwapChainDesc;

    dxgiSwapChainDesc.Width = 0;                           // use automatic sizing
    dxgiSwapChainDesc.Height = 0;
    dxgiSwapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // this is the most common swapchain format

    dxgiSwapChainDesc.Stereo = false;

    dxgiSwapChainDesc.SampleDesc.Count = 1;                // don't use multi-sampling
    dxgiSwapChainDesc.SampleDesc.Quality = 0;

    dxgiSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    dxgiSwapChainDesc.BufferCount = 2;                     // use double buffering to enable flip

    dxgiSwapChainDesc.Scaling = DXGI_SCALING_NONE;
    dxgiSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL; // all apps must use this SwapEffect

    dxgiSwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    dxgiSwapChainDesc.Flags = 0;
    //}}}
    dxgiFactory2->CreateSwapChainForHwnd (
      mD3dDevice1.Get(), mHWND, &dxgiSwapChainDesc, nullptr, nullptr, &mSwapChain);
    mDxgiDevice1->SetMaximumFrameLatency (1);
    }
  else {
    // resize
    HRESULT hr = mSwapChain->ResizeBuffers (0, 0, 0, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
    if (hr == DXGI_ERROR_DEVICE_REMOVED) {
      mSwapChain = nullptr;
      createDeviceResources();
      return;
      }
    }

  // get DXGIbackbuffer surface pointer from swapchain
  ComPtr<IDXGISurface> dxgiBackBuffer;
  //ComPtr<ID3D11Texture2D> backBuffer;
  //mSwapChain->GetBuffer (0, IID_PPV_ARGS (&backBuffer));
  mSwapChain->GetBuffer (0, IID_PPV_ARGS (&dxgiBackBuffer));
  D2D1_BITMAP_PROPERTIES1 d2d1_bitmapProperties =
    {DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE, 0,0,
     D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW, NULL };
  mDeviceContext->CreateBitmapFromDxgiSurface (dxgiBackBuffer.Get(), &d2d1_bitmapProperties, &mD2dTargetBitmap);

  // set Direct2D render target.
  mDeviceContext->SetTarget (mD2dTargetBitmap.Get());

  // grayscale text anti-aliasing
  mDeviceContext->SetTextAntialiasMode (D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
  }
//}}}

//{{{
void cD2dWindow::renderThread() {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::setThreadName ("rend");

  // wait for target bitmap
  while (mD2dTargetBitmap == nullptr)
    Sleep (10);

  while (mDeviceContext && !mExit) {
    if (mCountDown > 0) {
      mCountDown--;
      Sleep (10);
      }
    else {
      mCountDown = mChangeCountDown;

      mTimePoint = system_clock::now();
      mDeviceContext->BeginDraw();
      mDeviceContext->Clear (ColorF (ColorF::Black));
      onDraw (mDeviceContext.Get());
      mDeviceContext->EndDraw();
      mRenderTime = duration_cast<milliseconds>(system_clock::now() - mTimePoint).count() / 1000.f;

      if (mSwapChain->Present (1, 0) == DXGI_ERROR_DEVICE_REMOVED) {
        mSwapChain = nullptr;
        createDeviceResources();
        cLog::log (LOGERROR, "device removed");
        }
      }
    }

  cLog::log (LOGNOTICE, "stop");
  CoUninitialize();
  }
//}}}

//{{{
bool cD2dWindow::onMouseProx (bool inClient, cPoint pos) {

  bool change = false;
  auto lastProxBox = mProxBox;

  // search for prox in reverse draw order
  mProxBox = nullptr;
  for (auto boxIt = mBoxes.rbegin(); boxIt != mBoxes.rend(); ++boxIt) {
    bool wasPicked = (*boxIt)->getPick();
    if (!mProxBox && (*boxIt)->getEnable() && (*boxIt)->pick (inClient, pos, change)) {
      mProxBox = *boxIt;
      change |= mProxBox->onProx (inClient, pos - mProxBox->getTL());
      }
    else if (wasPicked) {
      (*boxIt)->setUnPick();
      change |= (*boxIt)->onProxExit();
      }
    }
  return change || (mProxBox != lastProxBox);
  };
//}}}
//{{{
bool cD2dWindow::onMouseWheel (int delta, cPoint pos) {
  return mProxBox && mProxBox->onWheel (delta, pos- mProxBox->getTL());
  }
//}}}
//{{{
bool cD2dWindow::onMouseDown (bool right, cPoint pos) {

  mPressedBox = mProxBox;
  return mPressedBox && mPressedBox->onDown (right, pos - mPressedBox->getTL());
  }
//}}}
//{{{
bool cD2dWindow::onMouseMove (bool right, cPoint pos, cPoint inc) {
  return mPressedBox && mPressedBox->onMove (right, pos - mPressedBox->getTL(), inc);
  }
//}}}
//{{{
bool cD2dWindow::onMouseUp (bool right, bool mouseMoved, cPoint pos) {

  bool change = mPressedBox && mPressedBox->onUp (right, mouseMoved, pos - mPressedBox->getTL());
  mPressedBox = nullptr;
  return change;
  }
//}}}
//{{{
void cD2dWindow::onDraw (ID2D1DeviceContext* dc) {

  dc->Clear (ColorF(ColorF::Black));
  for (auto box : mBoxes)
    if (box->getShow())
      box->onDraw (dc);
  }
//}}}
//{{{
void cD2dWindow::onResize (ID2D1DeviceContext* dc) {

  for (auto box : mBoxes)
    box->onResize (dc);
  }
//}}}
