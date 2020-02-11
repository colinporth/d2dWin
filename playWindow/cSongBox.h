// cSongBox.h
#pragma once
//{{{  includes
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

#include "../../shared/utils/cLog.h"
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

    if (pos.y > mDstOverviewTop) {
      mOn = true;
      auto frame = int((pos.x * mSong.getTotalFrames()) / getWidth());
      mSong.setPlayFrame (frame);
      }

    return true;
    }
  //}}}
  //{{{
  bool onMove (bool right, cPoint pos, cPoint inc) {

    if (pos.y > mDstOverviewTop)
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

    mSpectroHeight = getHeight() - 200.f;
    mWaveHeight = 99.f;
    mSrcSilenceHeight = 1.f;
    mSrcMarkHeight = 1.f;
    mOverviewHeight = 99.f;

    mSrcSpectroTop = 0.f;
    mSrcWaveTop = mSrcSpectroTop + mSpectroHeight;
    mSrcWaveCentre = mSrcWaveTop + (mWaveHeight/2.f);
    mSrcSilenceTop = mSrcWaveTop + mWaveHeight;
    mSrcMarkTop = mSrcSilenceTop + mSrcSilenceHeight;
    mSrcOverviewTop = mSrcSilenceTop + mSrcMarkHeight;
    mSrcOverviewCentre = mSrcOverviewTop + (mOverviewHeight/2.f);

    mDstSpectroTop = mRect.top;
    mDstWaveTop = mDstSpectroTop + mSpectroHeight;
    mDstWaveCentre = mDstWaveTop + (mWaveHeight/2.f);
    mDstOverviewTop = mDstWaveTop + mWaveHeight + mSrcSilenceHeight + mSrcMarkHeight;
    mDstOverviewCentre = mDstOverviewTop + (mOverviewHeight/2.f);

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
  void drawFrameBitmaps (int frame, int toFrame, int playFrame, int rightFrame, int frameStep, float valueScale) {

    int spectrumSize = std::min (mSong.getMaxSpectrum(), (int)mSpectroHeight);
    int spectrumOffset = mSong.getMaxSpectrum() > (int)mSpectroHeight ?
                           mSong.getMaxSpectrum() - (int)mSpectroHeight : 0;

    cLog::log(LOGINFO, "drawFrameToBitmap %d %d %d", frame, toFrame, playFrame);
    while (frame < toFrame) {
      auto frameSrcIndex = frameToSrcIndex (frame);
      float frameSrc = (float)frameSrcIndex;

      // clear bitmap frame column, could do in 1 or 2 chunks before loop
      cRect r = { frameSrc, mSrcSpectroTop, frameSrc+1.f, mSrcOverviewTop };
      mWaveBitmapTarget->FillRectangle (r, mWindow->getClearBrush());

      if (frame >= 0) {
        // copy reversed spectrum column to bitmap, clip high freqs to height
        D2D1_RECT_U rectU = { (UINT32)frameSrcIndex, 0, (UINT32)frameSrcIndex+1, (UINT32)spectrumSize };
        mWaveBitmap->CopyFromMemory (&rectU, mSong.mFrames[frame]->getFreqLuma() + spectrumOffset, 1);

        //{{{  draw wave bitmap
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

        r.top = mSrcWaveCentre - (leftValue * valueScale);
        r.bottom  = mSrcWaveCentre + (rightValue * valueScale);
        mWaveBitmapTarget->FillRectangle (r, mWindow->getWhiteBrush());
        //}}}

        if (mSong.mFrames[frame]->isSilent()) {
          //{{{  draw silence bitmap
          r.top = mSrcSilenceTop;
          r.bottom = mSrcSilenceTop + mSrcSilenceHeight;
          mWaveBitmapTarget->FillRectangle (r, mWindow->getWhiteBrush());
          }
          //}}}

        if (mSong.mFrames[frame]->hasTitle()) {
          //{{{  draw song title bitmap
          r.top = mSrcSilenceTop;
          r.bottom = mSrcMarkTop + mSrcMarkHeight;
          mWaveBitmapTarget->FillRectangle (r, mWindow->getWhiteBrush());

          //auto str = mSong.mFrames[frame]->getTitle();

          //IDWriteTextLayout* textLayout;
          //mWindow->getDwriteFactory()->CreateTextLayout (
          //  std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
          //  mWindow->getTextFormat(), getWidth(), getHeight(), &textLayout);

          //if (textLayout) {
          //  dc->DrawTextLayout (cPoint (r.left, getBL().y - mWaveHeight - 20.f), textLayout, mWindow->getWhiteBrush());
          //  textLayout->Release();
          //  }
          }
          //}}}
        }
      frame += frameStep;
      }
    }
  //}}}
  //{{{
  void drawWave (ID2D1DeviceContext* dc, int playFrame) {

    float valueScale = mWaveHeight / 2.f / mSong.getMaxPowerValue();

    int frameStep = (mZoom > 0) ? mZoom+1 : 1; // zoomOut summing frameStep frames per pix
    int frameWidth = (mZoom < 0) ? -mZoom+1 : 1; // zoomIn expanding frame to frameWidth pix

    // calc leftFrame,rightFrame
    auto leftFrame = playFrame - (((getWidthInt() + frameWidth) / 2) * frameStep) / frameWidth;
    auto rightFrame = playFrame + (((getWidthInt() + frameWidth) / 2) * frameStep) / frameWidth;
    rightFrame = std::min (rightFrame, mSong.getLastFrame());

    // check for bitmap range overlap
    mBitmapFramesOk = mBitmapFramesOk &&
                      (frameStep == mBitmapFrameStep) &&
                      (rightFrame > mBitmapFirstFrame) && (leftFrame < mBitmapLastFrame);

    // add frames to spectro,wave,silence.mark bitmap
    mWaveBitmapTarget->BeginDraw();

    if (!mBitmapFramesOk || (leftFrame >= mBitmapLastFrame) || (rightFrame < mBitmapFirstFrame))
      // update whole bitmap range
      drawFrameBitmaps (leftFrame, rightFrame, playFrame, rightFrame, frameStep, valueScale);
    else {
      // some overlap of range
      if (leftFrame < mBitmapFirstFrame)
        drawFrameBitmaps (leftFrame, mBitmapFirstFrame, playFrame, rightFrame, frameStep, valueScale);
      if (rightFrame > mBitmapLastFrame)
        drawFrameBitmaps (mBitmapLastFrame, rightFrame, playFrame, rightFrame, frameStep, valueScale);
      }
    mBitmapFramesOk = true;
    mBitmapFrameStep = frameStep;
    mBitmapFirstFrame = leftFrame;
    mBitmapLastFrame = rightFrame;

    mWaveBitmapTarget->EndDraw();

    // calc bitmap stamps
    float leftSrc = (float)frameToSrcIndex (leftFrame);
    float rightSrc = (float)frameToSrcIndex (rightFrame);
    float playSrc = (float)frameToSrcIndex (playFrame);
    bool wraparound = rightSrc <= leftSrc;
    float firstSrcEnd = wraparound ? float(mWaveBitmapSize.width) : rightSrc;

    //  stamp colours through alpha bitmap
    dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_ALIASED);
    //{{{  stamp first dst chunk
    // spectro
    cRect srcRect = { leftSrc, mSrcSpectroTop, firstSrcEnd, mSrcSpectroTop + mSpectroHeight };
    cRect dstRect = { mRect.left,
                      mDstSpectroTop,
                      mRect.left + (firstSrcEnd - leftSrc) * frameWidth,
                      mDstSpectroTop + mSpectroHeight };
    dc->FillOpacityMask (mWaveBitmap, mWindow->getGreenBrush(), dstRect, srcRect);

    // silence
    srcRect = { leftSrc, mSrcSilenceTop, firstSrcEnd, mSrcSilenceTop + mSrcSilenceHeight };
    dstRect = { mRect.left,
                mDstWaveCentre - 2.f,
                mRect.left + (firstSrcEnd - leftSrc) * frameWidth,
                mDstWaveCentre + 2.f, };
    dc->FillOpacityMask (mWaveBitmap, mWindow->getRedBrush(), dstRect, srcRect);

    // mark
    srcRect = { leftSrc, mSrcMarkTop, firstSrcEnd, mSrcMarkTop + mSrcMarkHeight };
    dstRect = { mRect.left,
                mDstWaveTop,
                mRect.left + (firstSrcEnd - leftSrc) * frameWidth,
                mDstWaveTop + mWaveHeight };
    dc->FillOpacityMask (mWaveBitmap, mWindow->getYellowBrush(), dstRect, srcRect);

    // wave
    srcRect = { leftSrc, mSrcWaveTop, firstSrcEnd, mSrcWaveTop + mWaveHeight };
    dstRect = { mRect.left,
                mDstWaveTop,
                mRect.left + (firstSrcEnd - leftSrc) * frameWidth,
                mDstWaveTop + mWaveHeight };
    dc->FillOpacityMask (mWaveBitmap, mWindow->getBlueBrush(), dstRect, srcRect);
    //}}}
    if (wraparound) {
      //{{{  stamp second dst chunk
      // spectro
      srcRect = { 0.f, mSrcSpectroTop, rightSrc, mSrcSpectroTop + mSpectroHeight };
      dstRect = { mRect.left + (firstSrcEnd - leftSrc) * frameWidth,
                  mDstSpectroTop,
                  mRect.left + (firstSrcEnd - leftSrc + rightSrc) * frameWidth,
                  mDstSpectroTop + mSpectroHeight };
      dc->FillOpacityMask (mWaveBitmap, mWindow->getYellowBrush(), dstRect, srcRect);

      // silence
      srcRect = { 0.f, mSrcSilenceTop, rightSrc, mSrcSilenceTop + mSrcSilenceHeight };
      dstRect = { mRect.left + (firstSrcEnd - leftSrc) * frameWidth,
                  mDstWaveCentre - 2.f,
                  mRect.left + (firstSrcEnd - leftSrc + rightSrc) * frameWidth,
                  mDstWaveCentre + 2.f };
      dc->FillOpacityMask (mWaveBitmap, mWindow->getRedBrush(), dstRect, srcRect);

      // mark
      srcRect = { 0.f, mSrcMarkTop, rightSrc, mSrcMarkTop + mSrcMarkHeight };
      dstRect = { mRect.left + (firstSrcEnd - leftSrc) * frameWidth,
                  mDstWaveTop,
                  mRect.left + (firstSrcEnd - leftSrc + rightSrc) * frameWidth,
                  mDstWaveTop + mWaveHeight };
      dc->FillOpacityMask (mWaveBitmap, mWindow->getYellowBrush(), dstRect, srcRect);

      // wave
      srcRect = { 0.f, mSrcWaveTop, rightSrc, mSrcWaveTop + mWaveHeight };
      dstRect = { mRect.left + (firstSrcEnd - leftSrc) * frameWidth,
                  mDstWaveTop,
                  mRect.left + (firstSrcEnd - leftSrc + rightSrc) * frameWidth,
                  mDstWaveTop + mWaveHeight };
      dc->FillOpacityMask (mWaveBitmap, mWindow->getGreyBrush(), dstRect, srcRect);
      }
      //}}}
    //{{{  stamp playFrame wave
    srcRect = { playSrc, mSrcWaveTop, playSrc+1.f, mSrcWaveTop + mWaveHeight };

    dstRect = { mRect.left + getCentreX(),
                mDstWaveTop,
                mRect.left + getCentreX() + frameWidth,
                mDstWaveTop + mWaveHeight };

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

    float valueScale = mOverviewHeight / 2.f / mSong.getMaxPowerValue();
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
        cRect r = { 0.f, mSrcOverviewTop, getWidth(), mSrcOverviewTop + mOverviewHeight };
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

          cRect r = { x, mSrcOverviewCentre - (leftValue * valueScale) - 2.f,
                      x+1.f, mSrcOverviewCentre + (rightValue * valueScale) + 2.f };
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
    cRect srcRect (0.f, mSrcOverviewTop, playFrameX, mSrcOverviewTop + mOverviewHeight);
    cRect dstRect (mRect.left, mDstOverviewTop,
                   mRect.left + playFrameX, mDstOverviewTop + mOverviewHeight);
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

    cRect r = { mRect.left + centreX - mLens, mDstOverviewTop,
                mRect.left + centreX + mLens, mRect.bottom - 1.f };

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
        dc->FillRectangle (cRect (r.left-1.f, mDstOverviewTop, r.left+1.f, mRect.bottom),
                           mWindow->getYellowBrush());

        auto str = mSong.mFrames[frame]->getTitle();

        IDWriteTextLayout* textLayout;
        mWindow->getDwriteFactory()->CreateTextLayout (
          std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
          mWindow->getTextFormat(), getWidth(), mOverviewHeight, &textLayout);
        if (textLayout) {
          dc->DrawTextLayout (cPoint (r.left+2.f, mDstOverviewTop), textLayout, mWindow->getWhiteBrush());
          textLayout->Release();
          }
        }
        //}}}
      if (mSong.mFrames[frame]->isSilent()) {
        //{{{  draw red silent frame
        r.top = mDstOverviewCentre - 2.f;
        r.bottom = mDstOverviewCentre + 2.f;
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

      r.top = mDstOverviewCentre - (leftValue * valueScale);
      r.bottom = mDstOverviewCentre + (rightValue * valueScale);
      dc->FillRectangle (r, colour);

      r.left = r.right;
      frame++;
      }
    }
  //}}}

  //{{{  private vars
  cSong& mSong;

  // vertical layout
  float mSpectroHeight = 0.f;
  float mWaveHeight = 0.f;
  float mSrcSilenceHeight = 0.f;
  float mSrcMarkHeight = 0.f;
  float mOverviewHeight = 0.f;

  float mSrcSpectroTop = 0.f;
  float mSrcWaveTop = 0.f;
  float mSrcWaveCentre = 0.f;
  float mSrcSilenceTop = 0.f;
  float mSrcMarkTop = 0.f;
  float mSrcOverviewTop = 0.f;
  float mSrcOverviewCentre = 0.f;

  float mDstSpectroTop = 0.f;
  float mDstWaveTop = 0.f;
  float mDstWaveCentre = 0.f;
  float mDstOverviewTop = 0.f;
  float mDstOverviewCentre = 0.f;

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
