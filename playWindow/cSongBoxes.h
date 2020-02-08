// cSongBoxes.h
#pragma once

#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include "cSong.h"

class cSongBox : public cD2dWindow::cBox {
public:
  //{{{
  cSongBox (cD2dWindow* window, float width, float height, cSong& song) :
      cBox("songBox", window, width, height), mSong(song) {

    mPin = true;

    mWindow->getDwriteFactory()->CreateTextFormat (L"Consolas", NULL,
      DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 50.f, L"en-us",
      &mTextFormat);
    mTextFormat->SetTextAlignment (DWRITE_TEXT_ALIGNMENT_TRAILING);
    }
  //}}}
  //{{{
  virtual ~cSongBox() {

    mTextFormat->Release();

    if (mSpecBitmap)
      mSpecBitmap->Release();

    free (mSummedPowerValues);
    }
  //}}}

  //{{{
  bool onDown (bool right, cPoint pos)  {

    if (pos.y > mLensY) {
      mOn = true;
      auto frame = int((pos.x * mSong.getTotalFrames()) / getWidth());
      mSong.setPlayFrame (frame);
      }

    return true;
    }
  //}}}
  //{{{
  bool onMove (bool right, cPoint pos, cPoint inc) {

    if (pos.y > mLensY)
      mSong.setPlayFrame (int((pos.x * mSong.getTotalFrames()) / getWidth()));
    else
      mSong.incPlayFrame (int(-inc.x));

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

    if (getShow()) {
      mZoom = std::min (std::max (mZoom - (delta/120), mSong.getMinZoomIndex()), mSong.getMaxZoomIndex());
      return true;
      }

    return false;
    }
  //}}}

  //{{{
  void layout() {

    mSummedNumFrames = -1;
    cD2dWindow::cBox::layout();

    mWaveY = getHeight() - 200.f;
    mLensY = getHeight() - 100.f;
    }
  //}}}

  //{{{
  void onDraw (ID2D1DeviceContext* dc) {
  // draw frames centred at playFrame

    auto playFrame = mSong.getPlayFrame();

    int frameStep = (mZoom > 0) ? mZoom+1 : 1; // zoomOut summing frameStep frames per pix
    int frameWidth = (mZoom < 0) ? -mZoom+1 : 1; // zoomIn expanding frame to frameWidth pix

    resizeBitmap (dc);

    mCompatibleRenderTarget->BeginDraw();
    mCompatibleRenderTarget->Clear (D2D1::ColorF (D2D1::ColorF::Black));

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

    auto colour = mWindow->getBlueBrush();

    float dstFirstX = mRect.left + firstX;
    cRect r (dstFirstX, 0.f, 0.f, 0.f);
    float centreY = mRect.top + ((mWaveY + mLensY) / 2.f);
    float valueScale = (mLensY - mWaveY) / 2.f / mSong.getMaxPowerValue();

    int dstBitmapX = 0;
    int dstBitmapWidth = 0;
    for (auto frame = firstFrame; (frame < lastFrame) && (r.left < mRect.right); frame += frameStep) {
      r.right = r.left + frameWidth;
      if (mSong.mFrames[frame]->hasTitle()) {
        //{{{  draw song title yellow bar and text
        dc->FillRectangle (cRect (r.left, centreY - ((mLensY - mWaveY)/2.f),
                                  r.right + 2.f, centreY + ((mLensY - mWaveY)/2.f)), mWindow->getYellowBrush());

        auto str = mSong.mFrames[frame]->getTitle();

        IDWriteTextLayout* textLayout;
        mWindow->getDwriteFactory()->CreateTextLayout (
          std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
          mWindow->getTextFormat(), getWidth(), getHeight(), &textLayout);

        if (textLayout) {
          dc->DrawTextLayout (cPoint (r.left, getBL().y - (mLensY - mWaveY) - 20.f), textLayout, mWindow->getWhiteBrush());
          textLayout->Release();
          }
        }
        //}}}
      if (mSong.mFrames[frame]->isSilent()) {
        //{{{  draw red silent frame
        r.top = centreY - 2.f;
        r.bottom = centreY + 2.f;
        dc->FillRectangle (r, mWindow->getRedBrush());
        }
        //}}}
      //{{{  draw frame waveform
      if (frame > playFrame)
        colour = mWindow->getGreyBrush();

      float leftValue = 0.f;
      float rightValue = 0.f;

      if (mZoom <= 0) {
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

      r.top = centreY - (leftValue * valueScale);
      r.bottom = centreY + (rightValue * valueScale);
      dc->FillRectangle (r, colour);

      cRect rr (r.left, r.top - mRect.top, r.right, r.bottom - mRect.top);
      mCompatibleRenderTarget->FillRectangle (rr, colour);
      //}}}

      //{{{  draw frame bitmap column
      if (!mFramesValid) {
        mFramesValid = true;
        mFirstFrameIndex = 0;
        mFirstFrame = frame;
        mLastFrame = frame;
        mSpecBitmap->CopyFromMemory (&D2D1::RectU (mFirstFrameIndex, 0, mFirstFrameIndex+1, mSpecBitmapSize.height),
                                     mSong.mFrames[frame]->getFreqLuma(), 1);
        }
      else if ((frame >= mFirstFrame) && (frame <= mLastFrame)) {
        // already in bit
        }
      else if (frame == mLastFrame + 1) {
        auto index = (mFirstFrameIndex + (frame - mFirstFrame)) % mSpecBitmapSize.width;
        mSpecBitmap->CopyFromMemory (&D2D1::RectU (index, 0, index+1, mSpecBitmapSize.height),
                                     mSong.mFrames[frame]->getFreqLuma(), 1);
        mLastFrame = frame;
        if (index == mFirstFrameIndex) {
          mFirstFrame++;
          mFirstFrameIndex = (mFirstFrameIndex + 1) % mSpecBitmapSize.width;
          }
        }
      //}}}

      dstBitmapWidth += frameWidth;
      r.left = r.right;
      }
    mCompatibleRenderTarget->EndDraw();

    if (dstBitmapWidth) {
      //{{{  stamp bitmap
      // stamp colour through used part of ID2D1Bitmap alpha
      dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_ALIASED);

      if (mFirstFrameIndex + dstBitmapWidth <= mSpecBitmapSize.width)
        dc->FillOpacityMask (mSpecBitmap, mWindow->getWhiteBrush(),
          &D2D1::RectF (dstFirstX, mRect.top, dstFirstX + dstBitmapWidth, mRect.top + (float)mSpecBitmapSize.height),
          &D2D1::RectF (0.f,0.f, (float)dstBitmapWidth, (float)mSpecBitmapSize.height));

      else {
        float firstFrameIndex = (float)mFirstFrameIndex;
        float firstStamp = mSpecBitmapSize.width - mFirstFrameIndex;
        float secondStamp = dstBitmapWidth - firstStamp;

        dc->FillOpacityMask (mSpecBitmap, mWindow->getGreenBrush(),
          &D2D1::RectF (dstFirstX, mRect.top, dstFirstX + firstStamp, mRect.top + (float)mSpecBitmapSize.height),
          &D2D1::RectF (mFirstFrameIndex,0.f, mFirstFrameIndex + firstStamp, (float)mSpecBitmapSize.height));

        dc->FillOpacityMask (mSpecBitmap, mWindow->getYellowBrush(),
          &D2D1::RectF (dstFirstX + firstStamp, mRect.top,
                        dstFirstX + firstStamp + secondStamp, mRect.top + (float)mSpecBitmapSize.height),
          &D2D1::RectF (0.f,0.f, secondStamp, (float)mSpecBitmapSize.height));
        }
          D2D1_SIZE_F sizeF = D2D1::SizeF (getWidthInt(), mLensY - mWaveY);

      auto hr = mCompatibleRenderTarget->GetBitmap (&mWaveBitmap);
      dc->FillOpacityMask (mWaveBitmap, mWindow->getWhiteBrush(),
        &D2D1::RectF (mRect.left, mRect.top + mWaveY, (mRect.left + getWidthInt(), mRect.top + mLensY - mWaveY)),
        &D2D1::RectF (0.f,0.f, (float)getWidthInt(), mLensY - mWaveY));

      dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
      }
      //}}}
    //cLog::log (LOGINFO, "left:%d firstx:%d w:%d % bw:%d", leftFrame, firstX, frameWidth, dstBitmapWidth);
    //{{{  on screen debug
    auto str = "i:" + dec (mFirstFrameIndex) + " bit:" + dec (mFirstFrame) + ":" + dec(mLastFrame) +
               " draw:" + dec (firstFrame) + ":" + dec(lastFrame);
    IDWriteTextLayout* textLayout;
    mWindow->getDwriteFactory()->CreateTextLayout (
      std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
      mWindow->getTextFormat(), getWidth(), getHeight(), &textLayout);

    if (textLayout) {
      dc->DrawTextLayout (getCentre(), textLayout, mWindow->getWhiteBrush());
      textLayout->Release();
      }
    //}}}

    drawOverView (dc, playFrame);
    drawFreq (dc, playFrame, getHeight() - mLensY);
    drawTime (dc, getFrameStr (playFrame) + " " + getFrameStr (mSong.getTotalFrames()));
    }
  //}}}

private:
  //{{{
  void resizeBitmap (ID2D1DeviceContext* dc) {
  //  create bitmapBuf and ID2D1Bitmap matching box size

    // want spectrum
    if (!mSpecBitmap ||
        (getWidthInt() > (int)mSpecBitmapSize.width) ||
        (mWaveY > mSpecBitmapSize.height)) {

      if (mSpecBitmap)
        mSpecBitmap->Release();

      mSpecBitmapSize = { (UINT32)getWidthInt(), (UINT32)mWaveY};
      dc->CreateBitmap (mSpecBitmapSize,
                        { DXGI_FORMAT_A8_UNORM, D2D1_ALPHA_MODE_STRAIGHT, 0,0 },
                        &mSpecBitmap);


      if (mWaveBitmap)
        mWaveBitmap->Release();
      D2D1_PIXEL_FORMAT pixelFormat = D2D1::PixelFormat(
        DXGI_FORMAT_A8_UNORM,
        D2D1_ALPHA_MODE_STRAIGHT
        );
      D2D1_SIZE_F sizeF = D2D1::SizeF (getWidthInt(), mLensY - mWaveY);
      mWaveBitmapSize = { (UINT32)getWidthInt(), (UINT32)(mLensY - mWaveY)};
      dc->CreateCompatibleRenderTarget (&sizeF, &mWaveBitmapSize, &pixelFormat,
                                        D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS_NONE,
                                        &mCompatibleRenderTarget);

      //dc->CreateBitmap (mWaveBitmapSize,
      //                  { DXGI_FORMAT_A8_UNORM, D2D1_ALPHA_MODE_STRAIGHT, 0,0 },
      //                  &mWaveBitmap);
      }
    }
  //}}}
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
  void drawTime (ID2D1DeviceContext* dc, const std::string& str) {

    IDWriteTextLayout* textLayout;
    mWindow->getDwriteFactory()->CreateTextLayout (
      std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
      mTextFormat, getWidth(), getHeight(), &textLayout);

    if (textLayout) {
      dc->DrawTextLayout (getBL() - cPoint(0.f, 50.f), textLayout, mWindow->getWhiteBrush());
      textLayout->Release();
      }
    }
  //}}}
  //{{{
  void drawFreq (ID2D1DeviceContext* dc, int playFrame, float height) {

    if (mSong.mMaxFreqValue > 0.f) {
      auto valueScale = height / mSong.mMaxFreqValue;
      float* freq = (mSong.mFrames[playFrame]->getFreqValues());
      for (int i = 0; (i < getWidth()) && (i < mSong.getMaxFreq()); i++) {
        auto value =  freq[i] * valueScale;
        if (value > 1.f)
          dc->FillRectangle (cRect (mRect.left+i*2, mRect.bottom - value,
                                    mRect.left+(i*2)+2, mRect.bottom), mWindow->getYellowBrush());
        }
      }
    }
  //}}}

  //{{{
  void drawSummed (ID2D1DeviceContext* dc, int playFrame, int playFrameX,
                   int firstX, int lastX, float centreY, float height) {

    auto colour = mWindow->getBlueBrush();
    float valueScale = height / 2.f / mSong.getMaxPowerValue();
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

      r.top = centreY - (leftValue * valueScale) - 2.f;
      r.bottom = centreY + (rightValue * valueScale) + 2.f;
      dc->FillRectangle (r, colour);
      r.left = r.right;
      }
    }
  //}}}
  //{{{
  void drawLens (ID2D1DeviceContext* dc, int playFrame, int width, int centreX, float centreY, float height) {
  // draw frames centred at playFrame -/+ width in pixels, centred at centreX, zoomed by zoomIndex

    // calc leftmost frame, clip to valid frame, adjust firstX which may overlap left up to frameWidth
    auto leftFrame = playFrame - width;
    int firstX = centreX - (playFrame - leftFrame);
    if (leftFrame < 0) {
      firstX += -leftFrame;
      leftFrame = 0;
      }

    float valueScale = height / 2.f / mSong.getMaxPowerValue();

    auto colour = mWindow->getBlueBrush();
    cRect r (mRect.left + firstX, 0.f, 0.f, 0.f);

    auto frame = leftFrame;
    auto rightFrame = playFrame + width;
    auto lastFrame = std::min (rightFrame, mSong.getLastFrame());
    while ((r.left < mRect.right) && (frame <= lastFrame)) {
      r.right = r.left + 1.f;

      if (mSong.mFrames[frame]->hasTitle()) {
        //{{{  draw song title yellow bar and text
        dc->FillRectangle (cRect (r.left, centreY - (height / 2.f),
                                  r.right + 2.f, centreY + (height/ 2.f)),
                           mWindow->getYellowBrush());

        auto str = mSong.mFrames[frame]->getTitle();

        IDWriteTextLayout* textLayout;

        mWindow->getDwriteFactory()->CreateTextLayout (
          std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
          mWindow->getTextFormat(), getWidth(), height, &textLayout);

        if (textLayout) {
          dc->DrawTextLayout (cPoint (r.left, mRect.top + centreY - 20.f), textLayout, mWindow->getWhiteBrush());
          textLayout->Release();
          }
        }
        //}}}
      if (mSong.mFrames[frame]->isSilent()) {
        //{{{  draw red silent frame
        r.top = centreY - 2.f;
        r.bottom = centreY + 2.f;
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

      r.top = centreY - (leftValue * valueScale);
      r.bottom = centreY + (rightValue * valueScale);
      dc->FillRectangle (r, colour);

      r.left = r.right;
      frame++;
      }
    }
  //}}}
  //{{{
  void drawOverView (ID2D1DeviceContext* dc, int playFrame) {

    auto centreY = mRect.top + ((mLensY + getHeight()) / 2.f);
    auto height = (getHeight() - mLensY) / 2.f;

    int playFrameX = (mSong.getTotalFrames() > 0) ? (playFrame * getWidthInt()) / mSong.getTotalFrames() : 0;
    updateSummedPowerValues();

    if (mOn) {
      if (mLens < getWidthInt() / 16)
        // animate on
        mLens += (getWidthInt() / 16) / 6;
      }
    else if (mLens <= 1) {
      mLens = 0;
      drawSummed (dc, playFrame, playFrameX, 0, mSummedMaxX, centreY, height);
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
      drawSummed (dc, playFrame, playFrameX, 0, lensCentreX - mLens, centreY, height);

    // draw lens
    drawLens (dc, playFrame, mLens-1, lensCentreX, centreY, height);

    if (lensCentreX < getWidthInt() - mLens) // draw right of lens
      drawSummed (dc, playFrame, playFrameX, lensCentreX + mLens, getWidthInt(), centreY, height);

    dc->DrawRectangle (cRect(mRect.left + lensCentreX - mLens, mRect.top + mLensY,
                             mRect.left + lensCentreX + mLens, mRect.top + getHeight() - 1.f),
                       mWindow->getYellowBrush(), 1.f);
    }
  //}}}

  // private vars
  cSong& mSong;
  int mZoom = 0;  // >0 = zoomOut framesPerPix, 0 = unity, <0 = zoomIn pixPerFrame

  float mWaveY = 0.f;
  float mLensY = 0.f;

  ID2D1Bitmap* mSpecBitmap = nullptr;
  D2D1_SIZE_U mSpecBitmapSize = { 0,0 };
  D2D1_SIZE_U mWaveBitmapSize = { 0,0 };
  ID2D1Bitmap* mWaveBitmap = nullptr;
  ID2D1BitmapRenderTarget* mCompatibleRenderTarget = nullptr;

  bool mFramesValid = false;
  int mFirstFrame = -1;
  int mFirstFrameIndex = -1;
  int mLastFrame = -1;

  IDWriteTextFormat* mTextFormat = nullptr;

  float* mSummedPowerValues = nullptr;
  int mSummedNumFrames = -1;
  int mSummedMaxX = 0;

  bool mOn = false;
  int mLens = 0;
  };

//{{{
class cSongBoxOk : public cD2dWindow::cBox {
public:
  //{{{
  cSongBoxOk (cD2dWindow* window, float width, float height, cSong& song) :
      cBox("songBox", window, width, height), mSong(song) {

    mPin = true;

    mWindow->getDwriteFactory()->CreateTextFormat (L"Consolas", NULL,
      DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 50.f, L"en-us",
      &mTextFormat);
    mTextFormat->SetTextAlignment (DWRITE_TEXT_ALIGNMENT_TRAILING);
    }
  //}}}
  //{{{
  virtual ~cSongBoxOk() {

    mTextFormat->Release();

    if (mSpecBitmap)
      mSpecBitmap->Release();
    free (mSpecBitmapBuf);

    free (mSummedPowerValues);
    }
  //}}}

  //{{{
  bool onDown (bool right, cPoint pos)  {

    if (pos.y > mLensY) {
      mOn = true;
      auto frame = int((pos.x * mSong.getTotalFrames()) / getWidth());
      mSong.setPlayFrame (frame);
      }

    return true;
    }
  //}}}
  //{{{
  bool onMove (bool right, cPoint pos, cPoint inc) {

    if (pos.y > mLensY)
      mSong.setPlayFrame (int((pos.x * mSong.getTotalFrames()) / getWidth()));
    else
      mSong.incPlayFrame (int(-inc.x));

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

    if (getShow()) {
      mZoom = std::min (std::max (mZoom - (delta/120), mSong.getMinZoomIndex()), mSong.getMaxZoomIndex());
      return true;
      }

    return false;
    }
  //}}}

  //{{{
  void layout() {

    mSummedNumFrames = -1;
    cD2dWindow::cBox::layout();

    mWaveY = getHeight() - 200.f;
    mLensY = getHeight() - 100.f;
    }
  //}}}

  //{{{
  void onDraw (ID2D1DeviceContext* dc) {
  // draw frames centred at playFrame

    auto playFrame = mSong.getPlayFrame();

    int frameStep = (mZoom > 0) ? mZoom+1 : 1; // zoomOut summing frameStep frames per pix
    int frameWidth = (mZoom < 0) ? -mZoom+1 : 1; // zoomIn expanding frame to frameWidth pix

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

    auto colour = mWindow->getBlueBrush();

    float dstFirstX = mRect.left + firstX;
    cRect r (dstFirstX, 0.f, 0.f, 0.f);
    float centreY = mRect.top + ((mWaveY + mLensY) / 2.f);
    float valueScale = (mLensY - mWaveY) / 2.f / mSong.getMaxPowerValue();

    int dstBitmapWidth = 0;
    auto dstBitmap = mSpecBitmapBuf + ((mSpecBitmapSize.height-1) * mSpecBitmapSize.width);
    for (auto frame = firstFrame; (frame <= lastFrame) && (r.left < mRect.right); frame += frameStep) {
      r.right = r.left + frameWidth;
      if (mSong.mFrames[frame]->hasTitle()) {
        //{{{  draw song title yellow bar and text
        dc->FillRectangle (cRect (r.left, centreY - ((mLensY - mWaveY)/2.f),
                                  r.right + 2.f, centreY + ((mLensY - mWaveY)/2.f)), mWindow->getYellowBrush());

        auto str = mSong.mFrames[frame]->getTitle();

        IDWriteTextLayout* textLayout;
        mWindow->getDwriteFactory()->CreateTextLayout (
          std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
          mWindow->getTextFormat(), getWidth(), getHeight(), &textLayout);

        if (textLayout) {
          dc->DrawTextLayout (cPoint (r.left, getBL().y - (mLensY - mWaveY) - 20.f), textLayout, mWindow->getWhiteBrush());
          textLayout->Release();
          }
        }
        //}}}
      if (mSong.mFrames[frame]->isSilent()) {
        //{{{  draw red silent frame
        r.top = centreY - 2.f;
        r.bottom = centreY + 2.f;
        dc->FillRectangle (r, mWindow->getRedBrush());
        }
        //}}}
      //{{{  draw frame waveform
      if (frame > playFrame)
        colour = mWindow->getGreyBrush();

      float leftValue = 0.f;
      float rightValue = 0.f;

      if (mZoom <= 0) {
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

      r.top = centreY - (leftValue * valueScale);
      r.bottom = centreY + (rightValue * valueScale);
      dc->FillRectangle (r, colour);
      //}}}
      //{{{  draw frame bitmap column
      auto srcFreqLuma = mSong.mFrames[frame]->getFreqLuma();

      for (int y = mSpecBitmapSize.height; y > 0; y--) {
        for (int i = frameWidth; i > 0; i--)
          *dstBitmap++ = *srcFreqLuma;
        dstBitmap -= mSpecBitmapSize.width + frameWidth;
        srcFreqLuma++;
        }

      // back to bottom of next frame bitmap column
      dstBitmap += (mSpecBitmapSize.height * mSpecBitmapSize.width) + frameWidth;
      dstBitmapWidth += frameWidth;
      //}}}
      r.left = r.right;
      }

    if (dstBitmapWidth) {
      //{{{  stamp bitmap
      // copy used part of bitmapBuf to ID2D1Bitmap
      mSpecBitmap->CopyFromMemory (&D2D1::RectU (0, 0, dstBitmapWidth, mSpecBitmapSize.height),
                                   mSpecBitmapBuf, mSpecBitmapSize.width);

      // stamp colour through used part of ID2D1Bitmap alpha
      dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_ALIASED);

      dc->FillOpacityMask (mSpecBitmap, mWindow->getWhiteBrush(),
        // dstRect
        &D2D1::RectF (dstFirstX, mRect.top,
                      dstFirstX + dstBitmapWidth, mRect.top + (float)mSpecBitmapSize.height),
        // srcRect
        &D2D1::RectF (0.f,0.f,
                      (float)dstBitmapWidth, (float)mSpecBitmapSize.height));

      dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
      }
      //}}}
    //cLog::log (LOGINFO, "left:%d firstx:%d w:%d % bw:%d", leftFrame, firstX, frameWidth, dstBitmapWidth);

    drawOverView (dc, playFrame);
    drawFreq (dc, playFrame, getHeight() - mLensY);
    drawTime (dc, getFrameStr (playFrame) + " " + getFrameStr (mSong.getTotalFrames()));
    }
  //}}}

private:
  //{{{
  void resizeBitmap (ID2D1DeviceContext* dc) {
  //  create bitmapBuf and ID2D1Bitmap matching box size

    // want spectrum
    if (!mSpecBitmap ||
        (getWidthInt() > (int)mSpecBitmapSize.width) ||
        (mWaveY > mSpecBitmapSize.height)) {
      // allocate or reallocate heap:mSpecBitmapBuf and D2D:mSpecBitmap
      free (mSpecBitmapBuf);
      mSpecBitmapBuf = (uint8_t*)calloc (1, getWidthInt() * int(mWaveY));

      if (mSpecBitmap)
        mSpecBitmap->Release();

      mSpecBitmapSize = { (UINT32)getWidthInt(), (UINT32)mWaveY};

      dc->CreateBitmap (mSpecBitmapSize,
                        { DXGI_FORMAT_A8_UNORM, D2D1_ALPHA_MODE_STRAIGHT, 0,0 },
                        &mSpecBitmap);
      }
    }
  //}}}
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
  void drawTime (ID2D1DeviceContext* dc, const std::string& str) {

    IDWriteTextLayout* textLayout;
    mWindow->getDwriteFactory()->CreateTextLayout (
      std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
      mTextFormat, getWidth(), getHeight(), &textLayout);

    if (textLayout) {
      dc->DrawTextLayout (getBL() - cPoint(0.f, 50.f), textLayout, mWindow->getWhiteBrush());
      textLayout->Release();
      }
    }
  //}}}
  //{{{
  void drawFreq (ID2D1DeviceContext* dc, int playFrame, float height) {

    if (mSong.mMaxFreqValue > 0.f) {
      auto valueScale = height / mSong.mMaxFreqValue;
      float* freq = (mSong.mFrames[playFrame]->getFreqValues());
      for (int i = 0; (i < getWidth()) && (i < mSong.getMaxFreq()); i++) {
        auto value =  freq[i] * valueScale;
        if (value > 1.f)
          dc->FillRectangle (cRect (mRect.left+i*2, mRect.bottom - value,
                                    mRect.left+(i*2)+2, mRect.bottom), mWindow->getYellowBrush());
        }
      }
    }
  //}}}

  //{{{
  void drawSummed (ID2D1DeviceContext* dc, int playFrame, int playFrameX,
                   int firstX, int lastX, float centreY, float height) {

    auto colour = mWindow->getBlueBrush();
    float valueScale = height / 2.f / mSong.getMaxPowerValue();
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

      r.top = centreY - (leftValue * valueScale) - 2.f;
      r.bottom = centreY + (rightValue * valueScale) + 2.f;
      dc->FillRectangle (r, colour);
      r.left = r.right;
      }
    }
  //}}}
  //{{{
  void drawLens (ID2D1DeviceContext* dc, int playFrame, int width, int centreX, float centreY, float height) {
  // draw frames centred at playFrame -/+ width in pixels, centred at centreX, zoomed by zoomIndex

    // calc leftmost frame, clip to valid frame, adjust firstX which may overlap left up to frameWidth
    auto leftFrame = playFrame - width;
    int firstX = centreX - (playFrame - leftFrame);
    if (leftFrame < 0) {
      firstX += -leftFrame;
      leftFrame = 0;
      }

    float valueScale = height / 2.f / mSong.getMaxPowerValue();

    auto colour = mWindow->getBlueBrush();
    cRect r (mRect.left + firstX, 0.f, 0.f, 0.f);

    auto frame = leftFrame;
    auto rightFrame = playFrame + width;
    auto lastFrame = std::min (rightFrame, mSong.getLastFrame());
    while ((r.left < mRect.right) && (frame <= lastFrame)) {
      r.right = r.left + 1.f;

      if (mSong.mFrames[frame]->hasTitle()) {
        //{{{  draw song title yellow bar and text
        dc->FillRectangle (cRect (r.left, centreY - (height / 2.f),
                                  r.right + 2.f, centreY + (height/ 2.f)),
                           mWindow->getYellowBrush());

        auto str = mSong.mFrames[frame]->getTitle();

        IDWriteTextLayout* textLayout;

        mWindow->getDwriteFactory()->CreateTextLayout (
          std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
          mWindow->getTextFormat(), getWidth(), height, &textLayout);

        if (textLayout) {
          dc->DrawTextLayout (cPoint (r.left, mRect.top + centreY - 20.f), textLayout, mWindow->getWhiteBrush());
          textLayout->Release();
          }
        }
        //}}}
      if (mSong.mFrames[frame]->isSilent()) {
        //{{{  draw red silent frame
        r.top = centreY - 2.f;
        r.bottom = centreY + 2.f;
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

      r.top = centreY - (leftValue * valueScale);
      r.bottom = centreY + (rightValue * valueScale);
      dc->FillRectangle (r, colour);

      r.left = r.right;
      frame++;
      }
    }
  //}}}
  //{{{
  void drawOverView (ID2D1DeviceContext* dc, int playFrame) {

    auto centreY = mRect.top + ((mLensY + getHeight()) / 2.f);
    auto height = (getHeight() - mLensY) / 2.f;

    int playFrameX = (mSong.getTotalFrames() > 0) ? (playFrame * getWidthInt()) / mSong.getTotalFrames() : 0;
    updateSummedPowerValues();

    if (mOn) {
      if (mLens < getWidthInt() / 16)
        // animate on
        mLens += (getWidthInt() / 16) / 6;
      }
    else if (mLens <= 1) {
      mLens = 0;
      drawSummed (dc, playFrame, playFrameX, 0, mSummedMaxX, centreY, height);
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
      drawSummed (dc, playFrame, playFrameX, 0, lensCentreX - mLens, centreY, height);

    // draw lens
    drawLens (dc, playFrame, mLens-1, lensCentreX, centreY, height);

    if (lensCentreX < getWidthInt() - mLens) // draw right of lens
      drawSummed (dc, playFrame, playFrameX, lensCentreX + mLens, getWidthInt(), centreY, height);

    dc->DrawRectangle (cRect(mRect.left + lensCentreX - mLens, mRect.top + mLensY,
                             mRect.left + lensCentreX + mLens, mRect.top + getHeight() - 1.f),
                       mWindow->getYellowBrush(), 1.f);
    }
  //}}}

  // private vars
  cSong& mSong;
  int mZoom = 0;  // >0 = zoomOut framesPerPix, 0 = unity, <0 = zoomIn pixPerFrame

  float mWaveY = 0.f;
  float mLensY = 0.f;

  uint8_t* mSpecBitmapBuf = nullptr;
  ID2D1Bitmap* mSpecBitmap = nullptr;
  D2D1_SIZE_U mSpecBitmapSize = { 0,0 };

  IDWriteTextFormat* mTextFormat = nullptr;

  float* mSummedPowerValues = nullptr;
  int mSummedNumFrames = -1;
  int mSummedMaxX = 0;

  bool mOn = false;
  int mLens = 0;
  };
//}}}
