// cSongBox.h
#pragma once
//{{{  includes
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include "cSong.h"
//}}}

class cSongBox : public cD2dWindow::cBox {
public:
  //{{{
  cSongBox (cD2dWindow* window, float width, float height, cSong& song) :
      cBox("songBox", window, width, height), mSong(song) {

    mPin = true;

    mWindow->getDwriteFactory()->CreateTextFormat (L"Consolas", NULL,
      DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 50.f, L"en-us",
      &mTimeTextFormat);
    mTimeTextFormat->SetTextAlignment (DWRITE_TEXT_ALIGNMENT_TRAILING);
    }
  //}}}
  //{{{
  virtual ~cSongBox() {

    if (mTimeTextFormat)
      mTimeTextFormat->Release();

    if (mSpecBitmap)
      mSpecBitmap->Release();

    if (mWaveBitmapTarget)
      mWaveBitmapTarget->Release();

    if (mWaveBitmap)
      mWaveBitmap->Release();

    if (mOverviewBitmapTarget)
      mOverviewBitmapTarget->Release();
    }
  //}}}

  //{{{
  bool onDown (bool right, cPoint pos)  {

    if (pos.y > mOverviewTop) {
      mOn = true;
      auto frame = int((pos.x * mSong.getTotalFrames()) / getWidth());
      mSong.setPlayFrame (frame);
      }

    return true;
    }
  //}}}
  //{{{
  bool onMove (bool right, cPoint pos, cPoint inc) {

    if (pos.y > mOverviewTop)
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

    cD2dWindow::cBox::layout();

    mWaveTop = getHeight() - 200.f;
    mOverviewTop = getHeight() - 100.f;

    mSpecHeight = mWaveTop;

    mWaveHeight = mOverviewTop - mWaveTop;
    mWaveCentre = mWaveHeight/2.f;

    mOverviewHeight = getHeight() - mOverviewTop;
    mOverviewCentre = mOverviewHeight/2.f;

    mOverviewNumFrames = 0;
    mOverviewTotalFrames = 0;
    mOverviewValueScale = 1.f;

    mBitmapFramesOk = false;
    mBitmapFirstFrame = 0;
    mBitmapLastFrame = 0;
    }
  //}}}
  //{{{
  void onDraw (ID2D1DeviceContext* dc) {
  // draw stuff centred at playFrame

    allocBitmaps (dc);

    if (!mSong.getNumFrames()) // nothing yet, give up
      return;

    auto playFrame = mSong.getPlayFrame();

    drawWave (dc, playFrame);
    drawOverview (dc, playFrame);
    drawFreq (dc, playFrame);
    drawTime (dc, getFrameStr (playFrame) + " " + getFrameStr (mSong.getTotalFrames()));
    }
  //}}}

private:
  //{{{
  void allocBitmaps (ID2D1DeviceContext* dc) {
  // create mSpecBitmap and mWaveBitmapTarget at size

    if (!mSpecBitmap || !mWaveBitmapTarget || !mOverviewBitmapTarget ||
        (getWidthInt() != (int)mSpecBitmapSize.width) ||
        (mWaveTop != mSpecBitmapSize.height) || (mWaveHeight != mWaveBitmapSize.height) || (mOverviewHeight != mOverviewBitmapSize.height)) {

      // spectrum bitmap
      if (mSpecBitmap)
        mSpecBitmap->Release();
      mSpecBitmapSize = { (UINT32)getWidthInt(), (UINT32)mWaveTop };
      dc->CreateBitmap (mSpecBitmapSize,
                        { DXGI_FORMAT_A8_UNORM, D2D1_ALPHA_MODE_STRAIGHT, 0,0 },
                        &mSpecBitmap);

      // wave bitmapTarget
      if (mWaveBitmapTarget)
        mWaveBitmapTarget->Release();
      D2D1_SIZE_F waveSizeF = D2D1::SizeF (getWidth(), mWaveHeight);
      mWaveBitmapSize = { (UINT32)getWidthInt(), (UINT32)mWaveHeight };
      D2D1_PIXEL_FORMAT pixelFormat = { DXGI_FORMAT_A8_UNORM, D2D1_ALPHA_MODE_UNKNOWN };
      dc->CreateCompatibleRenderTarget (&waveSizeF, &mWaveBitmapSize, &pixelFormat,
                                        D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS_NONE,
                                        &mWaveBitmapTarget);
      if (mWaveBitmap)
        mWaveBitmap->Release();
      mWaveBitmapTarget->GetBitmap (&mWaveBitmap);

      // over bitmapTarget
      if (mOverviewBitmapTarget)
        mOverviewBitmapTarget->Release();
      D2D1_SIZE_F overSizeF = D2D1::SizeF (getWidth(), mOverviewHeight);
      mOverviewBitmapSize = { (UINT32)getWidthInt(), (UINT32)mOverviewHeight };
      dc->CreateCompatibleRenderTarget (&overSizeF, &mOverviewBitmapSize, &pixelFormat,
                                        D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS_NONE,
                                        &mOverviewBitmapTarget);
      if (mOverviewBitmap)
        mOverviewBitmap->Release();
      mOverviewBitmapTarget->GetBitmap (&mOverviewBitmap);
      }
    }
  //}}}

  //{{{
  int frameToIndex (int frame) {
    return ((frame >= 0) ? frame : (mSpecBitmapSize.width + frame)) % mSpecBitmapSize.width; }
  //}}}
  //{{{
  void drawWave (ID2D1DeviceContext* dc, int playFrame) {

    float valueScale = mWaveCentre / mSong.getMaxPowerValue();

    int spectrumSize = std::min (mSong.getMaxSpectrum(), (int)mSpecBitmapSize.height);
    int spectrumOffset = mSong.getMaxSpectrum() > (int)mSpecBitmapSize.height ?
                           mSong.getMaxSpectrum() - mSpecBitmapSize.height : 0;

    int frameStep = (mZoom > 0) ? mZoom+1 : 1; // zoomOut summing frameStep frames per pix
    int frameWidth = (mZoom < 0) ? -mZoom+1 : 1; // zoomIn expanding frame to frameWidth pix

    // calc leftFrame, rightFrame
    auto leftFrame = playFrame - (((getWidthInt() + frameWidth) / 2) * frameStep) / frameWidth;
    auto rightFrame = playFrame + (((getWidthInt() + frameWidth) / 2) * frameStep) / frameWidth;
    rightFrame = std::min (rightFrame, mSong.getLastFrame());

    // check for bitmap cache overlap
    mBitmapFramesOk = mBitmapFramesOk && (rightFrame > mBitmapFirstFrame) && (leftFrame < mBitmapLastFrame);

    // update wave and spectrum bitmaps
    mWaveBitmapTarget->BeginDraw();
    for (auto frame = leftFrame; frame < rightFrame; frame += frameStep) {
      //if (mSong.mFrames[frame]->hasTitle()) {
        //{{{  draw song title yellow bar and text
        //dc->FillRectangle (cRect (r.left, 0.f, r.right + 2.f, mWaveHeight) + getTR(), mWindow->getYellowBrush());

        //auto str = mSong.mFrames[frame]->getTitle();

        //IDWriteTextLayout* textLayout;
        //mWindow->getDwriteFactory()->CreateTextLayout (
          //std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
          //mWindow->getTextFormat(), getWidth(), getHeight(), &textLayout);

        //if (textLayout) {
          //dc->DrawTextLayout (cPoint (r.left, getBL().y - mWaveHeight - 20.f), textLayout, mWindow->getWhiteBrush());
          //textLayout->Release();
          //}
        //}
        //}}}
      //if (mSong.mFrames[frame]->isSilent()) {
        //{{{  draw red silent frame
        //r.top = mWaveCentre - 2.f;
        //r.bottom = mWaveCentre + 2.f;
        //dc->FillRectangle (r + getTR(), mWindow->getRedBrush());
        //}
        //}}}
      if (!mBitmapFramesOk || (frame < mBitmapFirstFrame) || (frame >= mBitmapLastFrame)) {
        //{{{  copy reversed spectrum column to bitmap, clipping higher freqs to height
        UINT32 index = frameToIndex (frame);
        D2D1_RECT_U rectU = { index,0, index+1,(UINT32)spectrumSize };

        if (frame >= 0)
          mSpecBitmap->CopyFromMemory (&rectU, mSong.mFrames[frame]->getFreqLuma() + spectrumOffset, 1);
        else {
          uint8_t zeros[1024] = { 0 };
          mSpecBitmap->CopyFromMemory (&rectU, &zeros, 1);
          }

        cRect dstRect = { float(index), 0.f,
                          float(index+1), mWaveHeight };
        mWaveBitmapTarget->FillRectangle (dstRect, mWindow->getClearBrush());

        if (frame >= 0) {
          float leftValue = 0.f;
          float rightValue = 0.f;
          if (mZoom <= 0) {
            // no zoom, or zoomIn expanding frame
            auto powerValues = mSong.mFrames[frame]->getPowerValues();
            leftValue = *powerValues++;
            rightValue = *powerValues;
            }
          else {
            // zoomOut, summing frames
            int firstSumFrame = frame - (frame % frameStep);
            int nextSumFrame = firstSumFrame + frameStep;
            for (auto i = firstSumFrame; i < firstSumFrame + frameStep; i++) {
              auto powerValues = mSong.mFrames[std::min (i, rightFrame)]->getPowerValues();
              if (i == playFrame) {
                leftValue = *powerValues++;
                rightValue = *powerValues;
                break;
                }
              leftValue += *powerValues++ / frameStep;
              rightValue += *powerValues / frameStep;
              }
            }

          dstRect.top = mWaveCentre - (leftValue * valueScale);
          dstRect.bottom  = mWaveCentre + (rightValue * valueScale);
          mWaveBitmapTarget->FillRectangle (dstRect, mWindow->getWhiteBrush());
          }
        }
        //}}}
      }
    mWaveBitmapTarget->EndDraw();

    // save bitmap range
    mBitmapFramesOk = true;
    mBitmapFirstFrame = leftFrame;
    mBitmapLastFrame = rightFrame;

    //{{{  stamp colours through bitmap alpha
    // stamp left spectrum chunk
    dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_ALIASED);

    float leftSrcIndex = float(frameToIndex (leftFrame));
    float rightSrcIndex = float(frameToIndex (rightFrame));
    float endSrcIndex = float(mSpecBitmapSize.width);

    cRect srcRect;
    cRect dstRect;

    float leftStampWidth = endSrcIndex - leftSrcIndex;
    srcRect = { leftSrcIndex, 0.f,
                endSrcIndex, mSpecHeight };
    dstRect = { mRect.left, mRect.top,
                mRect.left + leftStampWidth, mRect.top + mSpecHeight };
    dc->FillOpacityMask (mSpecBitmap, mWindow->getGreenBrush(), dstRect, srcRect);

    float rightStampWidth = rightSrcIndex;
    srcRect = { 0.f, 0.f,
                rightStampWidth, mSpecHeight };
    dstRect = { mRect.left + leftStampWidth, mRect.top,
                mRect.left + leftStampWidth + rightStampWidth, mRect.top + mSpecHeight };
    dc->FillOpacityMask (mSpecBitmap, mWindow->getYellowBrush(), dstRect, srcRect);

    // stamp left waveform chunk
    srcRect = { leftSrcIndex, 0.f,
                endSrcIndex, mWaveHeight };
    dstRect = { mRect.left, mRect.top + mWaveTop,
                mRect.left + leftStampWidth, mRect.top + mWaveTop + mWaveHeight };
    dc->FillOpacityMask (mWaveBitmap, mWindow->getBlueBrush(), dstRect, srcRect);

    // stamp right waveform chunk
    srcRect = { 0.f, 0.f,
                rightStampWidth, mWaveHeight };
    dstRect = { mRect.left + leftStampWidth, mRect.top + mWaveTop,
                mRect.left + leftStampWidth + rightStampWidth, mRect.top + mWaveTop + mWaveHeight };
    dc->FillOpacityMask (mWaveBitmap, mWindow->getGreyBrush(), dstRect, srcRect);

    // draw playFrame waveform
    auto powerValues = mSong.mFrames[playFrame]->getPowerValues();
    auto leftValue = (*powerValues++) * valueScale;
    auto rightValue = (*powerValues) * valueScale;
    dstRect = { mRect.left + ((playFrame - leftFrame) * frameWidth), mRect.top + mWaveTop + mWaveCentre - leftValue,
                mRect.left + ((playFrame -leftFrame) + 1.f) * frameWidth, mRect.top + mWaveTop + mWaveCentre + rightValue };
    dc->FillRectangle (dstRect, mWindow->getWhiteBrush());
    dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    // debug
    auto str = dec(leftFrame) + "," + dec(rightFrame) +
               " bit" + dec(mBitmapFirstFrame) + "," + dec(mBitmapLastFrame) +
               " lsi:" + dec(leftSrcIndex) + " rsi:" + dec(rightSrcIndex) +
               " wid:" + dec(frameWidth) + " step:" + dec(frameStep);

    IDWriteTextLayout* textLayout;
    mWindow->getDwriteFactory()->CreateTextLayout (
      std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
      mWindow->getTextFormat(), getWidth(), getHeight(), &textLayout);

    if (textLayout) {
      dc->DrawTextLayout ({ 0.f, 100.f }, textLayout, mWindow->getWhiteBrush());
      textLayout->Release();
      }
    //}}}
    }
  //}}}
  //{{{
  void drawFreq (ID2D1DeviceContext* dc, int playFrame) {

    if (mSong.getMaxFreqValue() > 0.f) {
      auto valueScale = mOverviewHeight / mSong.getMaxFreqValue();
      auto maxFreqIndex = std::min (getWidthInt()/2, mSong.getMaxFreq());

      cRect r (0.f,0.f, 0.f,mRect.bottom);
      auto freq = mSong.mFrames[playFrame]->getFreqValues();
      for (auto i = 0; i < maxFreqIndex; i++) {
        auto value =  freq[i] * valueScale;
        if (value > 1.f)  {
          r.left = mRect.left + (i*2);
          r.right = r.left + 2;
          r.top = mRect.bottom - value;
          dc->FillRectangle (r, mWindow->getYellowBrush());
          }
        }
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
  void drawTime (ID2D1DeviceContext* dc, const std::string& str) {

    IDWriteTextLayout* textLayout;
    mWindow->getDwriteFactory()->CreateTextLayout (
      std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
      mTimeTextFormat, getWidth(), getHeight(), &textLayout);

    if (textLayout) {
      dc->DrawTextLayout (getBL() - cPoint(0.f, 50.f), textLayout, mWindow->getWhiteBrush());
      textLayout->Release();
      }
    }
  //}}}

  //{{{
  void drawOverview (ID2D1DeviceContext* dc, int playFrame) {

    float valueScale = mOverviewCentre / mSong.getMaxPowerValue();
    float playFrameX = (mSong.getTotalFrames() > 0) ? (playFrame * getWidth()) / mSong.getTotalFrames() : 0.f;
    drawOverviewWave (dc, playFrame, playFrameX, valueScale);

    if (mOn) {
      //{{{  animate on
      if (mLens < getWidth() / 16.f)
        mLens += getWidth() / 16.f / 6.f;
      }
      //}}}
    else {
      //{{{  animate off
      if (mLens > 1.f)
        mLens /= 2.f;
      else  // animate done
        mLens = 0.f;
      }
      //}}}

    if (mLens > 0.f) {
      float lensCentreX = (float)playFrameX;
      if (lensCentreX - mLens < 0.f)
        lensCentreX = (float)mLens;
      else if (lensCentreX + mLens > getWidth())
        lensCentreX = getWidth() - mLens;

      drawOverviewLens (dc, playFrame, lensCentreX, mLens-1.f, valueScale);
      }
    }
  //}}}
  //{{{
  void drawOverviewWave (ID2D1DeviceContext* dc, int playFrame, float playFrameX, float valueScale) {
  // draw Overview using bitmap cache

    int numFrames = mSong.getNumFrames();
    int totalFrames = mSong.getTotalFrames();
    float framesPerPix = totalFrames / getWidth();
    bool forceRedraw = (mSong.getId() != mSongId) ||
                       (totalFrames != mOverviewTotalFrames) || (valueScale != mOverviewValueScale);

    if (forceRedraw || (numFrames > mOverviewNumFrames)) {
      mOverviewBitmapTarget->BeginDraw();
      if (forceRedraw) {
        //{{{  clear bitmap to alpha off
        D2D1_COLOR_F alphaOff = { 0.f };
        mOverviewBitmapTarget->Clear (&alphaOff);
        }
        //}}}

      int frame = 0;
      for (float x = 0.f; x < getWidth(); x += 1.f) {
        //{{{  iterate width
        int toFrame = int(x * framesPerPix);
        if (toFrame >= numFrames)
          break;

        if (forceRedraw || (frame >= mOverviewNumFrames)) {
          float* powerValues = mSong.mFrames[frame]->getPowerValues();
          float leftValue = *powerValues++;
          float rightValue = *powerValues;
          if (frame < toFrame) {
            int numSummedFrames = 1;
            frame++;
            while (frame < toFrame) {
              auto powerValues = mSong.mFrames[frame++]->getPowerValues();
              leftValue += *powerValues++;
              rightValue += *powerValues;
              numSummedFrames++;
              }
            leftValue /= numSummedFrames;
            rightValue /= numSummedFrames;
            }

          mOverviewBitmapTarget->FillRectangle (
            cRect (x, mOverviewCentre - (leftValue * valueScale) - 2.f,
                   x+1.f,  mOverviewCentre + (rightValue * valueScale) + 2.f), mWindow->getWhiteBrush());
          }

        frame = toFrame;
        }
        //}}}
      mOverviewBitmapTarget->EndDraw();

      mOverviewNumFrames = numFrames;
      mOverviewTotalFrames = totalFrames;
      mOverviewValueScale = valueScale;
      mSongId = mSong.getId();
      }

    // stamp Overview using overBitmap
    dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_ALIASED);

    // before playFrame
    cRect srcRect (0.f,0.f, playFrameX, mOverviewHeight);
    cRect dstRect (mRect.left, mRect.top + mOverviewTop, mRect.left + playFrameX, mRect.bottom);
    dc->FillOpacityMask (mOverviewBitmap, mWindow->getBlueBrush(), dstRect, srcRect);

    // on playFrame
    auto powerValues = mSong.mFrames[playFrame]->getPowerValues();
    auto leftValue = *powerValues++;
    auto rightValue = *powerValues;
    srcRect.right += 1.f;
    dstRect.left = dstRect.right;
    dstRect.right += 1.f;
    dc->FillRectangle (cRect (dstRect.left, dstRect.top + mOverviewCentre - (leftValue * valueScale),
                              dstRect.right, dstRect.top + mOverviewCentre + (rightValue * valueScale)),
                       mWindow->getWhiteBrush());

    // after playFrame
    srcRect.left = srcRect.right;
    srcRect.right += (float)getWidthInt();
    dstRect.left = dstRect.right;
    dstRect.right = mRect.left + getWidthInt();
    dc->FillOpacityMask (mOverviewBitmap, mWindow->getGreyBrush(), dstRect, srcRect);

    dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    }
  //}}}
  //{{{
  void drawOverviewLens (ID2D1DeviceContext* dc, int playFrame, float centreX, float width, float valueScale) {
  // draw frames centred at playFrame -/+ width in pixels, centred at centreX, zoomed by zoomIndex

    cRect r (mRect.left + centreX - mLens, mRect.top + mOverviewTop,
             mRect.left + centreX + mLens, mRect.bottom - 1.f);

    dc->FillRectangle (r, mWindow->getBlackBrush());
    dc->DrawRectangle (r, mWindow->getYellowBrush(), 1.f);

    // calc leftmost frame, clip to valid frame, adjust firstX which may overlap left up to frameWidth
    float leftFrame = playFrame - width;
    float firstX = centreX - (playFrame - leftFrame);
    if (leftFrame < 0) {
      firstX += -leftFrame;
      leftFrame = 0;
      }

    auto colour = mWindow->getBlueBrush();

    int frame = (int)leftFrame;
    int rightFrame = (int)(playFrame + width);
    int lastFrame = std::min (rightFrame, mSong.getLastFrame());

    r.left = mRect.left + firstX;
    while ((r.left < mRect.right) && (frame <= lastFrame)) {
      r.right = r.left + 1.f;

      if (mSong.mFrames[frame]->hasTitle()) {
        //{{{  draw song title yellow bar and text
        dc->FillRectangle (cRect (r.left-1.f, mRect.top + mOverviewTop, r.left+1.f, mRect.bottom), mWindow->getYellowBrush());

        auto str = mSong.mFrames[frame]->getTitle();

        IDWriteTextLayout* textLayout;
        mWindow->getDwriteFactory()->CreateTextLayout (
          std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
          mWindow->getTextFormat(), getWidth(), mOverviewHeight, &textLayout);
        if (textLayout) {
          dc->DrawTextLayout (cPoint (r.left+2.f, mRect.top + mOverviewTop), textLayout, mWindow->getWhiteBrush());
          textLayout->Release();
          }
        }
        //}}}
      if (mSong.mFrames[frame]->isSilent()) {
        //{{{  draw red silent frame
        r.top = mRect.top + mOverviewTop + mOverviewCentre - 2.f;
        r.bottom = mRect.top + mOverviewTop + mOverviewCentre + 2.f;
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

      r.top = mRect.top + mOverviewTop + mOverviewCentre - (leftValue * valueScale);
      r.bottom = mRect.top + mOverviewTop + mOverviewCentre + (rightValue * valueScale);
      dc->FillRectangle (r, colour);

      r.left = r.right;
      frame++;
      }
    }
  //}}}

  //{{{  private vars
  cSong& mSong;

  // vertical layout
  float mSpecHeight = 0.f;

  float mWaveTop = 0.f;
  float mWaveHeight = 0.f;
  float mWaveCentre = 0.f;

  float mOverviewTop = 0.f;
  float mOverviewHeight = 0.f;
  float mOverviewCentre = 0.f;

  int mSongId = 0;

  int mZoom = 0;  // >0 = zoomOut framesPerPix, 0 = unity, <0 = zoomIn pixPerFrame

  bool mOn = false;
  float mLens = 0.f;
  int mOverviewNumFrames = 0;
  int mOverviewTotalFrames = 0;
  float mOverviewValueScale = 1.f;

  bool mBitmapFramesOk = false;
  int mBitmapFirstFrame = 0;
  int mBitmapLastFrame = 0;

  D2D1_SIZE_U mSpecBitmapSize = { 0,0 };
  ID2D1Bitmap* mSpecBitmap = nullptr;

  D2D1_SIZE_U mWaveBitmapSize = { 0,0 };
  ID2D1BitmapRenderTarget* mWaveBitmapTarget = nullptr;
  ID2D1Bitmap* mWaveBitmap;

  D2D1_SIZE_U mOverviewBitmapSize = { 0,0 };
  ID2D1BitmapRenderTarget* mOverviewBitmapTarget = nullptr;
  ID2D1Bitmap* mOverviewBitmap;

  IDWriteTextFormat* mTimeTextFormat = nullptr;
  //}}}
  };
