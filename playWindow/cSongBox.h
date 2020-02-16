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
  cSongBox (cD2dWindow* window, float width, float height, cSong*& song) :
      cBox("songBox", window, width, height), mSong(song) {

    mPin = true;

    // time display font
    mWindow->getDwriteFactory()->CreateTextFormat (L"Consolas", NULL,
      DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 50.f, L"en-us",
      &mTimeTextFormat);

    if (mTimeTextFormat)
      mTimeTextFormat->SetTextAlignment (DWRITE_TEXT_ALIGNMENT_TRAILING);
    }
  //}}}
  //{{{
  virtual ~cSongBox() {

    if (mTimeTextFormat)
      mTimeTextFormat->Release();

    if (mBitmapTarget)
      mBitmapTarget->Release();

    if (mBitmap)
      mBitmap->Release();
    }
  //}}}

  //{{{
  void layout() {

    cD2dWindow::cBox::layout();
    cLog::log (LOGINFO, "cSongBox::layout %d %d %d %d", mRect.left, mRect.top, mRect.right, mRect.bottom);

    // invalidate frame bitmap
    mBitmapFramesOk = false;

    // invalidate overview bitmap
    mBitmapOverviewOk = false;
    }
  //}}}
  //{{{
  bool onDown (bool right, cPoint pos)  {

    std::lock_guard<std::mutex> lockGuard (mSong->getMutex());

    if (pos.y > mDstOverviewTop) {
      mOverviewPressed = true;
      auto frame = int((pos.x * mSong->getTotalFrames()) / getWidth());
      mSong->setPlayFrame (frame);
      }
    else
      mPressedFrame = (float)mSong->getPlayFrame();

    return true;
    }
  //}}}
  //{{{
  bool onMove (bool right, cPoint pos, cPoint inc) {

    std::lock_guard<std::mutex> lockGuard (mSong->getMutex());

    if (mOverviewPressed)
      mSong->setPlayFrame (int((pos.x * mSong->getTotalFrames()) / getWidth()));
    else {
      mPressedFrame -= (inc.x / mFrameWidth) * mFrameStep;
      mSong->setPlayFrame ((int)mPressedFrame);
      }

    return true;
    }
  //}}}
  //{{{
  bool onUp (bool right, bool mouseMoved, cPoint pos) {

    mOverviewPressed = false;
    return true;
    }
  //}}}
  //{{{
  bool onWheel (int delta, cPoint pos)  {

    if (getShow()) {
      setZoom (mZoom - (delta/120));
      return true;
      }

    return false;
    }
  //}}}
  //{{{
  void onDraw (ID2D1DeviceContext* dc) {
  // draw stuff centred at playFrame

    if (!mSong->getNumFrames()) // no frames yet, give up
      return;

    std::lock_guard<std::mutex> lockGuard (mSong->getMutex());

    auto playFrame = mSong->getPlayFrame();

    // src bitmap explicit layout
    mWaveHeight = 100.f;
    mOverviewHeight = 100.f;
    mFreqHeight = getHeight() - mWaveHeight - mOverviewHeight;

    mSrcPeakHeight = mWaveHeight;
    mSrcMarkHeight = 2.f;
    mSrcSilenceHeight = 2.f;
    mSrcHeight = mFreqHeight + mWaveHeight + mSrcPeakHeight + mSrcMarkHeight + mSrcSilenceHeight + mOverviewHeight;

    mSrcFreqTop = 0.f;
    mSrcWaveTop = mSrcFreqTop + mFreqHeight;
    mSrcPeakTop = mSrcWaveTop + mWaveHeight;
    mSrcMarkTop = mSrcPeakTop + mSrcPeakHeight;
    mSrcSilenceTop = mSrcMarkTop + mSrcMarkHeight;
    mSrcOverviewTop = mSrcSilenceTop + mSrcSilenceHeight;

    mSrcWaveCentre = mSrcWaveTop + (mWaveHeight/2.f);
    mSrcPeakCentre = mSrcPeakTop + (mSrcPeakHeight/2.f);
    mSrcOverviewCentre = mSrcOverviewTop + (mOverviewHeight/2.f);

    // dst box explicit layout
    mDstFreqTop = mRect.top;
    mDstWaveTop = mDstFreqTop + mFreqHeight + mSrcSilenceHeight;
    mDstOverviewTop = mDstWaveTop + mWaveHeight + mSrcMarkHeight;

    mDstWaveCentre = mDstWaveTop + (mWaveHeight/2.f);
    mDstOverviewCentre = mDstOverviewTop + (mOverviewHeight/2.f);

    reallocBitmap (mWindow->getDc());

    // draw
    drawWave (dc, playFrame);
    drawOverview (dc, playFrame);
    drawFreq (dc, playFrame);
    if (mSong->hasTime()) {
      auto startDatePoint = date::floor<date::days>(mSong->getStartTimePoint());
      auto seconds = std::chrono::duration_cast<std::chrono::seconds>(mSong->getStartTimePoint() - startDatePoint);
      uint64_t framesBase = (seconds.count() * mSong->getSampleRate()) / mSong->getSamplesPerFrame();

      drawTime (dc, " " + frameString (framesBase + playFrame) +
                    " " + frameString (framesBase + mSong->getTotalFrames()));
      }
    else
      drawTime (dc, frameString (playFrame) + " " + frameString (mSong->getTotalFrames()));
    }
  //}}}

private:
  //{{{
  std::string frameString (uint64_t frame) {

    if (mSong->getSamplesPerFrame() && mSong->getSampleRate()) {
      uint64_t frameHs = (frame * mSong->getSamplesPerFrame()) / (mSong->getSampleRate() / 100);

      uint64_t hs = frameHs % 100;

      frameHs /= 100;
      uint64_t secs = frameHs % 60;

      frameHs /= 60;
      uint64_t mins = frameHs % 60;

      frameHs /= 60;
      uint64_t hours = frameHs % 60;

      std::string str (hours ? (dec (hours) + ':' + dec (mins, 2, '0')) : dec (mins));
      return str + ':' + dec(secs, 2, '0') + ':' + dec(hs, 2, '0');
      }
    else
      return ("--:--:--");
    }
  //}}}
  //{{{
  void setZoom (int zoom) {

    mZoom = std::min (std::max (zoom, mMinZoom), mMaxZoom);

    // zoomIn expanding frame to mFrameWidth pix
    mFrameWidth = (mZoom < 0) ? -mZoom+1 : 1;

    // zoomOut summing mFrameStep frames per pix
    mFrameStep = (mZoom > 0) ? mZoom+1 : 1;
    }
  //}}}

  //{{{
  void drawBitmapFrames (int fromFrame, int toFrame, int playFrame, int rightFrame, float valueScale) {

    //cLog::log (LOGINFO, "drawFrameToBitmap %d %d %d", fromFrame, toFrame, playFrame);
    cRect bitmapRect;
    if (fromFrame < 0) {  // !!! div for neg maybe wrong !!!!
      //{{{  clear bitmap for -ve frames, allows simpler drawing logic later
      bitmapRect = { (float)((fromFrame / mFrameStep) & mBitmapMask), mSrcFreqTop, (float)mBitmapWidth, mSrcOverviewTop };
      mBitmapTarget->PushAxisAlignedClip (bitmapRect, D2D1_ANTIALIAS_MODE_ALIASED);
      mBitmapTarget->Clear ( { 0.f,0.f,0.f, 0.f } );
      mBitmapTarget->PopAxisAlignedClip();
      fromFrame = 0;
      }
      //}}}

    // clear bitmap as chunks
    auto fromSrcIndex = float((fromFrame / mFrameStep) & mBitmapMask);
    auto toSrcIndex = float((toFrame / mFrameStep) & mBitmapMask);
    bool wrap = toSrcIndex <= fromSrcIndex;
    float endSrcIndex = wrap ? float(mBitmapWidth) : toSrcIndex;
    //{{{  clear bitmap chunk before wrap
    bitmapRect = { fromSrcIndex, mSrcWaveTop, endSrcIndex, mSrcOverviewTop };
    mBitmapTarget->PushAxisAlignedClip (bitmapRect, D2D1_ANTIALIAS_MODE_ALIASED);
    mBitmapTarget->Clear ( { 0.f,0.f,0.f, 0.f } );
    mBitmapTarget->PopAxisAlignedClip();
    //}}}
    if (wrap) {
      //{{{  clear bitmap chunk after wrap
      bitmapRect = { 0.f, mSrcWaveTop, toSrcIndex, mSrcOverviewTop };
      mBitmapTarget->PushAxisAlignedClip (bitmapRect, D2D1_ANTIALIAS_MODE_ALIASED);
      mBitmapTarget->Clear ( { 0.f,0.f,0.f, 0.f } );
      mBitmapTarget->PopAxisAlignedClip();
      }
      //}}}

    // draw bitmap as frames
    for (auto frame = fromFrame; frame < toFrame; frame += mFrameStep) {
      //{{{  draw bitmap for frame
      bool mark;
      bool silence;
      float values[2];
      float peakValues[2];

      if (mFrameStep == 1) {
        // simple case
        mark = mSong->mFrames[frame]->hasTitle();
        silence = mSong->mFrames[frame]->isSilent();

        auto powerValues = mSong->mFrames[frame]->getPowerValues();
        auto peakPowerValues = mSong->mFrames[frame]->getPeakPowerValues();
        for (auto i = 0; i < 2; i++) {
          values[i] = *powerValues++;
          peakValues[i] = *peakPowerValues++;
          }
        }
      else {
        // sum mFrameStep frames, mFrameStep aligned
        silence = false;
        mark = false;
        for (auto i = 0; i < 2; i++)
          values[i] = 0.f;

        auto alignedFrame = frame - (frame % mFrameStep);
        auto toSumFrame = std::min (alignedFrame + mFrameStep, rightFrame);
        for (auto sumFrame = alignedFrame; sumFrame < toSumFrame; sumFrame++) {
          mark |= mSong->mFrames[sumFrame]->hasTitle();
          silence |= mSong->mFrames[sumFrame]->isSilent();
          auto powerValues = mSong->mFrames[sumFrame]->getPowerValues();
          for (auto i = 0; i < 2; i++)
            values[i] += *powerValues++;
          }

        for (auto i = 0; i < 2; i++)
          values[i] /= toSumFrame - alignedFrame + 1;
        }

      float srcIndex = float((frame / mFrameStep) & mBitmapMask);
      bitmapRect = { srcIndex, mSrcWaveCentre - (values[0] * valueScale),
                     srcIndex + 1.f, mSrcWaveCentre + (values[1] * valueScale) };
      mBitmapTarget->FillRectangle (bitmapRect, mWindow->getWhiteBrush());

      bitmapRect = { srcIndex, mSrcPeakCentre - (peakValues[0] * valueScale),
                     srcIndex + 1.f, mSrcPeakCentre + (peakValues[1] * valueScale) };
      mBitmapTarget->FillRectangle (bitmapRect, mWindow->getWhiteBrush());

      if (silence) {
        // draw silence bitmap
        bitmapRect.top = mSrcSilenceTop;
        bitmapRect.bottom = mSrcSilenceTop + 1.f;
        mBitmapTarget->FillRectangle (bitmapRect, mWindow->getWhiteBrush());
        }

      if (mark) {
        // draw song title bitmap
        bitmapRect.top = mSrcMarkTop;
        bitmapRect.bottom = mSrcMarkTop + 1.f;
        mBitmapTarget->FillRectangle (bitmapRect, mWindow->getWhiteBrush());
        }
      }
      //}}}

    // copy reversed spectrum column to bitmap, clip high freqs to height
    int freqSize = std::min (mSong->getNumFreqLuma(), (int)mFreqHeight);
    int freqOffset = mSong->getNumFreqLuma() > (int)mFreqHeight ? mSong->getNumFreqLuma() - (int)mFreqHeight : 0;

    if (mFrameStep == 1) {
      for (auto frame = fromFrame; frame < toFrame; frame += mFrameStep) {
        uint32_t bitmapIndex = (frame / mFrameStep) & mBitmapMask;
        D2D1_RECT_U bitmapRectU = { bitmapIndex, 0, bitmapIndex+1, (UINT32)freqSize };
        mBitmap->CopyFromMemory (&bitmapRectU, mSong->mFrames[frame]->getFreqLuma() + freqOffset, 1);
        }
      }
    else {
      // align to mFrameStep, could sum as well
      auto alignedFromFrame = fromFrame - (fromFrame % mFrameStep);
      for (auto frame = alignedFromFrame; frame < toFrame; frame += mFrameStep) {
        uint32_t bitmapIndex = (frame / mFrameStep) & mBitmapMask;
        D2D1_RECT_U bitmapRectU = { bitmapIndex, 0, bitmapIndex+1, (UINT32)freqSize };
        mBitmap->CopyFromMemory (&bitmapRectU, mSong->mFrames[frame]->getFreqLuma() + freqOffset, 1);
        }
      }
    }
  //}}}

  //{{{
  void reallocBitmap (ID2D1DeviceContext* dc) {
  // fixed bitmap width for big cache, src bitmap height tracks dst box height

    mBitmapWidth = 0x800; // 2048, power of 2
    mBitmapMask =  0x7FF; // wrap mask

    uint32_t bitmapHeight = (int)mSrcHeight;

    if (!mBitmapTarget || (bitmapHeight != mBitmapTarget->GetSize().height)) {
      cLog::log (LOGINFO, "reallocBitmap %d %d", bitmapHeight, mBitmapTarget ? mBitmapTarget->GetSize().height : -1);

      // invalidate bitmap caches
      mBitmapFramesOk = false;
      mBitmapOverviewOk = false;

      // release old
      if (mBitmap)
        mBitmap->Release();
      if (mBitmapTarget)
        mBitmapTarget->Release();

      // wave bitmapTarget
      D2D1_SIZE_U bitmapSizeU = { mBitmapWidth, bitmapHeight };
      D2D1_PIXEL_FORMAT pixelFormat = { DXGI_FORMAT_A8_UNORM, D2D1_ALPHA_MODE_STRAIGHT };
      dc->CreateCompatibleRenderTarget (NULL, &bitmapSizeU, &pixelFormat, D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS_NONE,
                                        &mBitmapTarget);
      mBitmapTarget->GetBitmap (&mBitmap);
      }
    }
  //}}}

  //{{{
  void drawWave (ID2D1DeviceContext* dc, int playFrame) {

    float valueScale = mWaveHeight / 2.f / mSong->getMaxPeakPowerValue();

    // calc leftFrame,rightFrame
    auto leftFrame = playFrame - (((getWidthInt() + mFrameWidth) / 2) * mFrameStep) / mFrameWidth;
    auto rightFrame = playFrame + (((getWidthInt() + mFrameWidth) / 2) * mFrameStep) / mFrameWidth;
    rightFrame = std::min (rightFrame, mSong->getLastFrame());

    mBitmapTarget->BeginDraw();
    if (mBitmapFramesOk &&
        (mFrameStep == mBitmapFrameStep) &&
        (rightFrame > mBitmapFirstFrame) && (leftFrame < mBitmapLastFrame)) {
      // overlap
      if (leftFrame < mBitmapFirstFrame) {
        //{{{  draw new bitmap leftFrames
        drawBitmapFrames (leftFrame, mBitmapFirstFrame, playFrame, rightFrame, valueScale);

        mBitmapFirstFrame = leftFrame;
        if (mBitmapLastFrame - mBitmapFirstFrame > (int)mBitmapWidth)
          mBitmapLastFrame = mBitmapFirstFrame + mBitmapWidth;
        }
        //}}}
      if (rightFrame > mBitmapLastFrame) {
        //{{{  draw new bitmap rightFrames
        drawBitmapFrames (mBitmapLastFrame, rightFrame, playFrame, rightFrame, valueScale);

        mBitmapLastFrame = rightFrame;
        if (mBitmapLastFrame - mBitmapFirstFrame > (int)mBitmapWidth)
          mBitmapFirstFrame = mBitmapLastFrame - mBitmapWidth;
        }
        //}}}
      }
    else {
      //{{{  no overlap, draw all bitmap frames
      drawBitmapFrames (leftFrame, rightFrame, playFrame, rightFrame, valueScale);

      mBitmapFirstFrame = leftFrame;
      mBitmapLastFrame = rightFrame;
      }
      //}}}
    mBitmapFramesOk = true;
    mBitmapFrameStep = mFrameStep;
    mBitmapTarget->EndDraw();

    // calc bitmap wrap chunks
    float leftSrcIndex = (float)((leftFrame / mFrameStep) & mBitmapMask);
    float rightSrcIndex = (float)((rightFrame / mFrameStep) & mBitmapMask);
    float playSrcIndex = (float)((playFrame / mFrameStep) & mBitmapMask);

    bool wrap = rightSrcIndex <= leftSrcIndex;
    float endSrcIndex = wrap ? float(mBitmapWidth) : rightSrcIndex;

    // draw dst chunks, mostly stamping colour through alpha bitmap
    cRect srcRect;
    cRect dstRect;
    dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_ALIASED);
    //{{{  stamp playFrame mark
    auto dstPlay = playSrcIndex - leftSrcIndex + (playSrcIndex < leftSrcIndex ? endSrcIndex : 0);
    dstRect = { mRect.left + (dstPlay+0.5f) * mFrameWidth, mDstFreqTop,
                mRect.left + ((dstPlay+0.5f) * mFrameWidth) + 1.f, mDstWaveTop + mWaveHeight };
    dc->FillRectangle (dstRect, mWindow->getDarkGreyBrush());
    //}}}
    //{{{  stamp chunk before wrap
    // freq
    srcRect = { leftSrcIndex, mSrcFreqTop, endSrcIndex, mSrcFreqTop + mFreqHeight };
    dstRect = { mRect.left, mDstFreqTop,
                mRect.left + (endSrcIndex - leftSrcIndex) * mFrameWidth, mDstFreqTop + mFreqHeight };
    dc->FillOpacityMask (mBitmap, mWindow->getWhiteBrush(), dstRect, srcRect);

    // silence
    srcRect = { leftSrcIndex, mSrcSilenceTop, endSrcIndex, mSrcSilenceTop + mSrcSilenceHeight };
    dstRect = { mRect.left, mDstWaveCentre - 2.f,
                mRect.left + (endSrcIndex - leftSrcIndex) * mFrameWidth, mDstWaveCentre + 2.f, };
    dc->FillOpacityMask (mBitmap, mWindow->getRedBrush(), dstRect, srcRect);

    // mark
    srcRect = { leftSrcIndex, mSrcMarkTop, endSrcIndex, mSrcMarkTop + 1.f };
    dstRect = { mRect.left, mDstWaveTop,
                mRect.left + (endSrcIndex - leftSrcIndex) * mFrameWidth, mDstWaveTop + mWaveHeight };
    dc->FillOpacityMask (mBitmap, mWindow->getYellowBrush(), dstRect, srcRect);

    // wave
    bool split = (playSrcIndex >= leftSrcIndex) && (playSrcIndex < endSrcIndex);


    // wave chunk before play
    srcRect = { leftSrcIndex, mSrcPeakTop, (split ? playSrcIndex : endSrcIndex), mSrcPeakTop + mSrcPeakHeight };
    dstRect = { mRect.left, mDstWaveTop,
                mRect.left + ((split ? playSrcIndex : endSrcIndex) - leftSrcIndex) * mFrameWidth, mDstWaveTop + mWaveHeight };
    dc->FillOpacityMask (mBitmap, mWindow->getDarkBlueBrush(), dstRect, srcRect);

    srcRect = { leftSrcIndex, mSrcWaveTop, (split ? playSrcIndex : endSrcIndex), mSrcWaveTop + mWaveHeight };
    dstRect = { mRect.left, mDstWaveTop,
                mRect.left + ((split ? playSrcIndex : endSrcIndex) - leftSrcIndex) * mFrameWidth, mDstWaveTop + mWaveHeight };
    dc->FillOpacityMask (mBitmap, mWindow->getBlueBrush(), dstRect, srcRect);

    if (split) {
      // split wave chunk after play
      srcRect = { playSrcIndex+1.f, mSrcPeakTop, endSrcIndex, mSrcPeakTop + mSrcPeakHeight };
      dstRect = { mRect.left + (playSrcIndex+1.f - leftSrcIndex) * mFrameWidth, mDstWaveTop,
                  mRect.left + (endSrcIndex - leftSrcIndex) * mFrameWidth, mDstWaveTop + mWaveHeight };
      dc->FillOpacityMask (mBitmap, mWindow->getDarkGreyBrush(), dstRect, srcRect);

      srcRect = { playSrcIndex+1.f, mSrcWaveTop, endSrcIndex, mSrcWaveTop + mWaveHeight };
      dstRect = { mRect.left + (playSrcIndex+1.f - leftSrcIndex) * mFrameWidth, mDstWaveTop,
                  mRect.left + (endSrcIndex - leftSrcIndex) * mFrameWidth, mDstWaveTop + mWaveHeight };
      dc->FillOpacityMask (mBitmap, mWindow->getLightGreyBrush(), dstRect, srcRect);
      }
    //}}}
    if (wrap) {
      //{{{  stamp second chunk after wrap
      // Freq
      srcRect = { 0.f, mSrcFreqTop,  rightSrcIndex, mSrcFreqTop + mFreqHeight };
      dstRect = { mRect.left + (endSrcIndex - leftSrcIndex) * mFrameWidth, mDstFreqTop,
                   mRect.left + (endSrcIndex - leftSrcIndex + rightSrcIndex) * mFrameWidth, mDstFreqTop + mFreqHeight };
      dc->FillOpacityMask (mBitmap, mWindow->getWhiteBrush(), dstRect, srcRect);

      // silence
      srcRect = { 0.f, mSrcSilenceTop, rightSrcIndex, mSrcSilenceTop + mSrcSilenceHeight };
      dstRect = { mRect.left + (endSrcIndex - leftSrcIndex) * mFrameWidth, mDstWaveCentre - 2.f,
                  mRect.left + (endSrcIndex - leftSrcIndex + rightSrcIndex) * mFrameWidth, mDstWaveCentre + 2.f };
      dc->FillOpacityMask (mBitmap, mWindow->getRedBrush(), dstRect, srcRect);

      // mark
      srcRect = { 0.f, mSrcMarkTop,  rightSrcIndex, mSrcMarkTop + 1.f };
      dstRect = { mRect.left + (endSrcIndex - leftSrcIndex) * mFrameWidth, mDstWaveTop,
                  mRect.left + (endSrcIndex - leftSrcIndex + rightSrcIndex) * mFrameWidth, mDstWaveTop + mWaveHeight };
      dc->FillOpacityMask (mBitmap, mWindow->getYellowBrush(), dstRect, srcRect);

      // wave
      bool split = playSrcIndex < rightSrcIndex;
      if (split) {
        // split chunk before play
        srcRect = { 0.f, mSrcPeakTop,  playSrcIndex, mSrcPeakTop + mSrcPeakHeight };
        dstRect = { mRect.left + (endSrcIndex - leftSrcIndex) * mFrameWidth, mDstWaveTop,
                    mRect.left + (endSrcIndex - leftSrcIndex + playSrcIndex) * mFrameWidth, mDstWaveTop + mWaveHeight };
        dc->FillOpacityMask (mBitmap, mWindow->getDarkBlueBrush(), dstRect, srcRect);

        srcRect = { 0.f, mSrcWaveTop,  playSrcIndex, mSrcWaveTop + mWaveHeight };
        dstRect = { mRect.left + (endSrcIndex - leftSrcIndex) * mFrameWidth, mDstWaveTop,
                    mRect.left + (endSrcIndex - leftSrcIndex + playSrcIndex) * mFrameWidth, mDstWaveTop + mWaveHeight };
        dc->FillOpacityMask (mBitmap, mWindow->getBlueBrush(), dstRect, srcRect);
        }

      // chunk after play
      srcRect = { split ? playSrcIndex+1.f : 0.f, mSrcPeakTop,  rightSrcIndex, mSrcPeakTop + mSrcPeakHeight };
      dstRect = { mRect.left + (endSrcIndex - leftSrcIndex + (split ? (playSrcIndex+1.f) : 0.f)) * mFrameWidth, mDstWaveTop,
                  mRect.left + (endSrcIndex - leftSrcIndex + rightSrcIndex) * mFrameWidth, mDstWaveTop + mWaveHeight };
      dc->FillOpacityMask (mBitmap, mWindow->getDarkGreyBrush(), dstRect, srcRect);

      srcRect = { split ? playSrcIndex+1.f : 0.f, mSrcWaveTop,  rightSrcIndex, mSrcWaveTop + mWaveHeight };
      dstRect = { mRect.left + (endSrcIndex - leftSrcIndex + (split ? (playSrcIndex+1.f) : 0.f)) * mFrameWidth, mDstWaveTop,
                  mRect.left + (endSrcIndex - leftSrcIndex + rightSrcIndex) * mFrameWidth, mDstWaveTop + mWaveHeight };
      dc->FillOpacityMask (mBitmap, mWindow->getLightGreyBrush(), dstRect, srcRect);
      }
      //}}}

    //{{{  draw playFrame
    auto powerValues = mSong->mFrames[playFrame]->getPeakPowerValues();
    dstRect = { mRect.left + (dstPlay * mFrameWidth)-1.f, mDstWaveCentre - (*powerValues++ * valueScale),
                mRect.left + ((dstPlay+1.f) * mFrameWidth)+1.f, mDstWaveCentre + (*powerValues * valueScale) };
    dc->FillRectangle (dstRect, mWindow->getGreenBrush());
    //}}}

    dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    }
  //}}}
  //{{{
  void drawFreq (ID2D1DeviceContext* dc, int playFrame) {

    if (mSong->getMaxFreqValue() > 0.f) {
      auto valueScale = mOverviewHeight / mSong->getMaxFreqValue();
      auto maxFreqIndex = std::min (getWidthInt()/2, mSong->getNumFreq());

      cRect dstRect = { 0.f,0.f, 0.f,mRect.bottom };
      auto freq = mSong->mFrames[playFrame]->getFreqValues();
      for (auto i = 0; i < maxFreqIndex; i++) {
        auto value =  freq[i] * valueScale;
        if (value > 1.f)  {
          dstRect.left = mRect.left + (i*2);
          dstRect.right = dstRect.left + 2;
          dstRect.top = mRect.bottom - value;
          dc->FillRectangle (dstRect, mWindow->getYellowBrush());
          }
        }
      }
    }
  //}}}
  //{{{
  void drawTime (ID2D1DeviceContext* dc, const std::string& str) {

    if (mTimeTextFormat) {
      IDWriteTextLayout* textLayout;
      mWindow->getDwriteFactory()->CreateTextLayout (
        std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
        mTimeTextFormat, getWidth(), getHeight(), &textLayout);

      if (textLayout) {
        dc->DrawTextLayout (getBL() - cPoint(0.f, 50.f), textLayout, mWindow->getWhiteBrush());
        textLayout->Release();
        }
      }
    }
  //}}}
  //{{{
  void drawOverview (ID2D1DeviceContext* dc, int playFrame) {

    float valueScale = mOverviewHeight / 2.f / mSong->getMaxPowerValue();
    float playFrameX = (mSong->getTotalFrames() > 0) ? (playFrame * getWidth()) / mSong->getTotalFrames() : 0.f;

    drawOverviewWave (dc, playFrame, playFrameX, valueScale);

    if (mOverviewPressed) {
      //{{{  animate on
      if (mOverviewLens < getWidth() / 16.f) {
        mOverviewLens += getWidth() / 16.f / 6.f;
        mWindow->changed();
        }
      }
      //}}}
    else {
      //{{{  animate off
      if (mOverviewLens > 1.f) {
        mOverviewLens /= 2.f;
        mWindow->changed();
        }
      else if (mOverviewLens > 0.f) {
        // finish animate
        mOverviewLens = 0.f;
        mWindow->changed();
        }
      }
      //}}}

    if (mOverviewLens > 0.f) {
      float overviewLensCentreX = (float)playFrameX;
      if (overviewLensCentreX - mOverviewLens < 0.f)
        overviewLensCentreX = (float)mOverviewLens;
      else if (overviewLensCentreX + mOverviewLens > getWidth())
        overviewLensCentreX = getWidth() - mOverviewLens;

      drawOverviewLens (dc, playFrame, overviewLensCentreX, mOverviewLens-1.f);
      }
    }
  //}}}
  //{{{
  void drawOverviewWave (ID2D1DeviceContext* dc, int playFrame, float playFrameX, float valueScale) {
  // draw Overview using bitmap cache

    int numFrames = mSong->getNumFrames();
    int totalFrames = mSong->getTotalFrames();

    bool forceRedraw = !mBitmapOverviewOk ||
                       (mSong->getId() != mSongLastId) ||
                       (totalFrames > mOverviewTotalFrames) || (valueScale != mOverviewValueScale);

    if (forceRedraw || (numFrames > mOverviewNumFrames)) {
      mBitmapTarget->BeginDraw();

      if (forceRedraw) {
        // clear overview bitmap
        cRect bitmapRect = { 0.f, mSrcOverviewTop, getWidth(), mSrcOverviewTop + mOverviewHeight };
        mBitmapTarget->PushAxisAlignedClip (bitmapRect, D2D1_ANTIALIAS_MODE_ALIASED);
        mBitmapTarget->Clear ( {0.f,0.f,0.f, 0.f} );
        mBitmapTarget->PopAxisAlignedClip();
        }

      int frame = 0;
      for (auto x = 0; x < getWidthInt(); x++) {
        //{{{  iterate width
        int toFrame = (x * totalFrames) / getWidthInt();
        if (toFrame >= numFrames)
          break;

        if (forceRedraw || (frame >= mOverviewNumFrames)) {
          float* powerValues = mSong->mFrames[frame]->getPowerValues();
          float leftValue = *powerValues++;
          float rightValue = *powerValues;
          if (frame < toFrame) {
            int numSummedFrames = 1;
            frame++;
            while (frame < toFrame) {
              auto powerValues = mSong->mFrames[frame++]->getPowerValues();
              leftValue += *powerValues++;
              rightValue += *powerValues;
              numSummedFrames++;
              }
            leftValue /= numSummedFrames;
            rightValue /= numSummedFrames;
            }

          cRect bitmapRect = { (float)x, mSrcOverviewCentre - (leftValue * valueScale) - 2.f,
                               x+1.f, mSrcOverviewCentre + (rightValue * valueScale) + 2.f };
          mBitmapTarget->FillRectangle (bitmapRect, mWindow->getWhiteBrush());
          }

        frame = toFrame;
        }
        //}}}
      mBitmapTarget->EndDraw();

      mBitmapOverviewOk = true;
      mOverviewNumFrames = numFrames;
      mOverviewTotalFrames = totalFrames;
      mOverviewValueScale = valueScale;
      mSongLastId = mSong->getId();
      }

    // draw overview, stamping colours through alpha bitmap
    dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_ALIASED);

    // before playFrame
    cRect srcRect = { 0.f, mSrcOverviewTop,  playFrameX, mSrcOverviewTop + mOverviewHeight };
    cRect dstRect = { mRect.left, mDstOverviewTop, mRect.left + playFrameX, mDstOverviewTop + mOverviewHeight };
    dc->FillOpacityMask (mBitmap, mWindow->getBlueBrush(), dstRect, srcRect);

    //  draw playFrame
    dstRect = { mRect.left + playFrameX, mDstOverviewTop,
                mRect.left + playFrameX+1.f, mDstOverviewTop + mOverviewHeight };
    auto powerValues = mSong->mFrames[playFrame]->getPowerValues();
    dstRect.top = mDstOverviewCentre - (*powerValues++ * valueScale);
    dstRect.bottom = mDstOverviewCentre + (*powerValues * valueScale);
    dc->FillRectangle (dstRect, mWindow->getWhiteBrush());

    // after playFrame
    srcRect = { playFrameX+1.f, mSrcOverviewTop,  getWidth(), mSrcOverviewTop + mOverviewHeight };
    dstRect = { mRect.left + playFrameX+1.f, mDstOverviewTop, mRect.right, mDstOverviewTop + mOverviewHeight };
    dc->FillOpacityMask (mBitmap, mWindow->getGreyBrush(), dstRect, srcRect);

    dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    }
  //}}}
  //{{{
  void drawOverviewLens (ID2D1DeviceContext* dc, int playFrame, float centreX, float width) {
  // draw frames centred at playFrame -/+ width in pixels, centred at centreX

    // cut hole and frame it
    cRect dstRect = { mRect.left + centreX - mOverviewLens, mDstOverviewTop,
                      mRect.left + centreX + mOverviewLens, mRect.bottom - 1.f };
    dc->FillRectangle (dstRect, mWindow->getBlackBrush());
    dc->DrawRectangle (dstRect, mWindow->getYellowBrush(), 1.f);

    // calc leftmost frame, clip to valid frame, adjust firstX which may overlap left up to mFrameWidth
    float leftFrame = playFrame - width;
    float firstX = centreX - (playFrame - leftFrame);
    if (leftFrame < 0) {
      firstX += -leftFrame;
      leftFrame = 0;
      }

    int rightFrame = (int)(playFrame + width);
    rightFrame = std::min (rightFrame, mSong->getLastFrame());

    // calc lens max power
    float maxPowerValue = 0.f;
    for (auto frame = int(leftFrame); frame <= rightFrame; frame++) {
      auto powerValues = mSong->mFrames[frame]->getPowerValues();
      maxPowerValue = std::max (maxPowerValue, *powerValues++);
      maxPowerValue = std::max (maxPowerValue, *powerValues);
      }

    // simple draw of unzoomed waveform, no use of bitmap cache
    auto colour = mWindow->getBlueBrush();
    float valueScale = mOverviewHeight / 2.f / maxPowerValue;
    dstRect.left = mRect.left + firstX;
    for (auto frame = int(leftFrame); frame <= rightFrame; frame++) {
      dstRect.right = dstRect.left + 1.f;

      if (mSong->mFrames[frame]->hasTitle()) {
        //{{{  draw song title yellow bar and text
        cRect barRect = { dstRect.left-1.f, mDstOverviewTop, dstRect.left+1.f, mRect.bottom };
        dc->FillRectangle (barRect, mWindow->getYellowBrush());

        auto str = mSong->mFrames[frame]->getTitle();

        IDWriteTextLayout* textLayout;
        mWindow->getDwriteFactory()->CreateTextLayout (
          std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
          mWindow->getTextFormat(), getWidth(), mOverviewHeight, &textLayout);
        if (textLayout) {
          dc->DrawTextLayout (cPoint (dstRect.left+2.f, mDstOverviewTop), textLayout, mWindow->getWhiteBrush());
          textLayout->Release();
          }
        }
        //}}}
      if (mSong->mFrames[frame]->isSilent()) {
        //{{{  draw red silent frame
        dstRect.top = mDstOverviewCentre - 2.f;
        dstRect.bottom = mDstOverviewCentre + 2.f;
        dc->FillRectangle (dstRect, mWindow->getRedBrush());
        }
        //}}}

      auto powerValues = mSong->mFrames[frame]->getPowerValues();
      dstRect.top = mDstOverviewCentre - (*powerValues++ * valueScale);
      dstRect.bottom = mDstOverviewCentre + (*powerValues * valueScale);
      if (frame == playFrame)
        colour = mWindow->getWhiteBrush();
      dc->FillRectangle (dstRect, colour);
      if (frame == playFrame)
        colour = mWindow->getGreyBrush();

      dstRect.left = dstRect.right;
      }
    }
  //}}}

  //{{{  private vars
  cSong*& mSong;
  int mSongLastId = 0;

  // vertical layout
  float mFreqHeight = 0.f;
  float mWaveHeight = 0.f;
  float mOverviewHeight = 0.f;

  float mSrcPeakHeight = 0.f;
  float mSrcMarkHeight = 0.f;
  float mSrcSilenceHeight = 0.f;

  float mSrcFreqTop = 0.f;
  float mSrcWaveTop = 0.f;
  float mSrcPeakTop = 0.f;
  float mSrcSilenceTop = 0.f;
  float mSrcMarkTop = 0.f;
  float mSrcOverviewTop = 0.f;
  float mSrcHeight = 0.f;

  float mSrcWaveCentre = 0.f;
  float mSrcPeakCentre = 0.f;
  float mSrcOverviewCentre = 0.f;

  float mDstFreqTop = 0.f;
  float mDstWaveTop = 0.f;
  float mDstOverviewTop = 0.f;

  float mDstWaveCentre = 0.f;
  float mDstOverviewCentre = 0.f;

  // zoom - 0 unity, > 0 zoomOut framesPerPix, < 0 zoomIn pixPerFrame
  int mZoom = 0;
  int mMinZoom = -32;
  int mMaxZoom = 8;

  int mFrameWidth = 1;
  int mFrameStep = 1;
  float mPressedFrame = 0.f;

  bool mBitmapOverviewOk = false;
  bool mOverviewPressed = false;
  int mOverviewNumFrames = 0;
  int mOverviewTotalFrames = 0;
  float mOverviewValueScale = 1.f;
  float mOverviewLens = 0.f;

  bool mBitmapFramesOk = false;
  int mBitmapFrameStep = 0;
  int mBitmapFirstFrame = 0;
  int mBitmapLastFrame = 0;

  uint32_t mBitmapWidth = 0;
  uint32_t mBitmapMask = 0;
  ID2D1BitmapRenderTarget* mBitmapTarget = nullptr;
  ID2D1Bitmap* mBitmap;

  IDWriteTextFormat* mTimeTextFormat = nullptr;
  //}}}
  };
