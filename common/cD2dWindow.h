// cD2dWindow.h
#pragma once
//{{{  includes
#include <wrl.h>
#include <deque>
#include <vector>

#include "../../shared/utils/utils.h"
#include "../../shared/utils/date.h"
#include "../../shared/utils/cLog.h"

#include "cPointRect.h"
#include "cView2d.h"

using namespace std;
//}}}
const float kTextHeight = 20.f;

class cD2dWindow {
public:
  static cD2dWindow* mD2dWindow;
  //{{{  static utils
  static bool resolveShortcut (const string& shortcut, string& fullName);
  static void scanDirectory (vector<string>& fileNames, const string& parentName,
                             const string& directoryName, const string& pathMatchName);
  static vector<string> getFiles (const string& fileName, const string& match);
  static void getTimeOfDay (struct timeval* tp, struct timezone* tzp);
  //}}}

  //{{{
  class cBox {
  public:
    //{{{
    cBox (string name, cD2dWindow* window, float width, float height)
        : mName(name), mWindow(window), mLayoutWidth(width), mLayoutHeight(height) {
      mWindow->changed();
      }
    //}}}
    virtual ~cBox() {}

    // gets
    string getName() { return mName; }

    bool getEnable() { return mEnable; }
    bool getPick() { return mPick; }
    bool getShow() { return mEnable && (mPick || mPin); }

    cPoint getSize() { return mRect.getSize(); }
    float getWidth() { return mRect.getWidth(); }
    float getHeight() { return mRect.getHeight(); }
    int getWidthInt() { return mRect.getWidthInt(); }
    int getHeightInt() { return mRect.getHeightInt(); }
    cPoint getTL() { return mRect.getTL(); }
    cPoint getTL (float offset) { return mRect.getTL (offset); }
    cPoint getTR() { return mRect.getTR(); }
    cPoint getBL() { return mRect.getBL(); }
    cPoint getBR() { return mRect.getBR(); }
    cPoint getCentre() { return mRect.getCentre(); }
    float getCentreX() { return mRect.getCentreX(); }
    float getCentreY() { return mRect.getCentreY(); }

    //{{{
    void setPos (cPoint pos) {
      mLayoutX = pos.x;
      mLayoutY = pos.y;
      layout();
      }
    //}}}
    //{{{
    void setPos (float x, float y) {
      mLayoutX = x;
      mLayoutY = y;
      layout();
      }
    //}}}
    void setEnable (bool enable) { mEnable = enable; }
    void setUnPick() { mPick = false; }
    void setPin (bool pin) { mPin = pin; }
    void togglePin() { mPin = !mPin; }

    // overrides
    //{{{
    virtual void layout() {

      mRect.left = (mLayoutX < 0) ? mWindow->getSize().x + mLayoutX : mLayoutX;
      if (mLayoutWidth > 0)
        mRect.right = mRect.left + mLayoutWidth;
      else if (mLayoutWidth == 0)
        mRect.right = mWindow->getSize().x - mLayoutX;
      else // mLayoutWidth < 0
        mRect.right = mWindow->getSize().x;

      mRect.top = (mLayoutY < 0) ? mWindow->getSize().y + mLayoutY : mLayoutY;
      if (mLayoutHeight > 0)
        mRect.bottom = mRect.top + mLayoutHeight;
      else if (mLayoutHeight == 0)
        mRect.bottom = mWindow->getSize().y - mLayoutY;
      else // mLayoutHeight < 0
        mRect.bottom = mWindow->getSize().y;
      }
    //}}}
    //{{{
    virtual bool pick (bool inClient, cPoint pos, bool& change) {

      bool lastPick = mPick;

      mPick = inClient && pos.x >= mRect.left && pos.y >= mRect.top &&
                          pos.x < mRect.right && pos.y < mRect.bottom;
      if (!change && (mPick != lastPick))
        change = true;

      return mPick;
      }
    //}}}
    virtual bool onProx (bool inClient, cPoint pos) { return false; }
    virtual bool onProxExit() { return false; }
    virtual bool onWheel (int delta, cPoint pos)  { return false; }
    virtual bool onDown (bool right, cPoint pos)  { return false; }
    virtual bool onMove (bool right, cPoint pos, cPoint inc)  { return false; }
    virtual bool onUp (bool right, bool mouseMoved, cPoint pos) { return false; }
    virtual void onDraw (ID2D1DeviceContext* dc) = 0;
    virtual void onResize (ID2D1DeviceContext* dc) { layout(); }

  protected:
    //{{{
    float measureText (ID2D1DeviceContext* dc, const string& str, IDWriteTextFormat* textFormat,
                       const cRect& r, float textHeight) {

      IDWriteTextLayout* textLayout;
      mWindow->getDwriteFactory()->CreateTextLayout (
        strToWstr(str).data(), (uint32_t)str.size(), textFormat,
        r.getWidth(), r.getHeight(), &textLayout);

      if (textLayout) {
        textLayout->SetFontSize (textHeight, {0, (uint32_t)str.size()});

        struct DWRITE_TEXT_METRICS textMetrics;
        textLayout->GetMetrics (&textMetrics);
        textLayout->Release();

        return textMetrics.width;
        }
      else
        return 0;
      }
    //}}}
    //{{{
    float drawText (ID2D1DeviceContext* dc, const string& str, IDWriteTextFormat* textFormat,
                    const cRect& r, ID2D1SolidColorBrush* brush, float textHeight) {

      IDWriteTextLayout* textLayout;
      mWindow->getDwriteFactory()->CreateTextLayout (
        strToWstr(str).data(), (uint32_t)str.size(), textFormat,
        r.getWidth(), r.getHeight(), &textLayout);

      if (textLayout) {
        textLayout->SetFontSize (textHeight, {0, (uint32_t)str.size()});

        struct DWRITE_TEXT_METRICS textMetrics;
        textLayout->GetMetrics (&textMetrics);

        dc->DrawTextLayout (r.getTL(), textLayout, brush);
        textLayout->Release();

        return textMetrics.width;
        }
      else
        return 0;
      }
    //}}}

    //{{{
    float measureText (ID2D1DeviceContext* dc, const wstring& wstr, IDWriteTextFormat* textFormat,
                       const cRect& r, float textHeight) {

      IDWriteTextLayout* textLayout;
      mWindow->getDwriteFactory()->CreateTextLayout (wstr.data(), (uint32_t)wstr.size(), textFormat,
                                                     r.getWidth(), r.getHeight(), &textLayout);
      if (textLayout) {
        textLayout->SetFontSize (textHeight, {0, (uint32_t)wstr.size()});

        struct DWRITE_TEXT_METRICS textMetrics;
        textLayout->GetMetrics (&textMetrics);
        textLayout->Release();

        return textMetrics.width;
        }
      else
        return 0;
      }
    //}}}
    //{{{
    float drawText (ID2D1DeviceContext* dc, const wstring& wstr, IDWriteTextFormat* textFormat,
                    const cRect& r, ID2D1SolidColorBrush* brush, float textHeight) {

      IDWriteTextLayout* textLayout;
      mWindow->getDwriteFactory()->CreateTextLayout (wstr.data(), (uint32_t)wstr.size(), textFormat,
                                                     r.getWidth(), r.getHeight(), &textLayout);
      if (textLayout) {
        textLayout->SetFontSize (textHeight, {0, (uint32_t)wstr.size()});

        struct DWRITE_TEXT_METRICS textMetrics;
        textLayout->GetMetrics (&textMetrics);

        dc->DrawTextLayout (r.getTL(), textLayout, brush);
        textLayout->Release();

        return textMetrics.width;
        }
      else
        return 0;
      }
    //}}}

    //{{{
    void drawDebug (ID2D1DeviceContext* dc, int value, cRect& r) {
      drawDebug (dc, dec(value), r);
      }
    //}}}
    //{{{
    void drawDebug (ID2D1DeviceContext* dc, const string& title, int value, cRect& r) {
      drawDebug (dc, title+dec(value), r);
      }
    //}}}
    //{{{
    void drawDebug (ID2D1DeviceContext* dc, const string& str, cRect& r) {
      drawDebug (dc, str, mWindow->getGreyBrush(), r);
      }
    //}}}
    //{{{
    void drawDebug (ID2D1DeviceContext* dc, const string& str, ID2D1SolidColorBrush* brush, cRect& r) {

      dc->FillRectangle (r, brush);

      IDWriteTextLayout* textLayout;
      mWindow->getDwriteFactory()->CreateTextLayout (
        strToWstr(str).data(), (uint32_t)str.size(),
        mWindow->getTextFormat(), r.getWidth(), r.getHeight(), &textLayout);

      dc->DrawTextLayout (r.getTL(2.f), textLayout, mWindow->getBlackBrush());
      dc->DrawTextLayout (r.getTL(), textLayout, mWindow->getWhiteBrush());
      textLayout->Release();

      r.top = r.bottom;
      r.bottom += kTextHeight;
      }
    //}}}

    const ColorF kTransparentBlack = {0.f, 0.f, 0.f, 0.8f };

    string mName;
    cD2dWindow* mWindow;

    bool mEnable = true;
    bool mPick = false;
    bool mPin = false;

    float mLayoutWidth;
    float mLayoutHeight;
    float mLayoutX = 0;
    float mLayoutY = 0;

    cRect mRect = {0,0,0,0};
    };
  //}}}
  //{{{
  class cView : public cBox {
  public:
    cView (string name, cD2dWindow* window, float width, float height)
      : cBox(name, window, width, height) {}
    virtual ~cView() {}

    virtual cPoint getSrcSize() { return getSize(); }
    //{{{
    bool pick (bool inClient, cPoint pos, bool& change) {

      bool lastPick = mPick;

      mPick = inClient && cRect (getSrcSize()).inside (mView2d.getDstToSrc (pos-getTL()));
      if (mPick != lastPick)
        change = true;

      return mPick;
      }
    //}}}
    //{{{
    bool onWheel (int delta, cPoint pos) {

      float ratio = mWindow->getShift() ? 2.f : mWindow->getControl() ? 1.25f : 1.1f;
      if (delta < 0)
        ratio = 1.f / ratio;
      mView2d.multiplyBy (Matrix3x2F::Scale (Size(ratio, ratio), pos));

      return true;
      }
    //}}}
    //{{{
    bool onMove (bool right, cPoint pos, cPoint inc) {

      auto ratio = mWindow->getControl() ? 2.f : mWindow->getShift() ? 1.5f : 1.f;
      return mView2d.setPos (mView2d.getPos() + (inc * ratio));
      }
    //}}}

    //{{{
    void drawTab (ID2D1DeviceContext* dc, string str, cRect dst, ID2D1SolidColorBrush* brush) {

      if (dst.left < 0)
        dst.left = 0;
      if (dst.top - kTextHeight < 0)
        dst.top = kTextHeight;

      IDWriteTextLayout* textLayout;
      mWindow->getDwriteFactory()->CreateTextLayout (
        wstring (str.begin(), str.end()).data(), (uint32_t)str.size(), mWindow->getTextFormat(),
        dst.right - dst.left, kTextHeight, &textLayout);

      struct DWRITE_TEXT_METRICS textMetrics;
      textLayout->GetMetrics (&textMetrics);

      dc->FillRectangle (cRect (dst.left-1.f, dst.top - kTextHeight,
                                dst.left + textMetrics.width + 4.f, dst.top), brush);
      dc->DrawTextLayout (cPoint (dst.left-1.f, dst.top- kTextHeight), textLayout, mWindow->getBlackBrush());

      textLayout->Release();
      }
    //}}}

  protected:
    cView2d mView2d;
    };
  //}}}

  void initialise (string title, int width, int height, bool fullScreen);
  cBox* add (cBox* box, cPoint pos);
  cBox* add (cBox* box, float x, float y);
  cBox* add (cBox* box);
  cBox* addBelow (cBox* box);

  //{{{  gets
  ID3D11Device* getD3d11Device() { return mD3device.Get(); }
  ID2D1Factory1* getD2d1Factory() { return mD2D1Factory.Get(); }
  IDWriteFactory* getDwriteFactory() { return mDWriteFactory.Get(); }
  ID2D1DeviceContext* getDc() { return mDeviceContext.Get(); }

  cPoint getSize() { return mClientF; }
  cPoint getCentre() { return mClientF/2.f; }
  float getWidth() { return mClientF.x; }
  float getHeight() { return mClientF.y; }

  IDWriteTextFormat* getTextFormat() { return mTextFormat.Get(); }
  //{{{  get brushes
  ID2D1SolidColorBrush* getBlackBrush() { return mBlackBrush.Get(); }
  ID2D1SolidColorBrush* getDarkGreyBrush() { return mDarkGreyBrush.Get(); }
  ID2D1SolidColorBrush* getGreyBrush() { return mGreyBrush.Get(); }
  ID2D1SolidColorBrush* getLightGreyBrush() { return mLightGreyBrush.Get(); }
  ID2D1SolidColorBrush* getWhiteBrush() { return mWhiteBrush.Get(); }
  ID2D1SolidColorBrush* getBlueBrush() { return mBlueBrush.Get(); }
  ID2D1SolidColorBrush* getGreenBrush() { return mGreenBrush.Get(); }
  ID2D1SolidColorBrush* getYellowBrush() { return mYellowBrush.Get(); }
  ID2D1SolidColorBrush* getRedBrush() { return mRedBrush.Get(); }
  ID2D1SolidColorBrush* getOrangeBrush() { return mOrangeBrush.Get(); }
  ID2D1SolidColorBrush* getTransparentBlackBrush() { return mTransparentBlackBrush.Get(); }
  //}}}

  bool getShift() { return mShiftKeyDown; }
  bool getControl() { return mControlKeyDown; }
  bool getMouseDown() { return mMouseDown; }
  //}}}
  //{{{  full screen
  bool getFullScreen() { return mFullScreen; }
  void toggleFullScreen();
  void onResize();
  //}}}

  int getDaylightSeconds() { return mDaylightSeconds; }

  bool getExit() { return mExit; }
  void setExit() { mExit = true; }

  void changed() { mCountDown = 0; }
  void setChangeCountDown (int countDown) { mChangeCountDown = countDown; }

  void cursorChanged() { mCursorDown = mCursorCountDown; }

  LRESULT wndProc (HWND hWnd, unsigned int msg, WPARAM wparam, LPARAM lparam);
  void messagePump();

  chrono::time_point<chrono::system_clock> mTimePoint;

protected:
  virtual bool onKey (int key) = 0;
  virtual bool onKeyUp (int key) { return false; }

  // exposed for simple boxes
  float mRenderTime = 0;
  int mDaylightSeconds = 0;

private:
  void createDirect2d();
  void createDeviceResources();
  void createSizedResources();

  void updateClockTime();
  void renderThread();

  bool onMouseProx (bool inClient, cPoint pos);
  bool onMouseWheel (int delta, cPoint pos);
  bool onMouseDown (bool right, cPoint pos);
  bool onMouseMove (bool right, cPoint pos, cPoint inc);
  bool onMouseUp (bool right, bool mouseMoved, cPoint pos);
  void onDraw (ID2D1DeviceContext* dc);
  void onResize (ID2D1DeviceContext* dc);

  // vars
  HWND mHWND = 0;
  bool mExit = false;
  //{{{  resources
  // device independent resources
  ComPtr<ID2D1Factory1> mD2D1Factory;
  ComPtr<IDWriteFactory> mDWriteFactory;

  // device resources
  ComPtr<ID3D11Device> mD3device;
  ComPtr<ID3D11Device1> mD3dDevice1;

  ComPtr<IDXGIDevice> mDxgiDevice;
  ComPtr<IDXGIDevice1> mDxgiDevice1;

  ComPtr<ID3D11DeviceContext1> mD3dContext1;
  ComPtr<ID2D1DeviceContext> mDeviceContext;

  // sized resources
  ComPtr<IDXGISwapChain1> mSwapChain;
  ComPtr<ID2D1Bitmap1> mD2dTargetBitmap;

  D2D1_SIZE_U mClient;
  cPoint mClientF;

  // useful resources
  ComPtr<IDWriteTextFormat> mTextFormat;

  ComPtr<ID2D1SolidColorBrush> mBlackBrush;
  ComPtr<ID2D1SolidColorBrush> mDarkGreyBrush;
  ComPtr<ID2D1SolidColorBrush> mGreyBrush;
  ComPtr<ID2D1SolidColorBrush> mLightGreyBrush;
  ComPtr<ID2D1SolidColorBrush> mWhiteBrush;
  ComPtr<ID2D1SolidColorBrush> mBlueBrush;
  ComPtr<ID2D1SolidColorBrush> mGreenBrush;
  ComPtr<ID2D1SolidColorBrush> mYellowBrush;
  ComPtr<ID2D1SolidColorBrush> mRedBrush;
  ComPtr<ID2D1SolidColorBrush> mOrangeBrush;
  ComPtr<ID2D1SolidColorBrush> mTransparentBlackBrush;
  //}}}
  //{{{  boxes
  cBox* mProxBox = nullptr;
  cBox* mPressedBox = nullptr;
  deque <cBox*> mBoxes;
  //}}}
  //{{{  mouse
  bool mMouseTracking = false;
  bool mMouseDown = false;
  bool mRightDown = false;
  bool mMouseMoved = false;

  cPoint mProxMouse;
  cPoint mDownMouse;
  cPoint mLastMouse;
  //}}}
  //{{{  key
  int mKeyDown = 0;
  bool mShiftKeyDown = false;
  bool mControlKeyDown = false;
  //}}}
  //{{{  fullScreen
  bool mFullScreen = false;
  RECT mScreenRect;
  DWORD mScreenStyle;
  //}}}
  //{{{  render
  uint32_t mCountDown = 0;
  uint32_t mChangeCountDown = 100;

  bool mCursorOn = true;
  uint32_t mCursorDown = 50;
  uint32_t mCursorCountDown = 50;

  thread mRenderThread;
  //}}}
  };
