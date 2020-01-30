// cSongBoxes.h
#pragma once

#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

#include "../../shared/utils/cSong.h"

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
      int clicks = -delta / 120;

      if (mZoomDenominator > 1)
        mZoomDenominator += clicks;
      else if (mZoomNumerator > 1)
        mZoomNumerator -= clicks;
      else if (mZoomDenominator == 1)
        mZoomNumerator -= clicks;
      else if (mZoomNumerator == 1)
        mZoomDenominator += clicks;

      mZoomNumerator = std::max (mZoomNumerator, 1);
      mZoomDenominator = std::min (std::max (mZoomDenominator, 1),
                                   2 * (1 + mSong.getTotalFrames() / getWidthInt()));
      return true;
      }

    return false;
    }
  //}}}
  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    auto playFrame = mSong.getPlayFrame();
    auto leftFrame = playFrame - ((mZoomDenominator * getWidthInt() / 2)) / mZoomNumerator;
    auto rightFrame = playFrame + ((mZoomDenominator * getWidthInt() / 2)) / mZoomNumerator;
    auto firstX = (leftFrame < 0) ? (((-leftFrame) / mZoomDenominator) * mZoomNumerator) : 0;

    draw (dc, playFrame, leftFrame, rightFrame, firstX, mZoomNumerator, mZoomDenominator);
    }
  //}}}

protected:
  //{{{
  void draw (ID2D1DeviceContext* dc, int playFrame, int leftFrame, int rightFrame, int firstX,
             int zoomNumerator, int zoomDenominator) {

    bool unity = (zoomNumerator == 1) && (zoomDenominator == 1);
    bool zoomOut = (zoomNumerator == 1) && (zoomDenominator > 1);
    bool zoomIn = (zoomNumerator > 1) && (zoomDenominator == 1);

    // clip left right to valid frames
    leftFrame = std::max (0, leftFrame);
    rightFrame = std::min (rightFrame, (int)mSong.getNumFrames());

    auto colour = mWindow->getBlueBrush();
    float valueScale = getHeight() / 2.f / mSong.getMaxPowerValue();
    cRect r (mRect.left + firstX, 0.f, 0.f, 0.f);

    auto frame = leftFrame;
    while (frame < rightFrame) {
      r.right = r.left + zoomNumerator;

      if (mSong.mFrames[frame]->hasTitle()) {
        //{{{  draw song title yellow bar and text
        dc->FillRectangle (cRect (r.left, getCentreY() - (getHeight() / 2.f),
                                  r.right + 2.f, getCentreY() + (getHeight() / 2.f)),
                           mWindow->getYellowBrush());

        auto str = mSong.mFrames[frame]->getTitle();
        IDWriteTextLayout* textLayout;
        mWindow->getDwriteFactory()->CreateTextLayout (
          std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
          mWindow->getTextFormat(), getWidth(), getHeight(), &textLayout);
        if (textLayout) {
          dc->DrawTextLayout (cPoint (r.left, getTL().y - 20.f), textLayout, mWindow->getWhiteBrush());
          textLayout->Release();
          }
        }
        //}}}
      if (mSong.mFrames[frame]->isSilent()) {
        //{{{  draw red silent frame
        r.top = getCentreY() - 2.f;
        r.bottom = getCentreY() + 2.f;
        dc->FillRectangle (r, mWindow->getRedBrush());
        }
        //}}}

      if (frame > playFrame)
        colour = mWindow->getGreyBrush();

      float leftValue = 0.f;
      float rightValue = 0.f;

      if (unity) {
        //{{{  simple
        if (frame == playFrame)
          colour = mWindow->getWhiteBrush();

        auto powerValues = mSong.mFrames[frame]->getPowerValues();
        leftValue = *powerValues++;
        rightValue = *powerValues;
        }
        //}}}
      else if (zoomOut) {
        //{{{  summed
        int firstSumFrame = frame - (frame % zoomDenominator);
        int nextSumFrame = firstSumFrame + zoomDenominator;

        for (auto i = firstSumFrame; i < firstSumFrame + zoomDenominator; i++) {
          // !!!must clip i to valid frames !!!!
          auto powerValues = mSong.mFrames[i]->getPowerValues();
          if (i == playFrame) {
            colour = mWindow->getWhiteBrush();
            leftValue = *powerValues++;
            rightValue = *powerValues;
            break;
            }

          leftValue += *powerValues++ / zoomDenominator;
          rightValue += *powerValues / zoomDenominator;
          }
        }
        //}}}
      else if (zoomIn) {
        //{{{  repeated
        int firstSumFrame = frame - (frame % zoomDenominator);
        int nextSumFrame = firstSumFrame + zoomDenominator;

        for (auto i = firstSumFrame; i < firstSumFrame + zoomDenominator; i++) {
          // !!!must clip i to valid frames !!!!
          auto powerValues = mSong.mFrames[i]->getPowerValues();
          if (i == playFrame) {
            colour = mWindow->getWhiteBrush();
            leftValue = *powerValues++;
            rightValue = *powerValues;
            break;
            }

          leftValue += *powerValues++ / zoomDenominator;
          rightValue += *powerValues / zoomDenominator;
          }
        }
        //}}}

      r.top = getCentreY() - (leftValue * valueScale);
      r.bottom = getCentreY() + (rightValue * valueScale);
      dc->FillRectangle (r, colour);

      r.left = r.right;
      frame += zoomDenominator;
      }
    }
  //}}}

  cSong& mSong;
  int mZoomNumerator = 1;
  int mZoomDenominator = 1;
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
    free (mSummedPowerValues);
    }
  //}}}

  //{{{
  void layout() {
    mSummedNumFrames = -1;
    cSongWaveBox::layout();
    }
  //}}}

  //{{{
  bool onDown (bool right, cPoint pos)  {

    mOn = true;

    auto frame = int((pos.x * mSong.getTotalFrames()) / getWidth());
    mSong.setPlayFrame (frame);

    return true;
    }
  //}}}
  //{{{
  bool onMove (bool right, cPoint pos, cPoint inc) {
    auto frame = int((pos.x * mSong.getTotalFrames()) / getWidth());
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

    auto playFrame = mSong.getPlayFrame();
    if (mOn) {
      if (mLens < getWidthInt() / 16)
        // animate on
        mLens += (getWidthInt() / 16) / 6;
      }
    else if (mLens <= 0) {
      mLens = 0;
      draw (dc, playFrame, 0, mSummedMaxX);
      return;
      }
    else // animate off
      mLens /= 2;

    int curFrameX = (mSong.getTotalFrames() > 0) ? (playFrame * getWidthInt()) / mSong.getTotalFrames() : 0;
    int leftLensX = curFrameX - mLens;
    int rightLensX = curFrameX + mLens;
    if (leftLensX < 0) {
      rightLensX -= leftLensX;
      leftLensX = 0;
      }
    else
      draw (dc, playFrame, 0, leftLensX);

    if (rightLensX > getWidthInt()) {
      leftLensX -= rightLensX - getWidthInt();
      rightLensX = getWidthInt();
      }
    else
      draw (dc, playFrame, rightLensX, getWidthInt());

    cSongWaveBox::draw (dc, playFrame, playFrame - mLens, playFrame + mLens-2, leftLensX+1, 1, 1);

    dc->DrawRectangle (cRect(mRect.left + leftLensX, mRect.top + 1.f,
                             mRect.left + rightLensX, mRect.top + getHeight() - 1.f),
                       mWindow->getYellowBrush(), 1.f);
    }
  //}}}

private:
  //{{{
  void updateSummedPowerValues() {

    if (mSummedNumFrames != mSong.getNumFrames()) {
      // song changed, cache values summed to width, scaled to height
      mSummedNumFrames = mSong.getNumFrames();
      mSummedPowerValues = (float*)realloc (mSummedPowerValues, getWidthInt() * mSong.getNumChannels() * sizeof(float));

      mSummedMaxX = 0;
      int startSummedFrame = 0;
      auto summedPowerValues = mSummedPowerValues;
      for (auto x = 0; x < getWidthInt(); x++) {
        int frame = x * mSong.getTotalFrames() / getWidthInt();
        if (frame >= mSong.getNumFrames())
          break;

        float* powerValues = mSong.mFrames[frame]->getPowerValues();
        float lValue = powerValues[0];
        float rValue = powerValues[1];
        if (frame > startSummedFrame) {
          int numSummedPowerValues = 1;
          for (auto i = startSummedFrame; i < frame; i++) {
            auto powerValues = mSong.mFrames[i]->getPowerValues();
            lValue += powerValues[0];
            rValue += powerValues[1];
            numSummedPowerValues++;
            }
          lValue /= numSummedPowerValues;
          rValue /= numSummedPowerValues;
          }
        *summedPowerValues++ = lValue;
        *summedPowerValues++ = rValue;

        mSummedMaxX = x;
        startSummedFrame = frame + 1;
        }
      }
    }
  //}}}
  //{{{
  void draw (ID2D1DeviceContext* dc, int playFrame, int firstX, int lastX) {

    updateSummedPowerValues();

    float curFrameX = mRect.left;
    if (mSong.getTotalFrames() > 0)
      curFrameX += (playFrame * getWidth()) / mSong.getTotalFrames();

    auto colour = mWindow->getBlueBrush();
    float valueScale = getHeight() / 2.0f / mSong.getMaxPowerValue();
    auto summedPowerValues = mSummedPowerValues + (firstX * mSong.getNumChannels());

    cRect r (mRect.left + firstX, 0.f,0.f,0.f);
    for (auto x = firstX; x < lastX; x++) {
      r.right = r.left + 1.f;

      float leftValue = *summedPowerValues++;
      float rightValue = *summedPowerValues++;
      if (x >= curFrameX) {
        if (x == curFrameX) {
          // override with playFrame in white
          auto powerValues = mSong.mFrames[playFrame]->getPowerValues();
          leftValue = *powerValues++;
          rightValue = *powerValues;
          colour = mWindow->getWhiteBrush();
          }
        else
          colour = mWindow->getGreyBrush();
        }

      r.top = getCentreY() - (leftValue * valueScale) - 2.f;
      r.bottom = getCentreY() + (rightValue * valueScale) + 2.f;
      dc->FillRectangle (r, colour);
      r.left = r.right;
      }
    }
  //}}}

  float* mSummedPowerValues = nullptr;
  int mSummedNumFrames = -1;
  int mSummedMaxX = 0;

  bool mOn = false;
  int mLens = 0;
  };
//}}}

//{{{
class cSongFreqBox : public cD2dWindow::cBox {
public:
  cSongFreqBox (cD2dWindow* window, float width, float height, cSong& song) :
      cBox ("songWaveBox", window, width, height), mSong(song) {
    mPin = true;
    mPickable = false;
    }
  virtual ~cSongFreqBox() {}

  void onDraw (ID2D1DeviceContext* dc) {
    auto frame = mSong.getPlayFrame();
    if (frame > 0) {
      float* freq = (mSong.mFrames[frame]->getFreqValues());
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
class cSongSpecBox : public cD2dWindow::cBox {
public:
  //{{{
  cSongSpecBox (cD2dWindow* window, float width, float height, cSong& song) :
      cBox("songSpectrumBox", window, width, height), mSong(song) {

    mPin = true;
    //mTransform = D2D1::Matrix3x2F::Scale ({2.0f, 2.0f}, {0.f, 0.f});
    }
  //}}}
  //{{{
  virtual ~cSongSpecBox() {
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
    auto playFrame = mSong.getPlayFrame();
    auto leftFrame = playFrame - (getWidthInt()/2);
    auto rightFrame = playFrame + (getWidthInt()/2);

    // first and last known frames
    auto firstFrame = std::max (leftFrame, 0);
    auto lastFrame = std::min (rightFrame, mSong.getNumFrames());

    // bottom of first bitmapBuf column
    auto dstPtr = mBitmapBuf + ((mBitmapHeight-1) * mBitmapWidth);
    for (int frame = firstFrame; frame < lastFrame; frame++) {
      // copy freqLuma to bitmapBuf column
      auto srcPtr = mSong.mFrames[frame]->getFreqLuma();
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

  //D2D1::Matrix3x2F mTransform = D2D1::Matrix3x2F::Identity();
  };
//}}}

//{{{
class cSongTimeBox : public cD2dWindow::cBox {
public:
  //{{{
  cSongTimeBox (cD2dWindow* window, float width, float height, cSong& song) :
      cBox("songTimeBox", window, width, height), mSong(song) {

    mWindow->getDwriteFactory()->CreateTextFormat (L"Consolas", NULL,
      DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 50.f, L"en-us",
      &mTextFormat);
    mTextFormat->SetTextAlignment (DWRITE_TEXT_ALIGNMENT_TRAILING);

    mPin = true;
    }
  //}}}
  //{{{
  virtual ~cSongTimeBox() {
    mTextFormat->Release();
    }
  //}}}

  void onDraw (ID2D1DeviceContext* dc) {
    std::string str = getFrameStr (mSong.getPlayFrame()) + " " + getFrameStr (mSong.getTotalFrames());

    IDWriteTextLayout* textLayout;
    mWindow->getDwriteFactory()->CreateTextLayout (
      std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
      mTextFormat, getWidth(), getHeight(), &textLayout);
    if (textLayout) {
      dc->DrawTextLayout (getTL (2.f), textLayout, mWindow->getBlackBrush());
      dc->DrawTextLayout (getTL(), textLayout, mWindow->getWhiteBrush());
      }
    textLayout->Release();
    }

private:
  //{{{
  std::string getFrameStr (uint32_t frame) {

    if (mSong.getSamplesPerFrame() && mSong.getSampleRate()) {
      uint32_t frameHs = (frame * mSong.getSamplesPerFrame()) / (mSong.getSampleRate() / 100);

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
    else
      return ("--:--:--");
    }
  //}}}

  cSong& mSong;

  IDWriteTextFormat* mTextFormat = nullptr;
  };
//}}}
