// mp3Window.cpp
//{{{  includes
#include "stdafx.h"

#include "../../shared/utils/cFileList.h"
#include "../boxes/cFileListBox.h"

using namespace std;
using namespace chrono;
//}}}
//{{{  const
const int kMaxChannels = 2;

const int kSilentThreshold = 3;

const int kSilentWindow = 10;       // about a half second analyse silence

const int kPlayFrameThreshold = 10; // about a half second analyse before play

const int kMaxSamples = 2048;
const int kMaxFreq = 1025;
const int kMaxSpectrum = 512;
//}}}

class cAppWindow : public cD2dWindow {
public:
  cAppWindow() : mPlayDoneSem("playDone") {}
  virtual ~cAppWindow() {}

  //{{{
  void run (bool stream, const string& title, int width, int height, const string& name) {

    initialise (title, width, height, false);
    add (new cCalendarBox (this, 190.f,150.f, mTimePoint), -190.f,0.f);
    add (new cClockBox (this, 40.f, mTimePoint), -82.f,150.f);

    mJpegImageView = new cJpegImageView (this, 0.f,-220.f, false, false, mSong.mImage);
    add (mJpegImageView);

    add (new cLogBox (this, 200.f,-200.f, true), 0.f,-200.f)->setPin (false);

    add (new cSongFreqBox (this, 0,100.f, mSong), 0,-640.f);
    add (new cSongSpectrumBox (this, 0,300.f, mSong), 0,-540.f);
    add (new cSongWaveBox (this, 0,100.f, mSong), 0,-220.f);
    add (new cSongLensBox (this, 0,100.f, mSong), 0.f,-120.f);
    add (new cSongTimeBox (this, 600.f,50.f, mSong), -600.f,-50.f);

    mFileList = new cFileList (name, "*.aac;*.mp3");
    thread([=]() { mFileList->watchThread(); }).detach();
    add (new cAppFileListBox (this, 0.f,-220.f, mFileList))->setPin (true);

    mVolumeBox = new cVolumeBox (this, 12.f,0.f, nullptr);
    add (mVolumeBox, -12.f,0.f);
    add (new cWindowBox (this, 60.f,24.f), -60.f,0.f)->setPin (false);

    if (stream) {
      // allocate stream
      mStreamFirst = (uint8_t*)malloc (200000000);
      mStreamLast = mStreamFirst;

      thread ([=]() { httpThread (name.c_str()); }).detach();
      thread ([=]() { analyseStreamThread(); }).detach();
      }
    else if (!mFileList->empty())
      thread ([=](){ analyseThread(); }).detach();

    // loop till quit
    messagePump();

    delete mFileList;
    delete mJpegImageView;
    }
  //}}}

protected:
  //{{{
  bool onKey (int key) {

    switch (key) {
      case 0x00: break;
      case 0x1B: return true;
      case  'F': toggleFullScreen(); break;

      case  ' ': mPlaying = !mPlaying; break;

      case 0x21: mSong.prevSilence(); changed(); break;; // page up
      case 0x22: mSong.nextSilence(); changed(); break;; // page down

      case 0x25: mSong.incPlaySec (getControl() ? -10 : -1);  changed(); break; // left arrow  - 1 sec
      case 0x27: mSong.incPlaySec (getControl() ?  10 :  1);  changed(); break; // right arrow  + 1 sec

      case 0x24: mSong.setPlayFrame (0); changed(); break; // home
      case 0x23: mSong.setPlayFrame (mSong.mNumFrames-1); changed(); break; // end

      case 0x26: if (mFileList->prevIndex()) changed(); break; // up arrow
      case 0x28: if (mFileList->nextIndex()) changed(); break; // down arrow
      case 0x0d: mChanged = true; changed(); break; // enter - play file

      default  : cLog::log (LOGINFO, "key %x", key);
      }

    return false;
    }
  //}}}

private:
  //{{{
  class cFrame {
  public:
    cFrame (uint32_t streamIndex, uint32_t len, uint8_t* powerValues, float* freqValues, uint8_t* lumaValues)
        : mStreamIndex(streamIndex), mLen(len) {

      memcpy (mPowerValues, powerValues, kMaxChannels);
      memcpy (mFreqValues, freqValues, kMaxFreq*4);
      memcpy (mFreqLuma, lumaValues, kMaxSpectrum);

      mSilent = isSilentThreshold();
      }

    bool isSilent() { return mSilent; }
    bool isSilentThreshold() { return mPowerValues[0] + mPowerValues[1] <= kSilentThreshold; }

    bool hasTitle() { return !mTitle.empty(); }
    void setTitle (const string& title) { mTitle = title; }

    // vars
    uint32_t mStreamIndex;
    uint32_t mLen;

    bool mSilent;
    uint8_t mPowerValues[kMaxChannels];
    float mFreqValues[kMaxFreq];
    uint8_t mFreqLuma[kMaxSpectrum];

    string mTitle;
    };
  //}}}
  //{{{
  class cSong {
  public:
    //{{{
    cSong() {
      }
    //}}}
    //{{{
    virtual ~cSong() {
      mFrames.clear();
      }
    //}}}

    //{{{
    void init (string fileName, bool aac, uint16_t samplesPerFrame) {

      mFrames.clear();

      mPlayFrame = 0;
      mNumFrames = 0;

      mFileName = fileName;
      mPathName = "";

      mAac = aac;
      mSamplesPerFrame = samplesPerFrame;

      mMaxPowerValue = 0;

      mMaxFreqValue = 0.f;
      for (int i = 0; i < kMaxFreq; i++)
        mMaxFreqValues[i] = 0.f;

      fftrConfig = kiss_fftr_alloc (kMaxSamples, 0, 0, 0);
      }
    //}}}
    //{{{
    int addFrame (uint32_t streamIndex, uint32_t frameLen, int numSamples, int16_t* samples, uint32_t streamLen) {
    // return true if enough frames added to start playing

     // sum of squares of channel power
     float powerSum[kMaxChannels] = {0};
     for (auto i = 0; i < numSamples; i++) {
        timeBuf[i] = (float)samples[i * 2] + (float)samples[(i * 2) + 1];
        powerSum[0] += samples[i*2] * samples[i*2];
        powerSum[1] += samples[(i*2) + 1] * samples[(i*2) + 1];
        }

      // uint8_t channel powerValues
      uint8_t powerValues[kMaxChannels];
      powerValues[0] = uint8_t(sqrtf (powerSum[0]) / numSamples);
      powerValues[1] = uint8_t(sqrtf (powerSum[1]) / numSamples);
      mMaxPowerValue = max (mMaxPowerValue, powerValues[0]);
      mMaxPowerValue = max (mMaxPowerValue, powerValues[1]);

      kiss_fftr (fftrConfig, timeBuf, freqBuf);

      float freqValues[kMaxFreq];
      for (auto i = 0; i < kMaxFreq; i++) {
        freqValues[i] = sqrt ((freqBuf[i].r * freqBuf[i].r) + (freqBuf[i].i * freqBuf[i].i));
        mMaxFreqValue = max (mMaxFreqValue, freqValues[i]);
        mMaxFreqValues[i] = max (mMaxFreqValues[i], freqValues[i]);
        }

      uint8_t lumaValues[kMaxFreq];
      for (auto i = 0; i < kMaxSpectrum; i++) {
        auto value = uint8_t((freqValues[i] / mMaxFreqValue) * 1024.f);
        lumaValues[i] = value > 255 ? 255 : value;
        }

      mFrames.push_back (cFrame (streamIndex, frameLen, powerValues, freqValues, lumaValues));

      // estimate numFrames
      mNumFrames = int (uint64_t(streamLen - mFrames[0].mStreamIndex) * (uint64_t)mFrames.size() /
                        uint64_t(streamIndex + frameLen - mFrames[0].mStreamIndex));

      // calc silent window
      auto frame = getNumLoadedFrames()-1;
      if (mFrames[frame].isSilent()) {
        auto window = kSilentWindow;
        auto windowFrame = frame - 1;
        while ((window >= 0) && (windowFrame >= 0)) {
          // walk backwards looking for no silence
          if (!mFrames[windowFrame].isSilentThreshold()) {
            mFrames[frame].mSilent = false;
            break;
            }
          windowFrame--;
          window--;
          }
        }

      return frame;
      }
    //}}}
    //{{{
    void setTitle (string title) {
      if (!mFrames.empty())
        mFrames.back().setTitle (title);
      }
    //}}}

    // gets
    int getNumLoadedFrames() { return (int)mFrames.size(); }
    //{{{
    uint32_t getPlayFrame() {

      if (mFrames.empty())
        return 0;
      else if (mPlayFrame < mFrames.size())
        return mPlayFrame;
      else
        return (uint32_t)mFrames.size() - 1;
      }
    //}}}
    //{{{
    uint32_t getPlayFrameStreamIndex() {

      if (mFrames.empty())
        return 0;
      else if (mPlayFrame < mFrames.size())
        return mFrames[mPlayFrame].mStreamIndex;
      else
        return mFrames[0].mStreamIndex;
      }
    //}}}
    int getSamplesSize() { return mMaxSamplesPerFrame * mChannels * mBytesPerSample; }

    // sets
    //{{{
    void setPlayFrame (int frame) {
      mPlayFrame = min (max (frame, 0), mNumFrames-1);
      }
    //}}}
    void incPlayFrame (int frames) { setPlayFrame (mPlayFrame + frames); }
    void incPlaySec (int secs) { incPlayFrame (secs * mSamplesPerSec / mSamplesPerFrame); }

    //{{{
    void prevSilence() {
      mPlayFrame = skipPrev (mPlayFrame, false);
      mPlayFrame = skipPrev (mPlayFrame, true);
      mPlayFrame = skipPrev (mPlayFrame, false);
      }
    //}}}
    //{{{
    void nextSilence() {
      mPlayFrame = skipNext (mPlayFrame, true);
      mPlayFrame = skipNext (mPlayFrame, false);
      mPlayFrame = skipNext (mPlayFrame, true);
      }
    //}}}

    // vars
    string mFileName;
    string mPathName;

    bool mAac = false;
    uint16_t mChannels = 2;
    uint16_t mBytesPerSample = 2;
    uint16_t mBitsPerSample = 16;
    uint16_t mSamplesPerFrame = 1152;
    uint16_t mMaxSamplesPerFrame = kMaxSamples;
    uint16_t mSamplesPerSec = 44100;

    concurrency::concurrent_vector<cFrame> mFrames;

    int mPlayFrame = 0;
    int mNumFrames = 0;

    uint8_t mMaxPowerValue = 0;

    kiss_fftr_cfg fftrConfig;
    kiss_fft_scalar timeBuf[kMaxSamples];
    kiss_fft_cpx freqBuf[kMaxFreq];

    float mMaxFreqValue = 0.f;
    float mMaxFreqValues[kMaxFreq];

    cJpegImage* mImage = nullptr;

  private:
    //{{{
    int skipPrev (int fromFrame, bool silent) {

      for (auto frame = fromFrame-1; frame >= 0; frame--)
        if (mFrames[frame].isSilent() ^ silent)
          return frame;

      return fromFrame;
      }
    //}}}
    //{{{
    int skipNext (int fromFrame, bool silent) {

      for (auto frame = fromFrame; frame < getNumLoadedFrames(); frame++)
        if (mFrames[frame].isSilent() ^ silent)
          return frame;

      return fromFrame;
      }
    //}}}
    };
  //}}}

  //{{{
  class cSongFreqBox : public cBox {
  public:
    //{{{
    cSongFreqBox (cD2dWindow* window, float width, float height, cSong& song) :
        cBox ("songWaveBox", window, width, height), mSong(song) {

      mPin = true;
      }
    //}}}
    virtual ~cSongFreqBox() {}

    void onDraw (ID2D1DeviceContext* dc) {

      int frame = mSong.getPlayFrame();
      if (frame > 0) {
        float* freq = (mSong.mFrames[frame].mFreqValues);
        for (int i = 0; (i < getWidth()) && (i < kMaxFreq); i++)
          dc->FillRectangle (cRect (mRect.left+i*2, mRect.bottom - ((freq[i] / mSong.mMaxFreqValue) * getHeight()),
                                    mRect.left+(i*2)+2, mRect.bottom), mWindow->getYellowBrush());
        }
      }

  protected:
    cSong& mSong;
    };
  //}}}
  //{{{
  class cSongSpectrumBox : public cBox {
  public:
    //{{{
    cSongSpectrumBox (cD2dWindow* window, float width, float height, cSong& song) :
        cBox("songSpectrumBox", window, width, height), mSong(song) {

      mPin = true;
      //mTransform = D2D1::Matrix3x2F::Scale ({2.0f, 2.0f}, {0.f, 0.f});
      }
    //}}}
    //{{{
    virtual ~cSongSpectrumBox() {
      if (mBitmap)
        mBitmap->Release();
      free (mBitmapBuf);
      }
    //}}}

    void onDraw (ID2D1DeviceContext* dc) {
      if (!mBitmap || (mBitmapWidth != getWidthInt()) || (mBitmapHeight = getHeightInt())) {
        //{{{  create bitmapBuf and ID2D1Bitmap matching box size
        mBitmapWidth = getWidthInt();
        mBitmapHeight = getHeightInt();

        free (mBitmapBuf);
        mBitmapBuf = (uint8_t*)malloc (mBitmapWidth * mBitmapHeight);

        if (mBitmap)
          mBitmap->Release();
        dc->CreateBitmap (D2D1::SizeU (mBitmapWidth, mBitmapHeight),
                          { DXGI_FORMAT_A8_UNORM, D2D1_ALPHA_MODE_STRAIGHT, 0,0 },
                          &mBitmap);
        }
        //}}}

      // left and right edge frames, may be outside known frames
      auto leftFrame = mSong.mPlayFrame - (getWidthInt()/2);
      auto rightFrame = mSong.mPlayFrame + (getWidthInt()/2);

      // first and last known frames
      auto firstFrame = max (leftFrame, 0);
      auto lastFrame = min (rightFrame, mSong.getNumLoadedFrames());

      // bottom of first bitmapBuf column
      auto dstPtr = mBitmapBuf + ((mBitmapHeight-1) * mBitmapWidth);
      for (int frame = firstFrame; frame < lastFrame; frame++) {
        // copy freqLuma to bitmapBuf column
        auto srcPtr = mSong.mFrames[frame].mFreqLuma;
        for (int y = 0; y < mBitmapHeight; y++) {
          *dstPtr = *srcPtr++;
          dstPtr -= mBitmapWidth;
          }

        // bottom of next bitmapBuf column
        dstPtr += (mBitmapWidth * mBitmapHeight) + 1;
        }

      // copy bitmapBuf to ID2D1Bitmap
      mBitmap->CopyFromMemory (&D2D1::RectU (0, 0, lastFrame - firstFrame, mBitmapHeight), mBitmapBuf, mBitmapWidth);

      // stamp colour through ID2D1Bitmap alpha using offset and width
      float dstLeft = (firstFrame > leftFrame) ? float(firstFrame - leftFrame) : 0.f;
      //dc->SetTransform (mTransform);
      dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_ALIASED);
      dc->FillOpacityMask (mBitmap, mWindow->getWhiteBrush(),
                           &D2D1::RectF (dstLeft, mRect.top, dstLeft + lastFrame - firstFrame, mRect.bottom), // dstRect
                           &D2D1::RectF (0.f,0.f, float(lastFrame - firstFrame), (float)mBitmapHeight));      // srcRect
      dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
      //dc->SetTransform (D2D1::Matrix3x2F::Identity());
      }

  private:
    cSong& mSong;

    int mBitmapWidth = 0;
    int mBitmapHeight = 0;

    uint8_t* mBitmapBuf = nullptr;
    ID2D1Bitmap* mBitmap = nullptr;

    D2D1::Matrix3x2F mTransform = D2D1::Matrix3x2F::Identity();
    };
  //}}}
  //{{{
  class cSongWaveBox : public cBox {
  public:
    //{{{
    cSongWaveBox (cD2dWindow* window, float width, float height, cSong& song) :
        cBox ("songWaveBox", window, width, height), mSong(song) {

      mPin = true;
      }
    //}}}
    virtual ~cSongWaveBox() {}

    //{{{
    bool onMove (bool right, cPoint pos, cPoint inc) {
      mSong.incPlayFrame (int(-inc.x));
      return true;
      }
    //}}}
    //{{{
    bool onWheel (int delta, cPoint pos)  {

      if (getShow()) {
        mZoom -= delta/120;
        mZoom = min (max(mZoom, 1), 2 * (1 + mSong.mNumFrames / getWidthInt()));
        return true;
        }

      return false;
      }
    //}}}
    //{{{
    void onDraw (ID2D1DeviceContext* dc) {

      auto leftFrame = mSong.mPlayFrame - (mZoom * getWidthInt()/2);
      auto rightFrame = mSong.mPlayFrame + (mZoom * getWidthInt()/2);
      auto firstX = (leftFrame < 0) ? (-leftFrame) / mZoom : 0;

      draw (dc, leftFrame, rightFrame, firstX, mZoom);
      }
    //}}}

  protected:
    int getMaxPowerValue() { return mSong.mMaxPowerValue > 0 ? mSong.mMaxPowerValue : 1; }

    //{{{
    void draw (ID2D1DeviceContext* dc, int leftFrame, int rightFrame, int firstX, int zoom) {

      leftFrame = (leftFrame < 0) ? 0 : leftFrame;
      if (rightFrame > mSong.getNumLoadedFrames())
        rightFrame = mSong.getNumLoadedFrames();

      // draw frames
      auto colour = mWindow->getBlueBrush();

      float yCentre = getCentreY();
      float valueScale = getHeight() / 2 / getMaxPowerValue();

      bool centre = false;
      float xl = mRect.left + firstX;
      for (auto frame = leftFrame; frame < rightFrame; frame += zoom) {
        float xr = xl + 1.f;
        if (mSong.mFrames[frame].hasTitle()) {
          dc->FillRectangle (cRect (xl, yCentre-(getHeight()/2), xr+2, yCentre+(getHeight()/2)), mWindow->getYellowBrush());

          string str = mSong.mFrames[frame].mTitle;
          IDWriteTextLayout* textLayout;
          mWindow->getDwriteFactory()->CreateTextLayout (
            wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
            mWindow->getTextFormat(), getWidth(), getHeight(), &textLayout);

          dc->DrawTextLayout (cPoint (xl, getTL().y-20.f), textLayout, mWindow->getWhiteBrush());
          textLayout->Release();
          }

        if (mSong.mFrames[frame].isSilent())
          dc->FillRectangle (cRect (xl, yCentre-kSilentThreshold, xr, yCentre+kSilentThreshold), mWindow->getRedBrush());

        if (!centre && (frame >= mSong.mPlayFrame)) {
          auto yLeft = mSong.mFrames[frame].mPowerValues[0] * valueScale;
          auto yRight = mSong.mFrames[frame].mPowerValues[1] * valueScale;
          dc->FillRectangle (cRect (xl, yCentre - yLeft, xr, yCentre + yRight), mWindow->getWhiteBrush());
          colour = mWindow->getGreyBrush();
          centre = true;
          }
        else {
          float yLeft = 0;
          float yRight = 0;
          for (auto i = 0; i < zoom; i++) {
            yLeft += mSong.mFrames[frame+i].mPowerValues[0];
            yRight += mSong.mFrames[frame+i].mPowerValues[1];
            }
          yLeft = (yLeft / zoom) * valueScale;
          yRight = (yRight / zoom) * valueScale;
          dc->FillRectangle (cRect (xl, yCentre - yLeft, xr, yCentre + yRight), colour);
          }
        xl = xr;
        }
      }
    //}}}

    cSong& mSong;
    int mZoom = 1;
    };
  //}}}
  //{{{
  class cSongLensBox : public cSongWaveBox {
  public:
    //{{{
    cSongLensBox (cD2dWindow* window, float width, float height, cSong& frameSet)
      : cSongWaveBox(window, width, height, frameSet) {}
    //}}}
    //{{{
    virtual ~cSongLensBox() {
      bigFree (mSummedValues);
      }
    //}}}

    //{{{
    void layout() {
      mSummedFrame = -1;
      cSongWaveBox::layout();
      }
    //}}}

    //{{{
    bool onDown (bool right, cPoint pos)  {

      mOn = true;

      auto frame = int((pos.x * mSong.mNumFrames) / getWidth());
      mSong.setPlayFrame (frame);

      return true;
      }
    //}}}
    //{{{
    bool onMove (bool right, cPoint pos, cPoint inc) {
      auto frame = int((pos.x * mSong.mNumFrames) / getWidth());
      mSong.setPlayFrame (frame);
      return true;
      }
    //}}}
    //{{{
    bool onUp (bool right, bool mouseMoved, cPoint pos) {
      mOn = false;
      return cSongWaveBox::onUp (right, mouseMoved, pos);
      }
    //}}}
    //{{{
    bool onWheel (int delta, cPoint pos)  {
      return false;
      }
    //}}}

    //{{{
    void onDraw (ID2D1DeviceContext* dc) {

      if (mOn) {
        if (mLens < getWidthInt() / 16)
          // animate on
          mLens += (getWidthInt() / 16) / 6;
        }
      else if (mLens <= 0) {
        mLens = 0;
        draw (dc, 0, mMaxSummedX);
        return;
        }
      else // animate off
        mLens /= 2;

      int curFrameX = (mSong.mNumFrames > 0) ? (mSong.mPlayFrame * getWidthInt()) / mSong.mNumFrames : 0;
      int leftLensX = curFrameX - mLens;
      int rightLensX = curFrameX + mLens;
      if (leftLensX < 0) {
        rightLensX -= leftLensX;
        leftLensX = 0;
        }
      else
        draw (dc, 0, leftLensX);

      if (rightLensX > getWidthInt()) {
        leftLensX -= rightLensX - getWidthInt();
        rightLensX = getWidthInt();
        }
      else
        draw (dc, rightLensX, getWidthInt());

      cSongWaveBox::draw (dc, mSong.mPlayFrame - mLens, mSong.mPlayFrame + mLens-2, leftLensX+1, 1);

      dc->DrawRectangle (cRect(mRect.left + leftLensX, mRect.top + 1.f,
                               mRect.left + rightLensX, mRect.top + getHeight() - 1.f),
                         mWindow->getYellowBrush(), 1.f);
      }
    //}}}

  private:
    //{{{
    void makeSummedWave() {

      if (mSummedFrame != mSong.getNumLoadedFrames()) {
        // frameSet changed, cache values summed to width, scaled to height
        mSummedFrame = mSong.getNumLoadedFrames();

        mSummedValues = (uint8_t*)realloc (mSummedValues, getWidthInt() * 2 * sizeof(int16_t));
        auto summedValuesPtr = mSummedValues;

        mMaxSummedX = 0;
        auto startFrame = 0;
        for (auto x = 0; x < getWidthInt(); x++) {
          int frame = x * mSong.mNumFrames / getWidthInt();
          if (frame >= mSong.getNumLoadedFrames())
            break;

          int lValue = mSong.mFrames[frame].mPowerValues[0];
          int rValue = mSong.mFrames[frame].mPowerValues[1];
          if (frame > startFrame) {
            int num = 1;
            for (auto i = startFrame; i < frame; i++) {
              lValue += mSong.mFrames[i].mPowerValues[0];
              rValue += mSong.mFrames[i].mPowerValues[1];
              num++;
              }
            lValue /= num;
            rValue /= num;
            }
          *summedValuesPtr++ = lValue;
          *summedValuesPtr++ = rValue;

          mMaxSummedX = x;
          startFrame = frame + 1;
          }
        }
      }
    //}}}
    //{{{
    void draw (ID2D1DeviceContext* dc, int firstX, int lastX) {

      makeSummedWave();

      // draw cached graphic
      auto colour = mWindow->getBlueBrush();

      auto centreY = getCentreY();
      float valueScale = getHeight() / 2 / getMaxPowerValue();

      float curFrameX = mRect.left;
      if (mSong.mNumFrames > 0)
        curFrameX += mSong.mPlayFrame * getWidth() / mSong.mNumFrames;

      bool centre = false;
      float xl = mRect.left + firstX;
      auto summedValuesPtr = mSummedValues + (firstX * 2);
      for (auto x = firstX; x < lastX; x++) {
        float xr = xl + 1.f;
        if (!centre && (x >= curFrameX) && (mSong.mPlayFrame < mSong.getNumLoadedFrames())) {
          float leftValue = mSong.mFrames[mSong.mPlayFrame].mPowerValues[0] * valueScale;
          float rightValue = mSong.mFrames[mSong.mPlayFrame].mPowerValues[1] * valueScale;
          dc->FillRectangle (cRect(xl, centreY - leftValue - 2.f, xr, centreY + rightValue + 2.f),
                             mWindow->getWhiteBrush());
          colour = mWindow->getGreyBrush();
          centre = true;
          }

        else if (x < mMaxSummedX) {
          auto leftValue = *summedValuesPtr++ * valueScale;
          auto rightValue = *summedValuesPtr++ * valueScale;
          dc->FillRectangle (cRect(xl, centreY - leftValue - 2.f, xr, centreY + rightValue + 2.f),
                             colour);
          }
        else
          break;
        xl = xr;
        }
      }
    //}}}

    uint8_t* mSummedValues;
    int mSummedFrame = -1;
    int mMaxSummedX = 0;

    bool mOn = false;
    int mLens = 0;
    };
  //}}}
  //{{{
  class cSongTimeBox : public cBox {
  public:
    //{{{
    cSongTimeBox (cAppWindow* window, float width, float height, cSong& song) :
        cBox("frameSetTime", window, width, height), mSong(song) {

      mWindow->getDwriteFactory()->CreateTextFormat (L"Consolas", NULL,
        DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 50.f, L"en-us",
        &mTextFormat);
      mTextFormat->SetTextAlignment (DWRITE_TEXT_ALIGNMENT_TRAILING);

      mPin = true;
      }
    //}}}
    //{{{
    virtual ~cSongTimeBox() {
      mTextFormat->Release();
      }
    //}}}

    //{{{
    bool onDown (bool right, cPoint pos)  {

      auto appWindow = dynamic_cast<cAppWindow*>(mWindow);
      appWindow->mPlaying = !appWindow->mPlaying;
      return true;
      }
    //}}}
    //{{{
    void onDraw (ID2D1DeviceContext* dc) {

      string str = getFrameStr (mSong.mPlayFrame) + " " + getFrameStr (mSong.mNumFrames);

      IDWriteTextLayout* textLayout;
      mWindow->getDwriteFactory()->CreateTextLayout (
        wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
        mTextFormat, getWidth(), getHeight(), &textLayout);

      dc->DrawTextLayout (getTL (2.f), textLayout, mWindow->getBlackBrush());
      dc->DrawTextLayout (getTL(), textLayout, mWindow->getWhiteBrush());

      textLayout->Release();
      }
    //}}}

  private:
    //{{{
    string getFrameStr (uint32_t frame) {

      uint32_t frameHs = frame * mSong.mSamplesPerFrame / (mSong.mSamplesPerSec / 100);

      uint32_t hs = frameHs % 100;

      frameHs /= 100;
      uint32_t secs = frameHs % 60;

      frameHs /= 60;
      uint32_t mins = frameHs % 60;

      frameHs /= 60;
      uint32_t hours = frameHs % 60;

      string str (hours ? (dec (hours) + ':' + dec (mins, 2, '0')) : dec (mins));
      return str + ':' + dec(secs, 2, '0') + ':' + dec(hs, 2, '0');
      }
    //}}}

    cSong& mSong;

    IDWriteTextFormat* mTextFormat = nullptr;
    };
  //}}}
  //{{{
  class cAppFileListBox : public cFileListBox {
  public:
    cAppFileListBox (cD2dWindow* window, float width, float height, cFileList* fileList) :
      cFileListBox (window, width, height, fileList) {}

    void onHit() {
      (dynamic_cast<cAppWindow*>(getWindow()))->mChanged = true;
      }
    };
  //}}}

  //{{{
  bool parseId3Tag (uint8_t* stream, uint8_t* streamLast) {
  // look for ID3 Jpeg tag

    auto ptr = stream;
    auto tag = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];

    if (tag == 0x49443303)  {
      // ID3 tag
      auto tagSize = (ptr[6] << 21) | (ptr[7] << 14) | (ptr[8] << 7) | ptr[9];
      cLog::log (LOGINFO, "parseId3Tag - %c%c%c ver:%d %02x flags:%02x tagSize:%d",
                           ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], tagSize);
      ptr += 10;

      while (ptr < stream + tagSize) {
        auto tag = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];
        auto frameSize = (ptr[4] << 24) | (ptr[5] << 16) | (ptr[6] << 8) | ptr[7];
        if (!frameSize)
          break;

        auto frameFlags1 = ptr[8];
        auto frameFlags2 = ptr[9];
        string info;
        for (auto i = 0; i < (frameSize < 40 ? frameSize : 40); i++)
          if ((ptr[10+i] >= 0x20) && (ptr[10+i] < 0x7F))
            info += ptr[10+i];

        cLog::log (LOGINFO, "parseId3Tag - %c%c%c%c %02x %02x %d %s",
                             ptr[0], ptr[1], ptr[2], ptr[3], frameFlags1, frameFlags2, frameSize, info.c_str());

        if (tag == 0x41504943) {
          // APIC tag
          cLog::log (LOGINFO3, "parseId3Tag - APIC jpeg tag found");
          auto jpegLen = frameSize - 14;
          auto jpegBuf =  (uint8_t*)malloc (jpegLen);
          memcpy (jpegBuf, ptr + 10 + 14, jpegLen);

          cLog::log (LOGINFO2, "found jpeg tag");

          // create new
          mSong.mImage = new cJpegImage();
          mSong.mImage->setBuf (jpegBuf, jpegLen);
          mJpegImageView->setImage (mSong.mImage);
          return true;
          }

        ptr += frameSize + 10;
        }
      }

    return false;
    }
  //}}}
  //{{{
  bool parseFrame (uint8_t* stream, uint8_t* streamLast, uint8_t*& frame, int& frameLen, bool& aac, bool& id3Tag, int& skip) {
  // start of mp3 / aac adts parser

    skip = 0;

    while ((streamLast - stream) >= 6) {
      if (stream[0] == 'I' && stream[1] == 'D' && stream[2] == '3') {
        //{{{  id3 header
        auto tagSize = (stream[6] << 21) | (stream[7] << 14) | (stream[8] << 7) | stream[9];

        // return tag & size
        frame = stream;
        frameLen = 10 + tagSize;

        aac = false;
        id3Tag = true;

        // check for enough bytes for frame body
        return stream + frameLen < streamLast;
        }
        //}}}

      else if ((stream[0] == 0xFF) && ((stream[1] & 0xF0) == 0xF0)) {
        // syncWord found
        if ((stream[1] & 0x06) == 0) {
          //{{{  aac header
          // Header consists of 7 or 9 bytes (without or with CRC).
          // AAAAAAAA AAAABCCD EEFFFFGH HHIJKLMM MMMMMMMM MMMOOOOO OOOOOOPP (QQQQQQQQ QQQQQQQQ)
          //
          // A 12   syncword 0xFFF, all bits must be 1
          // B  1  MPEG Version: 0 for MPEG-4, 1 for MPEG-2
          // C  2  Layer: always 0
          // D  1  protection absent, Warning, set to 1 if there is no CRC and 0 if there is CRC
          // E  2  profile, the MPEG-4 Audio Object Type minus 1
          // F  4  MPEG-4 Sampling Frequency Index (15 is forbidden)
          // G  1  private bit, guaranteed never to be used by MPEG, set to 0 when encoding, ignore when decoding
          // H  3  MPEG-4 Channel Configuration (in the case of 0, the channel configuration is sent via an inband PCE)
          // I  1  originality, set to 0 when encoding, ignore when decoding
          // J  1  home, set to 0 when encoding, ignore when decoding
          // K  1  copyrighted id bit, the next bit of a centrally registered copyright identifier, set to 0 when encoding, ignore when decoding
          // L  1  copyright id start, signals that this frame's copyright id bit is the first bit of the copyright id, set to 0 when encoding, ignore when decoding
          // M 13  frame length, this value must include 7 or 9 bytes of header length: FrameLength = (ProtectionAbsent == 1 ? 7 : 9) + size(AACFrame)
          // O 11  buffer fullness
          // P  2  Number of AAC frames (RDBs) in ADTS frame minus 1, for maximum compatibility always use 1 AAC frame per ADTS frame
          // Q 16  CRC if protection absent is 0

          const int sampleRates[16] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350, 0,0,0};

          auto sampleRate = sampleRates [(stream[2] & 0x3c) >> 2];

          // return aacFrame & size
          frame = stream;
          frameLen = (((unsigned int)stream[3] & 0x3) << 11) | (((unsigned int)stream[4]) << 3) | (stream[5] >> 5);

          aac = true;
          id3Tag = false;

          // check for enough bytes for frame body
          return stream + frameLen < streamLast;
          }
          //}}}
        else {
          //{{{  mp3 header
          // AAAAAAAA AAABBCCD EEEEFFGH IIJJKLMM
          //{{{  A 31:21  Frame sync (all bits must be set)
          //}}}
          //{{{  B 20:19  MPEG Audio version ID
          //   0 - MPEG Version 2.5 (later extension of MPEG 2)
          //   01 - reserved
          //   10 - MPEG Version 2 (ISO/IEC 13818-3)
          //   11 - MPEG Version 1 (ISO/IEC 11172-3)
          //   Note: MPEG Version 2.5 was added lately to the MPEG 2 standard.
          // It is an extension used for very low bitrate files, allowing the use of lower sampling frequencies.
          // If your decoder does not support this extension, it is recommended for you to use 12 bits for synchronization
          // instead of 11 bits.
          //}}}
          //{{{  C 18:17  Layer description
          //   00 - reserved
          //   01 - Layer III
          //   10 - Layer II
          //   11 - Layer I
          //}}}
          //{{{  D    16  Protection bit
          //   0 - Protected by CRC (16bit CRC follows header)
          //   1 - Not protected
          //}}}
          //{{{  E 15:12  Bitrate index
          // V1 - MPEG Version 1
          // V2 - MPEG Version 2 and Version 2.5
          // L1 - Layer I
          // L2 - Layer II
          // L3 - Layer III
          //   bits  V1,L1   V1,L2  V1,L3  V2,L1  V2,L2L3
          //   0000   free    free   free   free   free
          //   0001  32  32  32  32  8
          //   0010  64  48  40  48  16
          //   0011  96  56  48  56  24
          //   0100  128 64  56  64  32
          //   0101  160 80  64  80  40
          //   0110  192 96  80  96  48
          //   0111  224 112 96  112 56
          //   1000  256 128 112 128 64
          //   1001  288 160 128 144 80
          //   1010  320 192 160 160 96
          //   1011  352 224 192 176 112
          //   1100  384 256 224 192 128
          //   1101  416 320 256 224 144
          //   1110  448 384 320 256 160
          //   1111  bad bad bad bad bad
          //   values are in kbps
          //}}}
          //{{{  F 11:10  Sampling rate index
          //   bits  MPEG1     MPEG2    MPEG2.5
          //    00  44100 Hz  22050 Hz  11025 Hz
          //    01  48000 Hz  24000 Hz  12000 Hz
          //    10  32000 Hz  16000 Hz  8000 Hz
          //    11  reserv.   reserv.   reserv.
          //}}}
          //{{{  G     9  Padding bit
          //   0 - frame is not padded
          //   1 - frame is padded with one extra slot
          //   Padding is used to exactly fit the bitrate.
          // As an example: 128kbps 44.1kHz layer II uses a lot of 418 bytes
          // and some of 417 bytes long frames to get the exact 128k bitrate.
          // For Layer I slot is 32 bits long, for Layer II and Layer III slot is 8 bits long.
          //}}}
          //{{{  H     8  Private bit. This one is only informative.
          //}}}
          //{{{  I   7:6  Channel Mode
          //   00 - Stereo
          //   01 - Joint stereo (Stereo)
          //   10 - Dual channel (2 mono channels)
          //   11 - Single channel (Mono)
          //}}}
          //{{{  K     3  Copyright
          //   0 - Audio is not copyrighted
          //   1 - Audio is copyrighted
          //}}}
          //{{{  L     2  Original
          //   0 - Copy of original media
          //   1 - Original media
          //}}}
          //{{{  M   1:0  emphasis
          //   00 - none
          //   01 - 50/15 ms
          //   10 - reserved
          //   11 - CCIT J.17
          //}}}

          const uint32_t bitRates[16] = {     0,  32000,  40000, 48000,
                                          56000,  64000,  80000,  96000,
                                         112000, 128000, 160000, 192000,
                                         224000, 256000, 320000,      0};

          const uint32_t sampleRates[4] = { 44100, 48000, 32000, 0};

          uint8_t version = (stream[1] & 0x08) >> 3;
          uint8_t layer = (stream[1] & 0x06) >> 1;
          uint8_t errp = stream[1] & 0x01;

          uint8_t bitrateIndex = (stream[2] & 0xf0) >> 4;
          uint8_t sampleRateIndex = (stream[2] & 0x0c) >> 2;
          uint8_t pad = (stream[2] & 0x02) >> 1;
          uint8_t priv = (stream[2] & 0x01);

          uint8_t mode = (stream[3] & 0xc0) >> 6;
          uint8_t modex = (stream[3] & 0x30) >> 4;
          uint8_t copyright = (stream[3] & 0x08) >> 3;
          uint8_t original = (stream[3] & 0x04) >> 2;
          uint8_t emphasis = (stream[3] & 0x03);

          uint32_t bitrate = bitRates[bitrateIndex];
          uint32_t sampleRate = sampleRates[sampleRateIndex];

          int scale = (layer == 3) ? 48 : 144;
          int size = (bitrate * scale) / sampleRate;
          if (pad)
            size++;

          // return mp3Frame & size
          frame = stream;
          frameLen = size;
          aac = false;
          id3Tag = false;

          // check for enough bytes for frame body
          return stream + frameLen < streamLast;
          }
          //}}}
        }

      else {
        skip++;
        stream++;
        }
      }

    frame = nullptr;
    frameLen = 0;
    aac = false;
    id3Tag = false;

    return false;
    }
  //}}}
  //{{{
  bool parseFrames (uint8_t* stream, uint8_t* streamLast) {
  // return true if stream is aac adts

    int tags = 0;
    int frames = 0;
    int lostSync = 0;
    bool resultAac = false;

    uint8_t* frame = nullptr;
    int frameLen = 0;
    bool frameAac = false;
    bool tag = false;
    int skip = 0;
    while (parseFrame (stream, streamLast, frame, frameLen, frameAac, tag, skip)) {
      if (tag)
        tags++;
      else {
        resultAac = frameAac;
        frames++;
        }
      // onto next frame
      stream += skip + frameLen;
      lostSync += skip;
      }

    cLog::log (LOGINFO, "parseFrames f:%d lost:%d aac:%d tags:%d", frames, lostSync, resultAac, tags);

    return resultAac;
    }
  //}}}

  //{{{
  void addIcyInfo (string icyInfo) {
  // called by httpThread

    mIcyStr = icyInfo;
    cLog::log (LOGINFO1, "addIcyInfo " + mIcyStr);

    string searchStr = "StreamTitle=\'";
    auto searchStrPos = mIcyStr.find (searchStr);
    if (searchStrPos != string::npos) {
      auto searchEndPos = mIcyStr.find ("\';", searchStrPos + searchStr.size());
      if (searchEndPos != string::npos) {
        string titleStr = mIcyStr.substr (searchStrPos + searchStr.size(), searchEndPos - searchStrPos - searchStr.size());
        if (titleStr != mTitleStr) {
          cLog::log (LOGINFO1, "addIcyInfo found title = " + titleStr);
          mSong.setTitle (titleStr);
          mTitleStr = titleStr;
          }
        }
      }

    mUrlStr = "no url";
    searchStr = "StreamUrl=\'";
    searchStrPos = mIcyStr.find (searchStr);
    if (searchStrPos != string::npos) {
      auto searchEndPos = mIcyStr.find ('\'', searchStrPos + searchStr.size());
      if (searchEndPos != string::npos) {
        mUrlStr = mIcyStr.substr (searchStrPos + searchStr.size(), searchEndPos - searchStrPos - searchStr.size());
        cLog::log (LOGINFO1, "addIcyInfo found url = " + mUrlStr);
        }
      }
    }
  //}}}
  //{{{
  static int httpTrace (CURL* handle, curl_infotype type, char* ptr, size_t size, cAppWindow* appWindow) {

    switch (type) {
      case CURLINFO_TEXT:
        cLog::log (LOGINFO, "TEXT %s", ptr);
        break;

      case CURLINFO_HEADER_OUT:
        cLog::log (LOGINFO, "HEADER_OUT %s", ptr);
        break;

      case CURLINFO_DATA_OUT:
        cLog::log (LOGINFO, "DATA_OUT %s", ptr);
        break;

      case CURLINFO_SSL_DATA_OUT:
        cLog::log (LOGINFO, "SSL_DATA_OUT %s", ptr);
        break;

      case CURLINFO_HEADER_IN:
        cLog::log (LOGINFO, "HEADER_IN %s", ptr);
        break;

      case CURLINFO_DATA_IN:
        //cLog::log (LOGINFO, "DATA_IN size:%d", ptr);
        break;

      case CURLINFO_SSL_DATA_IN:
        cLog::log (LOGINFO, "SSL_DATA_IN size:%d", ptr);
        break;

      default:
        return 0;
      }

    return 0;
    }
  //}}}
  //{{{
  static size_t httpHeader (const char* ptr, size_t size, size_t numItems, cAppWindow* appWindow) {

    auto len = numItems * size;
    cLog::log (LOGINFO2, "len:%d  %s", len, ptr);

    string str (ptr);
    string searchStr ("icy-metaint:");

    auto searchStrPos = str.find (searchStr);
    if (searchStrPos != string::npos) {
      auto numStr = str.substr (searchStrPos + searchStr.size(), str.size() - searchStrPos - searchStr.size());
      auto num = stoi (numStr);
      cLog::log (LOGINFO, "httpHeader - found %s value:%d", searchStr.c_str(), num);

      appWindow->mIcySkipLen = num;
      }

    return len;
    }
  //}}}
  //{{{
  static size_t httpBody (uint8_t* ptr, size_t size, size_t numItems, cAppWindow* appWindow) {

    auto len = numItems * size;

    if ((appWindow->mIcyInfoCount >= appWindow->mIcyInfoLen)  &&
        (appWindow->mIcySkipCount + len <= appWindow->mIcySkipLen)) {

      cLog::log (LOGINFO1, "body simple copy len:%d", len);

      // simple copy of whole body, no metaInfo
      memcpy (appWindow->mStreamLast, ptr, len);
      appWindow->mStreamLast += len;
      appWindow->mIcySkipCount += (int)len;
      }

    else {
      cLog::log (LOGINFO1, "body split copy len:%d info:%d:%d skip:%d:%d ",
                            len,
                            appWindow->mIcyInfoCount, appWindow->mIcyInfoLen,
                            appWindow->mIcySkipCount, appWindow->mIcySkipLen);

      // dumb copy for metaInfo straddling body, could be much better
      for (int i = 0; i < len; i++) {
        if (appWindow->mIcyInfoCount < appWindow->mIcyInfoLen) {
          appWindow->mIcyInfo [appWindow->mIcyInfoCount] = ptr[i];
          appWindow->mIcyInfoCount++;
          if (appWindow->mIcyInfoCount >= appWindow->mIcyInfoLen)
            appWindow->addIcyInfo (appWindow->mIcyInfo);
          }
        else if (appWindow->mIcySkipCount >= appWindow->mIcySkipLen) {
          appWindow->mIcyInfoLen = ptr[i] * 16;
          appWindow->mIcyInfoCount = 0;
          appWindow->mIcySkipCount = 0;
          cLog::log (LOGINFO1, "body icyInfo len:", ptr[i] * 16);
          }
        else {
          appWindow->mIcySkipCount++;
          *appWindow->mStreamLast = ptr[i];
          appWindow->mStreamLast++;
          }
        }
      }

    appWindow->mStreamSem.notifyAll();
    return len;
    }
  //}}}

  //{{{
  void httpThread (const char* url) {

    cLog::setThreadName ("http");

    auto curl = curl_easy_init();
    if (curl) {
      //curl_easy_setopt (curl, CURLOPT_DEBUGFUNCTION, httpTrace);
      //curl_easy_setopt (curl, CURLOPT_DEBUGDATA, this);
      //curl_easy_setopt (curl, CURLOPT_VERBOSE, 1L);

      curl_easy_setopt (curl, CURLOPT_FOLLOWLOCATION, 1L);
      curl_easy_setopt (curl, CURLOPT_URL, url);

      struct curl_slist* slist = NULL;
      slist = curl_slist_append (slist, "Icy-MetaData: 1");
      curl_easy_setopt (curl, CURLOPT_HTTPHEADER, slist);

      curl_easy_setopt (curl, CURLOPT_HEADERFUNCTION, httpHeader);
      curl_easy_setopt (curl, CURLOPT_HEADERDATA, this);

      curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, httpBody);
      curl_easy_setopt (curl, CURLOPT_WRITEDATA, this);

      curl_easy_perform (curl);

      curl_slist_free_all (slist);
      }

    // never gets here
    curl_easy_cleanup (curl);
    }
  //}}}
  //{{{
  void analyseStreamThread() {

    cLog::setThreadName ("anls");

    // wait for a bit of stream to get codec
    while ((mStreamLast - mStreamFirst) < 1440)
      mStreamSem.wait();

    mStreamAac = parseFrames (mStreamFirst, mStreamLast);
    mSong.init ("stream", mStreamAac, mStreamAac ? 2048 : 1152);

    auto codec = avcodec_find_decoder (mStreamAac ? AV_CODEC_ID_AAC : AV_CODEC_ID_MP3);
    auto context = avcodec_alloc_context3 (codec);
    avcodec_open2 (context, codec, NULL);
    AVPacket avPacket;
    av_init_packet (&avPacket);
    auto avFrame = av_frame_alloc();
    auto samples = (int16_t*)malloc (mSong.getSamplesSize());

    auto stream = mStreamFirst;
    while (!getExit()) {
      bool frameAac;
      bool tag;
      int skip;
      while (parseFrame (stream, mStreamLast, avPacket.data, avPacket.size, frameAac, tag, skip)) {
        if (!tag && (frameAac == mStreamAac)) {
          auto ret = avcodec_send_packet (context, &avPacket);
          while (ret >= 0) {
            ret = avcodec_receive_frame (context, avFrame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF || ret < 0)
              break;
            cLog::log (LOGINFO2, "frame size:%d", avPacket.size);

            if (avFrame->nb_samples > 0) {
              //{{{  covert planar avFrame->data to interleaved int16_t samples
              switch (context->sample_fmt) {
                case AV_SAMPLE_FMT_S16P:
                  //{{{  16bit signed planar, copy planar to interleaved
                  for (auto channel = 0; channel < avFrame->channels; channel++) {
                    auto srcPtr = (short*)avFrame->data[channel];
                    auto dstPtr = (short*)(samples) + channel;
                    for (auto i = 0; i < avFrame->nb_samples; i++) {
                      *dstPtr = *srcPtr++;
                      dstPtr += avFrame->channels;
                      }
                    }
                  break;
                  //}}}
                case AV_SAMPLE_FMT_FLTP:
                  //{{{  32bit float planar, copy planar to interleaved
                  for (auto channel = 0; channel < avFrame->channels; channel++) {
                    auto srcPtr = (float*)avFrame->data[channel];
                    auto dstPtr = (short*)(samples) + channel;
                    for (auto i = 0; i < avFrame->nb_samples; i++) {
                      auto int16 = (short)(*srcPtr++ * 0x8000);
                      *dstPtr = int16;
                      dstPtr += avFrame->channels;
                      }
                    }
                  break;
                  //}}}
                default:
                  cLog::log (LOGERROR, "analyseStreamThread - unrecognised sample_fmt " + dec (context->sample_fmt));
                }
              //}}}
              if (mSong.addFrame (uint32_t(avPacket.data - mStreamFirst), avPacket.size,
                                  avFrame->nb_samples, samples, int(mStreamLast - mStreamFirst)) == kPlayFrameThreshold) {
                //{{{  launch playThread
                auto threadHandle = thread ([=](){ playThread (false); });
                SetThreadPriority (threadHandle.native_handle(), THREAD_PRIORITY_TIME_CRITICAL);
                threadHandle.detach();
                }
                //}}}
              changed();
              }
            }
          }
        stream += skip + avPacket.size;
        }

      mStreamSem.wait();
      }

    // done
    free (samples);
    av_frame_free (&avFrame);
    if (context)
      avcodec_close (context);
    }
  //}}}
  //{{{
  void analyseThread() {

    cLog::setThreadName ("anal");

    while (!getExit()) {
      // open file mapping
      auto fileHandle = CreateFile (mFileList->getCurFileItem().getFullName().c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
      auto mapping = CreateFileMapping (fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
      mStreamFirst = (uint8_t*)MapViewOfFile (mapping, FILE_MAP_READ, 0, 0, 0);
      mStreamLast = mStreamFirst + GetFileSize (fileHandle, NULL);

      mStreamAac = parseFrames (mStreamFirst, mStreamLast);
      mSong.init (mFileList->getCurFileItem().getFullName(), mStreamAac, mStreamAac ? 2048 : 1152);

      // replace jpeg
      auto temp = mSong.mImage;
      mSong.mImage = nullptr;
      delete temp;
      parseId3Tag (mStreamFirst, mStreamLast);

      auto codec = avcodec_find_decoder (mStreamAac ? AV_CODEC_ID_AAC : AV_CODEC_ID_MP3);
      auto context = avcodec_alloc_context3 (codec);
      avcodec_open2 (context, codec, NULL);

      AVPacket avPacket;
      av_init_packet (&avPacket);
      auto avFrame = av_frame_alloc();
      auto samples = (int16_t*)malloc (mSong.getSamplesSize());

      auto stream = mStreamFirst;
      bool frameAac;
      bool tag;
      int skip;
      while (!getExit() && !mChanged &&
             parseFrame (stream, mStreamLast, avPacket.data, avPacket.size, frameAac, tag, skip)) {
        if ((frameAac == mStreamAac) && !tag) {
          auto ret = avcodec_send_packet (context, &avPacket);
          while (ret >= 0) {
            ret = avcodec_receive_frame (context, avFrame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF || ret < 0)
              break;
            if (avFrame->nb_samples > 0) {
              //{{{  covert planar avFrame->data to interleaved int16_t samples
              switch (context->sample_fmt) {
                case AV_SAMPLE_FMT_S16P:
                  //{{{  16bit signed planar, copy planar to interleaved
                  for (auto channel = 0; channel < avFrame->channels; channel++) {
                    auto srcPtr = (short*)avFrame->data[channel];
                    auto dstPtr = (short*)(samples) + channel;
                    for (auto i = 0; i < avFrame->nb_samples; i++) {
                      *dstPtr = *srcPtr++;
                      dstPtr += avFrame->channels;
                      }
                    }
                  break;
                  //}}}
                case AV_SAMPLE_FMT_FLTP:
                  //{{{  32bit float planar, copy planar to interleaved
                  for (auto channel = 0; channel < avFrame->channels; channel++) {
                    auto srcPtr = (float*)avFrame->data[channel];
                    auto dstPtr = (short*)(samples) + channel;
                    for (auto i = 0; i < avFrame->nb_samples; i++) {
                      auto int16 = (short)(*srcPtr++ * 0x8000);
                      *dstPtr = int16;
                      dstPtr += avFrame->channels;
                      }
                    }
                  break;
                  //}}}
                default:
                  cLog::log (LOGERROR, "analyseThread - unrecognised sample_fmt " + dec (context->sample_fmt));
                }
              //}}}
              if (mSong.addFrame (uint32_t(avPacket.data - mStreamFirst), avPacket.size,
                                  avFrame->nb_samples, samples, int(mStreamLast - mStreamFirst)) == kPlayFrameThreshold) {
                //{{{  launch playThread
                auto threadHandle = thread ([=](){ playThread (true); });
                SetThreadPriority (threadHandle.native_handle(), THREAD_PRIORITY_TIME_CRITICAL);
                threadHandle.detach();
                }
                //}}}
              changed();
              }
            }
          }
        stream += skip + avPacket.size;
        }

      // done
      free (samples);
      av_frame_free (&avFrame);
      if (context)
        avcodec_close (context);

      // wait for play to end or abort
      mPlayDoneSem.wait();
      //{{{  close file mapping
      UnmapViewOfFile (mStreamFirst);
      CloseHandle (fileHandle);
      //}}}

      if (mChanged) // use changed fileIndex
        mChanged = false;
      else if (!mFileList->nextIndex())
        break;
      }

    setExit();
    }
  //}}}
  //{{{
  void playThread (bool stopAtEnd) {

    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::setThreadName ("play");

    auto codec = avcodec_find_decoder (mStreamAac ? AV_CODEC_ID_AAC : AV_CODEC_ID_MP3);
    auto context = avcodec_alloc_context3 (codec);
    avcodec_open2 (context, codec, NULL);

    AVPacket avPacket;
    av_init_packet (&avPacket);
    auto avFrame = av_frame_alloc();
    auto samples = (int16_t*)malloc (mSong.getSamplesSize());

    cWinAudio audio (mSong.mChannels, mSong.mSamplesPerSec);
    mVolumeBox->setAudio (&audio);

    while (!getExit() && !mChanged && !(stopAtEnd && (mSong.mPlayFrame >= mSong.getNumLoadedFrames()-1))) {
      if (mPlaying) {
        uint8_t* stream = mStreamFirst + mSong.getPlayFrameStreamIndex();
        bool aac;
        bool tag;
        int skip;
        if (parseFrame (stream, mStreamLast, avPacket.data, avPacket.size, aac, tag, skip)) {
          if (!tag && (mStreamAac == mStreamAac)) {
            auto ret = avcodec_send_packet (context, &avPacket);
            while (ret >= 0) {
              ret = avcodec_receive_frame (context, avFrame);
              if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF || ret < 0)
                break;
              if (avFrame->nb_samples > 0) {
                //{{{  covert planar avFrame->data to interleaved int16_t samples
                switch (context->sample_fmt) {
                  case AV_SAMPLE_FMT_S16P:
                    // 16bit signed planar, copy planar to interleaved
                    for (auto channel = 0; channel < avFrame->channels; channel++) {
                      auto srcPtr = (short*)avFrame->data[channel];
                      auto dstPtr = (short*)(samples) + channel;
                      for (auto i = 0; i < avFrame->nb_samples; i++) {
                        *dstPtr = *srcPtr++;
                        dstPtr += avFrame->channels;
                        }
                      }
                    break;

                  case AV_SAMPLE_FMT_FLTP:
                    // 32bit float planar, copy planar to interleaved
                    for (auto channel = 0; channel < avFrame->channels; channel++) {
                      auto srcPtr = (float*)avFrame->data[channel];
                      auto dstPtr = (short*)(samples) + channel;
                      for (auto i = 0; i < avFrame->nb_samples; i++) {
                        auto int16 = (short)(*srcPtr++ * 0x8000);
                        *dstPtr = int16;
                        dstPtr += avFrame->channels;
                        }
                      }
                    break;

                  default:
                    cLog::log (LOGERROR, "playThread - unrecognised sample_fmt " + dec (context->sample_fmt));
                  }
                //}}}
                audio.play (avFrame->channels, samples, avFrame->nb_samples, 1.f);
                mSong.incPlayFrame (1);
                changed();
                }
              }
            }
          }
        }
      else
        audio.play (mSong.mChannels, nullptr, mSong.mSamplesPerFrame, 1.f);
      }

    // done
    mVolumeBox->setAudio (nullptr);

    free (samples);
    av_frame_free (&avFrame);
    if (context)
      avcodec_close (context);

    mPlayDoneSem.notifyAll();

    cLog::log (LOGINFO, "exit");
    CoUninitialize();
    }
  //}}}

  //{{{  vars
  cFileList* mFileList;
  cSong mSong;

  uint8_t* mStreamFirst = nullptr;
  uint8_t* mStreamLast = nullptr;
  bool mStreamAac = false;
  cSemaphore mStreamSem;

  // icyMeta parsed into
  string mIcyStr;
  string mTitleStr;
  string mUrlStr;

  cJpegImageView* mJpegImageView = nullptr;

  bool mChanged = false;
  bool mPlaying = true;
  cSemaphore mPlayDoneSem;

  cVolumeBox* mVolumeBox = nullptr;

  int mIcySkipCount = 0;
  int mIcySkipLen = 0;
  int mIcyInfoCount = 0;
  int mIcyInfoLen = 0;
  char mIcyInfo[255] = {0};
  //}}}
  };

//{{{
int __stdcall WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::init (LOGINFO, true);
  //cLog::init (LOGINFO, false, "");

  avcodec_register_all();

  int numArgs;
  auto args = CommandLineToArgvW (GetCommandLineW(), &numArgs);

  cAppWindow appWindow;

  if (numArgs == 1) {
    //const string url = "http://stream.wqxr.org/wqxr.aac";
    const string url = "http://stream.wqxr.org/js-stream.aac";
    //const string url = "http://tx.planetradio.co.uk/icecast.php?i=jazzhigh.aac";
    //const string url = "http://us4.internet-radio.com:8266/";
    //const string url = "http://tx.planetradio.co.uk/icecast.php?i=countryhits.aac";
    //const string url = "http://live-absolute.sharp-stream.com/absoluteclassicrockhigh.aac";
    //const string url = "http://media-ice.musicradio.com:80/SmoothCountry";

    cLog::log (LOGNOTICE, "mp3Window stream " + url);
    appWindow.run (true, "mp3Window " + url, 800, 800, url);
    }
  else {
    const wstring wstr(args[1]);

    #pragma warning(push)
      #pragma warning(disable: 4244)
      const string fileName = string (wstr.begin(), wstr.end());
    #pragma warning(pop)

    //string fileName = "C:/Users/colin/Music/Elton John";
    cLog::log (LOGNOTICE, "mp3Window - " + fileName);
    appWindow.run (false, "mp3Window", 800, 800, fileName);
    }

  CoUninitialize();
  }
//}}}
