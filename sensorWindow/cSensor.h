// cSensor.h
#pragma once
#include "../common/cPointRect.h"
#include "../../shared/utils/cSemaphore.h"

#include "../inc/jpeglib/jpeglib.h"

const int kMt9d111 = 0x1519; // mt9d111 sensor id - auto focus
const int kMt9d112 = 0x1580; // mt9d112 sensor id - black pcb

//{{{
class cVector {
public:
  //{{{
  cVector() {
    startValues();
    memset (mShowValues, 0, kBins);
    }
  //}}}

  //{{{
  void startValues() {
    memset (mValues, 0, kBins);
    }
  //}}}
  //{{{
  void addValue (uint8_t y, uint8_t u, uint8_t v) {
    if (y > mValues [((v << kBinShiftV) & kBinShiftMaskV) | (u >> kBinShift)])
      mValues [((v << kBinShiftV) & kBinShiftMaskV) | (u >> kBinShift)] = y;
    }
  //}}}
  //{{{
  void finishValues() {
    for (auto i = 0; i < kBins; i++)
      mShowValues[i] = mValues[i] / 256.0f;
    }
  //}}}

  static const uint8_t kBinShift = 1;
  static const uint16_t kBins = (256 >> kBinShift) * (256 >> kBinShift);

  float mShowValues [kBins];

private:
  static const uint8_t kBinShiftV = 6;
  static const uint16_t kBinShiftMaskV = 0x3F80;

  uint8_t mValues [kBins];
  };
//}}}
//{{{
class cHistogram {
public:
  //{{{
  cHistogram() {
    startValues();
    memset (mShowValues, 0, kBins);
    }
  //}}}

  //{{{
  void startValues() {
    memset (mValues, 0, kBins * sizeof(uint32_t));
    }
  //}}}
  //{{{
  void incValue (uint8_t value) {
    mValues[value >> kBinShift]++;
    }
  //}}}
  //{{{
  void incValue (uint8_t y1, uint8_t y2) {
    mValues[y1 >> kBinShift]++;
    mValues[y2 >> kBinShift]++;
    }
  //}}}
  //{{{
  void finishValues() {

    uint32_t maxValue = 0;
    for (auto i = 1; i < kBins - 1; i++)
      maxValue = max (maxValue, mValues[i]);

    float scale = 256.0f / maxValue;
    for (auto i = 0; i < kBins; i++)
      mShowValues[i] = mValues[i] *scale;
    }
  //}}}

  static const uint8_t kBinShift = 1;
  static const uint8_t kBins = (256 >> kBinShift);

  float mShowValues [kBins];

private:
  uint32_t mValues [kBins];
  };
//}}}
//{{{
class cRgbHistogram {
public:
  //{{{
  cRgbHistogram() {
    startValues();
    memset (mShowRedValues, 0, kBins);
    memset (mShowGreenValues, 0, kBins);
    memset (mShowBlueValues, 0, kBins);
    }
  //}}}

  //{{{
  void startValues() {
    memset (mRedValues, 0, 0x80 * sizeof(uint32_t));
    memset (mGreenValues, 0, 0x80 * sizeof(uint32_t));
    memset (mBlueValues, 0, 0x80 * sizeof(uint32_t));
    }
  //}}}
  //{{{
  void incValue (uint8_t r, uint8_t g, uint8_t b) {
    mRedValues[r >> kBinShift]++;
    mGreenValues[g >> kBinShift]++;
    mBlueValues[b >> kBinShift]++;
    }
  //}}}
  //{{{
  void finishValues() {

    uint32_t maxValue = 0;
    for (auto i = 1; i < kBins-1; i++) {
      maxValue = max (maxValue, mRedValues[i]);
      maxValue = max (maxValue, mGreenValues[i]);
      maxValue = max (maxValue, mBlueValues[i]);
      }

    float scale = 256.0f / maxValue;
    for (auto i = 0; i < kBins; i++) {
      mShowRedValues[i] = mRedValues[i] * scale;
      mShowGreenValues[i] = mGreenValues[i] * scale;
      mShowBlueValues[i] = mBlueValues[i] * scale;
      }
    }
  //}}}

  static const uint8_t kBinShift = 1;
  static const uint8_t kBins = (256 >> kBinShift);

  float mShowRedValues [kBins];
  float mShowGreenValues [kBins];
  float mShowBlueValues [kBins];

private:
  uint32_t mRedValues [kBins];
  uint32_t mGreenValues [kBins];
  uint32_t mBlueValues [kBins];
  };
//}}}

class cSensor {
public:
  cSensor() {}
  //{{{
  ~cSensor() {
    if (mBitmap) {
      mBitmap->Release();
      mBitmap = nullptr;
      }
    }
  //}}}

  void run();

  // gets
  int getId() { return mId; }

  enum eMode { ePreview, ePreviewRgb565, eCapture, eCaptureRgb565, eBayer, eJpeg };
  eMode getMode() { return mMode; }

  int getWidth() { return mWidth; }
  int getHeight() { return mHeight; }
  cPoint getSize() { return cPoint (float(mWidth), float(mHeight)); }

  ID2D1Bitmap* getBitmap() { return mBitmap; }
  float getFrameTime() { return mFrameTime; }

  cVector& getVector() { return mVector; }
  cHistogram& getLumaHistogram() { return mLumaHistogram; }
  cRgbHistogram& getRgbHistogram() { return mRgbHistogram; }

  ID2D1Bitmap* getBitmapFrame (ID2D1DeviceContext* dc, int bayer, bool info);
  float getRgbTime() { return mRgbTime; }

  // sets
  void setMode (eMode mode);

  void setSlowPll();
  void setFastPll();
  void setPll (int m, int n, int p);

  float getFocus();
  void setFocus (float focus);

  string mTitle;

private:
  uint8_t limitByte (float v);
  uint8_t* getFramePtr (int& frameLen);
  uint8_t* getRgbFrame (int bayer, bool info);

  void updateTitle();
  void listenThread();

  //{{{  vars
  eMode mMode = ePreview;
  int mWidth = 800;
  int mHeight = 600;

  string mSensorTitle = "sensor";

  int mId = 0;
  float mFocus = 0;
  //{{{  pll
  int mPllm = 16;
  int mPlln = 1;
  int mPllp = 1;
  //}}}

  uint8_t* mBuffer = nullptr;
  uint8_t* mBufferEnd = nullptr;

  int mFrameLen = 0;
  uint8_t* mLastFramePtr = nullptr;
  int mLastFrameLen = 0;
  ID2D1Bitmap* mBitmap = nullptr;

  int mFrameCount = 0;

  float mFrameTime = 0;
  chrono::time_point<chrono::system_clock> mLastTime;
  float mRgbTime = 0;

  cHistogram mLumaHistogram;
  cRgbHistogram mRgbHistogram;
  cVector mVector;

  cSemaphore mSem;
  //}}}
  };
