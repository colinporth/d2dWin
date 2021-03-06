// cD2dWindow.h
//{{{  includes
#pragma once

#include <wrl.h>

#include <deque>
#include <chrono>
#include <functional>

#include "../../shared/date/date.h"
#include "../../shared/utils/utils.h"
#include "../../shared/utils/cLog.h"
#include "../../shared/utils/iChange.h"

#include "cPointRect.h"
#include "cView2d.h"
//}}}

//{{{
inline std::string wcharToString (const wchar_t* input) {

  int required_characters = WideCharToMultiByte (CP_UTF8, 0, input, -1, nullptr, 0, nullptr, nullptr);
  if (required_characters <= 0)
    return {};

  std::string output;
  output.resize (static_cast<size_t>(required_characters-1));
  WideCharToMultiByte (CP_UTF8, 0, input, -1, output.data(), static_cast<int>(output.size()), nullptr, nullptr);

  return output;
  }
//}}}
//{{{
inline std::string wstringToString (const std::wstring& input) {

  int required_characters = WideCharToMultiByte (CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()),
                                                 nullptr, 0, nullptr, nullptr);
  if (required_characters <= 0)
    return {};

  std::string output;
  output.resize (static_cast<size_t>(required_characters));
  WideCharToMultiByte (CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()),
                       output.data(), static_cast<int>(output.size()), nullptr, nullptr);

  return output;
  }
//}}}

const float kLineHeight = 20.f;
const float kConsoleHeight = 16.f;

class cD2dWindow : public iChange {
public:
  //{{{
  class cBox {
  public:
    //{{{
    cBox (std::string name, cD2dWindow* window, float width, float height)
        : mName(name), mWindow(window), mLayoutWidth(width), mLayoutHeight(height) {
      mWindow->changed();
      }
    //}}}
    virtual ~cBox() {}

    // gets
    std::string getName() const { return mName; }
    cD2dWindow* getWindow() { return mWindow; }

    bool getEnable() { return mEnable; }
    bool getPick() { return mPick; }
    bool getShow() { return mEnable && (mPick || mPin); }
    bool getTimedOn() { return mTimedOn; }

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
    cBox* setPos (cPoint pos) {
      mLayoutX = pos.x;
      mLayoutY = pos.y;
      layout();
      return this;
      }
    //}}}
    //{{{
    cBox* setPos (float x, float y) {
      mLayoutX = x;
      mLayoutY = y;
      layout();
      return this;
      }
    //}}}
    cBox* setEnable (bool enable) { mEnable = enable; return this;  }
    cBox* setPickable (bool pickable) { mPickable = pickable;  return this; }
    cBox* setUnPick() { mPick = false;  return this; }
    cBox* setPin (bool pin) { mPin = pin; return this; }
    cBox* togglePin() { mPin = !mPin;  return this; }
    cBox* setTimedOn() { mTimedOn = true; return this;  }

    // overrides
    //{{{
    virtual void layout() {

      mRect.left = (mLayoutX < 0) ? mWindow->getSize().x + mLayoutX : mLayoutX;
      if (mLayoutWidth > 0)
        mRect.right = mRect.left + mLayoutWidth;
      else if (mLayoutWidth == 0)
        mRect.right = mWindow->getSize().x - mLayoutX;
      else // mLayoutWidth < 0
        mRect.right = mWindow->getSize().x + mLayoutWidth + mLayoutX;

      mRect.top = (mLayoutY < 0) ? mWindow->getSize().y + mLayoutY : mLayoutY;
      if (mLayoutHeight > 0)
        mRect.bottom = mRect.top + mLayoutHeight;
      else if (mLayoutHeight == 0)
        mRect.bottom = mWindow->getSize().y - mLayoutY;
      else // mLayoutHeight < 0
        mRect.bottom = mWindow->getSize().y + mLayoutHeight + mLayoutY;
      }
    //}}}
    //{{{
    virtual bool pick (bool inClient, cPoint pos, bool& change) {

      bool lastPick = mPick;

      mPick = inClient && mRect.inside (pos) & mPickable;
      if (!change && (mPick != lastPick))
        change = true;

      return mPick;
      }
    //}}}
    virtual bool onKey (int key) { return false; }
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
    float measureText (ID2D1DeviceContext* dc, const std::string& str, IDWriteTextFormat* textFormat,
                       const cRect& r, float fontSize) {

      IDWriteTextLayout* textLayout;
      mWindow->getDwriteFactory()->CreateTextLayout (
        strToWstr(str).data(), (uint32_t)str.size(), textFormat,
        r.getWidth(), r.getHeight(), &textLayout);

      if (textLayout) {
        textLayout->SetFontSize (fontSize, {0, (uint32_t)str.size()});
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
    float measureText (ID2D1DeviceContext* dc, const std::wstring& wstr, IDWriteTextFormat* textFormat,
                       const cRect& r, float fontSize) {

      IDWriteTextLayout* textLayout;
      mWindow->getDwriteFactory()->CreateTextLayout (wstr.data(), (uint32_t)wstr.size(), textFormat,
                                                     r.getWidth(), r.getHeight(), &textLayout);
      if (textLayout) {
        textLayout->SetFontSize (fontSize, {0, (uint32_t)wstr.size()});
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
    float drawText (ID2D1DeviceContext* dc, const std::string& str, IDWriteTextFormat* textFormat,
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
    float drawText (ID2D1DeviceContext* dc, const std::wstring& wstr, IDWriteTextFormat* textFormat,
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
    void drawDebug (ID2D1DeviceContext* dc, const std::string& title, int value, cRect& r) {
      drawDebug (dc, title+dec(value), r);
      }
    //}}}
    //{{{
    void drawDebug (ID2D1DeviceContext* dc, const std::string& str, cRect& r) {
      drawDebug (dc, str, mWindow->getGrayBrush(), r);
      }
    //}}}
    //{{{
    void drawDebug (ID2D1DeviceContext* dc, const std::string& str, ID2D1SolidColorBrush* brush, cRect& r) {

      dc->FillRectangle (r, brush);

      IDWriteTextLayout* textLayout;
      mWindow->getDwriteFactory()->CreateTextLayout (
        strToWstr(str).data(), (uint32_t)str.size(),
        mWindow->getTextFormat(), r.getWidth(), r.getHeight(), &textLayout);
      if (textLayout) {
        dc->DrawTextLayout (r.getTL(2.f), textLayout, mWindow->getBlackBrush());
        dc->DrawTextLayout (r.getTL(), textLayout, mWindow->getWhiteBrush());
        textLayout->Release();
        }

      r.top = r.bottom;
      r.bottom += kLineHeight;
      }
    //}}}

    std::string mName;
    cD2dWindow* mWindow;

    bool mEnable = true;
    bool mPick = false;
    bool mPickable = true;
    bool mPin = true;
    bool mTimedOn = false;

    float mLayoutWidth;
    float mLayoutHeight;
    float mLayoutX = 0;
    float mLayoutY = 0;

    cRect mRect = { 0.f };
    };
  //}}}
  //{{{
  class cView : public cBox {
  public:
    cView (std::string name, cD2dWindow* window, float width, float height)
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

      if (mWindow->getShift())
        mView2d.multiplyBy (D2D1::Matrix3x2F::Rotation (45.f * (delta / 120.f), getCentre()));
      else {
        float ratio = mWindow->getControl() ? 1.5f : 1.1f;
        if (delta < 0)
          ratio = 1.f / ratio;
        mView2d.multiplyBy (D2D1::Matrix3x2F::Scale (ratio, ratio, pos));
        }
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
    void drawTab (ID2D1DeviceContext* dc, std::string str, cRect dst, ID2D1SolidColorBrush* brush) {

      if (dst.left < 0)
        dst.left = 0;
      if (dst.top - kLineHeight < 0)
        dst.top = kLineHeight;

      IDWriteTextLayout* textLayout;
      mWindow->getDwriteFactory()->CreateTextLayout (
        std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(), mWindow->getTextFormat(),
        dst.right - dst.left, kLineHeight, &textLayout);

      struct DWRITE_TEXT_METRICS textMetrics;
      textLayout->GetMetrics (&textMetrics);

      dc->FillRectangle (cRect (dst.left-1.f, dst.top - kLineHeight,
                                dst.left + textMetrics.width + 4.f, dst.top), brush);
      dc->DrawTextLayout (cPoint (dst.left-1.f, dst.top- kLineHeight), textLayout, mWindow->getBlackBrush());

      textLayout->Release();
      }
    //}}}

  protected:
    cView2d mView2d;
    };
  //}}}

  void init (const std::string& title, int width = 1920, int height = 1080, bool fullScreen = false);
  static cD2dWindow* getD2dWindow() { return mD2dWindow; };

  //{{{  gets
  ID3D11Device* getD3d11Device() { return mD3device.Get(); }
  ID2D1DeviceContext* getDc() { return mDeviceContext.Get(); }

  ID2D1Factory1* getD2d1Factory() { return mD2D1Factory; }
  IDWriteFactory* getDwriteFactory() { return mDWriteFactory; }

  cPoint getCentre() { return mClientF/2.f; }
  cPoint getSize() { return mClientF; }
  float getWidth() { return mClientF.x; }
  float getHeight() { return mClientF.y; }

  IDWriteTextFormat* getTextFormat() { return mTextFormat; }
  IDWriteTextFormat* getConsoleTextFormat() { return mConsoleTextFormat; }

  // brushes
  ID2D1SolidColorBrush* getDimBgndBrush() { return mDimBgndBrush; }
  ID2D1SolidColorBrush* getTransparentBgndBrush() { return mTransparentBgndBrush; }

  ID2D1SolidColorBrush* getBlackBrush() { return mBlackBrush; }
  ID2D1SolidColorBrush* getDarkGrayBrush() { return mDarkGrayBrush; }
  ID2D1SolidColorBrush* getDimGrayBrush() { return mDimGrayBrush; }
  ID2D1SolidColorBrush* getGrayBrush() { return mGrayBrush; }
  ID2D1SolidColorBrush* getLightGrayBrush() { return mLightGrayBrush; }
  ID2D1SolidColorBrush* getWhiteBrush() { return mWhiteBrush; }

  ID2D1SolidColorBrush* getDarkBlueBrush() { return mDarkBlueBrush; }
  ID2D1SolidColorBrush* getBlueBrush() { return mBlueBrush; }
  ID2D1SolidColorBrush* getGreenBrush() { return mGreenBrush; }
  ID2D1SolidColorBrush* getRedBrush() { return mRedBrush; }
  ID2D1SolidColorBrush* getYellowBrush() { return mYellowBrush; }
  ID2D1SolidColorBrush* getOrangeBrush() { return mOrangeBrush; }

  bool getShift() { return mShiftKeyDown; }
  bool getControl() { return mControlKeyDown; }
  bool getMouseDown() { return mMouseDown; }
  bool getTimedMenuOn() { return mTimedMenuOn; }

  int getCountDown() { return mCursorDown; }

  std::chrono::system_clock::time_point getNow();
  std::chrono::system_clock::time_point getNowRaw();
  int getDayLightSeconds() { return mDayLightSeconds; }

  bool getFullScreen() { return mFullScreen; }

  bool getExit() { return mExit; }
  //}}}
  //{{{  sets
  void toggleFullScreen();

  void keyChanged() { mCursorDown = mCursorCountDown; }
  void cursorChanged() { mCursorDown = mCursorCountDown; }

  void setExit() { mExit = true; }
  void setTimedMenuOff() { mTimedMenuOn = false; }
  void setChangeCountDown (int countDown) { mChangeCountDown = countDown; }
  //}}}

  // boxes
  cBox* add (cBox* box);
  cBox* add (cBox* box, cPoint pos);
  cBox* add (cBox* box, float x, float y);
  cBox* addRight (cBox* box, float offset = 1.0f);
  cBox* addBelow (cBox* box, float offset = 1.0f);
  cBox* addFront (cBox* box);
  cBox* addFront (cBox* box, float x, float y);
  void removeBox (cBox* box);

  // iChanged
  void changed() { mCountDown = 0; }

  void onResize();

  LRESULT wndProc (HWND hWnd, unsigned int msg, WPARAM wparam, LPARAM lparam);

protected:
  ~cD2dWindow();

  void messagePump();

  virtual bool onKey (int key) = 0;
  virtual bool onKeyUp (int key) { return false; }

  float mRenderTime = 0;

private:
  void createFactories();
  void createDeviceResources();
  void createSizedResources();

  bool onMouseProx (bool inClient, cPoint pos);
  bool onMouseWheel (int delta, cPoint pos);
  bool onMouseDown (bool right, cPoint pos);
  bool onMouseMove (bool right, cPoint pos, cPoint inc);
  bool onMouseUp (bool right, bool mouseMoved, cPoint pos);
  void onDraw (ID2D1DeviceContext* dc);
  void onResize (ID2D1DeviceContext* dc);

  // private vars
  inline static cD2dWindow* mD2dWindow = nullptr;

  HWND mHWND = 0;
  bool mExit = false;

  //{{{  resources
  // device independent resources
  ID2D1Factory1* mD2D1Factory;
  IDWriteFactory* mDWriteFactory;

  // device resources
  Microsoft::WRL::ComPtr<ID3D11Device> mD3device;
  Microsoft::WRL::ComPtr<ID3D11Device1> mD3dDevice1;

  Microsoft::WRL::ComPtr<IDXGIDevice> mDxgiDevice;
  Microsoft::WRL::ComPtr<IDXGIDevice1> mDxgiDevice1;

  Microsoft::WRL::ComPtr<ID3D11DeviceContext1> mD3dContext1;
  Microsoft::WRL::ComPtr<ID2D1DeviceContext> mDeviceContext;

  // sized resources
  Microsoft::WRL::ComPtr<IDXGISwapChain1> mSwapChain;
  Microsoft::WRL::ComPtr<ID2D1Bitmap1> mD2dTargetBitmap;

  D2D1_SIZE_U mClient = { 0 };
  cPoint mClientF;

  // useful resources
  IDWriteTextFormat* mTextFormat = nullptr;
  IDWriteTextFormat* mConsoleTextFormat = nullptr;

  ID2D1SolidColorBrush* mDimBgndBrush = nullptr;
  ID2D1SolidColorBrush* mTransparentBgndBrush = nullptr;

  ID2D1SolidColorBrush* mBlackBrush = nullptr;
  ID2D1SolidColorBrush* mDarkGrayBrush = nullptr;
  ID2D1SolidColorBrush* mDimGrayBrush = nullptr;
  ID2D1SolidColorBrush* mGrayBrush = nullptr;
  ID2D1SolidColorBrush* mLightGrayBrush = nullptr;
  ID2D1SolidColorBrush* mWhiteBrush = nullptr;

  ID2D1SolidColorBrush* mDarkBlueBrush = nullptr;
  ID2D1SolidColorBrush* mBlueBrush = nullptr;
  ID2D1SolidColorBrush* mGreenBrush = nullptr;
  ID2D1SolidColorBrush* mRedBrush = nullptr;
  ID2D1SolidColorBrush* mYellowBrush = nullptr;
  ID2D1SolidColorBrush* mOrangeBrush = nullptr;
  //}}}
  //{{{  boxes
  cBox* mProxBox = nullptr;
  cBox* mPressedBox = nullptr;
  std::deque <cBox*> mBoxes;
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
  RECT mScreenRect = { 0 };
  DWORD mScreenStyle = 0;
  //}}}
  //{{{  render
  uint32_t mCountDown = 0;
  uint32_t mChangeCountDown = 100;

  bool mCursorOn = true;
  bool mTimedMenuOn = true;
  uint32_t mCursorDown = 50;
  uint32_t mCursorCountDown = 50;
  //}}}
  int mDayLightSeconds = 0;
  };
