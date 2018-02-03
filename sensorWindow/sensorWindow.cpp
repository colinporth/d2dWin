// sensorWindow.cpp
//{{{  includes
#include "stdafx.h"

#include "../boxes/cFloatBox.h"
#include "../boxes/cValueBox.h"
#include "../boxes/cIndexBox.h"
#include "../boxes/cLogBox.h"
#include "../boxes/cWindowBox.h"
#include "../boxes/cClockBox.h"

#include "cSensor.h"
//}}}
//{{{  const
const int kFullScreen = true;
//}}}

class cAppWindow : public cD2dWindow {
public:
  //{{{
  void run (string title, int width, int height) {

    mSensor = new cSensor();
    mSensor->run();

    initialise (title, width+20, height+20, kFullScreen);
    add (new cSensorView (this, 0,0, mSensor));
    add (new cLogBox (this, 200.f,0, true), 200.f,0);

    mLumaHistogramBox = new cHistogramBox (this, 256.f,256.f, mSensor->getLumaHistogram());
    add (mLumaHistogramBox, -276.f,-532.f);
    mRgbHistogramBox = new cRgbHistogramBox (this, 256.f,256.f, mSensor->getRgbHistogram());
    add (mRgbHistogramBox, -276.f,-276.f);
    mVectorBox = new cVectorBox (this, 256.f,256.f, mSensor->getVector());
    add (mVectorBox, -276.f,-276.f);

    add (new cWindowBox (this, 60.f,24.f), -60.f,0);

    mBayerValueBox = new cValueBox (this, 60.f,24.f, "bayer", 0,5.f, mBayer, mBayerChanged);
    add (mBayerValueBox, -60.f,-24.f);

    if (mSensor->getId() == kMt9d112)
      add (new cIndexBox (this, 50.f, 4*20.f,
                                      {"pvw", "full", "bayer"}, (int&)mMode, &mModeChanged));
    else if (mSensor->getId() == kMt9d111) {
      add (new cIndexBox (this, 50.f, 4*20.f,
                                      {"pvw", "full", "bayer", "jpeg"}, (int&)mMode, &mModeChanged));
      add (
        new cValueBox (this, 100.f,20.f, "focus", 0,255.f, mFocus, mFocusChanged), -100.f,0);
      }
    add (new cFloatBox (this, 50.f,20.f, mRenderTime), -50.f,-20.f);
    add (new cClockBox (this, 40.f, mTimePoint, true, true), -82.f,-82.f);

    if (mSensor->getId() == kMt9d111)
      setMode (cSensor::ePreview);
    else
      setMode (cSensor::eCapture);

    thread ([=](){ changeThread(); }).detach();

    messagePump();
    }
  //}}}

protected:
  //{{{
  bool onKey (int key) {

    switch (key) {
      case 0x00:
      case 0x10: // shift
      case 0x11: // control
        return false;
      case 0x1B: return true; // escape abort

      case 0x21: break; // page up
      case 0x22: break; // page down
      case 0x23: break; // end
      case 0x24: break; // home

      case 0x25: break; // left arrow
      case 0x26: break; // up arrow
      case 0x27: break; // right arrow
      case 0x28: break; // down arrow

      case 'P': mMode = (mMode == cSensor::ePreview) ? cSensor::eCapture : cSensor::ePreview;
                mModeChanged = true;
                break;
      case 'B': mMode = (mMode == cSensor::eBayer) ? cSensor::ePreview : cSensor::eBayer;
                mModeChanged = true;
                break;
      case 'J': mMode = (mMode == cSensor::eJpeg) ? cSensor::ePreview : cSensor::eJpeg;
                mModeChanged = true;
                break;

      case 'T': mSensor->setSlowPLL(); break;  // 48 Mhz
      case 'R': mSensor->set80PLL(); break; // 80 Mhz
      case 'A': mSensor->setPll (++mPllm, mPlln, mPllp); break;
      case 'Z': mSensor->setPll (--mPllm, mPlln, mPllp); break;
      case 'S': mSensor->setPll (mPllm, ++mPlln, mPllp); break;
      case 'X': mSensor->setPll (mPllm, --mPlln, mPllp); break;
      case 'D': mSensor->setPll (mPllm, mPlln, ++mPllp); break;;
      case 'C': mSensor->setPll (mPllm, mPlln, --mPllp); break;

      case 'F': toggleFullScreen(); break;

      case 0xbc: mSensor->setFocus (mSensor->getFocus() - 1); break;
      case 0xbe: mSensor->setFocus (mSensor->getFocus() + 1); break;

      default:   cLog::log (LOGERROR, "key " + hex(key));
      }

    return false;
    }
  //}}}

private:
  //{{{
  class cSensorView : public cView {
  public:
    //{{{
    cSensorView(cD2dWindow* window, float width, float height, cSensor* sensor)
        : cView("sensor", window, width, height), mSensor(sensor) {
      mPin = true;
      }
    //}}}
    virtual ~cSensorView() {}

    //{{{
    cPoint getSrcSize() {
      return mSensor->getSize();
      }
    //}}}
    //{{{
    void layout() {
      mView2d.setPos (mWindow->getSize()/2.f);
      }
    //}}}
    //{{{
    void onDraw (ID2D1DeviceContext* dc) {

      auto bitmap = mSensor->getBitmap();

      if (bitmap) {
        auto scale = (bitmap->GetPixelSize().width <= 800) ? 2.f : 1.f;
        mView2d.setSrcPos (getSrcSize()*scale/-2.f);
        mView2d.setSrcScale (scale);
        }

      auto dst = mView2d.getSrcToDst (cRect (getSrcSize()));

      if (bitmap) {
        dc->SetTransform (mView2d.mTransform);
        dc->DrawBitmap (bitmap, cRect(bitmap->GetPixelSize()),
                        1.f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
        dc->DrawRectangle (cRect(bitmap->GetPixelSize()), mWindow->getWhiteBrush());
        dc->SetTransform (Matrix3x2F::Identity());
        }

      drawTab (dc, mSensor->mTitle, dst, mWindow->getLightGreyBrush());
      }
    //}}}

  private:
    cSensor* mSensor;
    };
  //}}}
  //{{{
  class cVectorBox : public cBox {
  public:
    //{{{
    cVectorBox (cD2dWindow* window, float width, float height, cVector& vector)
        : cBox("vector", window, width, height), mVector(vector) {
      mPin = true;
      mWindow->getDc()->CreateSolidColorBrush (ColorF (ColorF::White), &mBrush);
      }
    //}}}
    //{{{
    virtual ~cVectorBox() {
      mBrush->Release();
      }
    //}}}

    //{{{
    bool onDown (bool right, cPoint pos)  {
      togglePin();
      return true;
      }
    //}}}
    //{{{
    void onDraw (ID2D1DeviceContext* dc) {

      // crosshair
      auto r = mRect;
      dc->FillRectangle (RectF (r.left, r.top+128.f, r.right,r.top+128.f+1.f), mWindow->getWhiteBrush());
      dc->FillRectangle (RectF (r.left+128.f, r.top, r.left + 128.f+1.f,r.bottom), mWindow->getWhiteBrush());

      // uv vectorScope
      auto colour = ColorF(0,0,0, 1.f);
      auto valuesPtr = mVector.mShowValues;
      for (auto v = -0.5f; v < 0.5f; v += 2.f/256.f) {
        r.bottom = r.top + 2.f;
        for (auto u = -0.5f; u < 0.5f; u += 2.f/256.f) {
          r.right = r.left + 2.f;
          if (*valuesPtr) {
            colour.r = *valuesPtr + v*1.5748f;
            colour.g = *valuesPtr - u*0.1873f - v*0.4681f;
            colour.b = *valuesPtr + u*1.8556f;
            mBrush->SetColor (colour);
            dc->FillRectangle (r, mBrush);
            }
          valuesPtr++;
          r.left = r.right;
          }

        r.left = mRect.left;
        r.top = r.bottom;
        }
      }
    //}}}

  private:
    //{{{
    uint8_t limitByte (float v) {

      if (v <= 0.0)
        return 0;
      if (v >= 255.0)
        return 255;
      return (uint8_t)v;
      }
    //}}}

    cVector& mVector;

    ID2D1SolidColorBrush* mBrush = nullptr;
    };
  //}}}
  //{{{
  class cHistogramBox : public cBox {
  public:
    //{{{
    cHistogramBox (cD2dWindow* window, float width, float height, cHistogram& histogram)
        : cBox("histogram", window, width, height), mHistogram(histogram) {
      mPin = true;
      mWindow->getDc()->CreateSolidColorBrush (ColorF (ColorF::White, 0.8f), &mBrush);
      }
    //}}}
    //{{{
    virtual ~cHistogramBox() {
      mBrush->Release();
      }
    //}}}

    //{{{
    bool onDown (bool right, cPoint pos)  {
      togglePin();
      return true;
      }
    //}}}
    //{{{
    void onDraw (ID2D1DeviceContext* dc) {

      auto inc = powf (2.f, mHistogram.kBinShift);

      auto r = mRect;
      auto colour = ColorF(0,0,0, 0.9f);
      for (auto i = 0; i < mHistogram.kBins; i++) {
        r.right = r.left + inc;
        r.top = mRect.bottom - mHistogram.mShowValues[i];
        colour.r = i/128.f;
        colour.g = colour.r;
        colour.b = colour.r;
        mBrush->SetColor (colour);
        dc->FillRectangle (r, mBrush);
        r.left = r.right;
        }
      }
    //}}}

  private:
    cHistogram& mHistogram;

    ID2D1SolidColorBrush* mBrush = nullptr;
    };
  //}}}
  //{{{
  class cRgbHistogramBox : public cBox {
  public:
    //{{{
    cRgbHistogramBox (cD2dWindow* window, float width, float height, cRgbHistogram& rgbHistogram)
        : cBox("rgbHistogram", window, width, height), mRgbHistogram(rgbHistogram) {
      mPin = true;
      mWindow->getDc()->CreateSolidColorBrush (ColorF (ColorF::Green, 0.75f), &mGreenBrush);
      mWindow->getDc()->CreateSolidColorBrush (ColorF (ColorF::Red, 0.75f), &mRedBrush);
      mWindow->getDc()->CreateSolidColorBrush (ColorF (ColorF::Blue, 0.75f), &mBlueBrush);
      }
    //}}}
    //{{{
    virtual ~cRgbHistogramBox() {
      mRedBrush->Release();
      mGreenBrush->Release();
      mBlueBrush->Release();
    }
    //}}}

    //{{{
    bool onDown (bool right, cPoint pos)  {
      togglePin();
      return true;
      }
    //}}}
    //{{{
    void onDraw (ID2D1DeviceContext* dc) {

      auto inc = powf (2.f, mRgbHistogram.kBinShift);

      auto r = mRect;
      for (auto i = 0; i < mRgbHistogram.kBins; i++) {
        r.right = r.left + inc;
        r.top = mRect.bottom - mRgbHistogram.mShowGreenValues[i];
        dc->FillRectangle (r, mGreenBrush);
        r.top = mRect.bottom - mRgbHistogram.mShowRedValues[i];
        dc->FillRectangle (r, mRedBrush);
        r.top = mRect.bottom - mRgbHistogram.mShowBlueValues[i];
        dc->FillRectangle (r, mBlueBrush);
        r.left = r.right;
        }
      }
    //}}}

  private:
    cRgbHistogram& mRgbHistogram;

    ID2D1SolidColorBrush* mGreenBrush = nullptr;
    ID2D1SolidColorBrush* mRedBrush = nullptr;
    ID2D1SolidColorBrush* mBlueBrush = nullptr;
    };
  //}}}

  //{{{
  void setMode (cSensor::eMode mode) {

    mMode = mode;
    mSensor->setMode (mode);

    mVectorBox->setEnable (mode == cSensor::ePreview || mode == cSensor::eCapture);
    mLumaHistogramBox->setEnable (mode == cSensor::ePreview || mode == cSensor::eCapture);
    mRgbHistogramBox->setEnable (mode == cSensor::eBayer);

    mBayerValueBox->setEnable (mode == cSensor::eBayer);

    changed();

    mModeChanged = false;
    }
  //}}}
  //{{{
  void changeThread() {

    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::log (LOGNOTICE, "changeThread - start");

    while (true) {
      if (mModeChanged)
        setMode (mMode);
      if (mBayerChanged) {
        //{{{  bayer changed
        changed();
        mBayerChanged = false;
        }
        //}}}
      if (mFocusChanged) {
        //{{{  set focus
        mSensor->setFocus (mFocus);
        mFocusChanged = false;
        changed();
        }
        //}}}

      // block waiting for new frame
      mSensor->getBitmapFrame (getDc(), int(mBayer), true);
      changed();
      }

    cLog::log (LOGERROR, "changeThread - exit");
    CoUninitialize();
    }
  //}}}

  //{{{  vars
  cSensor* mSensor = nullptr;

  cVectorBox* mVectorBox;
  cHistogramBox* mLumaHistogramBox;
  cRgbHistogramBox* mRgbHistogramBox;
  cValueBox* mBayerValueBox;

  float mBayer = 0;
  bool mBayerChanged = false;

  cSensor::eMode mMode = cSensor::ePreview;
  bool mModeChanged = false;

  bool mFocusChanged = false;
  float mFocus = 0;

  // pll
  int mPllm = 16;
  int mPlln = 1;
  int mPllp = 1;
  //}}}
  };

//{{{
int __stdcall WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);

  cLog::init (LOGINFO, true); //, "C:/Users/colin/Desktop");

  cAppWindow appWindow;
  appWindow.run ("sensorWindow", 800, 600);

  CoUninitialize();
  }
//}}}
