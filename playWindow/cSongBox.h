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
    cLog::log (LOGINFO, "cSongBox layout %d %d %d %d", mRect.left, mRect.top, mRect.right, mRect.bottom);

    // invalidate frame bitmap
    mBitmapFramesOk = false;

    // invalidate overview bitmap
    mBitmapOverviewOk = false;
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
    drawTime (dc, getTimeStr (playFrame) + " " + getTimeStr (mSong.getTotalFrames()));
    }
  //}}}

private:
  //{{{
  void reallocBitmap (ID2D1DeviceContext* dc) {
  // fixed bitmap width for big cache, src bitmap height tracks dst box height

    mBitmapWidth = 2048;
    uint32_t bitmapHeight = (int)mSrcHeight;

    if (!mBitmapTarget || (bitmapHeight != mBitmapTarget->GetSize().height)) {
      // fixed width for more cache
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
      dc->CreateCompatibleRenderTarget (NULL, &bitmapSizeU, &pixelFormat,
                                        D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS_NONE,
                                        &mBitmapTarget);
      mBitmapTarget->GetBitmap (&mBitmap);
      }
    }
  //}}}

  //{{{
  int frameToSrcIndex (int frame) {
  // circular buffer mod with continuity for through +ve to -ve frame numbers

    while (frame < 0)
      frame += mBitmapWidth;

    return frame % mBitmapWidth;
    }
  //}}}
  //{{{
  void drawFrameBitmap (int frame, int toFrame, int playFrame, int rightFrame, int frameStep, float valueScale) {

    cLog::log (LOGINFO, "drawFrameToBitmap %d %d %d", frame, toFrame, playFrame);

    int freqSize = std::min (mSong.getNumFreqLuma(), (int)mFreqHeight);
    int freqOffset = mSong.getNumFreqLuma() > (int)mFreqHeight ?
                       mSong.getNumFreqLuma() - (int)mFreqHeight : 0;

    while (frame < toFrame) {
      auto srcIndex = frameToSrcIndex (frame);

      if (frame >= 0) {
        // copy reversed Freqrum column to bitmap, clip high freqs to height
        D2D1_RECT_U rectU = { (UINT32)srcIndex, 0, (UINT32)srcIndex+1, (UINT32)freqSize };
        mBitmap->CopyFromMemory (&rectU, mSong.mFrames[frame]->getFreqLuma() + freqOffset, 1);

        // clear wave,silence,mark bitmap frame column
        cRect r = { (float)srcIndex, mSrcWaveTop, (float)srcIndex+1.f, mSrcOverviewTop };
        mBitmapTarget->PushAxisAlignedClip (r, D2D1_ANTIALIAS_MODE_ALIASED);
        mBitmapTarget->Clear ( { 0.f,0.f,0.f, 0.f } );
        mBitmapTarget->PopAxisAlignedClip();

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
        r.bottom = mSrcWaveCentre + (rightValue * valueScale);
        mBitmapTarget->FillRectangle (r, mWindow->getWhiteBrush());
        //}}}

        if (mSong.mFrames[frame]->isSilent()) {
          //{{{  draw silence bitmap
          r.top = mSrcSilenceTop;
          r.bottom = mSrcSilenceTop + 1.f;
          mBitmapTarget->FillRectangle (r, mWindow->getWhiteBrush());
          }
          //}}}

        if (mSong.mFrames[frame]->hasTitle()) {
          //{{{  draw song title bitmap
          r.top = mSrcMarkTop;
          r.bottom = mSrcMarkTop + 1.f;
          mBitmapTarget->FillRectangle (r, mWindow->getWhiteBrush());
          }
          //}}}
        }
      else {
        // clear freq,wave,silence,mark bitmap frame column
        cRect r = { (float)srcIndex, mSrcFreqTop, (float)srcIndex+1.f, mSrcOverviewTop };
        mBitmapTarget->PushAxisAlignedClip (r, D2D1_ANTIALIAS_MODE_ALIASED);
        mBitmapTarget->Clear ( { 0.f,0.f,0.f, 0.f } );
        mBitmapTarget->PopAxisAlignedClip();
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

    // add frames to freq,wave,silence,mark bitmap
    mBitmapTarget->BeginDraw();
    if (!mBitmapFramesOk || (leftFrame >= mBitmapLastFrame) || (rightFrame < mBitmapFirstFrame)) {
      //{{{  draw all bitmap frames
      drawFrameBitmap (leftFrame, rightFrame, playFrame, rightFrame, frameStep, valueScale);
      mBitmapFirstFrame = leftFrame;
      mBitmapLastFrame = rightFrame;
      }
      //}}}
    else {
      //{{{  frame range overlaps bitmap frames
      if (leftFrame < mBitmapFirstFrame) {
        // draw new bitmap leftFrames
        drawFrameBitmap (leftFrame, mBitmapFirstFrame, playFrame, rightFrame, frameStep, valueScale);
        mBitmapFirstFrame = leftFrame;
        if (mBitmapLastFrame - mBitmapFirstFrame > (int)mBitmapWidth)
          mBitmapLastFrame = mBitmapFirstFrame + mBitmapWidth;
        }
      if (rightFrame > mBitmapLastFrame) {
        // draw new bitmap rightFrames
        drawFrameBitmap (mBitmapLastFrame, rightFrame, playFrame, rightFrame, frameStep, valueScale);
        mBitmapLastFrame = rightFrame;
        if (mBitmapLastFrame - mBitmapFirstFrame > (int)mBitmapWidth)
          mBitmapFirstFrame = mBitmapLastFrame - mBitmapWidth;
        }
      }
      //}}}
    mBitmapFramesOk = true;
    mBitmapFrameStep = frameStep;
    mBitmapTarget->EndDraw();

    // calc bitmap stamps
    float leftSrcIndex = (float)frameToSrcIndex (leftFrame);
    float rightSrcIndex = (float)frameToSrcIndex (rightFrame);
    float playSrcIndex = (float)frameToSrcIndex (playFrame);

    bool wraparound = rightSrcIndex <= leftSrcIndex;
    float firstEndSrcIndex = wraparound ? float(mBitmapWidth) : rightSrcIndex;

    //  stamp colours through alpha bitmap
    dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_ALIASED);
    //{{{  stamp first dst chunk
    bool split = (playSrcIndex >= leftSrcIndex) && (playSrcIndex < firstEndSrcIndex);

    cRect srcRect;
    cRect dstRect;

    // Freq
    srcRect = { leftSrcIndex, mSrcFreqTop, firstEndSrcIndex, mSrcFreqTop + mFreqHeight };
    dstRect = { mRect.left,
                mDstFreqTop,
                mRect.left + (firstEndSrcIndex - leftSrcIndex) * frameWidth,
                mDstFreqTop + mFreqHeight };
    dc->FillOpacityMask (mBitmap, mWindow->getGreenBrush(), dstRect, srcRect);

    // silence
    srcRect = { leftSrcIndex, mSrcSilenceTop, firstEndSrcIndex, mSrcSilenceTop + mSrcSilenceHeight };
    dstRect = { mRect.left,
                mDstWaveCentre - 2.f,
                mRect.left + (firstEndSrcIndex - leftSrcIndex) * frameWidth,
                mDstWaveCentre + 2.f, };
    dc->FillOpacityMask (mBitmap, mWindow->getRedBrush(), dstRect, srcRect);

    // mark
    srcRect = { leftSrcIndex, mSrcMarkTop, firstEndSrcIndex, mSrcMarkTop + 1.f };
    dstRect = { mRect.left,
                mDstWaveTop,
                mRect.left + (firstEndSrcIndex - leftSrcIndex) * frameWidth,
                mDstWaveTop + mWaveHeight };
    dc->FillOpacityMask (mBitmap, mWindow->getYellowBrush(), dstRect, srcRect);

    // wave
    srcRect = { leftSrcIndex, mSrcWaveTop, (split ? playSrcIndex : firstEndSrcIndex), mSrcWaveTop + mWaveHeight };
    dstRect = { mRect.left,
                mDstWaveTop,
                mRect.left + ((split ? playSrcIndex : firstEndSrcIndex) - leftSrcIndex) * frameWidth,
                mDstWaveTop + mWaveHeight };
    dc->FillOpacityMask (mBitmap, mWindow->getBlueBrush(), dstRect, srcRect);

    if (split) {
      // split chunk after play
      srcRect = { playSrcIndex, mSrcWaveTop, firstEndSrcIndex, mSrcWaveTop + mWaveHeight };
      dstRect = { mRect.left + (playSrcIndex - leftSrcIndex) * frameWidth,
                  mDstWaveTop,
                  mRect.left + (firstEndSrcIndex - leftSrcIndex) * frameWidth,
                  mDstWaveTop + mWaveHeight };
      dc->FillOpacityMask (mBitmap, mWindow->getGreyBrush(), dstRect, srcRect);
      }
    //}}}
    if (wraparound) {
      //{{{  stamp second dst chunk
      bool split = false;
      // Freq
      srcRect = { 0.f, mSrcFreqTop, rightSrcIndex, mSrcFreqTop + mFreqHeight };
      dstRect = { mRect.left + (firstEndSrcIndex - leftSrcIndex) * frameWidth,
                  mDstFreqTop,
                  mRect.left + (firstEndSrcIndex - leftSrcIndex + rightSrcIndex) * frameWidth,
                  mDstFreqTop + mFreqHeight };
      dc->FillOpacityMask (mBitmap, mWindow->getYellowBrush(), dstRect, srcRect);

      // silence
      srcRect = { 0.f, mSrcSilenceTop, rightSrcIndex, mSrcSilenceTop + mSrcSilenceHeight };
      dstRect = { mRect.left + (firstEndSrcIndex - leftSrcIndex) * frameWidth,
                  mDstWaveCentre - 2.f,
                  mRect.left + (firstEndSrcIndex - leftSrcIndex + rightSrcIndex) * frameWidth,
                  mDstWaveCentre + 2.f };
      dc->FillOpacityMask (mBitmap, mWindow->getRedBrush(), dstRect, srcRect);

      // mark
      srcRect = { 0.f, mSrcMarkTop, rightSrcIndex, mSrcMarkTop + 1.f };
      dstRect = { mRect.left + (firstEndSrcIndex - leftSrcIndex) * frameWidth,
                  mDstWaveTop,
                  mRect.left + (firstEndSrcIndex - leftSrcIndex + rightSrcIndex) * frameWidth,
                  mDstWaveTop + mWaveHeight };
      dc->FillOpacityMask (mBitmap, mWindow->getYellowBrush(), dstRect, srcRect);

      // wave
      if (split) {
        srcRect = { 0.f, mSrcWaveTop, playSrcIndex, mSrcWaveTop + mWaveHeight };
        dstRect = { mRect.left + (firstEndSrcIndex - leftSrcIndex) * frameWidth,
                    mDstWaveTop,
                    mRect.left + (firstEndSrcIndex - leftSrcIndex + playSrcIndex) * frameWidth,
                    mDstWaveTop + mWaveHeight };
        dc->FillOpacityMask (mBitmap, mWindow->getBlueBrush(), dstRect, srcRect);

        srcRect = { playSrcIndex, mSrcWaveTop, rightSrcIndex, mSrcWaveTop + mWaveHeight };
        dstRect = { mRect.left + (firstEndSrcIndex - leftSrcIndex + playSrcIndex) * frameWidth,
                    mDstWaveTop,
                    mRect.left + (firstEndSrcIndex - leftSrcIndex + rightSrcIndex) * frameWidth,
                    mDstWaveTop + mWaveHeight };
        dc->FillOpacityMask (mBitmap, mWindow->getGreyBrush(), dstRect, srcRect);
        }
      else
        srcRect = { 0.f, mSrcWaveTop, rightSrcIndex, mSrcWaveTop + mWaveHeight };
        dstRect = { mRect.left + (firstEndSrcIndex - leftSrcIndex) * frameWidth,
                    mDstWaveTop,
                    mRect.left + (firstEndSrcIndex - leftSrcIndex + rightSrcIndex) * frameWidth,
                    mDstWaveTop + mWaveHeight };
        dc->FillOpacityMask (mBitmap, mWindow->getGreyBrush(), dstRect, srcRect);
        }
      //}}}
    //{{{  stamp playFrame wave
    srcRect = { playSrcIndex, mSrcWaveTop, playSrcIndex+1.f, mSrcWaveTop + mWaveHeight };

    dstRect = { mRect.left + getCentreX(),
                mDstWaveTop,
                mRect.left + getCentreX() + frameWidth,
                mDstWaveTop + mWaveHeight };

    dc->FillOpacityMask (mBitmap, mWindow->getWhiteBrush(), dstRect, srcRect);
    //}}}
    dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    // debug
    auto str = dec(leftFrame) + "," + dec(rightFrame) + (wraparound ? " wrap" : "") +
               " bit" + dec(mBitmapFirstFrame) + "," + dec(mBitmapLastFrame) +
               " leftInd:" + dec(leftSrcIndex) + " rightInd:" + dec(rightSrcIndex) + " firstEndInd:" + dec(firstEndSrcIndex) +
               " w:" + dec(frameWidth) + " s:" + dec(frameStep);
    //{{{  draw debug str
    IDWriteTextLayout* textLayout;
    mWindow->getDwriteFactory()->CreateTextLayout (
      std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
      mWindow->getTextFormat(), getWidth(), getHeight(), &textLayout);

    if (textLayout) {
      dc->DrawTextLayout ({ 0.f, 40.f }, textLayout, mWindow->getWhiteBrush());
      textLayout->Release();
      }
    //}}}
    }
  //}}}

  //{{{
  void drawFreq (ID2D1DeviceContext* dc, int playFrame) {

    if (mSong.getMaxFreqValue() > 0.f) {
      auto valueScale = mOverviewHeight / mSong.getMaxFreqValue();
      auto maxFreqIndex = std::min (getWidthInt()/2, mSong.getNumFreq());

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
  std::string getTimeStr (uint32_t frame) {

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

    bool forceRedraw = !mBitmapOverviewOk ||
                       (mSong.getId() != mSongId) ||
                       (totalFrames > mOverviewTotalFrames) || (valueScale != mOverviewValueScale);

    if (forceRedraw || (numFrames > mOverviewNumFrames)) {
      mBitmapTarget->BeginDraw();

      if (forceRedraw) {
        cRect r = { 0.f, mSrcOverviewTop, getWidth(), mSrcOverviewTop + mOverviewHeight };
        mBitmapTarget->PushAxisAlignedClip (r, D2D1_ANTIALIAS_MODE_ALIASED);
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

          cRect r = { (float)x, mSrcOverviewCentre - (leftValue * valueScale) - 2.f,
                      x+1.f, mSrcOverviewCentre + (rightValue * valueScale) + 2.f };
          mBitmapTarget->FillRectangle (r, mWindow->getWhiteBrush());
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

    // stamp Overview using overBitmap
    dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_ALIASED);

    // before playFrame
    cRect srcRect (0.f, mSrcOverviewTop, playFrameX, mSrcOverviewTop + mOverviewHeight);
    cRect dstRect (mRect.left, mDstOverviewTop,
                   mRect.left + playFrameX, mDstOverviewTop + mOverviewHeight);
    dc->FillOpacityMask (mBitmap, mWindow->getBlueBrush(), dstRect, srcRect);

    // on playFrame
    srcRect.left = srcRect.right;
    srcRect.right += 1.f;
    dstRect.left = dstRect.right;
    dstRect.right += 1.f;
    dc->FillOpacityMask (mBitmap, mWindow->getWhiteBrush(), dstRect, srcRect);

    // after playFrame
    srcRect.left = srcRect.right;
    srcRect.right = getWidth();
    dstRect.left = dstRect.right;
    dstRect.right = mRect.right;
    dc->FillOpacityMask (mBitmap, mWindow->getGreyBrush(), dstRect, srcRect);

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

  bool mOn = false;
  float mLens = 0.f;

  bool mBitmapOverviewOk = false;
  int mOverviewNumFrames = 0;
  int mOverviewTotalFrames = 0;
  float mOverviewValueScale = 1.f;

  bool mBitmapFramesOk = false;
  int mBitmapFrameStep = 0;
  int mBitmapFirstFrame = 0;
  int mBitmapLastFrame = 0;

  uint32_t mBitmapWidth = 0;
  ID2D1BitmapRenderTarget* mBitmapTarget = nullptr;
  ID2D1Bitmap* mBitmap;

  IDWriteTextFormat* mTimeTextFormat = nullptr;
  //}}}
  };
