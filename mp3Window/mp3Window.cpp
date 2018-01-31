// mp3Window.cpp
//{{{  includes
#include "stdafx.h"

#include "../../shared/utils/cWinAudio.h"
#include "../../shared/decoders/cMp3Decoder.h"
#include "../../shared/utils/cFileList.h"

#include "../common/cJpegImage.h"

#include "../common/box/cCalendarBox.h"
#include "../common/box/cClockBox.h"

#include "../common/cJpegImageView.h"
#include "../common/box/cFileListBox.h"
#include "../common/box/cLogBox.h"

#include "../common/box/cVolumeBox.h"
#include "../common/box/cWindowBox.h"

using namespace chrono;
using namespace concurrency;
//}}}
//{{{  const
const uint16_t kSamplesPerSec = 44100;
const uint16_t kSamplesPerFrame = 1152;
const uint16_t kChannels = 2;

const uint16_t kBytesPerSample = 2;
const uint16_t kBitsPerSample = 16;

const int kSilentThreshold = 3;
const int kSilentWindow = 12;

const int kPlayFrameThreshold = 40; // about a second of analyse before playing
//}}}

class cAppWindow : public cD2dWindow, public cWinAudio {
public:
  cAppWindow() : mAnalyseSem("analyse"), mPlayDoneSem("playDone") {}
  //{{{
  void run (string title, int width, int height, string fileName) {

    initialise (title, width, height, false);
    add (new cCalendarBox (this, 190.f,150.f, mTimePoint), -190.f,0.f);
    add (new cClockBox (this, 40.f, mTimePoint), -82.f,150.f);
    add (new cLogBox (this, 200.f,0.f, true), 0.f,200.f);

    mJpegImageView = (cJpegImageView*)add (new cJpegImageView (this, 0.f,-120.f, false, false, mFrameSet.mImage));

    mFileList = new cFileList (fileName, "*.mp3");
    add (new cAppFileListBox (this, 0,-220.f, mFileList));

    add (new cFrameSetLensBox (this, 0,100.f, mFrameSet), 0.f,-120.f);
    add (new cFrameSetBox (this, 0,100.f, mFrameSet), 0,-220.f);
    add (new cFrameSetTimeBox (this, 600.f,50.f, mFrameSet), -600.f,-50.f);
    add (new cVolumeBox (this, 12.f,0.f), -12.f,0.f);
    add (new cWindowBox (this, 60.f,24.f), -60.f,0.f);

    if (!mFileList->empty()) {
      thread ([=](){ analyseThread(); }).detach();

      auto threadHandle = thread ([=](){ playThread(); });
      SetThreadPriority (threadHandle.native_handle(), THREAD_PRIORITY_TIME_CRITICAL);
      threadHandle.detach();
      }

    // loop till quit
    messagePump();
    }
  //}}}

protected:
  //{{{
  bool onKey (int key) {

    switch (key) {
      case 0x00: break;
      case 0x1B: return true;

      case  ' ': mPlaying = !mPlaying; break;

      case 0x21: if (mFileList->prev()) changed(); break; // page up - prev file
      case 0x22: if (mFileList->next()) changed(); break; // page down - next file

      case 0x25: mFrameSet.incPlaySec (getControl() ? -10 : -1);  changed(); break; // left arrow  - 1 sec
      case 0x27: mFrameSet.incPlaySec (getControl() ? 10 : 1);  changed(); break; // right arrow  + 1 sec

      case 0x23: mFrameSet.setPlayFrame (mFrameSet.mNumFrames-1); mPlaying = false; changed(); break; // end
      case 0x24: mFrameSet.setPlayFrame (0); changed(); break; // home

      case 0x26: mFrameSet.prevSilence(); changed(); break;; // up arrow
      case 0x28: mFrameSet.nextSilence(); changed(); break;; // down arrow

      case  'F': toggleFullScreen(); break;

      default  : cLog::log (LOGINFO, "key %x", key);
      }

    return false;
    }
  //}}}

private:
  //{{{
  class cFrame {
  public:
    //{{{
    cFrame (uint32_t streamPos, uint32_t len, uint8_t values[kChannels]) :
        mStreamPos(streamPos), mLen(len) {

      for (auto i = 0; i < kChannels; i++)
        mValues[i] = values[i];

      mSilent = isSilentThreshold();
      }
    //}}}

    bool isSilent() { return mSilent; }
    bool isSilentThreshold() { return mValues[0] + mValues[1] <= kSilentThreshold; }

    // vars
    uint32_t mStreamPos;
    uint32_t mLen;

    uint8_t mValues[kChannels];
    bool mSilent;
    };
  //}}}
  //{{{
  class cFrameSet {
  public:
    //{{{
    virtual ~cFrameSet() {
      mFrames.clear();
      }
    //}}}

    //{{{
    void init (string fileName) {

      mFrames.clear();

      mPlayFrame = 0;
      mMaxValue = 0;
      mNumFrames = 0;

      mFileName = fileName;
      mPathName = "";
      }
    //}}}
    //{{{
    bool addFrame (uint32_t streamPos, uint32_t frameLen, uint8_t values[kChannels], uint32_t streamLen) {
    // return true if enough frames added to start playing

      mFrames.push_back (cFrame (streamPos, frameLen, values));

      mMaxValue = max (mMaxValue, values[0]);
      mMaxValue = max (mMaxValue, values[1]);

      // estimate numFrames
      mNumFrames = int (uint64_t (streamLen - mFrames[0].mStreamPos) * (uint64_t)mFrames.size() /
                        uint64_t(streamPos + frameLen - mFrames[0].mStreamPos));

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

      return frame == kPlayFrameThreshold;
      }
    //}}}

    // gets
    int getNumdFrames() { return mNumFrames; }
    int getNumLoadedFrames() { return (int)mFrames.size(); }
    //{{{
    uint32_t getPlayFrameStreamPos() {

      if (mFrames.empty())
        return 0;
      else if (mPlayFrame < mFrames.size())
        return mFrames[mPlayFrame].mStreamPos;
      else
        return mFrames[0].mStreamPos;
      }
    //}}}

    // sets
    void setPlayFrame (int frame) { mPlayFrame = min (max (frame, 0), mNumFrames-1); }
    void incPlayFrame (int frames) { setPlayFrame (mPlayFrame + frames); }
    void incPlaySec (int secs) { incPlayFrame (secs * kSamplesPerSec / kSamplesPerFrame); }

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
    concurrent_vector<cFrame> mFrames;
    int mNumFrames = 0;
    uint8_t mMaxValue = 0;

    int mPlayFrame = 0;

    string mFileName;
    string mPathName;
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
  class cFrameSetBox : public cBox {
  public:
    //{{{
    cFrameSetBox (cD2dWindow* window, float width, float height, cFrameSet& frameSet) :
        cBox ("frameSet", window, width, height), mFrameSet(frameSet) {
      mPin = true;
      }
    //}}}
    virtual ~cFrameSetBox() {}

    //{{{
    bool onWheel (int delta, cPoint pos)  {

      if (getShow()) {
        mZoom -= delta/120;
        mZoom = min (max(mZoom, 1), 2 * (1 + mFrameSet.mNumFrames / getWidthInt()));
        return true;
        }

      return false;
      }
    //}}}
    //{{{
    void onDraw (ID2D1DeviceContext* dc) {

      auto leftFrame = mFrameSet.mPlayFrame - (mZoom * getWidthInt()/2);
      auto rightFrame = mFrameSet.mPlayFrame + (mZoom * getWidthInt()/2);
      auto firstX = (leftFrame < 0) ? (-leftFrame) / mZoom : 0;

      draw (dc, leftFrame, rightFrame, firstX, mZoom);
      }
    //}}}

  protected:
    int getMaxValue() { return mFrameSet.mMaxValue > 0 ? mFrameSet.mMaxValue : 1; }
    //{{{
    void draw (ID2D1DeviceContext* dc, int leftFrame, int rightFrame, int firstX, int zoom) {

      leftFrame = (leftFrame < 0) ? 0 : leftFrame;
      if (rightFrame > mFrameSet.getNumLoadedFrames())
        rightFrame = mFrameSet.getNumLoadedFrames();

      // draw frames
      auto colour = mWindow->getBlueBrush();

      float yCentre = getCentreY();
      float valueScale = getHeight() / 2 / getMaxValue();

      bool centre = false;
      float xl = mRect.left + firstX;
      for (auto frame = leftFrame; frame < rightFrame; frame += zoom) {
        float xr = xl + 1.f;
        if (mFrameSet.mFrames[frame].isSilent())
          dc->FillRectangle (cRect (xl, yCentre-kSilentThreshold, xr, yCentre+kSilentThreshold), mWindow->getRedBrush());

        if (!centre && (frame >= mFrameSet.mPlayFrame)) {
          auto yLeft = mFrameSet.mFrames[frame].mValues[0] * valueScale;
          auto yRight = mFrameSet.mFrames[frame].mValues[1] * valueScale;
          dc->FillRectangle (cRect (xl, yCentre - yLeft, xr, yCentre + yRight), mWindow->getWhiteBrush());
          colour = mWindow->getGreyBrush();
          centre = true;
          }
        else {
          float yLeft = 0;
          float yRight = 0;
          for (auto i = 0; i < zoom; i++) {
            yLeft += mFrameSet.mFrames[frame+i].mValues[0];
            yRight += mFrameSet.mFrames[frame+i].mValues[1];
            }
          yLeft = (yLeft / zoom) * valueScale;
          yRight = (yRight / zoom) * valueScale;
          dc->FillRectangle (cRect (xl, yCentre - yLeft, xr, yCentre + yRight), colour);
          }
        xl = xr;
        }
      }
    //}}}

    cFrameSet& mFrameSet;
    int mZoom = 1;
    };
  //}}}
  //{{{
  class cFrameSetLensBox : public cFrameSetBox {
  public:
    //{{{
    cFrameSetLensBox (cD2dWindow* window, float width, float height, cFrameSet& frameSet)
      : cFrameSetBox (window, width, height, frameSet) {}
    //}}}
    //{{{
    virtual ~cFrameSetLensBox() {
      bigFree (mSummedValues);
      }
    //}}}

    //{{{
    void layout() {
      mSummedFrame = -1;
      cFrameSetBox::layout();
      }
    //}}}
    //{{{
    bool onWheel (int delta, cPoint pos)  {
      return false;
      }
    //}}}
    //{{{
    bool onDown (bool right, cPoint pos)  {
      mOn = true;

      auto frame = int((pos.x * mFrameSet.mNumFrames) / getWidth());
      mFrameSet.setPlayFrame (frame);

      return true;
      }
    //}}}
    //{{{
    bool onUp (bool right, bool mouseMoved, cPoint pos) {
      mOn = false;
      return cFrameSetBox::onUp (right, mouseMoved, pos);
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

      int curFrameX = (mFrameSet.mNumFrames > 0) ? (mFrameSet.mPlayFrame * getWidthInt()) / mFrameSet.mNumFrames : 0;
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

      cFrameSetBox::draw (dc, mFrameSet.mPlayFrame - mLens, mFrameSet.mPlayFrame + mLens, leftLensX, 1);

      dc->DrawRectangle (cRect(mRect.left + leftLensX, mRect.top + 1.f,
                               mRect.left + rightLensX, mRect.top + getHeight() - 1.f),
                         mWindow->getYellowBrush(), 1.f);
      }
    //}}}

  private:
    //{{{
    void makeSummedWave() {

      if (mSummedFrame != mFrameSet.getNumLoadedFrames()) {
        // frameSet changed, cache values summed to width, scaled to height
        mSummedFrame = mFrameSet.getNumLoadedFrames();

        mSummedValues = (uint8_t*)realloc (mSummedValues, getWidthInt() * 2 * sizeof(int16_t));
        auto summedValuesPtr = mSummedValues;

        mMaxSummedX = 0;
        auto startFrame = 0;
        for (auto x = 0; x < getWidthInt(); x++) {
          int frame = x * mFrameSet.mNumFrames / getWidthInt();
          if (frame >= mFrameSet.getNumLoadedFrames())
            break;

          int leftValue = mFrameSet.mFrames[frame].mValues[0];
          int rightValue = mFrameSet.mFrames[frame].mValues[1];
          if (frame > startFrame) {
            int num = 1;
            for (auto i = startFrame; i < frame; i++) {
              leftValue += mFrameSet.mFrames[i].mValues[0];
              rightValue += mFrameSet.mFrames[i].mValues[1];
              num++;
              }
            leftValue /= num;
            rightValue /= num;
            }
          *summedValuesPtr++ = leftValue;
          *summedValuesPtr++ = rightValue;

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
      float valueScale = getHeight() / 2 / getMaxValue();

      float curFrameX = mRect.left;
      if (mFrameSet.mNumFrames > 0)
        curFrameX += mFrameSet.mPlayFrame * getWidth() / mFrameSet.mNumFrames;

      bool centre = false;
      float xl = mRect.left + firstX;
      auto summedValuesPtr = mSummedValues + (firstX * 2);
      for (auto x = firstX; x < lastX; x++) {
        float xr = xl + 1.f;
        if (!centre && (x >= curFrameX) && (mFrameSet.mPlayFrame < mFrameSet.getNumLoadedFrames())) {
          float leftValue = mFrameSet.mFrames[mFrameSet.mPlayFrame].mValues[0] * valueScale;
          float rightValue = mFrameSet.mFrames[mFrameSet.mPlayFrame].mValues[1] * valueScale;
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
  class cFrameSetTimeBox : public cBox {
  public:
    //{{{
    cFrameSetTimeBox (cAppWindow* window, float width, float height, cFrameSet& frameSet) :
        cBox("frameSetTime", window, width, height), mFrameSet(frameSet) {

      mWindow->getDwriteFactory()->CreateTextFormat (L"Consolas", NULL,
        DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 50.f, L"en-us",
        &mTextFormat);
      mTextFormat->SetTextAlignment (DWRITE_TEXT_ALIGNMENT_TRAILING);

      mPin = true;
      }
    //}}}
    //{{{
    virtual ~cFrameSetTimeBox() {
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

      string str = getFrameStr (mFrameSet.mPlayFrame) + " " + getFrameStr (mFrameSet.mNumFrames);

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

      uint32_t frameHs = frame * kSamplesPerFrame / (kSamplesPerSec / 100);

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

    cFrameSet& mFrameSet;

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
  void analyseThread() {

    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::setThreadName ("anal");

    while (!getExit()) {
      //{{{  open file mapping
      auto fileHandle = CreateFile (mFileList->getCurFileItem().getFullName().c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
      mStreamBuf = (uint8_t*)MapViewOfFile (CreateFileMapping (fileHandle, NULL, PAGE_READONLY, 0, 0, NULL), FILE_MAP_READ, 0, 0, 0);
      mStreamLen = (int)GetFileSize (fileHandle, NULL);
      //}}}
      mFrameSet.init (mFileList->getCurFileItem().getFullName());

      auto time = system_clock::now();

      cMp3Decoder mMp3Decoder;
      unsigned streamPos = mMp3Decoder.findId3tag (mStreamBuf, mStreamLen);

      int jpegLen;
      auto jpegBuf = mMp3Decoder.getJpeg (jpegLen);
      if (jpegBuf) {
        cLog::log (LOGINFO2, "found jpeg tag");
        // delete old
        auto temp = mFrameSet.mImage;
        mFrameSet.mImage = nullptr;
        delete temp;

        // create new
        mFrameSet.mImage = new cJpegImage();
        mFrameSet.mImage->setBuf (jpegBuf, jpegLen);
        mJpegImageView->setImage (mFrameSet.mImage);
        }

      auto frameNum = 0;
      while (!getExit() && !mChanged && (streamPos < mStreamLen)) {
        uint8_t values[kChannels];
        int frameLen = mMp3Decoder.decodeNextFrame (mStreamBuf + streamPos, mStreamLen - streamPos, values, nullptr);
        if (frameLen <= 0)
          break;
        if (mFrameSet.addFrame (streamPos, frameLen, values, mStreamLen))
          mAnalyseSem.notifyAll();

        streamPos += frameLen;
        changed();
        }

      // report analyse time
      auto doneTime = (float)duration_cast<milliseconds>(system_clock::now() - time).count();
      cLog::log (LOGINFO, "last took " + dec(doneTime) + "ms");

      // wait for play to end
      mPlayDoneSem.wait();

      //{{{  close old file mapping
      UnmapViewOfFile (mStreamBuf);
      CloseHandle (fileHandle);
      //}}}

      if (mChanged) // use changed fileIndex
        mChanged = false;
      else if (!mFileList->next())
        break;
      }

    cLog::log (LOGINFO, "exit - frames loaded:%d calc:%d streamLen:%d",
                        mFrameSet.getNumLoadedFrames(), mFrameSet.mNumFrames, mStreamLen);
    CoUninitialize();
    }
  //}}}
  //{{{
  void playThread() {

    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::setThreadName ("play");

    // cleanup
    //{{{  allocate samples, init to silence
    auto samples = (int16_t*)malloc (kSamplesPerFrame * kChannels * kBytesPerSample);
    memset (samples, 0, kSamplesPerFrame * kChannels * kBytesPerSample);
    //}}}

    while (!getExit()) {
      // wait for first frame
      mAnalyseSem.wait();

      cMp3Decoder mMp3Decoder;
      audOpen (kChannels, kSamplesPerSec);

      mPlaying = true;
      mFrameSet.setPlayFrame (0);
      while (!getExit() && !mChanged && (mFrameSet.mPlayFrame <= mFrameSet.getNumLoadedFrames())) {
        if (mPlaying) {
          auto streamPos = mFrameSet.getPlayFrameStreamPos();
          mMp3Decoder.decodeNextFrame (mStreamBuf + streamPos, mStreamLen - streamPos, nullptr, samples);
          if (samples) {
            audPlay (2, samples, kSamplesPerFrame, 1.f);
            mFrameSet.incPlayFrame (1);
            changed();
            }
          else
            break;
          }
        else
          audPlay (2, nullptr, kSamplesPerFrame, 1.f);
        }

      audClose();
      mPlayDoneSem.notifyAll();
      }

    free (samples);
    CoUninitialize();

    cLog::log (LOGERROR, "exit");
    }
  //}}}

  //{{{  private vars
  cFileList* mFileList;
  bool mChanged = false;

  uint8_t* mStreamBuf = nullptr;
  uint32_t mStreamLen = 0;
  cSemaphore mAnalyseSem;
  cSemaphore mPlayDoneSem;

  cJpegImageView* mJpegImageView = nullptr;

  cFrameSet mFrameSet;
  bool mPlaying = true;
  //}}}
  };

//{{{
int __stdcall WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::init (LOGINFO1, true);

  int numArgs;
  auto args = CommandLineToArgvW (GetCommandLineW(), &numArgs);
  if (numArgs > 1) {
    wstring wstr (args[1]);
    auto fileName = string (wstr.begin(), wstr.end());

    cAppWindow appWindow;
    appWindow.run ("mp3Window", 800, 500, fileName);
    }
  CoUninitialize();
  }
//}}}
