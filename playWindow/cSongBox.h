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
      &mTextFormat);
    mTextFormat->SetTextAlignment (DWRITE_TEXT_ALIGNMENT_TRAILING);
    }
  //}}}
  //{{{
  virtual ~cSongBox() {

    mTextFormat->Release();

    if (mSpecBitmap)
      mSpecBitmap->Release();
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

    mWaveHeight = mOverY - mWaveY;
    mWaveCentre = mWaveHeight / 2.f;

    mOverHeight = getHeight() - mOverY;
    mOverCentre = mOverHeight / 2.f;
    mOverNumFrames = 0;
    mOverTotalFrames = 0;
    mOverValueScale = 1.f;
    }
  //}}}
  //{{{
  void onDraw (ID2D1DeviceContext* dc) {
  // draw frames centred at playFrame

    auto playFrame = mSong.getPlayFrame();

    int frameStep = (mZoom > 0) ? mZoom+1 : 1; // zoomOut summing frameStep frames per pix
    int frameWidth = (mZoom < 0) ? -mZoom+1 : 1; // zoomIn expanding frame to frameWidth pix

    allocBitmaps (dc);

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

    float dstFirstX = (float)firstX;
    float valueScale = mWaveCentre / mSong.getMaxPowerValue();

    // update wave and spectrum bitmaps
    D2D1_COLOR_F off = { 0.f };
    mWaveBitmapTarget->BeginDraw();
    mWaveBitmapTarget->Clear (&off);
    float playFrameX = 0;
    int dstBitmapX = 0;
    int dstBitmapWidth = 0;
    cRect r (dstFirstX, 0.f, dstFirstX+1.f, 0.f);
    for (auto frame = firstFrame; frame < lastFrame; frame += frameStep) {
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
          auto powerValues = mSong.mFrames[std::min (i, lastFrame)]->getPowerValues();
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
      dstBitmapWidth++;
      r.left += 1.f;
      r.right += 1.f;
      }
    mWaveBitmapTarget->EndDraw();

    if (dstBitmapWidth) {
      //{{{  stamp bitmap
      // stamp colour through used part of ID2D1Bitmap alpha
      dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_ALIASED);

      if (mFirstFrameIndex + dstBitmapWidth <= (int)mSpecBitmapSize.width)
        dc->FillOpacityMask (mSpecBitmap, mWindow->getWhiteBrush(),
          &D2D1::RectF (dstFirstX, mRect.top, dstFirstX + dstBitmapWidth, mRect.top + (float)mSpecBitmapSize.height),
          &D2D1::RectF (0.f,0.f, (float)dstBitmapWidth, (float)mSpecBitmapSize.height));

      else {
        float firstFrameIndex = (float)mFirstFrameIndex;
        float firstStamp = (float)mSpecBitmapSize.width - mFirstFrameIndex;
        float secondStamp = dstBitmapWidth - firstStamp;

        dc->FillOpacityMask (mSpecBitmap, mWindow->getGreenBrush(),
          &D2D1::RectF (dstFirstX, mRect.top, dstFirstX + firstStamp, mRect.top + (float)mSpecBitmapSize.height),
          &D2D1::RectF ((float)mFirstFrameIndex,0.f, mFirstFrameIndex + firstStamp, (float)mSpecBitmapSize.height));

        dc->FillOpacityMask (mSpecBitmap, mWindow->getYellowBrush(),
          &D2D1::RectF (dstFirstX + firstStamp, mRect.top,
                        dstFirstX + firstStamp + secondStamp, mRect.top + (float)mSpecBitmapSize.height),
          &D2D1::RectF (0.f,0.f, secondStamp, (float)mSpecBitmapSize.height));
        }

      // draw waveform 3 stamps, before, on, after playframe
      ID2D1Bitmap* waveBitmap;
      mWaveBitmapTarget->GetBitmap (&waveBitmap);

      cRect srcRect (0.f,0.f, playFrameX, mWaveHeight);
      cRect dstRect (mRect.left, mRect.top + mWaveY, mRect.left + playFrameX, mRect.top + mOverY);
      dc->FillOpacityMask (waveBitmap, mWindow->getBlueBrush(), dstRect, srcRect);

      srcRect.left = srcRect.right;
      srcRect.right += 1.f;
      dstRect.left = dstRect.right;
      dstRect.right += 1.f;
      dc->FillOpacityMask (waveBitmap, mWindow->getWhiteBrush(), dstRect, srcRect);

      srcRect.left = srcRect.right;
      srcRect.right += (float)getWidthInt();
      dstRect.left = dstRect.right;
      dstRect.right = mRect.left + getWidthInt();
      dc->FillOpacityMask (waveBitmap, mWindow->getGreyBrush(), dstRect, srcRect);

      waveBitmap->Release();

      dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

      //mWaveBitmapTarget->GetBitmap (&mWaveBitmap);
      //dc->DrawBitmap (mWaveBitmap,
      //  &D2D1::RectF (mRect.left, mRect.top + mWaveY, mRect.left + getWidthInt(), mRect.top + mOverY),
      //  1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
      //  &D2D1::RectF (0.f,0.f, (float)getWidthInt(), mWaveHeight));
      }
      //}}}

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
    drawFreq (dc, playFrame, getHeight() - mOverY);
    drawTime (dc, getFrameStr (playFrame) + " " + getFrameStr (mSong.getTotalFrames()));
    }
  //}}}

private:
  //{{{
  void allocBitmaps (ID2D1DeviceContext* dc) {
  // create mSpecBitmap and mWaveBitmapTarget at size

    if (!mSpecBitmap || !mWaveBitmapTarget || !mOverBitmapTarget ||
        (getWidthInt() > (int)mSpecBitmapSize.width) ||
        (mWaveY > mSpecBitmapSize.height) || (mWaveHeight > mWaveBitmapSize.height) || (mOverHeight > mOverBitmapSize.height)) {

      // spectrum bitmap
      if (mSpecBitmap)
        mSpecBitmap->Release();
      mSpecBitmapSize = { (UINT32)getWidthInt(), (UINT32)mWaveY};
      dc->CreateBitmap (mSpecBitmapSize,
                        { DXGI_FORMAT_A8_UNORM, D2D1_ALPHA_MODE_STRAIGHT, 0,0 },
                        &mSpecBitmap);

      // wave bitmapTarget
      if (mWaveBitmapTarget)
        mWaveBitmapTarget->Release();
      D2D1_SIZE_F waveSizeF = D2D1::SizeF (getWidth(), mWaveHeight);
      mWaveBitmapSize = { (UINT32)getWidthInt(), (UINT32)mWaveHeight};
      D2D1_PIXEL_FORMAT pixelFormat = { DXGI_FORMAT_A8_UNORM, D2D1_ALPHA_MODE_UNKNOWN };
      dc->CreateCompatibleRenderTarget (&waveSizeF, &mWaveBitmapSize, &pixelFormat,
                                        D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS_NONE,
                                        &mWaveBitmapTarget);

      // over bitmapTarget
      if (mOverBitmapTarget)
        mOverBitmapTarget->Release();
      D2D1_SIZE_F overSizeF = D2D1::SizeF (getWidth(), mOverHeight);
      mOverBitmapSize = { (UINT32)getWidthInt(), (UINT32)mOverHeight};
      dc->CreateCompatibleRenderTarget (&overSizeF, &mOverBitmapSize, &pixelFormat,
                                        D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS_NONE,
                                        &mOverBitmapTarget);
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
  void drawOverWave (ID2D1DeviceContext* dc, int playFrame, float playFrameX, float valueScale) {

    int numFrames = mSong.getNumFrames();
    int totalFrames = mSong.getTotalFrames();
    float framesPerPix = totalFrames / getWidth();
    bool forceTotalRedraw = (totalFrames != mOverTotalFrames) || (valueScale != mOverValueScale);

    if (forceTotalRedraw || (numFrames > mOverNumFrames)) {
      D2D1_COLOR_F off = { 0.f };
      mOverBitmapTarget->BeginDraw();
      if (forceTotalRedraw)
        mOverBitmapTarget->Clear (&off);

      int frame = 0;
      for (float x = 0.f; x < getWidth(); x += 1.f) {
        //{{{  iterate width
        int toFrame = int(x * framesPerPix);
        if (toFrame >= numFrames)
          break;

        if (forceTotalRedraw || (frame >= mOverNumFrames)) {
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
                   x + 1.f,  mOverCentre + (rightValue * valueScale) + 2.f), mWindow->getWhiteBrush());
          }

        frame = toFrame;
        }
        //}}}
      mOverBitmapTarget->EndDraw();
      mOverNumFrames = numFrames;
      mOverValueScale = valueScale;
      }

    // stamp playFrame using overBitmap
    ID2D1Bitmap* overBitmap;
    mOverBitmapTarget->GetBitmap (&overBitmap);

    dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_ALIASED);

    // before playFrame
    cRect srcRect (0.f,0.f, playFrameX, mOverHeight);
    cRect dstRect (mRect.left, mRect.top + mOverY, mRect.left + playFrameX, mRect.bottom);
    dc->FillOpacityMask (overBitmap, mWindow->getBlueBrush(), dstRect, srcRect);

    // on playFrame
    srcRect.left = srcRect.right;
    srcRect.right += 1.f;
    dstRect.left = dstRect.right;
    dstRect.right += 1.f;
    dc->FillOpacityMask (overBitmap, mWindow->getWhiteBrush(), dstRect, srcRect);

    // after playFrame
    srcRect.left = srcRect.right;
    srcRect.right += (float)getWidthInt();
    dstRect.left = dstRect.right;
    dstRect.right = mRect.left + getWidthInt();
    dc->FillOpacityMask (overBitmap, mWindow->getGreyBrush(), dstRect, srcRect);

    dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    overBitmap->Release();
    }
  //}}}
  //{{{
  void drawOverLens (ID2D1DeviceContext* dc, int playFrame, float centreX, float width, float valueScale) {
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
        dc->FillRectangle (cRect (r.left, mRect.top + mOverY, r.left + 2.f, mRect.bottom), mWindow->getYellowBrush());

        auto str = mSong.mFrames[frame]->getTitle();

        IDWriteTextLayout* textLayout;
        mWindow->getDwriteFactory()->CreateTextLayout (
          std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
          mWindow->getTextFormat(), getWidth(), mOverHeight, &textLayout);
        if (textLayout) {
          dc->DrawTextLayout (cPoint (r.left, mRect.top + mOverY), textLayout, mWindow->getWhiteBrush());
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
    drawOverWave (dc, playFrame, playFrameX, valueScale);

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

      drawOverLens (dc, playFrame, lensCentreX, mLens-1.f, valueScale);
      }
    }
  //}}}

  // private vars
  cSong& mSong;
  int mZoom = 0;  // >0 = zoomOut framesPerPix, 0 = unity, <0 = zoomIn pixPerFrame

  float mWaveY = 0.f;
  float mWaveHeight = 0.f;
  float mWaveCentre = 0.f;
  float mOverY = 0.f;
  float mOverHeight = 0.f;
  float mOverCentre = 0.f;

  bool mFramesValid = false;
  int mFirstFrame = -1;
  int mFirstFrameIndex = -1;
  int mLastFrame = -1;

  D2D1_SIZE_U mSpecBitmapSize = { 0,0 };
  ID2D1Bitmap* mSpecBitmap = nullptr;

  D2D1_SIZE_U mWaveBitmapSize = { 0,0 };
  ID2D1BitmapRenderTarget* mWaveBitmapTarget = nullptr;

  D2D1_SIZE_U mOverBitmapSize = { 0,0 };
  ID2D1BitmapRenderTarget* mOverBitmapTarget = nullptr;
  int mOverNumFrames = 0;
  int mOverTotalFrames = 0;
  float mOverValueScale = 1.f;

  bool mOn = false;
  float mLens = 0.f;

  IDWriteTextFormat* mTextFormat = nullptr;
  };
