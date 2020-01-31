// cSongBoxes.h
#pragma once

#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include "../../shared/utils/cSong.h"

//{{{
class cSongBox : public cD2dWindow::cBox {
public:
  //{{{
  cSongBox (cD2dWindow* window, float width, float height, float waveHeight, cSong& song) :
      cBox("songBox", window, width, height), mSong(song), mWaveHeight(waveHeight) {

    mPin = true;
    }
  //}}}
  //{{{
  virtual ~cSongBox() {
    if (mBitmap)
      mBitmap->Release();
    free (mBitmapBuf);
    }
  //}}}

  //{{{
  bool onMove (bool right, cPoint pos, cPoint inc) {
    mSong.incPlayFrame (int(-inc.x));
    return true;
    }
  //}}}
  //{{{
  bool onWheel (int delta, cPoint pos)  {

    if (getShow()) {
      mZoomIndex -= delta / 120;
      mZoomIndex = std::min (std::max (mZoomIndex, mSong.getMinZoomIndex()), mSong.getMaxZoomIndex());
      return true;
      }

    return false;
    }
  //}}}

  //{{{
  void onDraw (ID2D1DeviceContext* dc) {
  // draw frames centred at playFrame -/+ width in pixels, centred at centreX, zoomed by mZoomIndex

    auto playFrame = mSong.getPlayFrame();

    bool drawWaveform = mWaveHeight > 0.f;
    bool drawSpectrum = mWaveHeight < getHeight();
    int frameStep = (mZoomIndex > 0) ? mZoomIndex+1 : 1; // zoomOut summing frameStep frames per pix
    int frameWidth = (mZoomIndex < 0) ? -mZoomIndex+1 : 1; // zoomIn expanding frame to frameWidth pix

    resizeBitmap (dc);

    // calc leftFrame, clip to valid frame, adjust firstX which may overlap left up to frameWidth
    auto leftFrame = playFrame - (((getWidthInt() + frameWidth) / 2) * frameStep) / frameWidth;
    auto rightFrame = playFrame + (((getWidthInt() + frameWidth) / 2) * frameStep) / frameWidth;

    auto firstFrame = leftFrame;
    int firstX = (getWidthInt()/2) - (((playFrame - leftFrame) * frameWidth) / frameStep) - (frameWidth/2);
    if (firstFrame < 0) {
      firstX += (-firstFrame * frameWidth) / frameStep;
      firstFrame = 0;
      }
    auto lastFrame = std::min (rightFrame, mSong.getLastFrame());
    //cLog::log (LOGINFO, "leftFrame:%d firstx:%d width:%d", leftFrame, firstX, frameWidth);

    auto colour = mWindow->getBlueBrush();

    cRect r (mRect.left + firstX, 0.f, 0.f, 0.f);
    float waveCentreY = mRect.bottom - mWaveHeight/2.f;
    float valueScale = (mWaveHeight/2.f) / mSong.getMaxPowerValue();

    auto dstPtr = mBitmapBuf + ((mSpectrumHeight-1) * mBitmapAllocated.width);
    int dstWidth = 0;

    auto frame = firstFrame;
    while ((r.left < mRect.right) && (frame <= lastFrame)) {
      r.right = r.left + frameWidth;

      if (drawWaveform) {
        //{{{  draw frame waveform
        if (mSong.mFrames[frame]->hasTitle()) {
          //{{{  draw song title yellow bar and text
          dc->FillRectangle (cRect (r.left, waveCentreY - (getHeight() / 2.f),
                                    r.right + 2.f, waveCentreY + (getHeight() / 2.f)),
                             mWindow->getYellowBrush());

          auto str = mSong.mFrames[frame]->getTitle();
          IDWriteTextLayout* textLayout;
          mWindow->getDwriteFactory()->CreateTextLayout (
            std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
            mWindow->getTextFormat(), getWidth(), getHeight(), &textLayout);
          if (textLayout) {
            dc->DrawTextLayout (cPoint (r.left, getBL().y - 120.f), textLayout, mWindow->getWhiteBrush());
            textLayout->Release();
            }
          }
          //}}}
        if (mSong.mFrames[frame]->isSilent()) {
          //{{{  draw red silent frame
          r.top = waveCentreY - 2.f;
          r.bottom = waveCentreY + 2.f;
          dc->FillRectangle (r, mWindow->getRedBrush());
          }
          //}}}

        if (frame > playFrame)
          colour = mWindow->getGreyBrush();

        float leftValue = 0.f;
        float rightValue = 0.f;

        if (mZoomIndex <= 0) {
          // no zoom, or zoomIn expanding frame
          if (frame == playFrame)
            colour = mWindow->getWhiteBrush();
          auto powerValues = mSong.mFrames[frame]->getPowerValues();
          leftValue = *powerValues++;
          rightValue = *powerValues;
          }
        else {
          // zoomOut, summing frames
          int firstSumFrame = frame - (frame % frameStep);
          int nextSumFrame = firstSumFrame + frameStep;
          for (auto i = firstSumFrame; i < firstSumFrame + frameStep; i++) {
            auto powerValues = mSong.mFrames[std::min (i, lastFrame)]->getPowerValues();
            if (i == playFrame) {
              colour = mWindow->getWhiteBrush();
              leftValue = *powerValues++;
              rightValue = *powerValues;
              break;
              }
            leftValue += *powerValues++ / frameStep;
            rightValue += *powerValues / frameStep;
            }
          }

        r.top = waveCentreY - (leftValue * valueScale);
        r.bottom = waveCentreY + (rightValue * valueScale);
        dc->FillRectangle (r, colour);
        }
        //}}}
      if (drawSpectrum) {
        //{{{  draw frame bitmap column
        for (int i = 0; i < frameWidth; i++) {
          //if (dstWidth > mBitmapWidth)
          //  break;
          auto srcPtr = mSong.mFrames[frame]->getFreqLuma();
          for (UINT32 y = 0; y < mSpectrumHeight; y++) {
            *dstPtr = *srcPtr++;
            dstPtr -= mBitmapAllocated.width;
            }

          // bottom of next bitmapBuf column
          dstPtr += (mBitmapAllocated.width * mSpectrumHeight) + 1;
          dstWidth++;
          }
        }
        //}}}

      r.left = r.right;
      frame += frameStep;
      }

    if (mWaveHeight < getHeight()) {
      //{{{  stamp bitmap
      // copy spectrum part of bitmapBuf to ID2D1Bitmap
      mBitmap->CopyFromMemory (&D2D1::RectU (0, 0, dstWidth, mSpectrumHeight), mBitmapBuf, mBitmapAllocated.width);

      // stamp colour through spectrum part of ID2D1Bitmap alpha
      float dstLeft = (firstFrame > leftFrame) ? float(firstFrame - leftFrame) : 0.f;
      dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_ALIASED);

      dc->FillOpacityMask (mBitmap, mWindow->getWhiteBrush(),
        &D2D1::RectF (dstLeft, mRect.top,
                      dstLeft + (float)dstWidth,
                      mRect.top + (float)mSpectrumHeight), // dstRect
        &D2D1::RectF (0.f,0.f,
                     (float)dstWidth,
                     (float)mSpectrumHeight));  // srcRect

      dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
      }
      //}}}
    }
  //}}}

private:
  //{{{
  void resizeBitmap (ID2D1DeviceContext* dc) {
  //  create bitmapBuf and ID2D1Bitmap matching box size

    if (mWaveHeight < getHeight()) {
      // want spectrum
      if (!mBitmap ||
          (getWidthInt() > (int)mBitmapAllocated.width) ||
          (getHeightInt() > (int)mBitmapAllocated.height)) {
        // allocate or reallocate heap:mBitmapBuf and D2D:mBitmap
        free (mBitmapBuf);
        mBitmapBuf = (uint8_t*)calloc (1, getWidthInt() * getHeightInt());

        if (mBitmap)
          mBitmap->Release();

        mBitmapAllocated = { (UINT32)getWidthInt(), (UINT32)getHeightInt() };
        mSpectrumHeight = (UINT32)getHeightInt() - UINT32(mWaveHeight);

        dc->CreateBitmap (mBitmapAllocated,
                          { DXGI_FORMAT_A8_UNORM, D2D1_ALPHA_MODE_STRAIGHT, 0,0 },
                          &mBitmap);
        }
      }
    }
  //}}}

  cSong& mSong;
  int mZoomIndex = 0;
  float mWaveHeight = 0.f;

  D2D1_SIZE_U mBitmapAllocated = { 0,0 };
  UINT32 mSpectrumHeight = 0;
  uint8_t* mBitmapBuf = nullptr;
  ID2D1Bitmap* mBitmap = nullptr;
  };
//}}}
//{{{
class cSongLensBox : public cD2dWindow::cBox {
public:
  //{{{
  cSongLensBox (cD2dWindow* window, float width, float height, cSong& song)
    : cBox ("songLensBox", window, width, height), mSong(song) {}
  //}}}
  //{{{
  virtual ~cSongLensBox() {
    free (mSummedPowerValues);
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
    mSong.setPlayFrame (int((pos.x * mSong.getTotalFrames()) / getWidth()));
    return true;
    }
  //}}}
  //{{{
  bool onUp (bool right, bool mouseMoved, cPoint pos) {
    mOn = false;
    return true;
    }
  //}}}
  //{{{
  bool onWheel (int delta, cPoint pos)  {
    return false;
    }
  //}}}

  //{{{
  void layout() {
    int mSummedNumFrames = -1;
    cD2dWindow::cBox::layout();
    }
                                               //}}}  s

  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    auto playFrame = mSong.getPlayFrame();
    int playFrameX = (mSong.getTotalFrames() > 0) ? (playFrame * getWidthInt()) / mSong.getTotalFrames() : 0;

    updateSummedPowerValues();

    if (mOn) {
      if (mLens < getWidthInt() / 16)
        // animate on
        mLens += (getWidthInt() / 16) / 6;
      }
    else if (mLens <= 1) {
      mLens = 0;
      drawSummed (dc, playFrame, playFrameX, 0, mSummedMaxX);
      return;
      }
    else // animate off
      mLens /= 2;

    // calc lensCentreX, adjust lensCentreX to show all of lens
    int lensCentreX = playFrameX;
    if (lensCentreX - mLens < 0)
      lensCentreX = mLens;
    else if (lensCentreX + mLens > getWidthInt())
      lensCentreX = getWidthInt() - mLens;

    if (lensCentreX > mLens) // draw left of lens
      drawSummed (dc, playFrame, playFrameX, 0, lensCentreX - mLens);

    // draw lens
    drawLens (dc, playFrame, mLens-1, lensCentreX);

    if (lensCentreX < getWidthInt() - mLens) // draw right of lens
      drawSummed (dc, playFrame, playFrameX, lensCentreX + mLens, getWidthInt());

    dc->DrawRectangle (cRect(mRect.left + lensCentreX - mLens, mRect.top + 1.f,
                             mRect.left + lensCentreX + mLens, mRect.top + getHeight() - 1.f),
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
  void drawSummed (ID2D1DeviceContext* dc, int playFrame, int playFrameX, int firstX, int lastX) {

    auto colour = mWindow->getBlueBrush();
    float valueScale = getHeight() / 2.0f / mSong.getMaxPowerValue();
    auto summedPowerValues = mSummedPowerValues + (firstX * mSong.getNumChannels());

    cRect r (mRect.left + firstX, 0.f,0.f,0.f);
    for (auto x = firstX; x < lastX; x++) {
      r.right = r.left + 1.f;

      float leftValue = *summedPowerValues++;
      float rightValue = *summedPowerValues++;
      if (x >= playFrameX) {
        if (x == playFrameX) {
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
  //{{{
  void drawLens (ID2D1DeviceContext* dc, int playFrame, int width, int centreX) {
  // draw frames centred at playFrame -/+ width in pixels, centred at centreX, zoomed by zoomIndex

    // calc leftmost frame, clip to valid frame, adjust firstX which may overlap left up to frameWidth
    auto leftFrame = playFrame - width;
    int firstX = centreX - (playFrame - leftFrame);
    if (leftFrame < 0) {
      firstX += -leftFrame;
      leftFrame = 0;
      }

    auto colour = mWindow->getBlueBrush();
    float valueScale = getHeight() / 2.f / mSong.getMaxPowerValue();
    cRect r (mRect.left + firstX, 0.f, 0.f, 0.f);

    auto frame = leftFrame;
    auto rightFrame = playFrame + width;
    auto lastFrame = std::min (rightFrame, mSong.getLastFrame());
    while (r.left < mRect.right && frame <= lastFrame) {
      r.right = r.left + 1.f;

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

      if (frame == playFrame)
        colour = mWindow->getWhiteBrush();
      auto powerValues = mSong.mFrames[frame]->getPowerValues();
      leftValue = *powerValues++;
      rightValue = *powerValues;

      r.top = getCentreY() - (leftValue * valueScale);
      r.bottom = getCentreY() + (rightValue * valueScale);
      dc->FillRectangle (r, colour);

      r.left = r.right;
      frame++;
      }
    }
  //}}}

  cSong& mSong;

  float* mSummedPowerValues = nullptr;
  int mSummedNumFrames = -1;
  int mSummedMaxX = 0;

  bool mOn = false;
  int mLens = 0;
  int mZoomIndex = 0;
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
