// cSongBoxes.h
#pragma once

#include <windows.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <d2d1.h>
#include <d2d1_1.h>
#include <d2d1helper.h>
#include <DXGI1_2.h>
#include <dwrite.h>

#include "../../shared/utils/cSong.h"

//{{{
class cSongFreqBox : public cD2dWindow::cBox {
public:
  cSongFreqBox (cD2dWindow* window, float width, float height, cSong& song) :
      cBox ("songWaveBox", window, width, height), mSong(song) {
    mPin = true;
    }
  virtual ~cSongFreqBox() {}

  void onDraw (ID2D1DeviceContext* dc) {
    auto frame = mSong.getPlayFrame();
    if (frame > 0) {
      float* freq = (mSong.mFrames[frame].mFreqValues);
      for (int i = 0; (i < getWidth()) && (i < mSong.getMaxFreq()); i++)
        dc->FillRectangle (cRect (mRect.left+i*2, mRect.bottom - ((freq[i] / mSong.mMaxFreqValue) * getHeight()),
                                  mRect.left+(i*2)+2, mRect.bottom), mWindow->getYellowBrush());
      }
    }

protected:
  cSong& mSong;
  };
//}}}
//{{{
class cSongWaveBox : public cD2dWindow::cBox {
public:
  //{{{
  cSongWaveBox (cD2dWindow* window, float width, float height, cSong& song) :
      cBox ("songWaveBox", window, width, height), mSong(song) {
    mPin = true;
    }
  //}}}
  virtual ~cSongWaveBox() {}

  //{{{
  bool onMove (bool right, cPoint pos, cPoint inc) {
    mSong.incPlayFrame (int(-inc.x));
    return true;
    }
  //}}}
  //{{{
  bool onWheel (int delta, cPoint pos)  {

    if (getShow()) {
      mZoom -= delta/120;
      mZoom = min (max(mZoom, 1), 2 * (1 + mSong.mNumFrames / getWidthInt()));
      return true;
      }

    return false;
    }
  //}}}
  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    auto leftFrame = mSong.mPlayFrame - (mZoom * getWidthInt()/2);
    auto rightFrame = mSong.mPlayFrame + (mZoom * getWidthInt()/2);
    auto firstX = (leftFrame < 0) ? (-leftFrame) / mZoom : 0;

    draw (dc, leftFrame, rightFrame, firstX, mZoom);
    }
  //}}}

protected:
  float getMaxPowerValue() { return mSong.mMaxPowerValue > 0.f ? mSong.mMaxPowerValue : 1.f; }

  //{{{
  void draw (ID2D1DeviceContext* dc, int leftFrame, int rightFrame, int firstX, int zoom) {

    leftFrame = (leftFrame < 0) ? 0 : leftFrame;
    if (rightFrame > mSong.getNumParsedFrames())
      rightFrame = mSong.getNumParsedFrames();

    // draw frames
    auto colour = mWindow->getBlueBrush();

    auto yCentre = getCentreY();
    auto valueScale = (getHeight()/2.f) / getMaxPowerValue();

    auto centre = false;
    auto xl = mRect.left + firstX;
    for (auto frame = leftFrame; frame < rightFrame; frame += zoom) {
      float xr = xl + 1.f;
      if (mSong.mFrames[frame].hasTitle()) {
        dc->FillRectangle (cRect (xl, yCentre-(getHeight()/2.f), xr+2.f, yCentre+(getHeight()/2.f)), mWindow->getYellowBrush());

        string str = mSong.mFrames[frame].mTitle;
        IDWriteTextLayout* textLayout;
        mWindow->getDwriteFactory()->CreateTextLayout (wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
                                                      mWindow->getTextFormat(), getWidth(), getHeight(), &textLayout);
        if (textLayout) {
          dc->DrawTextLayout (cPoint (xl, getTL().y-20.f), textLayout, mWindow->getWhiteBrush());
          textLayout->Release();
          }         
        }

      if (mSong.mFrames[frame].isSilent()) {
        float silenceHeight = 2.f;
        dc->FillRectangle (cRect (xl, yCentre - silenceHeight, xr, yCentre + silenceHeight), mWindow->getRedBrush());
        }

      if (!centre && (frame >= mSong.mPlayFrame)) {
        auto yL = mSong.mFrames[frame].mPowerValues[0] * valueScale;
        auto yR = mSong.mFrames[frame].mPowerValues[1] * valueScale;
        dc->FillRectangle (cRect (xl, yCentre - yL, xr, yCentre + yR), mWindow->getWhiteBrush());
        colour = mWindow->getGreyBrush();
        centre = true;
        }
      else {
        auto yL = 0.f;
        auto yR = 0.f;
        for (auto i = 0; i < zoom; i++) {
          yL += mSong.mFrames[frame+i].mPowerValues[0];
          yR += mSong.mFrames[frame+i].mPowerValues[1];
          }
        yL = (yL / zoom) * valueScale;
        yR = (yR / zoom) * valueScale;
        dc->FillRectangle (cRect (xl, yCentre - yL, xr, yCentre + yR), colour);
        }
      xl = xr;
      }
    }
  //}}}

  cSong& mSong;
  int mZoom = 1;
  };
//}}}
//{{{
class cSongLensBox : public cSongWaveBox {
public:
  //{{{
  cSongLensBox (cD2dWindow* window, float width, float height, cSong& frameSet)
    : cSongWaveBox(window, width, height, frameSet) {}
  //}}}
  //{{{
  virtual ~cSongLensBox() {
    bigFree (mSummedValues);
    }
  //}}}

  //{{{
  void layout() {
    mSummedFrame = -1;
    cSongWaveBox::layout();
    }
  //}}}

  //{{{
  bool onDown (bool right, cPoint pos)  {

    mOn = true;

    auto frame = int((pos.x * mSong.mNumFrames) / getWidth());
    mSong.setPlayFrame (frame);

    return true;
    }
  //}}}
  //{{{
  bool onMove (bool right, cPoint pos, cPoint inc) {
    auto frame = int((pos.x * mSong.mNumFrames) / getWidth());
    mSong.setPlayFrame (frame);
    return true;
    }
  //}}}
  //{{{
  bool onUp (bool right, bool mouseMoved, cPoint pos) {
    mOn = false;
    return cSongWaveBox::onUp (right, mouseMoved, pos);
    }
  //}}}
  //{{{
  bool onWheel (int delta, cPoint pos)  {
    return false;
    }
  //}}}

  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    if (mOn) {
      if (mLens < getWidthInt() / 16)
        // animate on
        mLens += (getWidthInt() / 16) / 6;
      }
    else if (mLens <= 0) {
      mLens = 0;
      draw (dc, 0, mMaxSummedX);
      return;
      }
    else // animate off
      mLens /= 2;

    int curFrameX = (mSong.mNumFrames > 0) ? (mSong.mPlayFrame * getWidthInt()) / mSong.mNumFrames : 0;
    int leftLensX = curFrameX - mLens;
    int rightLensX = curFrameX + mLens;
    if (leftLensX < 0) {
      rightLensX -= leftLensX;
      leftLensX = 0;
      }
    else
      draw (dc, 0, leftLensX);

    if (rightLensX > getWidthInt()) {
      leftLensX -= rightLensX - getWidthInt();
      rightLensX = getWidthInt();
      }
    else
      draw (dc, rightLensX, getWidthInt());

    cSongWaveBox::draw (dc, mSong.mPlayFrame - mLens, mSong.mPlayFrame + mLens-2, leftLensX+1, 1);

    dc->DrawRectangle (cRect(mRect.left + leftLensX, mRect.top + 1.f,
                             mRect.left + rightLensX, mRect.top + getHeight() - 1.f),
                       mWindow->getYellowBrush(), 1.f);
    }
  //}}}

private:
  //{{{
  void makeSummedWave() {

    if (mSummedFrame != mSong.getNumParsedFrames()) {
      // frameSet changed, cache values summed to width, scaled to height
      mSummedFrame = mSong.getNumParsedFrames();

      mSummedValues = (float*)realloc (mSummedValues, getWidthInt() * 2 * sizeof(float));
      auto summedValuesPtr = mSummedValues;

      mMaxSummedX = 0;
      auto startFrame = 0;
      for (auto x = 0; x < getWidthInt(); x++) {
        int frame = x * mSong.mNumFrames / getWidthInt();
        if (frame >= mSong.getNumParsedFrames())
          break;

        float lValue = mSong.mFrames[frame].mPowerValues[0];
        float rValue = mSong.mFrames[frame].mPowerValues[1];
        if (frame > startFrame) {
          int num = 1;
          for (auto i = startFrame; i < frame; i++) {
            lValue += mSong.mFrames[i].mPowerValues[0];
            rValue += mSong.mFrames[i].mPowerValues[1];
            num++;
            }
          lValue /= num;
          rValue /= num;
          }
        *summedValuesPtr++ = lValue;
        *summedValuesPtr++ = rValue;

        mMaxSummedX = x;
        startFrame = frame + 1;
        }
      }
    }
  //}}}
  //{{{
  void draw (ID2D1DeviceContext* dc, int firstX, int lastX) {

    makeSummedWave();

    // draw cached graphic
    auto colour = mWindow->getBlueBrush();

    auto centreY = getCentreY();
    float valueScale = getHeight() / 2 / getMaxPowerValue();

    float curFrameX = mRect.left;
    if (mSong.mNumFrames > 0)
      curFrameX += mSong.mPlayFrame * getWidth() / mSong.mNumFrames;

    bool centre = false;
    float xl = mRect.left + firstX;
    auto summedValuesPtr = mSummedValues + (firstX * 2);
    for (auto x = firstX; x < lastX; x++) {
      float xr = xl + 1.f;
      if (!centre && (x >= curFrameX) && (mSong.mPlayFrame < mSong.getNumParsedFrames())) {
        float leftValue = mSong.mFrames[mSong.mPlayFrame].mPowerValues[0] * valueScale;
        float rightValue = mSong.mFrames[mSong.mPlayFrame].mPowerValues[1] * valueScale;
        dc->FillRectangle (cRect(xl, centreY - leftValue - 2.f, xr, centreY + rightValue + 2.f),
                           mWindow->getWhiteBrush());
        colour = mWindow->getGreyBrush();
        centre = true;
        }

      else if (x < mMaxSummedX) {
        auto leftValue = *summedValuesPtr++ * valueScale;
        auto rightValue = *summedValuesPtr++ * valueScale;
        dc->FillRectangle (cRect(xl, centreY - leftValue - 2.f, xr, centreY + rightValue + 2.f),
                           colour);
        }
      else
        break;
      xl = xr;
      }
    }
  //}}}

  float* mSummedValues;
  int mSummedFrame = -1;
  int mMaxSummedX = 0;

  bool mOn = false;
  int mLens = 0;
  };
//}}}
//{{{
class cSongTimeCodeBox : public cD2dWindow::cBox {
public:
  //{{{
  cSongTimeCodeBox (cD2dWindow* window, float width, float height, cSong& song) :
      cBox("songTimeBox", window, width, height), mSong(song) {

    mWindow->getDwriteFactory()->CreateTextFormat (L"Consolas", NULL,
      DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 50.f, L"en-us",
      &mTextFormat);
    mTextFormat->SetTextAlignment (DWRITE_TEXT_ALIGNMENT_TRAILING);

    mPin = true;
    }
  //}}}
  //{{{
  virtual ~cSongTimeCodeBox() {
    mTextFormat->Release();
    }
  //}}}

  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    std::string str = getFrameStr (mSong.mPlayFrame) + " " + getFrameStr (mSong.mNumFrames);

    IDWriteTextLayout* textLayout;
    mWindow->getDwriteFactory()->CreateTextLayout (
      wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
      mTextFormat, getWidth(), getHeight(), &textLayout);
    if (textLayout) {
      dc->DrawTextLayout (getTL (2.f), textLayout, mWindow->getBlackBrush());
      dc->DrawTextLayout (getTL(), textLayout, mWindow->getWhiteBrush());
      } 
    textLayout->Release();
    }
  //}}}

private:
  //{{{
  std::string getFrameStr (uint32_t frame) {

    uint32_t frameHs = (frame * mSong.mSamplesPerFrame) / (mSong.mSampleRate / 100);

    uint32_t hs = frameHs % 100;

    frameHs /= 100;
    uint32_t secs = frameHs % 60;

    frameHs /= 60;
    uint32_t mins = frameHs % 60;

    frameHs /= 60;
    uint32_t hours = frameHs % 60;

    std::string str (hours ? (dec (hours) + ':' + dec (mins, 2, '0')) : dec (mins));
    return str + ':' + dec(secs, 2, '0') + ':' + dec(hs, 2, '0');
    }
  //}}}

  cSong& mSong;

  IDWriteTextFormat* mTextFormat = nullptr;
  };
//}}}
//{{{
class cSongSpectrumBox : public cD2dWindow::cBox {
public:
  //{{{
  cSongSpectrumBox (cD2dWindow* window, float width, float height, cSong& song) :
      cBox("songSpectrumBox", window, width, height), mSong(song) {

    mPin = true;
    //mTransform = D2D1::Matrix3x2F::Scale ({2.0f, 2.0f}, {0.f, 0.f});
    }
  //}}}
  //{{{
  virtual ~cSongSpectrumBox() {
    if (mBitmap)
      mBitmap->Release();
    free (mBitmapBuf);
    }
  //}}}

  void onDraw (ID2D1DeviceContext* dc) {
    if (!mBitmap || (mBitmapWidth != getWidthInt()) || (mBitmapHeight = getHeightInt())) {
      //{{{  create bitmapBuf and ID2D1Bitmap matching box size
      mBitmapWidth = getWidthInt();
      mBitmapHeight = getHeightInt();

      free (mBitmapBuf);
      mBitmapBuf = (uint8_t*)malloc (mBitmapWidth * mBitmapHeight);

      if (mBitmap)
        mBitmap->Release();
      dc->CreateBitmap (D2D1::SizeU (mBitmapWidth, mBitmapHeight),
                        { DXGI_FORMAT_A8_UNORM, D2D1_ALPHA_MODE_STRAIGHT, 0,0 },
                        &mBitmap);
      }
      //}}}

    // left and right edge frames, may be outside known frames
    auto leftFrame = mSong.mPlayFrame - (getWidthInt()/2);
    auto rightFrame = mSong.mPlayFrame + (getWidthInt()/2);

    // first and last known frames
    auto firstFrame = max (leftFrame, 0);
    auto lastFrame = min (rightFrame, mSong.getNumParsedFrames());

    // bottom of first bitmapBuf column
    auto dstPtr = mBitmapBuf + ((mBitmapHeight-1) * mBitmapWidth);
    for (int frame = firstFrame; frame < lastFrame; frame++) {
      // copy freqLuma to bitmapBuf column
      auto srcPtr = mSong.mFrames[frame].mFreqLuma;
      for (int y = 0; y < mBitmapHeight; y++) {
        *dstPtr = *srcPtr++;
        dstPtr -= mBitmapWidth;
        }

      // bottom of next bitmapBuf column
      dstPtr += (mBitmapWidth * mBitmapHeight) + 1;
      }

    // copy bitmapBuf to ID2D1Bitmap
    mBitmap->CopyFromMemory (&D2D1::RectU (0, 0, lastFrame - firstFrame, mBitmapHeight), mBitmapBuf, mBitmapWidth);

    // stamp colour through ID2D1Bitmap alpha using offset and width
    float dstLeft = (firstFrame > leftFrame) ? float(firstFrame - leftFrame) : 0.f;
    //dc->SetTransform (mTransform);
    dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_ALIASED);
    dc->FillOpacityMask (mBitmap, mWindow->getWhiteBrush(),
                         &D2D1::RectF (dstLeft, mRect.top, dstLeft + lastFrame - firstFrame, mRect.bottom), // dstRect
                         &D2D1::RectF (0.f,0.f, float(lastFrame - firstFrame), (float)mBitmapHeight));      // srcRect
    dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    //dc->SetTransform (D2D1::Matrix3x2F::Identity());
    }

private:
  cSong& mSong;

  int mBitmapWidth = 0;
  int mBitmapHeight = 0;

  uint8_t* mBitmapBuf = nullptr;
  ID2D1Bitmap* mBitmap = nullptr;

  D2D1::Matrix3x2F mTransform = D2D1::Matrix3x2F::Identity();
  };
//}}}
