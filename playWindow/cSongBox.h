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

    // time display font
    mWindow->getDwriteFactory()->CreateTextFormat (L"Consolas", NULL,
      DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 25.f, L"en-us",
      &mSmallTimeTextFormat);

    mWindow->getDwriteFactory()->CreateTextFormat (L"Consolas", NULL,
      DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 45.f, L"en-us",
      &mBigTimeTextFormat);
    mBigTimeTextFormat->SetTextAlignment (DWRITE_TEXT_ALIGNMENT_CENTER);
    }
  //}}}
  //{{{
  virtual ~cSongBox() {

    if (mSmallTimeTextFormat)
      mSmallTimeTextFormat->Release();
    if (mBigTimeTextFormat)
      mBigTimeTextFormat->Release();

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
    mFramesBitmapOk = false;

    // invalidate overview bitmap
    mOverviewBitmapOk = false;
    }
  //}}}
  //{{{
  bool onDown (bool right, cPoint pos)  {

    std::shared_lock<std::shared_mutex> lock (mSong.getSharedMutex());

    if (pos.y > mDstOverviewTop) {
      mOverviewPressed = true;
      mSong.setPlayFrame (mSong.getFirstFrame() + int((pos.x * mSong.getTotalFrames()) / getWidth()));
      }
    else
      mPressedFrame = (float)mSong.getPlayFrame();

    return true;
    }
  //}}}
  //{{{
  bool onMove (bool right, cPoint pos, cPoint inc) {

    std::shared_lock<std::shared_mutex> lock (mSong.getSharedMutex());

    if (mOverviewPressed)
      mSong.setPlayFrame (mSong.getFirstFrame() + int((pos.x * mSong.getTotalFrames()) / getWidth()));
    else {
      mPressedFrame -= (inc.x / mFrameWidth) * mFrameStep;
      mSong.setPlayFrame ((int)mPressedFrame);
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

    std::shared_lock<std::shared_mutex> lock (mSong.getSharedMutex());

    if (!mSong.getNumFrames()) // no frames yet, give up
      return;

    if (mSong.getId() != mSongLastId) {
      mFramesBitmapOk = false;
      mOverviewBitmapOk = false;
      mSongLastId = mSong.getId();
      }

    auto playFrame = mSong.getPlayFrame();

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

    if (mSong.hasHlsBase())
      drawTime (dc, getFrameString (mSong.getFirstFrame()),
                    getFrameString (mSong.getPlayFrame()), getFrameString (mSong.getLastFrame()));
    else
      drawTime (dc, L"", getFrameString (mSong.getPlayFrame()), getFrameString (mSong.getTotalFrames()));
    }
  //}}}

private:
  //{{{
  int getSrcIndex (int frame) {
    return (frame / mFrameStep) & mBitmapMask;
    }
  //}}}
  //{{{
  int getSignedSrcIndex (int frame) {
    if (frame >= 0)
      return (frame / mFrameStep) & mBitmapMask;
    else
      return (-((mFrameStep-1 - frame) / mFrameStep)) & mBitmapMask;
    }
  //}}}
  //{{{
  std::wstring getFrameString (uint64_t frame) {

    if (mSong.getSamplesPerFrame() && mSong.getSampleRate()) {
      uint64_t hundredthSeconds = (frame * mSong.getSamplesPerFrame()) / (mSong.getSampleRate() / 100);

      uint64_t subSeconds = hundredthSeconds % 100;

      hundredthSeconds /= 100;
      uint64_t seconds = hundredthSeconds % 60;

      hundredthSeconds /= 60;
      uint64_t minutes = hundredthSeconds % 60;

      hundredthSeconds /= 60;
      uint64_t hours = hundredthSeconds % 60;

      // !!! must be a better formatter lib !!!
      return (hours > 0) ? (wdec (hours) + L':' + wdec (minutes, 2, '0') + L':' + wdec(seconds, 2, '0')) :
               ((minutes > 0) ? (wdec (minutes) + L':' + wdec(seconds, 2, '0') + L':' + wdec(subSeconds, 2, '0')) :
                 (wdec(seconds) + L':' + wdec(subSeconds, 2, '0')));
      }
    else
      return (L"--:--:--");
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
  bool drawBitmapFrames (int fromFrame, int toFrame, int playFrame, int rightFrame) {

    //cLog::log (LOGINFO, "drawFrameToBitmap %d %d %d", fromFrame, toFrame, playFrame);
    bool allFramesOk = true;
    auto firstFrame = mSong.getFirstFrame();

    // clear bitmap as chunks
    auto fromSrcIndex = (float)getSignedSrcIndex (fromFrame);
    auto toSrcIndex = (float)getSrcIndex (toFrame);
    bool wrap = toSrcIndex <= fromSrcIndex;
    float endSrcIndex = wrap ? float(mBitmapWidth) : toSrcIndex;

    mBitmapTarget->BeginDraw();
    cRect bitmapRect;
    //{{{  clear bitmap chunk before wrap
    bitmapRect = { fromSrcIndex, mSrcFreqTop, endSrcIndex, mSrcOverviewTop };
    mBitmapTarget->PushAxisAlignedClip (bitmapRect, D2D1_ANTIALIAS_MODE_ALIASED);
    mBitmapTarget->Clear ( { 0.f,0.f,0.f, 0.f } );
    mBitmapTarget->PopAxisAlignedClip();
    //}}}
    if (wrap) {
      //{{{  clear bitmap chunk after wrap
      bitmapRect = { 0.f, mSrcFreqTop, toSrcIndex, mSrcOverviewTop };
      mBitmapTarget->PushAxisAlignedClip (bitmapRect, D2D1_ANTIALIAS_MODE_ALIASED);
      mBitmapTarget->Clear ( { 0.f,0.f,0.f, 0.f } );
      mBitmapTarget->PopAxisAlignedClip();
      }
      //}}}

    // draw bitmap as frames
    for (auto frame = fromFrame; frame < toFrame; frame += mFrameStep) {
      //{{{  draw bitmap for frame
      bool mark = false;
      bool silence = false;
      float powerValues[2];
      float peakValues[2];
      float srcIndex = (float)getSrcIndex (frame);

      if (mFrameStep == 1) {
        //{{{  simple case, draw peak and power scaled to maxPeak
        auto framePtr = mSong.getFramePtr (frame);
        if (framePtr) {
          if (framePtr->getPowerValues()) {
            float valueScale = mWaveHeight / 2.f / mSong.getMaxPeakValue();
            mark = framePtr->hasTitle();
            silence = framePtr->isSilence();

            auto powerValuesPtr = framePtr->getPowerValues();
            auto peakValuesPtr =  framePtr->getPeakValues();
            for (auto i = 0; i < 2; i++) {
              powerValues[i] = *powerValuesPtr++ * valueScale;
              peakValues[i] = *peakValuesPtr++ * valueScale;
              }

            bitmapRect = { srcIndex, mSrcPeakCentre - peakValues[0], srcIndex + 1.f, mSrcPeakCentre + peakValues[1] };
            mBitmapTarget->FillRectangle (bitmapRect, mWindow->getWhiteBrush());
            }
          }
        else
          allFramesOk = false;
        }
        //}}}
      else {
        //{{{  sum mFrameStep frames, mFrameStep aligned, just draw power scaled to maxPower
        float valueScale = mWaveHeight / 2.f / mSong.getMaxPowerValue();
        silence = false;
        mark = false;

        for (auto i = 0; i < 2; i++)
          powerValues[i] = 0.f;

        auto alignedFrame = frame - (frame % mFrameStep);
        auto toSumFrame = std::min (alignedFrame + mFrameStep, rightFrame);
        for (auto sumFrame = alignedFrame; sumFrame < toSumFrame; sumFrame++) {
          auto framePtr = mSong.getFramePtr (sumFrame);
          if (framePtr) {
            mark |= framePtr->hasTitle();
            silence |= framePtr->isSilence();
            if (framePtr->getPowerValues()) {
              auto powerValuesPtr = framePtr->getPowerValues();
              for (auto i = 0; i < 2; i++)
                powerValues[i] += *powerValuesPtr++ * valueScale;
              }
            }
          else
            allFramesOk = false;
          }

        for (auto i = 0; i < 2; i++)
          powerValues[i] /= toSumFrame - alignedFrame + 1;
        }
        //}}}
      bitmapRect = { srcIndex, mSrcWaveCentre - powerValues[0], srcIndex + 1.f, mSrcWaveCentre + powerValues[1] };
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
    mBitmapTarget->EndDraw();

    // copy reversed spectrum column to bitmap, clip high freqs to height
    int freqSize = std::min (mSong.getNumFreqBytes(), (int)mFreqHeight);
    int freqOffset = mSong.getNumFreqBytes() > (int)mFreqHeight ? mSong.getNumFreqBytes() - (int)mFreqHeight : 0;

    // bitmap sampled aligned to mFrameStep, !!! could sum !!! ?? ok if neg frame ???
    auto alignedFromFrame = fromFrame - (fromFrame % mFrameStep);
    for (auto frame = alignedFromFrame; frame < toFrame; frame += mFrameStep) {
      auto framePtr = mSong.getFramePtr (frame);
      if (framePtr && framePtr->getFreqLuma()) {
        uint32_t bitmapIndex = getSrcIndex (frame);
        D2D1_RECT_U bitmapRectU = { bitmapIndex, 0, bitmapIndex+1, (UINT32)freqSize };
        mBitmap->CopyFromMemory (&bitmapRectU, framePtr->getFreqLuma() + freqOffset, 1);
        }
      }

    return allFramesOk;
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
      mFramesBitmapOk = false;
      mOverviewBitmapOk = false;

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

    // calc leftFrame,rightFrame
    auto leftFrame = playFrame - (((getWidthInt()+mFrameWidth)/2) * mFrameStep) / mFrameWidth;
    //std::max (playFrame - (((getWidthInt()+mFrameWidth)/2) * mFrameStep) / mFrameWidth, mSong.getFirstFrame());
    auto rightFrame =
      std::min (playFrame + (((getWidthInt()+mFrameWidth)/2) * mFrameStep) / mFrameWidth, mSong.getLastFrame());

    bool allFramesOk = true;
    if (mFramesBitmapOk &&
        (mFrameStep == mBitmapFrameStep) &&
        (rightFrame > mBitmapFirstFrame) && (leftFrame < mBitmapLastFrame)) {
      // overlap
      if (leftFrame < mBitmapFirstFrame) {
        //{{{  draw new bitmap leftFrames
        allFramesOk &= drawBitmapFrames (leftFrame, mBitmapFirstFrame, playFrame, rightFrame);

        mBitmapFirstFrame = leftFrame;
        if (mBitmapLastFrame - mBitmapFirstFrame > (int)mBitmapWidth)
          mBitmapLastFrame = mBitmapFirstFrame + mBitmapWidth;
        }
        //}}}
      if (rightFrame > mBitmapLastFrame) {
        //{{{  draw new bitmap rightFrames
        allFramesOk &= drawBitmapFrames (mBitmapLastFrame, rightFrame, playFrame, rightFrame);

        mBitmapLastFrame = rightFrame;
        if (mBitmapLastFrame - mBitmapFirstFrame > (int)mBitmapWidth)
          mBitmapFirstFrame = mBitmapLastFrame - mBitmapWidth;
        }
        //}}}
      }
    else {
      //{{{  no overlap, draw all bitmap frames
      allFramesOk &= drawBitmapFrames (leftFrame, rightFrame, playFrame, rightFrame);

      mBitmapFirstFrame = leftFrame;
      mBitmapLastFrame = rightFrame;
      }
      //}}}
    mFramesBitmapOk = allFramesOk;
    mBitmapFrameStep = mFrameStep;

    // calc bitmap wrap chunks
    float leftSrcIndex = (float)getSrcIndex (leftFrame);
    float rightSrcIndex = (float)getSrcIndex (rightFrame);
    float playSrcIndex = (float)getSrcIndex (playFrame);

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
    dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    //{{{  draw playFrame
    auto framePtr = mSong.getFramePtr (playFrame);
    if (framePtr) {
      float valueScale = mWaveHeight / 2.f / mSong.getMaxPeakValue();
      auto peakValues = framePtr->getPeakValues();

      dstRect = { mRect.left + (dstPlay * mFrameWidth)-1.f, mDstWaveCentre - (*peakValues++ * valueScale),
                  mRect.left + ((dstPlay+1.f) * mFrameWidth)+1.f, mDstWaveCentre + (*peakValues * valueScale) };
      dc->FillRectangle (dstRect, mWindow->getGreenBrush());
      }
    //}}}
    }
  //}}}
  //{{{
  void drawFreq (ID2D1DeviceContext* dc, int playFrame) {

    float valueScale = 100.f / 255.f;

    auto framePtr = mSong.getFramePtr (playFrame);
    if (framePtr && framePtr->getFreqValues()) {
      auto freqValues = framePtr->getFreqValues();
      for (auto i = 0; (i < mSong.getNumFreqBytes()) && ((i*2) < getWidthInt()); i++) {
        auto value =  freqValues[i] * valueScale;
        if (value > 1.f)  {
          cRect dstRect = { mRect.left + (i*2),mRect.bottom - value, mRect.left + ((i+1) * 2),mRect.bottom };
          dc->FillRectangle (dstRect, mWindow->getYellowBrush());
          }
        }
      }
    }
  //}}}
  //{{{
  void drawOverview (ID2D1DeviceContext* dc, int playFrame) {

    if (!mSong.getTotalFrames())
      return;

    int firstFrame = mSong.getFirstFrame();
    float playFrameX = ((playFrame - firstFrame) * getWidth()) / mSong.getTotalFrames();
    float valueScale = mOverviewHeight / 2.f / mSong.getMaxPowerValue();
    drawOverviewWave (dc, firstFrame, playFrame, playFrameX, valueScale);

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
    else {
      //  draw playFrame
      auto framePtr = mSong.getFramePtr (playFrame);
      if (framePtr && framePtr->getPowerValues()) {
        auto powerValues = framePtr->getPowerValues();
        cRect dstRect = { mRect.left + playFrameX, mDstOverviewCentre - (*powerValues++ * valueScale),
                          mRect.left + playFrameX+1.f, mDstOverviewCentre + (*powerValues * valueScale) };
        dc->FillRectangle (dstRect, mWindow->getWhiteBrush());
        }
      }
    }
  //}}}
  //{{{
  void drawOverviewWave (ID2D1DeviceContext* dc, int firstFrame, int playFrame, float playFrameX, float valueScale) {
  // draw Overview using bitmap cache

    int lastFrame = mSong.getLastFrame();
    int totalFrames = mSong.getTotalFrames();

    bool forceRedraw = !mOverviewBitmapOk ||
                       (valueScale != mOverviewValueScale) ||
                       (firstFrame != mOverviewFirstFrame) || (totalFrames > mOverviewTotalFrames);

    if (forceRedraw || (lastFrame > mOverviewLastFrame)) {
      mBitmapTarget->BeginDraw();
      if (forceRedraw) {
        //{{{  clear overview bitmap
        cRect bitmapRect = { 0.f, mSrcOverviewTop, getWidth(), mSrcOverviewTop + mOverviewHeight };
        mBitmapTarget->PushAxisAlignedClip (bitmapRect, D2D1_ANTIALIAS_MODE_ALIASED);
        mBitmapTarget->Clear ( {0.f,0.f,0.f, 0.f} );
        mBitmapTarget->PopAxisAlignedClip();
        }
        //}}}

      bool allFramesOk = true;
      for (auto x = 0; x < getWidthInt(); x++) {
        //{{{  iterate widget width
        int frame = firstFrame + ((x * totalFrames) / getWidthInt());
        int toFrame = firstFrame + (((x+1) * totalFrames) / getWidthInt());
        if (toFrame > lastFrame)
          toFrame = lastFrame+1;

        if (forceRedraw || (frame >= mOverviewLastFrame)) {
          auto framePtr = mSong.getFramePtr (frame);
          if (framePtr) {
            // !!! should accumulate silence and distinguish from silence better !!!
            if (framePtr->getPowerValues()) {
              float* powerValues = framePtr->getPowerValues();
              float leftValue = *powerValues++;
              float rightValue = *powerValues;
              if (frame < toFrame) {
                int numSummedFrames = 1;
                frame++;
                while (frame < toFrame) {
                  framePtr = mSong.getFramePtr (frame);
                  if (framePtr) {
                    if (framePtr->getPowerValues()) {
                      auto powerValues = framePtr->getPowerValues();
                      leftValue += *powerValues++;
                      rightValue += *powerValues;
                      numSummedFrames++;
                      }
                    }
                  else
                    allFramesOk = false;
                  frame++;
                  }
                leftValue /= numSummedFrames;
                rightValue /= numSummedFrames;
                }

              cRect bitmapRect = { (float)x, mSrcOverviewCentre - (leftValue * valueScale) - 2.f,
                                   x+1.f, mSrcOverviewCentre + (rightValue * valueScale) + 2.f };
              mBitmapTarget->FillRectangle (bitmapRect, mWindow->getWhiteBrush());
              }
            else
              allFramesOk = false;
            }
          }
        }
        //}}}
      mBitmapTarget->EndDraw();

      mOverviewBitmapOk = allFramesOk;
      mOverviewValueScale = valueScale;
      mOverviewFirstFrame = firstFrame;
      mOverviewLastFrame = lastFrame;
      mOverviewTotalFrames = totalFrames;
      }

    // draw overview, stamping colours through alpha bitmap
    dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_ALIASED);

    // before playFrame
    cRect srcRect = { 0.f, mSrcOverviewTop,  playFrameX, mSrcOverviewTop + mOverviewHeight };
    cRect dstRect = { mRect.left, mDstOverviewTop, mRect.left + playFrameX, mDstOverviewTop + mOverviewHeight };
    dc->FillOpacityMask (mBitmap, mWindow->getBlueBrush(), dstRect, srcRect);

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
    rightFrame = std::min (rightFrame, mSong.getLastFrame());

    // calc lens max power
    float maxPowerValue = 0.f;
    for (auto frame = int(leftFrame); frame <= rightFrame; frame++) {
      auto framePtr = mSong.getFramePtr (frame);
      if (framePtr && framePtr->getPowerValues()) {
        auto powerValues = framePtr->getPowerValues();
        maxPowerValue = std::max (maxPowerValue, *powerValues++);
        maxPowerValue = std::max (maxPowerValue, *powerValues);
        }
      }

    // simple draw of unzoomed waveform, no use of bitmap cache
    auto colour = mWindow->getBlueBrush();
    float valueScale = mOverviewHeight / 2.f / maxPowerValue;
    dstRect.left = mRect.left + firstX;
    for (auto frame = int(leftFrame); frame <= rightFrame; frame++) {
      dstRect.right = dstRect.left + 1.f;

      auto framePtr = mSong.getFramePtr (frame);
      if (framePtr && framePtr->getPowerValues()) {
        if (framePtr->hasTitle()) {
          //{{{  draw song title yellow bar and text
          cRect barRect = { dstRect.left-1.f, mDstOverviewTop, dstRect.left+1.f, mRect.bottom };
          dc->FillRectangle (barRect, mWindow->getYellowBrush());

          auto str = framePtr->getTitle();
          dc->DrawText (std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(), mWindow->getTextFormat(),
                        { dstRect.left+2.f, mDstOverviewTop, getWidth(), mDstOverviewTop + mOverviewHeight },
                        mWindow->getWhiteBrush());
          }
          //}}}
        if (framePtr->isSilence()) {
          //{{{  draw red silence
          dstRect.top = mDstOverviewCentre - 2.f;
          dstRect.bottom = mDstOverviewCentre + 2.f;
          dc->FillRectangle (dstRect, mWindow->getRedBrush());
          }
          //}}}

        auto powerValues = framePtr->getPowerValues();
        dstRect.top = mDstOverviewCentre - (*powerValues++ * valueScale);
        dstRect.bottom = mDstOverviewCentre + (*powerValues * valueScale);
        if (frame == playFrame)
          colour = mWindow->getWhiteBrush();
        dc->FillRectangle (dstRect, colour);
        if (frame == playFrame)
          colour = mWindow->getGreyBrush();
       }

      dstRect.left = dstRect.right;
      }
    }
  //}}}
  //{{{
  void drawTime (ID2D1DeviceContext* dc, const std::wstring& first, const std::wstring& play, const std::wstring& last) {

    // big play, centred
    cRect dstRect = mRect;
    dstRect.top = mRect.bottom - mBigTimeTextFormat->GetFontSize();
    dc->DrawText (play.data(), (uint32_t)play.size(), mBigTimeTextFormat, dstRect, mWindow->getWhiteBrush());

    // small first, left
    mSmallTimeTextFormat->SetTextAlignment (DWRITE_TEXT_ALIGNMENT_LEADING);
    dstRect.top = mRect.bottom - mSmallTimeTextFormat->GetFontSize();
    dc->DrawText (first.data(), (uint32_t)first.size(), mSmallTimeTextFormat, dstRect, mWindow->getWhiteBrush());

    // small coloured last, right
    mSmallTimeTextFormat->SetTextAlignment (DWRITE_TEXT_ALIGNMENT_TRAILING);
    dstRect.top = mRect.bottom - mSmallTimeTextFormat->GetFontSize();
    dc->DrawText (last.data(), (uint32_t)last.size(), mSmallTimeTextFormat, dstRect,
                  (mSong.getHlsLoad() == cSong::eHlsIdle) ? mWindow->getWhiteBrush() :
                    (mSong.getHlsLoad() == cSong::eHlsFailed) ? mWindow->getRedBrush() : mWindow->getGreenBrush());
    }
  //}}}

  //{{{  private vars
  cSong& mSong;
  int mSongLastId = 0;

  // zoom - 0 unity, > 0 zoomOut framesPerPix, < 0 zoomIn pixPerFrame
  int mZoom = 0;
  int mMinZoom = -8;
  int mMaxZoom = 8;
  int mFrameWidth = 1;
  int mFrameStep = 1;

  float mPressedFrame = 0.f;

  bool mFramesBitmapOk = false;
  int mBitmapFirstFrame = 0;
  int mBitmapLastFrame = 0;
  int mBitmapFrameStep = 1;

  bool mOverviewBitmapOk = false;
  bool mOverviewPressed = false;
  int mOverviewFirstFrame = 0;
  int mOverviewLastFrame = 0;
  int mOverviewTotalFrames = 0;
  float mOverviewValueScale = 1.f;
  float mOverviewLens = 0.f;

  uint32_t mBitmapWidth = 0;
  uint32_t mBitmapMask = 0;
  ID2D1Bitmap* mBitmap = nullptr;
  ID2D1BitmapRenderTarget* mBitmapTarget = nullptr;

  IDWriteTextFormat* mSmallTimeTextFormat = nullptr;
  IDWriteTextFormat* mBigTimeTextFormat = nullptr;

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

  //}}}
  };
