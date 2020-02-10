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

    if (mOverBitmapTarget)
      mOverBitmapTarget->Release();
    }
  //}}}

  //{{{
  bool onDown (bool right, cPoint pos)  {

    if (pos.y > mOverY) {
      mOn = true;
      auto frame = int((pos.x * mSong.getTotalFrames()) / getWidth());
      mSong.setPlayFrame (frame);
      }

    return true;
    }
  //}}}
  //{{{
  bool onMove (bool right, cPoint pos, cPoint inc) {

    if (pos.y > mOverY)
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

    mWaveY = getHeight() - 200.f;
    mOverY = getHeight() - 100.f;

    mSpecHeight = mWaveY;

    mWaveHeight = mOverY - mWaveY;
    mWaveCentre = mWaveHeight / 2.f;

    mOverHeight = getHeight() - mOverY;
    mOverCentre = mOverHeight / 2.f;

    mOverNumFrames = 0;
    mOverTotalFrames = 0;
    mOverValueScale = 1.f;

    mBitmapFramesOk = false;
    mBitmapFirstFrame = 0;
    mBitmapLastFrame = 0;
    }
  //}}}
  //{{{
  void onDraw (ID2D1DeviceContext* dc) {
  // draw centred at playFrame

    allocBitmaps (dc);

    auto playFrame = mSong.getPlayFrame();

    drawWave (dc, playFrame);
    drawOverView (dc, playFrame);
    drawFreq (dc, playFrame);
    drawTime (dc, getFrameStr (playFrame) + " " + getFrameStr (mSong.getTotalFrames()));
    }
  //}}}

private:
  //{{{
  void allocBitmaps (ID2D1DeviceContext* dc) {
  // create mSpecBitmap and mWaveBitmapTarget at size

    if (!mSpecBitmap || !mWaveBitmapTarget || !mOverBitmapTarget ||
        (getWidthInt() != (int)mSpecBitmapSize.width) ||
        (mWaveY != mSpecBitmapSize.height) || (mWaveHeight != mWaveBitmapSize.height) || (mOverHeight != mOverBitmapSize.height)) {

      // spectrum bitmap
      if (mSpecBitmap)
        mSpecBitmap->Release();
      mSpecBitmapSize = { (UINT32)getWidthInt(), (UINT32)mWaveY };
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
      if (mOverBitmapTarget)
        mOverBitmapTarget->Release();
      D2D1_SIZE_F overSizeF = D2D1::SizeF (getWidth(), mOverHeight);
      mOverBitmapSize = { (UINT32)getWidthInt(), (UINT32)mOverHeight };
      dc->CreateCompatibleRenderTarget (&overSizeF, &mOverBitmapSize, &pixelFormat,
                                        D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS_NONE,
                                        &mOverBitmapTarget);
      if (mOverBitmap)
        mOverBitmap->Release();
      mOverBitmapTarget->GetBitmap (&mOverBitmap);
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
  void drawWave (ID2D1DeviceContext* dc, int playFrame) {

    float valueScale = mWaveCentre / mSong.getMaxPowerValue();
    int spectrumSize = std::min (mSong.getMaxSpectrum(), (int)mSpecBitmapSize.height);
    int spectrumOffset = mSong.getMaxSpectrum() > (int)mSpecBitmapSize.height ?
                         mSong.getMaxSpectrum() - mSpecBitmapSize.height : 0;

    int frameStep = (mZoom > 0) ? mZoom+1 : 1; // zoomOut summing frameStep frames per pix
    int frameWidth = (mZoom < 0) ? -mZoom+1 : 1; // zoomIn expanding frame to frameWidth pix

    // calc leftFrame, clip to valid frame, adjust firstX which may overlap left up to frameWidth
    auto leftFrame = playFrame - (((getWidthInt() + frameWidth) / 2) * frameStep) / frameWidth;
    auto rightFrame = playFrame + (((getWidthInt() + frameWidth) / 2) * frameStep) / frameWidth;

    // calc dstFirstX, clip leftFrame,rightFrame to valid frames
    float dstFirstX = (getWidth()/2.f) - (((playFrame - leftFrame) * frameWidth) / frameStep) - (frameWidth/2);
    if (leftFrame < 0) {
      dstFirstX += (-leftFrame * frameWidth) / frameStep;
      leftFrame = 0;
      }
    rightFrame = std::min (rightFrame, mSong.getLastFrame());

    // check for bitmap cache overlap
    mBitmapFramesOk = mBitmapFramesOk && (rightFrame > mBitmapFirstFrame) && (leftFrame < mBitmapLastFrame);

    // update wave and spectrum bitmaps
    D2D1_COLOR_F off = { 0.f };
    mWaveBitmapTarget->BeginDraw();
    mWaveBitmapTarget->Clear (&off);

    float playFrameX = 0;
    cRect r;
    r.left = dstFirstX;
    for (auto frame = leftFrame; frame < rightFrame; frame += frameStep) {
      r.right = r.left + 1.f;
      if (mSong.mFrames[frame]->hasTitle()) {
        //{{{  draw song title yellow bar and text
        dc->FillRectangle (cRect (r.left, 0.f, r.right + 2.f, mWaveHeight) + getTR(), mWindow->getYellowBrush());

        auto str = mSong.mFrames[frame]->getTitle();

        IDWriteTextLayout* textLayout;
        mWindow->getDwriteFactory()->CreateTextLayout (
          std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
          mWindow->getTextFormat(), getWidth(), getHeight(), &textLayout);

        if (textLayout) {
          dc->DrawTextLayout (cPoint (r.left, getBL().y - mWaveHeight - 20.f), textLayout, mWindow->getWhiteBrush());
          textLayout->Release();
          }
        }
        //}}}
      if (mSong.mFrames[frame]->isSilent()) {
        //{{{  draw red silent frame
        r.top = mWaveCentre - 2.f;
        r.bottom = mWaveCentre + 2.f;
        dc->FillRectangle (r + getTR(), mWindow->getRedBrush());
        }
        //}}}
      //{{{  draw frame waveform
      float leftValue = 0.f;
      float rightValue = 0.f;

      if (mZoom <= 0) {
        // no zoom, or zoomIn expanding frame
        if (frame == playFrame)
          playFrameX = r.left;
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
            playFrameX = r.left;
            leftValue = *powerValues++;
            rightValue = *powerValues;
            break;
            }
          leftValue += *powerValues++ / frameStep;
          rightValue += *powerValues / frameStep;
          }
        }

      r.top = mWaveCentre - (leftValue * valueScale);
      r.bottom = mWaveCentre + (rightValue * valueScale);
      mWaveBitmapTarget->FillRectangle (r, mWindow->getWhiteBrush());
      //}}}
      if (!mBitmapFramesOk || (frame < mBitmapFirstFrame) || (frame >= mBitmapLastFrame)) {
        //{{{  copy reversed spectrum column to bitmap, clipping higher freqs to height
        auto index = frame % mSpecBitmapSize.width;
        D2D1_RECT_U rectU = { index,0, index+1,(UINT32)spectrumSize };
        mSpecBitmap->CopyFromMemory (&rectU, mSong.mFrames[frame]->getFreqLuma() + spectrumOffset, 1);
        }
        //}}}
      r.left += 1.f;
      }
    mWaveBitmapTarget->EndDraw();

    // save bitmap range
    mBitmapFramesOk = true;
    mBitmapFirstFrame = leftFrame;
    mBitmapLastFrame = rightFrame;

    //{{{  stamp colours through bitmap alpha

    float srcIndex = float(leftFrame % mSpecBitmapSize.width);
    float srcWidth = float(rightFrame - leftFrame);

    cRect srcRect;
    cRect dstRect;
    dstFirstX += mRect.left;

    // stamp first left chunk
    dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_ALIASED);
    srcRect = { srcIndex, 0.f,
                (float)mSpecBitmapSize.width, mSpecHeight };
    dstRect = { dstFirstX, mRect.top,
                dstFirstX + mSpecBitmapSize.width - srcIndex, mRect.top + mSpecHeight };
    dc->FillOpacityMask (mSpecBitmap, mWindow->getGreenBrush(), dstRect, srcRect);

    if (srcIndex > 0) {
      // stamp second right chunk
      srcRect = { 0.f, 0.f,
                  srcIndex + srcWidth - mSpecBitmapSize.width, mSpecHeight };
      dstRect = { dstFirstX + mSpecBitmapSize.width - srcIndex, mRect.top,
                  dstFirstX + srcWidth, mRect.top + mSpecHeight };
      dc->FillOpacityMask (mSpecBitmap, mWindow->getYellowBrush(), dstRect, srcRect);
      }

    // stamp left waveform
    srcRect = { 0.f,0.f,
                playFrameX, mWaveHeight };
    dstRect = { mRect.left, mRect.top + mWaveY,
                mRect.left + (playFrameX*frameWidth), mRect.top + mOverY };
    dc->FillOpacityMask (mWaveBitmap, mWindow->getBlueBrush(), dstRect, srcRect);

    // stamp right waveform
    srcRect = { playFrameX + 1.f,0.f,
                getWidth(), mWaveHeight };
    dstRect = { mRect.left + ((playFrameX+1.f) * frameWidth), mRect.top + mWaveY,
                mRect.left + getWidth(), mRect.top + mOverY };
    dc->FillOpacityMask (mWaveBitmap, mWindow->getGreyBrush(), dstRect, srcRect);

    // draw playFrame waveform without summing
    auto powerValues = mSong.mFrames[playFrame]->getPowerValues();
    auto leftValue = (*powerValues++) * valueScale;
    auto rightValue = (*powerValues) * valueScale;
    dstRect = { mRect.left + playFrameX, mRect.top + mWaveY + mWaveCentre - leftValue,
                mRect.left + playFrameX + frameWidth, mRect.top + mWaveY + mWaveCentre + rightValue };
    dc->FillRectangle (dstRect, mWindow->getWhiteBrush());
    dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    // on screen debug
    auto str = dec(leftFrame) + "," + dec(rightFrame) +
               " bit" + dec(mBitmapFirstFrame) + "," + dec(mBitmapLastFrame) +
               " srcIndex:" + dec(srcIndex) + " srcWidth:" + dec(srcWidth) +
               " frameWidth:" + dec(frameWidth) + " frameStep:" + dec(frameStep);

    IDWriteTextLayout* textLayout;
    mWindow->getDwriteFactory()->CreateTextLayout (
      std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
      mWindow->getTextFormat(), getWidth(), getHeight(), &textLayout);

    if (textLayout) {
      dc->DrawTextLayout ({ 0.f, getCentre().y}, textLayout, mWindow->getWhiteBrush());
      textLayout->Release();
      }
    //}}}
    }
  //}}}
  //{{{
  void drawFreq (ID2D1DeviceContext* dc, int playFrame) {

    if (mSong.getMaxFreqValue() > 0.f) {
      auto valueScale = mOverHeight / mSong.getMaxFreqValue();
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
  void drawOverViewWave (ID2D1DeviceContext* dc, int playFrame, float playFrameX, float valueScale) {
  // draw overview using bitmap cache

    int numFrames = mSong.getNumFrames();
    int totalFrames = mSong.getTotalFrames();
    float framesPerPix = totalFrames / getWidth();
    bool forceRedraw = (mSong.getId() != mSongId) ||
                       (totalFrames != mOverTotalFrames) || (valueScale != mOverValueScale);

    if (forceRedraw || (numFrames > mOverNumFrames)) {
      mOverBitmapTarget->BeginDraw();
      if (forceRedraw) {
        //{{{  clear bitmap to alpha off
        D2D1_COLOR_F alphaOff = { 0.f };
        mOverBitmapTarget->Clear (&alphaOff);
        }
        //}}}

      int frame = 0;
      for (float x = 0.f; x < getWidth(); x += 1.f) {
        //{{{  iterate width
        int toFrame = int(x * framesPerPix);
        if (toFrame >= numFrames)
          break;

        if (forceRedraw || (frame >= mOverNumFrames)) {
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

          mOverBitmapTarget->FillRectangle (
            cRect (x, mOverCentre - (leftValue * valueScale) - 2.f,
                   x+1.f,  mOverCentre + (rightValue * valueScale) + 2.f), mWindow->getWhiteBrush());
          }

        frame = toFrame;
        }
        //}}}
      mOverBitmapTarget->EndDraw();

      mOverNumFrames = numFrames;
      mOverTotalFrames = totalFrames;
      mOverValueScale = valueScale;
      mSongId = mSong.getId();
      }

    // stamp overview using overBitmap
    dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_ALIASED);

    // before playFrame
    cRect srcRect (0.f,0.f, playFrameX, mOverHeight);
    cRect dstRect (mRect.left, mRect.top + mOverY, mRect.left + playFrameX, mRect.bottom);
    dc->FillOpacityMask (mOverBitmap, mWindow->getBlueBrush(), dstRect, srcRect);

    // on playFrame
    auto powerValues = mSong.mFrames[playFrame]->getPowerValues();
    auto leftValue = *powerValues++;
    auto rightValue = *powerValues;
    srcRect.right += 1.f;
    dstRect.left = dstRect.right;
    dstRect.right += 1.f;
    dc->FillRectangle (cRect (dstRect.left, dstRect.top + mOverCentre - (leftValue * valueScale),
                              dstRect.right, dstRect.top + mOverCentre + (rightValue * valueScale)),
                       mWindow->getWhiteBrush());

    // after playFrame
    srcRect.left = srcRect.right;
    srcRect.right += (float)getWidthInt();
    dstRect.left = dstRect.right;
    dstRect.right = mRect.left + getWidthInt();
    dc->FillOpacityMask (mOverBitmap, mWindow->getGreyBrush(), dstRect, srcRect);

    dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    }
  //}}}
  //{{{
  void drawOverViewLens (ID2D1DeviceContext* dc, int playFrame, float centreX, float width, float valueScale) {
  // draw frames centred at playFrame -/+ width in pixels, centred at centreX, zoomed by zoomIndex

    cRect r (mRect.left + centreX - mLens, mRect.top + mOverY,
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
        dc->FillRectangle (cRect (r.left-1.f, mRect.top + mOverY, r.left+1.f, mRect.bottom), mWindow->getYellowBrush());

        auto str = mSong.mFrames[frame]->getTitle();

        IDWriteTextLayout* textLayout;
        mWindow->getDwriteFactory()->CreateTextLayout (
          std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
          mWindow->getTextFormat(), getWidth(), mOverHeight, &textLayout);
        if (textLayout) {
          dc->DrawTextLayout (cPoint (r.left+2.f, mRect.top + mOverY), textLayout, mWindow->getWhiteBrush());
          textLayout->Release();
          }
        }
        //}}}
      if (mSong.mFrames[frame]->isSilent()) {
        //{{{  draw red silent frame
        r.top = mRect.top + mOverY + mOverCentre - 2.f;
        r.bottom = mRect.top + mOverY + mOverCentre + 2.f;
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

      r.top = mRect.top + mOverY + mOverCentre - (leftValue * valueScale);
      r.bottom = mRect.top + mOverY + mOverCentre + (rightValue * valueScale);
      dc->FillRectangle (r, colour);

      r.left = r.right;
      frame++;
      }
    }
  //}}}
  //{{{
  void drawOverView (ID2D1DeviceContext* dc, int playFrame) {

    float valueScale = mOverCentre / mSong.getMaxPowerValue();
    float playFrameX = (mSong.getTotalFrames() > 0) ? (playFrame * getWidth()) / mSong.getTotalFrames() : 0.f;
    drawOverViewWave (dc, playFrame, playFrameX, valueScale);

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

      drawOverViewLens (dc, playFrame, lensCentreX, mLens-1.f, valueScale);
      }
    }
  //}}}

  // private vars
  cSong& mSong;
  int mZoom = 0;  // >0 = zoomOut framesPerPix, 0 = unity, <0 = zoomIn pixPerFrame

  //{{{  vertical layout
  float mSpecHeight = 0.f;

  float mWaveY = 0.f;
  float mWaveHeight = 0.f;
  float mWaveCentre = 0.f;

  float mOverY = 0.f;
  float mOverHeight = 0.f;
  float mOverCentre = 0.f;
  //}}}

  int mSongId = 0;

  bool mBitmapFramesOk = false;
  int mBitmapFirstFrame = 0;
  int mBitmapLastFrame = 0;

  D2D1_SIZE_U mSpecBitmapSize = { 0,0 };
  ID2D1Bitmap* mSpecBitmap = nullptr;

  D2D1_SIZE_U mWaveBitmapSize = { 0,0 };
  ID2D1BitmapRenderTarget* mWaveBitmapTarget = nullptr;
  ID2D1Bitmap* mWaveBitmap;

  bool mOn = false;
  float mLens = 0.f;
  int mOverNumFrames = 0;
  int mOverTotalFrames = 0;
  float mOverValueScale = 1.f;

  D2D1_SIZE_U mOverBitmapSize = { 0,0 };
  ID2D1BitmapRenderTarget* mOverBitmapTarget = nullptr;
  ID2D1Bitmap* mOverBitmap;

  IDWriteTextFormat* mTimeTextFormat = nullptr;
  };
