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

    if (mWaveBitmapTarget)
      mWaveBitmapTarget->Release();
    if (mWaveBitmap)
      mWaveBitmap->Release();
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

    // invalidate bitmap cache
    mBitmapFramesOk = false;
    }
  //}}}
  //{{{
  void onDraw (ID2D1DeviceContext* dc) {
  // draw stuff centred at playFrame

    allocBitmapCache (dc);

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
  void allocBitmapCache (ID2D1DeviceContext* dc) {
  // create mWaveBitmapTarget at size

    if (!mWaveBitmapTarget ||
        (getWidthInt() != mWaveBitmapSize.width) || (getHeightInt() != mWaveBitmapSize.height)) {

      // release old
      if (mWaveBitmap)
        mWaveBitmap->Release();
      if (mWaveBitmapTarget)
        mWaveBitmapTarget->Release();

      // wave bitmapTarget
      D2D1_SIZE_F waveSizeF = D2D1::SizeF (getWidth(), getHeight());
      mWaveBitmapSize = { UINT32(getWidthInt()), UINT32(getHeightInt()) };
      D2D1_PIXEL_FORMAT pixelFormat = { DXGI_FORMAT_A8_UNORM, D2D1_ALPHA_MODE_UNKNOWN };
      dc->CreateCompatibleRenderTarget (&waveSizeF, &mWaveBitmapSize, &pixelFormat,
                                        D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS_NONE,
                                        &mWaveBitmapTarget);

      // wave bitmap
      mWaveBitmapTarget->GetBitmap (&mWaveBitmap);

      // invalidate bitmap cache
      mBitmapFramesOk = false;
      }
    }
  //}}}

  //{{{
  int frameToSrcIndex (int frame) {
  // circular buffer mod with continuity for through +ve to -ve frame numbers

    if (frame < 0)
      frame += mWaveBitmapSize.width;
    return frame % mWaveBitmapSize.width;
    }
  //}}}
  //{{{
  void drawFrameToBitmap (int frame, int playFrame, int rightFrame, int frameStep, float valueScale) {

    int spectrumSize = std::min (mSong.getMaxSpectrum(), (int)mSpecHeight);
    int spectrumOffset = mSong.getMaxSpectrum() > (int)mSpecHeight ?
                           mSong.getMaxSpectrum() - (int)mSpecHeight : 0;

    auto frameSrcIndex = frameToSrcIndex (frame);
    float frameSrc = (float)frameSrcIndex;

    // clear frame column of spectrum + wave
    cRect r = { frameSrc,0.f, frameSrc+1.f, mSpecHeight + mWaveHeight };
    mWaveBitmapTarget->FillRectangle (r, mWindow->getClearBrush());

    if (frame >= 0) {
      // copy reversed spectrum column to bitmap, clip high freqs to height
      D2D1_RECT_U rectU = { (UINT32)frameSrcIndex,0, (UINT32)frameSrcIndex+1,(UINT32)spectrumSize };
      mWaveBitmap->CopyFromMemory (&rectU, mSong.mFrames[frame]->getFreqLuma() + spectrumOffset, 1);

      // draw wave rectangle
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

      r.top = mSpecHeight + mWaveCentre - (leftValue * valueScale);
      r.bottom  = mSpecHeight + mWaveCentre + (rightValue * valueScale);
      mWaveBitmapTarget->FillRectangle (r, mWindow->getWhiteBrush());
      }

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
    }
  //}}}
  //{{{
  void drawWave (ID2D1DeviceContext* dc, int playFrame) {

    float valueScale = mWaveCentre / mSong.getMaxPowerValue();

    int frameStep = (mZoom > 0) ? mZoom+1 : 1; // zoomOut summing frameStep frames per pix
    int frameWidth = (mZoom < 0) ? -mZoom+1 : 1; // zoomIn expanding frame to frameWidth pix

    // calc leftFrame,rightFrame
    auto leftFrame = playFrame - (((getWidthInt() + frameWidth) / 2) * frameStep) / frameWidth;
    auto rightFrame = playFrame + (((getWidthInt() + frameWidth) / 2) * frameStep) / frameWidth;
    rightFrame = std::min (rightFrame, mSong.getLastFrame());

    // check bitmap cache overlap
    mBitmapFramesOk = mBitmapFramesOk && 
                      (frameStep == mBitmapFrameStep) &&
                      (rightFrame > mBitmapFirstFrame) && (leftFrame < mBitmapLastFrame);

    // add new frames to spectrum,wave bitmaps
    mWaveBitmapTarget->BeginDraw();
    for (auto frame = leftFrame; frame < rightFrame; frame += frameStep)
      if (!mBitmapFramesOk || (frame < mBitmapFirstFrame) || (frame >= mBitmapLastFrame))
        drawFrameToBitmap (frame, playFrame, rightFrame, frameStep, valueScale);
    mWaveBitmapTarget->EndDraw();

    // save bitmap range
    mBitmapFramesOk = true;
    mBitmapFrameStep = frameStep;
    mBitmapFirstFrame = leftFrame;
    mBitmapLastFrame = rightFrame;

    // calc bitmap stamps
    float leftSrc = (float)frameToSrcIndex (leftFrame);
    float rightSrc = (float)frameToSrcIndex (rightFrame);
    float playSrc = (float)frameToSrcIndex (playFrame);
    bool wraparound = rightSrc <= leftSrc;
    float firstSrcEnd = wraparound ? float(mWaveBitmapSize.width) : rightSrc;

    //  stamp colours through bitmap alpha
    dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_ALIASED);
    //{{{  stamp spectrum first dst chunk
    cRect srcRect = { leftSrc, 0.f, firstSrcEnd, mSpecHeight };
    cRect dstRect = { mRect.left, mRect.top,
                      mRect.left + (firstSrcEnd - leftSrc) * frameWidth, mRect.top + mSpecHeight };
    dc->FillOpacityMask (mWaveBitmap, mWindow->getGreenBrush(), dstRect, srcRect);
    //}}}
    if (wraparound) {
      //{{{  stamp spectrum second dst chunk
      srcRect = { 0.f, 0.f, rightSrc, mSpecHeight };
      dstRect = { mRect.left + (firstSrcEnd - leftSrc) * frameWidth, mRect.top,
                  mRect.left + (firstSrcEnd - leftSrc + rightSrc) * frameWidth, mRect.top + mSpecHeight };
      dc->FillOpacityMask (mWaveBitmap, mWindow->getYellowBrush(), dstRect, srcRect);
      }
      //}}}
    //{{{  stamp waveform first dst chunk
    srcRect = { leftSrc, mSpecHeight, firstSrcEnd, mSpecHeight + mWaveHeight };
    dstRect = { mRect.left, mRect.top + mSpecHeight,
                mRect.left + (firstSrcEnd - leftSrc) * frameWidth, mRect.top + mSpecHeight + mWaveHeight };
    dc->FillOpacityMask (mWaveBitmap, mWindow->getBlueBrush(), dstRect, srcRect);
    //}}}
    if (wraparound) {
      //{{{  stamp waveform second dst chunk
      srcRect = { 0.f, mSpecHeight, rightSrc, mSpecHeight + mWaveHeight };
      dstRect = { mRect.left + (firstSrcEnd - leftSrc) * frameWidth, mRect.top + mSpecHeight,
                  mRect.left + (firstSrcEnd - leftSrc + rightSrc) * frameWidth, mRect.top + mSpecHeight + mWaveHeight };
      dc->FillOpacityMask (mWaveBitmap, mWindow->getGreyBrush(), dstRect, srcRect);
      }
      //}}}
    //{{{  draw playFrame waveform
    srcRect = { playSrc, mSpecHeight, playSrc+1.f, mSpecHeight + mWaveHeight };
    dstRect = { mRect.left + getCentreX(), mRect.top + mWaveTop,
                mRect.left + getCentreX() + frameWidth, mRect.top + mWaveTop + mWaveHeight };
    dc->FillOpacityMask (mWaveBitmap, mWindow->getWhiteBrush(), dstRect, srcRect);
    //}}}
    dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    // debug
    auto str = dec(leftFrame) + "," + dec(rightFrame) + (wraparound ? " wrapped" : "") +
               " bit" + dec(mBitmapFirstFrame) + "," + dec(mBitmapLastFrame) +
               " lsi:" + dec(leftSrc) + " rsi:" + dec(rightSrc) + " fsei:" + dec(firstSrcEnd) +
               " wid:" + dec(frameWidth) + " step:" + dec(frameStep);
    //{{{  draw debug str
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
      mWaveBitmapTarget->BeginDraw();
      if (forceRedraw) {
        cRect r = { 0.f, mSpecHeight+mWaveHeight, getWidth(), mSpecHeight+mWaveHeight + mOverviewHeight };
        mWaveBitmapTarget->FillRectangle (r, mWindow->getClearBrush());
        }

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

          cRect r = { x, mOverviewTop + mOverviewCentre - (leftValue * valueScale) - 2.f,
                      x+1.f, mOverviewTop + mOverviewCentre + (rightValue * valueScale) + 2.f };
          mWaveBitmapTarget->FillRectangle (r, mWindow->getWhiteBrush());
          }

        frame = toFrame;
        }
        //}}}
      mWaveBitmapTarget->EndDraw();

      mOverviewNumFrames = numFrames;
      mOverviewTotalFrames = totalFrames;
      mOverviewValueScale = valueScale;
      mSongId = mSong.getId();
      }

    // stamp Overview using overBitmap
    dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_ALIASED);

    // before playFrame
    cRect srcRect (0.f,mSpecHeight+mWaveHeight, playFrameX,mSpecHeight + mWaveHeight + mOverviewHeight);
    cRect dstRect (mRect.left, mRect.top + mSpecHeight+mWaveHeight,
                   mRect.left + playFrameX, mRect.top + mSpecHeight + mWaveHeight + mOverviewHeight);
    dc->FillOpacityMask (mWaveBitmap, mWindow->getBlueBrush(), dstRect, srcRect);

    // on playFrame
    srcRect.left = srcRect.right;
    srcRect.right += 1.f;
    dstRect.left = dstRect.right;
    dstRect.right += 1.f;
    dc->FillOpacityMask (mWaveBitmap, mWindow->getWhiteBrush(), dstRect, srcRect);

    // after playFrame
    srcRect.left = srcRect.right;
    srcRect.right = getWidth();
    dstRect.left = dstRect.right;
    dstRect.right = mRect.right;
    dc->FillOpacityMask (mWaveBitmap, mWindow->getGreyBrush(), dstRect, srcRect);

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
  int mBitmapFrameStep = 0;
  int mBitmapFirstFrame = 0;
  int mBitmapLastFrame = 0;

  D2D1_SIZE_U mWaveBitmapSize = { 0,0 };
  ID2D1BitmapRenderTarget* mWaveBitmapTarget = nullptr;
  ID2D1Bitmap* mWaveBitmap;

  IDWriteTextFormat* mTimeTextFormat = nullptr;
  //}}}
  };
