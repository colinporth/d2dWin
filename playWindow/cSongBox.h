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

    if (pos.y > mDstOverviewTop) {
      mOverviewPressed = true;
      auto frame = int((pos.x * mSong.getTotalFrames()) / getWidth());
      mSong.setPlayFrame (frame);
      }

    return true;
    }
  //}}}
  //{{{
  bool onMove (bool right, cPoint pos, cPoint inc) {

    if (mOverviewPressed)
      mSong.setPlayFrame (int((pos.x * mSong.getTotalFrames()) / getWidth()));
    else
      mSong.incPlayFrame (int(-inc.x));

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
      mZoom = std::min (std::max (mZoom - (delta/120), mSong.getMinZoomIndex()), mSong.getMaxZoomIndex());
      return true;
      }

    return false;
    }
  //}}}
  //{{{
  void onDraw (ID2D1DeviceContext* dc) {
  // draw stuff centred at playFrame

    if (!mSong.getNumFrames()) // no frames yet, give up
      return;

    auto playFrame = mSong.getPlayFrame();

    // src bitmap explicit layout
    mSrcHeight = getHeight();
    mWaveHeight = 98.f;
    mSrcSilenceHeight = 2.f;
    mSrcMarkHeight = 2.f;
    mOverviewHeight = 98.f;
    mFreqHeight = mSrcHeight - mWaveHeight - mSrcSilenceHeight - mSrcMarkHeight - mOverviewHeight;

    mSrcFreqTop = 0.f;
    mSrcWaveTop = mSrcFreqTop + mFreqHeight;
    mSrcWaveCentre = mSrcWaveTop + (mWaveHeight/2.f);
    mSrcSilenceTop = mSrcWaveTop + mWaveHeight;
    mSrcMarkTop = mSrcSilenceTop + mSrcSilenceHeight;
    mSrcOverviewTop = mSrcMarkTop + mSrcMarkHeight;
    mSrcOverviewCentre = mSrcOverviewTop + (mOverviewHeight/2.f);

    // dst box explicit layout
    mDstFreqTop = mRect.top;
    mDstWaveTop = mDstFreqTop + mFreqHeight + mSrcSilenceHeight;
    mDstWaveCentre = mDstWaveTop + (mWaveHeight/2.f);
    mDstOverviewTop = mDstWaveTop + mWaveHeight + mSrcMarkHeight;
    mDstOverviewCentre = mDstOverviewTop + (mOverviewHeight/2.f);

    reallocBitmap (mWindow->getDc());

    // draw
    drawWave (dc, playFrame);
    drawOverview (dc, playFrame);
    drawFreq (dc, playFrame);
    drawTime (dc, frameString (playFrame) + " " + frameString (mSong.getTotalFrames()));
    }
  //}}}

private:
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
  void drawBitmapFrames (int fromFrame, int toFrame, int playFrame, int rightFrame, int frameStep, float valueScale) {

    //cLog::log (LOGINFO, "drawFrameToBitmap %d %d %d", fromFrame, toFrame, playFrame);
    cRect bitmapRect;
    if (fromFrame < 0) {  // !!! div for neg maybe wrong !!!!
      //{{{  clear bitmap for -ve frames, allows simpler drawing logic later
      bitmapRect = { (float)((fromFrame / frameStep) & mBitmapMask), mSrcFreqTop, (float)mBitmapWidth, mSrcOverviewTop };
      mBitmapTarget->PushAxisAlignedClip (bitmapRect, D2D1_ANTIALIAS_MODE_ALIASED);
      mBitmapTarget->Clear ( { 0.f,0.f,0.f, 0.f } );
      mBitmapTarget->PopAxisAlignedClip();
      fromFrame = 0;
      }
      //}}}

    // clear bitmap as chunks
    auto fromSrcIndex = float((fromFrame / frameStep) & mBitmapMask);
    auto toSrcIndex = float((toFrame / frameStep) & mBitmapMask);
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
    for (auto frame = fromFrame; frame < toFrame; frame += frameStep) {
      //{{{  draw bitmap for frame
      float values[2];
      bool silence;
      bool mark;

      if (frameStep == 1) {
        // simple case
        silence = mSong.mFrames[frame]->isSilent();
        mark = mSong.mFrames[frame]->hasTitle();

        auto powerValues = mSong.mFrames[frame]->getPowerValues();
        for (auto i = 0; i < 2; i++)
          values[i] = *powerValues++;
        }
      else {
        // sum frameStep frames, frameStep aligned
        silence = false;
        mark = false;
        for (auto i = 0; i < 2; i++)
          values[i] = 0.f;

        auto alignedFrame = frame - (frame % frameStep);
        auto toSumFrame = std::min (alignedFrame + frameStep, rightFrame);
        for (auto sumFrame = alignedFrame; sumFrame < toSumFrame; sumFrame++) {
          silence |= mSong.mFrames[frame]->isSilent();
          mark |= mSong.mFrames[frame]->hasTitle();

          auto powerValues = mSong.mFrames[sumFrame]->getPowerValues();
          for (auto i = 0; i < 2; i++)
            values[i] += *powerValues++;
          }

        for (auto i = 0; i < 2; i++)
          values[i] /= toSumFrame - alignedFrame + 1;
        }

      float srcIndex = float((frame / frameStep) & mBitmapMask);
      bitmapRect = { srcIndex, mSrcWaveCentre - (values[0] * valueScale),
                     srcIndex + 1.f, mSrcWaveCentre + (values[1] * valueScale) };
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
    int freqSize = std::min (mSong.getNumFreqLuma(), (int)mFreqHeight);
    int freqOffset = mSong.getNumFreqLuma() > (int)mFreqHeight ? mSong.getNumFreqLuma() - (int)mFreqHeight : 0;

    if (frameStep == 1) {
      for (auto frame = fromFrame; frame < toFrame; frame += frameStep) {
        uint32_t bitmapIndex = (frame / frameStep) & mBitmapMask;
        D2D1_RECT_U bitmapRectU = { bitmapIndex, 0, bitmapIndex+1, (UINT32)freqSize };
        mBitmap->CopyFromMemory (&bitmapRectU, mSong.mFrames[frame]->getFreqLuma() + freqOffset, 1);
        }
      }
    else {
      // align to frameStep, could sum as well
      auto alignedFromFrame = fromFrame - (fromFrame % frameStep);
      for (auto frame = alignedFromFrame; frame < toFrame; frame += frameStep) {
        uint32_t bitmapIndex = (frame / frameStep) & mBitmapMask;
        D2D1_RECT_U bitmapRectU = { bitmapIndex, 0, bitmapIndex+1, (UINT32)freqSize };
        mBitmap->CopyFromMemory (&bitmapRectU, mSong.mFrames[frame]->getFreqLuma() + freqOffset, 1);
        }
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

    mBitmapTarget->BeginDraw();
    if (mBitmapFramesOk &&
        (frameStep == mBitmapFrameStep) &&
        (rightFrame > mBitmapFirstFrame) && (leftFrame < mBitmapLastFrame)) {
      // overlap
      if (leftFrame < mBitmapFirstFrame) {
        //{{{  draw new bitmap leftFrames
        drawBitmapFrames (leftFrame, mBitmapFirstFrame, playFrame, rightFrame, frameStep, valueScale);
        mBitmapFirstFrame = leftFrame;
        if (mBitmapLastFrame - mBitmapFirstFrame > (int)mBitmapWidth)
          mBitmapLastFrame = mBitmapFirstFrame + mBitmapWidth;
        }
        //}}}
      if (rightFrame > mBitmapLastFrame) {
        //{{{  draw new bitmap rightFrames
        drawBitmapFrames (mBitmapLastFrame, rightFrame, playFrame, rightFrame, frameStep, valueScale);
        mBitmapLastFrame = rightFrame;
        if (mBitmapLastFrame - mBitmapFirstFrame > (int)mBitmapWidth)
          mBitmapFirstFrame = mBitmapLastFrame - mBitmapWidth;
        }
        //}}}
      }
    else {
      //{{{  no overlap, draw all bitmap frames
      drawBitmapFrames (leftFrame, rightFrame, playFrame, rightFrame, frameStep, valueScale);
      mBitmapFirstFrame = leftFrame;
      mBitmapLastFrame = rightFrame;
      }
      //}}}
    mBitmapFramesOk = true;
    mBitmapFrameStep = frameStep;
    mBitmapTarget->EndDraw();

    // calc bitmap wrap chunks
    float leftSrcIndex = (float)((leftFrame / frameStep) & mBitmapMask);
    float rightSrcIndex = (float)((rightFrame / frameStep) & mBitmapMask);
    float playSrcIndex = (float)((playFrame / frameStep) & mBitmapMask);

    bool wrap = rightSrcIndex <= leftSrcIndex;
    float endSrcIndex = wrap ? float(mBitmapWidth) : rightSrcIndex;

    //  draw dst chunks, mostly stamping colour through alpha bitmap
    cRect srcRect;
    cRect dstRect;
    dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_ALIASED);
    //{{{  stamp playFrame mark
    auto dstPlay = playSrcIndex - leftSrcIndex + (playSrcIndex < leftSrcIndex ? endSrcIndex : 0);
    dstRect = { mRect.left + (dstPlay+0.5f) * frameWidth, mDstFreqTop,
                mRect.left + ((dstPlay+0.5f) * frameWidth) + 1.f, mDstWaveTop + mWaveHeight };
    dc->FillRectangle (dstRect, mWindow->getDarkGreyBrush());
    //}}}
    //{{{  stamp chunk before wrap
    // freq
    srcRect = { leftSrcIndex, mSrcFreqTop, endSrcIndex, mSrcFreqTop + mFreqHeight };
    dstRect = { mRect.left, mDstFreqTop,
                mRect.left + (endSrcIndex - leftSrcIndex) * frameWidth, mDstFreqTop + mFreqHeight };
    dc->FillOpacityMask (mBitmap, mWindow->getWhiteBrush(), dstRect, srcRect);

    // silence
    srcRect = { leftSrcIndex, mSrcSilenceTop, endSrcIndex, mSrcSilenceTop + mSrcSilenceHeight };
    dstRect = { mRect.left, mDstWaveCentre - 2.f,
                mRect.left + (endSrcIndex - leftSrcIndex) * frameWidth, mDstWaveCentre + 2.f, };
    dc->FillOpacityMask (mBitmap, mWindow->getRedBrush(), dstRect, srcRect);

    // mark
    srcRect = { leftSrcIndex, mSrcMarkTop, endSrcIndex, mSrcMarkTop + 1.f };
    dstRect = { mRect.left, mDstWaveTop,
                mRect.left + (endSrcIndex - leftSrcIndex) * frameWidth, mDstWaveTop + mWaveHeight };
    dc->FillOpacityMask (mBitmap, mWindow->getYellowBrush(), dstRect, srcRect);

    // wave
    bool split = (playSrcIndex >= leftSrcIndex) && (playSrcIndex < endSrcIndex);

    // wave chunk before play
    srcRect = { leftSrcIndex, mSrcWaveTop, (split ? playSrcIndex : endSrcIndex), mSrcWaveTop + mWaveHeight };
    dstRect = { mRect.left, mDstWaveTop,
                mRect.left + ((split ? playSrcIndex : endSrcIndex) - leftSrcIndex) * frameWidth, mDstWaveTop + mWaveHeight };
    dc->FillOpacityMask (mBitmap, mWindow->getBlueBrush(), dstRect, srcRect);

    if (split) {
      // split wave chunk after play
      srcRect = { playSrcIndex+1.f, mSrcWaveTop, endSrcIndex, mSrcWaveTop + mWaveHeight };
      dstRect = { mRect.left + (playSrcIndex+1.f - leftSrcIndex) * frameWidth, mDstWaveTop,
                  mRect.left + (endSrcIndex - leftSrcIndex) * frameWidth, mDstWaveTop + mWaveHeight };
      dc->FillOpacityMask (mBitmap, mWindow->getGreyBrush(), dstRect, srcRect);
      }
    //}}}
    if (wrap) {
      //{{{  stamp second chunk after wrap
      // Freq
      srcRect = { 0.f, mSrcFreqTop,  rightSrcIndex, mSrcFreqTop + mFreqHeight };
      dstRect = { mRect.left + (endSrcIndex - leftSrcIndex) * frameWidth, mDstFreqTop,
                   mRect.left + (endSrcIndex - leftSrcIndex + rightSrcIndex) * frameWidth, mDstFreqTop + mFreqHeight };
      dc->FillOpacityMask (mBitmap, mWindow->getWhiteBrush(), dstRect, srcRect);

      // silence
      srcRect = { 0.f, mSrcSilenceTop, rightSrcIndex, mSrcSilenceTop + mSrcSilenceHeight };
      dstRect = { mRect.left + (endSrcIndex - leftSrcIndex) * frameWidth, mDstWaveCentre - 2.f,
                  mRect.left + (endSrcIndex - leftSrcIndex + rightSrcIndex) * frameWidth, mDstWaveCentre + 2.f };
      dc->FillOpacityMask (mBitmap, mWindow->getRedBrush(), dstRect, srcRect);

      // mark
      srcRect = { 0.f, mSrcMarkTop,  rightSrcIndex, mSrcMarkTop + 1.f };
      dstRect = { mRect.left + (endSrcIndex - leftSrcIndex) * frameWidth, mDstWaveTop,
                  mRect.left + (endSrcIndex - leftSrcIndex + rightSrcIndex) * frameWidth, mDstWaveTop + mWaveHeight };
      dc->FillOpacityMask (mBitmap, mWindow->getYellowBrush(), dstRect, srcRect);

      // wave
      bool split = playSrcIndex < rightSrcIndex;
      if (split) {
        // split chunk before play
        srcRect = { 0.f, mSrcWaveTop,  playSrcIndex, mSrcWaveTop + mWaveHeight };
        dstRect = { mRect.left + (endSrcIndex - leftSrcIndex) * frameWidth, mDstWaveTop,
                    mRect.left + (endSrcIndex - leftSrcIndex + playSrcIndex) * frameWidth, mDstWaveTop + mWaveHeight };
        dc->FillOpacityMask (mBitmap, mWindow->getBlueBrush(), dstRect, srcRect);
        }

      // chunk after play
      srcRect = { split ? playSrcIndex+1.f : 0.f, mSrcWaveTop,  rightSrcIndex, mSrcWaveTop + mWaveHeight };
      dstRect = { mRect.left + (endSrcIndex - leftSrcIndex + (split ? (playSrcIndex+1.f) : 0.f)) * frameWidth, mDstWaveTop,
                  mRect.left + (endSrcIndex - leftSrcIndex + rightSrcIndex) * frameWidth, mDstWaveTop + mWaveHeight };
      dc->FillOpacityMask (mBitmap, mWindow->getGreyBrush(), dstRect, srcRect);
      }
      //}}}

    //{{{  draw playFrame
    auto powerValues = mSong.mFrames[playFrame]->getPowerValues();
    dstRect = { mRect.left + dstPlay * frameWidth, mDstWaveCentre - (*powerValues++ * valueScale),
                mRect.left + (dstPlay+1.f) * frameWidth, mDstWaveCentre + (*powerValues * valueScale) };
    dc->FillRectangle (dstRect, mWindow->getWhiteBrush());
    //}}}

    dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    }
  //}}}

  //{{{
  void drawFreq (ID2D1DeviceContext* dc, int playFrame) {

    if (mSong.getMaxFreqValue() > 0.f) {
      auto valueScale = mOverviewHeight / mSong.getMaxFreqValue();
      auto maxFreqIndex = std::min (getWidthInt()/2, mSong.getNumFreq());

      cRect dstRect = { 0.f,0.f, 0.f,mRect.bottom };
      auto freq = mSong.mFrames[playFrame]->getFreqValues();
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
  std::string frameString (uint32_t frame) {

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

      drawOverviewLens (dc, playFrame, overviewLensCentreX, mOverviewLens-1.f, valueScale);
      }
    }
  //}}}
  //{{{
  void drawOverviewWave (ID2D1DeviceContext* dc, int playFrame, float playFrameX, float valueScale) {
  // draw Overview using bitmap cache

    int numFrames = mSong.getNumFrames();
    int totalFrames = mSong.getTotalFrames();

    bool forceRedraw = !mBitmapOverviewOk ||
                       (mSong.getId() != mSongId) ||
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
      mSongId = mSong.getId();
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
    auto powerValues = mSong.mFrames[playFrame]->getPowerValues();
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
  void drawOverviewLens (ID2D1DeviceContext* dc, int playFrame, float centreX, float width, float valueScale) {
  // draw frames centred at playFrame -/+ width in pixels, centred at centreX

    // cut hole and frame it
    cRect dstRect = { mRect.left + centreX - mOverviewLens, mDstOverviewTop,
                      mRect.left + centreX + mOverviewLens, mRect.bottom - 1.f };
    dc->FillRectangle (dstRect, mWindow->getBlackBrush());
    dc->DrawRectangle (dstRect, mWindow->getYellowBrush(), 1.f);

    // calc leftmost frame, clip to valid frame, adjust firstX which may overlap left up to frameWidth
    float leftFrame = playFrame - width;
    float firstX = centreX - (playFrame - leftFrame);
    if (leftFrame < 0) {
      firstX += -leftFrame;
      leftFrame = 0;
      }

    // simple draw of unzoomed waveform, no use of bitmap cache
    auto colour = mWindow->getBlueBrush();

    int frame = (int)leftFrame;
    int rightFrame = (int)(playFrame + width);
    int lastFrame = std::min (rightFrame, mSong.getLastFrame());

    dstRect.left = mRect.left + firstX;
    while ((dstRect.left < mRect.right) && (frame <= lastFrame)) {
      dstRect.right = dstRect.left + 1.f;

      if (mSong.mFrames[frame]->hasTitle()) {
        //{{{  draw song title yellow bar and text
        dc->FillRectangle (cRect (dstRect.left-1.f, mDstOverviewTop, dstRect.left+1.f, mRect.bottom),
                           mWindow->getYellowBrush());

        auto str = mSong.mFrames[frame]->getTitle();

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
      if (mSong.mFrames[frame]->isSilent()) {
        //{{{  draw red silent frame
        dstRect.top = mDstOverviewCentre - 2.f;
        dstRect.bottom = mDstOverviewCentre + 2.f;
        dc->FillRectangle (dstRect, mWindow->getRedBrush());
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

      dstRect.top = mDstOverviewCentre - (leftValue * valueScale);
      dstRect.bottom = mDstOverviewCentre + (rightValue * valueScale);
      dc->FillRectangle (dstRect, colour);

      dstRect.left = dstRect.right;
      frame++;
      }
    }
  //}}}

  //{{{  private vars
  cSong& mSong;

  // vertical layout
  float mFreqHeight = 0.f;
  float mWaveHeight = 0.f;
  float mOverviewHeight = 0.f;

  float mSrcFreqTop = 0.f;
  float mSrcWaveTop = 0.f;
  float mSrcWaveCentre = 0.f;
  float mSrcSilenceTop = 0.f;
  float mSrcSilenceHeight = 0.f;
  float mSrcMarkTop = 0.f;
  float mSrcMarkHeight = 0.f;
  float mSrcOverviewTop = 0.f;
  float mSrcOverviewCentre = 0.f;
  float mSrcHeight = 0.f;

  float mDstFreqTop = 0.f;
  float mDstWaveTop = 0.f;
  float mDstWaveCentre = 0.f;
  float mDstOverviewTop = 0.f;
  float mDstOverviewCentre = 0.f;

  int mSongId = 0;

  int mZoom = 0;  // >0 = zoomOut framesPerPix, 0 = unity, <0 = zoomIn pixPerFrame

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
