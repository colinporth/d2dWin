// mp3Window.cpp
//{{{  includes
#include "stdafx.h"

#include "../../shared/utils/resolve.h"
#include "../../shared/utils/cFileList.h"
#include "../../shared/utils/cWinAudio.h"

#include "../common/cJpegImage.h"
#include "../common/cJpegImageView.h"

#include "../boxes/cLogBox.h"
#include "../boxes/cFileListBox.h"
#include "../boxes/cWindowBox.h"
#include "../boxes/cVolumeBox.h"
#include "../boxes/cCalendarBox.h"
#include "../boxes/cClockBox.h"
#include "../boxes/cIntBox.h"

extern "C" {
  #include <libavcodec/avcodec.h>
  #include <libavformat/avformat.h>
  }

#define CURL_STATICLIB
#include "../../curl/include/curl/curl.h"

using namespace std;
using namespace chrono;
using namespace concurrency;
//}}}
//{{{  const
const int kMaxChannels = 2;

const int kSilentThreshold = 3;
const int kSilentWindow = 12;

const int kPlayFrameThreshold = 10; // about a second of analyse before playing
//}}}


class cAppWindow : public cD2dWindow {
public:
  cAppWindow() : mPlayDoneSem("playDone") {}
  //{{{
  void run (const string& title, int width, int height, const string& fileName) {

    initialise (title, width, height, false);
    add (new cCalendarBox (this, 190.f,150.f, mTimePoint), -190.f,0.f);
    add (new cClockBox (this, 40.f, mTimePoint), -82.f,150.f);

    mJpegImageView = new cJpegImageView (this, 0.f,-220.f, false, false, mSong.mImage);
    add (mJpegImageView);

    add (new cLogBox (this, 200.f,-200.f, true), 0.f,-200.f)->setPin (false);

    add (new cSongLensBox (this, 0,100.f, mSong), 0.f,-120.f);
    add (new cSongBox (this, 0,100.f, mSong), 0,-220.f);
    add (new cSongTimeBox (this, 600.f,50.f, mSong), -600.f,-50.f);

    mFileList = new cFileList (fileName, "*.aac;*.mp3");
    thread([=]() { mFileList->watchThread(); }).detach();
    add (new cAppFileListBox (this, 0.f,-220.f, mFileList))->setPin (true);

    mVolumeBox = new cVolumeBox (this, 12.f,0.f, nullptr);
    add (mVolumeBox, -12.f,0.f);
    add (new cWindowBox (this, 60.f,24.f), -60.f,0.f)->setPin (false);

    if (!mFileList->empty())
      thread ([=](){ analyseThread(); }).detach();

    // loop till quit
    messagePump();

    delete mFileList;
    delete mJpegImageView;
    }
  //}}}
  //{{{
  void runStream (const string& title, int width, int height, const string& url) {

    initialise (title, width, height, false);
    add (new cIntBox (this, 100.f, 24.f, "frame ", mSong.mPlayFrame), 0.f, 0.f);
    add (new cIntBox (this, 100.f, 24.f, "frames ", mSong.mNumFrames), 100.f, 0.f);

    add (new cCalendarBox (this, 190.f,150.f, mTimePoint), -190.f, 0.f);
    add (new cClockBox (this, 40.f, mTimePoint), -82.f,150.f);

    add (new cLogBox (this, 200.f,-200.f, true), 0.f,-200.f)->setPin (false);

    add (new cSongLensBox (this, 0,100.f, mSong), 0.f,-120.f);
    add (new cSongBox (this, 0,100.f, mSong), 0,-220.f);
    add (new cSongTimeBox (this, 600.f,50.f, mSong), -600.f,-50.f);

    mVolumeBox = new cVolumeBox (this, 12.f,0.f, nullptr);
    add (mVolumeBox, -12.f,0.f);
    add (new cWindowBox (this, 60.f,24.f), -60.f,0.f)->setPin (false);

    WSADATA wsaData;
    if (WSAStartup (MAKEWORD(2,2), &wsaData))
      exit (0);

    mCurl = curl_easy_init();
    if (mCurl) {
      mStreamFirst = (uint8_t*)malloc (100000000);
      mStreamLast = mStreamFirst;

      thread ([=]() { httpThread (url.c_str()); }).detach();
      thread ([=]() { analyseStreamThread(); }).detach();

      messagePump();

      curl_easy_cleanup (mCurl);
      }
    else
      cLog::log (LOGERROR, "curl_easy_init error");
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
  class cDecoderFrame {
  public:
    cDecoderFrame (uint32_t streamPos, uint32_t frameLen, uint8_t values[kMaxChannels]) : mStreamPos(streamPos), mFrameLen(frameLen) {
      for (auto i = 0; i < kMaxChannels; i++)
        mValues[i] = values[i];
      mSilent = isSilentThreshold();
      }

    bool isSilent() { return mSilent; }
    bool isSilentThreshold() { return mValues[0] + mValues[1] <= kSilentThreshold; }

    // vars
    uint32_t mStreamPos;
    uint32_t mFrameLen;

    uint8_t mValues[kMaxChannels];
    bool mSilent;
    };
  //}}}
  //{{{
  class cSong {
  public:
    //{{{
    virtual ~cSong() {
      mDecoderFrames.clear();
      }
    //}}}

    //{{{
    void init (string fileName, bool aac, uint16_t samplesPerFrame) {

      mDecoderFrames.clear();

      mPlayFrame = 0;
      mMaxValue = 0;
      mNumFrames = 0;

      mFileName = fileName;
      mPathName = "";

      mAac = aac;
      mSamplesPerFrame = samplesPerFrame;
      }
    //}}}
    //{{{
    bool addDecoderFrame (uint32_t streamPos, uint32_t frameLen, uint8_t values[kMaxChannels], uint32_t streamLen) {
    // return true if enough frames added to start playing

      mDecoderFrames.push_back (cDecoderFrame (streamPos, frameLen, values));

      mMaxValue = max (mMaxValue, values[0]);
      mMaxValue = max (mMaxValue, values[1]);

      // estimate numFrames
      mNumFrames = int (uint64_t(streamLen - mDecoderFrames[0].mStreamPos) * (uint64_t)mDecoderFrames.size() /
                        uint64_t(streamPos + frameLen - mDecoderFrames[0].mStreamPos));

      // calc silent window
      auto frame = getNumLoadedFrames()-1;
      if (mDecoderFrames[frame].isSilent()) {
        auto window = kSilentWindow;
        auto windowFrame = frame - 1;
        while ((window >= 0) && (windowFrame >= 0)) {
          // walk backwards looking for no silence
          if (!mDecoderFrames[windowFrame].isSilentThreshold()) {
            mDecoderFrames[frame].mSilent = false;
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
    int getNumLoadedFrames() { return (int)mDecoderFrames.size(); }
    //{{{
    uint32_t getPlayFrameStreamPos() {

      if (mDecoderFrames.empty())
        return 0;
      else if (mPlayFrame < mDecoderFrames.size())
        return mDecoderFrames[mPlayFrame].mStreamPos;
      else
        return mDecoderFrames[0].mStreamPos;
      }
    //}}}
    int getSamplesSize() { return mMaxSamplesPerFrame * mChannels * mBytesPerSample; }

    // sets
    void setPlayFrame (int frame) { mPlayFrame = min (max (frame, 0), mNumFrames-1); }
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

    concurrent_vector<cDecoderFrame> mDecoderFrames;

    int mPlayFrame = 0;
    int mNumFrames = 0;
    uint8_t mMaxValue = 0;

    cJpegImage* mImage = nullptr;

    bool mAac = false;

    uint16_t mChannels = 2;
    uint16_t mBytesPerSample = 2;
    uint16_t mBitsPerSample = 16;
    uint16_t mSamplesPerFrame = 1152;
    uint16_t mMaxSamplesPerFrame = 2048;
    uint16_t mSamplesPerSec = 44100;

  private:
    //{{{
    int skipPrev (int fromFrame, bool silent) {

      for (auto frame = fromFrame-1; frame >= 0; frame--)
        if (mDecoderFrames[frame].isSilent() ^ silent)
          return frame;

      return fromFrame;
      }
    //}}}
    //{{{
    int skipNext (int fromFrame, bool silent) {

      for (auto frame = fromFrame; frame < getNumLoadedFrames(); frame++)
        if (mDecoderFrames[frame].isSilent() ^ silent)
          return frame;

      return fromFrame;
      }
    //}}}
    };
  //}}}

  //{{{
  class cSongBox : public cBox {
  public:
    //{{{
    cSongBox (cD2dWindow* window, float width, float height, cSong& frameSet) :
        cBox ("frameSet", window, width, height), mSong(frameSet) {
      mPin = true;
      }
    //}}}
    virtual ~cSongBox() {}

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
    int getMaxValue() { return mSong.mMaxValue > 0 ? mSong.mMaxValue : 1; }
    //{{{
    void draw (ID2D1DeviceContext* dc, int leftFrame, int rightFrame, int firstX, int zoom) {

      leftFrame = (leftFrame < 0) ? 0 : leftFrame;
      if (rightFrame > mSong.getNumLoadedFrames())
        rightFrame = mSong.getNumLoadedFrames();

      // draw frames
      auto colour = mWindow->getBlueBrush();

      float yCentre = getCentreY();
      float valueScale = getHeight() / 2 / getMaxValue();

      bool centre = false;
      float xl = mRect.left + firstX;
      for (auto frame = leftFrame; frame < rightFrame; frame += zoom) {
        float xr = xl + 1.f;
        if (mSong.mDecoderFrames[frame].isSilent())
          dc->FillRectangle (cRect (xl, yCentre-kSilentThreshold, xr, yCentre+kSilentThreshold), mWindow->getRedBrush());

        if (!centre && (frame >= mSong.mPlayFrame)) {
          auto yLeft = mSong.mDecoderFrames[frame].mValues[0] * valueScale;
          auto yRight = mSong.mDecoderFrames[frame].mValues[1] * valueScale;
          dc->FillRectangle (cRect (xl, yCentre - yLeft, xr, yCentre + yRight), mWindow->getWhiteBrush());
          colour = mWindow->getGreyBrush();
          centre = true;
          }
        else {
          float yLeft = 0;
          float yRight = 0;
          for (auto i = 0; i < zoom; i++) {
            yLeft += mSong.mDecoderFrames[frame+i].mValues[0];
            yRight += mSong.mDecoderFrames[frame+i].mValues[1];
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
  class cSongLensBox : public cSongBox {
  public:
    //{{{
    cSongLensBox (cD2dWindow* window, float width, float height, cSong& frameSet)
      : cSongBox (window, width, height, frameSet) {}
    //}}}
    //{{{
    virtual ~cSongLensBox() {
      bigFree (mSummedValues);
      }
    //}}}

    //{{{
    void layout() {
      mSummedFrame = -1;
      cSongBox::layout();
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
      return cSongBox::onUp (right, mouseMoved, pos);
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

      cSongBox::draw (dc, mSong.mPlayFrame - mLens, mSong.mPlayFrame + mLens-2, leftLensX+1, 1);

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

          int leftValue = mSong.mDecoderFrames[frame].mValues[0];
          int rightValue = mSong.mDecoderFrames[frame].mValues[1];
          if (frame > startFrame) {
            int num = 1;
            for (auto i = startFrame; i < frame; i++) {
              leftValue += mSong.mDecoderFrames[i].mValues[0];
              rightValue += mSong.mDecoderFrames[i].mValues[1];
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
      if (mSong.mNumFrames > 0)
        curFrameX += mSong.mPlayFrame * getWidth() / mSong.mNumFrames;

      bool centre = false;
      float xl = mRect.left + firstX;
      auto summedValuesPtr = mSummedValues + (firstX * 2);
      for (auto x = firstX; x < lastX; x++) {
        float xr = xl + 1.f;
        if (!centre && (x >= curFrameX) && (mSong.mPlayFrame < mSong.getNumLoadedFrames())) {
          float leftValue = mSong.mDecoderFrames[mSong.mPlayFrame].mValues[0] * valueScale;
          float rightValue = mSong.mDecoderFrames[mSong.mPlayFrame].mValues[1] * valueScale;
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
    cSongTimeBox (cAppWindow* window, float width, float height, cSong& frameSet) :
        cBox("frameSetTime", window, width, height), mSong(frameSet) {

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
  bool parseId3Tag (uint8_t* buf, uint8_t* lastBuf) {
  // look for ID3 Jpeg tag

    auto ptr = buf;
    auto tag = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];

    if (tag == 0x49443303)  {
      // ID3 tag
      auto tagSize = (ptr[6] << 21) | (ptr[7] << 14) | (ptr[8] << 7) | ptr[9];
      cLog::log (LOGINFO, "parseId3Tag - %c%c%c ver:%d %02x flags:%02x tagSize:%d",
                           ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], tagSize);
      ptr += 10;

      while (ptr < buf + tagSize) {
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
  bool parseFrame (uint8_t* buf, uint8_t* lastBuf, uint8_t*& framePtr, int& frameLen, bool& aac, bool& id3Tag, int& skipped) {
  // start of mp3 / aac adts parser

    skipped = 0;

    while ((lastBuf - buf) >= 6) {
      if (buf[0] == 'I' && buf[1] == 'D' && buf[2] == '3') {
        //{{{  id3 header
        auto tagSize = (buf[6] << 21) | (buf[7] << 14) | (buf[8] << 7) | buf[9];

        // return tag & size
        framePtr = buf;
        frameLen = 10 + tagSize;

        aac = false;
        id3Tag = true;

        // check for enough bytes for frame body
        return buf + frameLen < lastBuf;
        }
        //}}}

      else if ((buf[0] == 0xFF) && ((buf[1] & 0xF0) == 0xF0)) {
        // syncWord found
        if ((buf[1] & 0x06) == 0) {
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
          // O 11  Buffer fullness
          // P  2  Number of AAC frames (RDBs) in ADTS frame minus 1, for maximum compatibility always use 1 AAC frame per ADTS frame
          // Q 16  CRC if protection absent is 0

          const int sampleRates[16] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350, 0,0,0};

          auto sampleRate = sampleRates [(buf[2] & 0x3c) >> 2];

          // return aacFrame & size
          framePtr = buf;
          frameLen = (((unsigned int)buf[3] & 0x3) << 11) | (((unsigned int)buf[4]) << 3) | (buf[5] >> 5);

          aac = true;
          id3Tag = false;

          // check for enough bytes for frame body
          return buf + frameLen < lastBuf;
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

          uint8_t version = (buf[1] & 0x08) >> 3;
          uint8_t layer = (buf[1] & 0x06) >> 1;
          uint8_t errp = buf[1] & 0x01;

          uint8_t bitrateIndex = (buf[2] & 0xf0) >> 4;
          uint8_t sampleRateIndex = (buf[2] & 0x0c) >> 2;
          uint8_t pad = (buf[2] & 0x02) >> 1;
          uint8_t priv = (buf[2] & 0x01);

          uint8_t mode = (buf[3] & 0xc0) >> 6;
          uint8_t modex = (buf[3] & 0x30) >> 4;
          uint8_t copyright = (buf[3] & 0x08) >> 3;
          uint8_t original = (buf[3] & 0x04) >> 2;
          uint8_t emphasis = (buf[3] & 0x03);

          uint32_t bitrate = bitRates[bitrateIndex];
          uint32_t sampleRate = sampleRates[sampleRateIndex];

          int scale = (layer == 3) ? 48 : 144;
          int size = (bitrate * scale) / sampleRate;
          if (pad)
            size++;

          // return mp3Frame & size
          framePtr = buf;
          frameLen = size;
          aac = false;
          id3Tag = false;

          // check for enough bytes for frame body
          return buf + frameLen < lastBuf;
          }
          //}}}
        }

      else {
        skipped++;
        buf++;
        }
      }

    framePtr = nullptr;
    frameLen = 0;
    aac = false;
    id3Tag = false;

    return false;
    }
  //}}}
  //{{{
  bool parseFrames (uint8_t* buf, uint8_t* lastBuf) {
  // return true if stream is aac adts

    int tags = 0;
    int frames = 0;
    int lostSync = 0;
    bool resultAac = false;

    uint8_t* framePtr = nullptr;
    int frameLen = 0;
    bool frameAac = false;
    bool tag = false;
    int skipped = 0;
    while (parseFrame (buf, lastBuf, framePtr, frameLen, frameAac, tag, skipped)) {
      if (tag)
        tags++;
      else {
        resultAac = frameAac;
        frames++;
        }
      // onto next frame
      buf += skipped + frameLen;
      lostSync += skipped;
      }

    cLog::log (LOGINFO, "parseFrames f:%d lost:%d aac:%d tags:%d", frames, lostSync, resultAac, tags);

    return resultAac;
    }
  //}}}

  //{{{
  void httpThread (const char* url) {

    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::setThreadName ("http");

    //curl_easy_setopt (mCurl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt (mCurl, CURLOPT_URL, url);
    curl_easy_setopt (mCurl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt (mCurl, CURLOPT_HEADERFUNCTION, header);
    curl_easy_setopt (mCurl, CURLOPT_WRITEFUNCTION, body);
    curl_easy_setopt (mCurl, CURLOPT_WRITEDATA, this);

    curl_easy_perform (mCurl);

    // never gets here
    curl_easy_cleanup (mCurl);

    CoUninitialize();
    }
  //}}}
  //{{{
  void analyseStreamThread() {

    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::setThreadName ("anls");

    // wait for a bit of stream to get codec
    while ((mStreamLast - mStreamFirst) < 1440)
      Sleep (10);

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
      int skipped;
      while (parseFrame (stream, mStreamLast, avPacket.data, avPacket.size, frameAac, tag, skipped)) {
        if (!tag && (frameAac == mStreamAac)) {
          auto ret = avcodec_send_packet (context, &avPacket);
          while (ret >= 0) {
            ret = avcodec_receive_frame (context, avFrame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF || ret < 0)
              break;
            if (avFrame->nb_samples > 0) {
              uint8_t powers[kMaxChannels];
              //{{{  calc power for each channel
              int decimate = 2;

              switch (context->sample_fmt) {
                case AV_SAMPLE_FMT_S16P:
                  // 16bit signed planar, copy planar to interleaved, calc channel power
                  for (auto channel = 0; channel < avFrame->channels; channel++) {
                    float power = 0.f;
                    auto srcPtr = (short*)avFrame->data[channel];
                    for (auto i = 0; i < avFrame->nb_samples; i += decimate) {
                      auto sample = *srcPtr;
                      power += sample * sample;
                      srcPtr += decimate;
                      }
                    powers[channel] = uint8_t(sqrtf (power) / avFrame->nb_samples/decimate);
                    }
                  break;

                case AV_SAMPLE_FMT_FLTP:
                  // 32bit float planar, copy planar channel to interleaved, calc channel power
                  for (auto channel = 0; channel < avFrame->channels; channel++) {
                    float power = 0.f;
                    auto srcPtr = (float*)avFrame->data[channel];
                    for (auto i = 0; i < avFrame->nb_samples; i += decimate) {
                      auto sample = (short)(*srcPtr * 0x8000);
                      power += sample * sample;
                      srcPtr += decimate;
                      }
                    powers[channel] = uint8_t (sqrtf (power) / avFrame->nb_samples/decimate);
                    }
                  break;

                default:
                  cLog::log (LOGERROR, "analyseThread - unrecognised sample_fmt " + dec (context->sample_fmt));
                }
              //}}}
              cLog::log (LOGINFO1, "frame size:%d", avPacket.size);
              if (mSong.addDecoderFrame (uint32_t(avPacket.data - mStreamFirst), avPacket.size, powers, int(mStreamLast - mStreamFirst))) {
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
        stream += skipped + avPacket.size;
        }
      Sleep (10);
      }

    // done
    free (samples);
    av_frame_free (&avFrame);
    if (context)
      avcodec_close (context);

    CoUninitialize();
    }
  //}}}

  //{{{
  void analyseThread() {

    cLog::setThreadName ("anal");

    while (!getExit()) {
      auto time = system_clock::now();

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
      int skipped;
      while (!getExit() && !mChanged &&
             parseFrame (stream, mStreamLast, avPacket.data, avPacket.size, frameAac, tag, skipped)) {
        if ((frameAac == mStreamAac) && !tag) {
          auto ret = avcodec_send_packet (context, &avPacket);
          while (ret >= 0) {
            ret = avcodec_receive_frame (context, avFrame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF || ret < 0)
              break;
            if (avFrame->nb_samples > 0) {
              uint8_t powers[kMaxChannels];
              //{{{  calc power for each channel
              int decimate = 2;

              switch (context->sample_fmt) {
                case AV_SAMPLE_FMT_S16P:
                  // 16bit signed planar, copy planar to interleaved, calc channel power
                  for (auto channel = 0; channel < avFrame->channels; channel++) {
                    float power = 0.f;
                    auto srcPtr = (short*)avFrame->data[channel];
                    for (auto i = 0; i < avFrame->nb_samples; i += decimate) {
                      auto sample = *srcPtr;
                      power += sample * sample;
                      srcPtr += decimate;
                      }
                    powers[channel] = uint8_t(sqrtf (power) / avFrame->nb_samples/decimate);
                    }
                  break;

                case AV_SAMPLE_FMT_FLTP:
                  // 32bit float planar, copy planar channel to interleaved, calc channel power
                  for (auto channel = 0; channel < avFrame->channels; channel++) {
                    float power = 0.f;
                    auto srcPtr = (float*)avFrame->data[channel];
                    for (auto i = 0; i < avFrame->nb_samples; i += decimate) {
                      auto sample = (short)(*srcPtr * 0x8000);
                      power += sample * sample;
                      srcPtr += decimate;
                      }
                    powers[channel] = uint8_t (sqrtf (power) / avFrame->nb_samples/decimate);
                    }
                  break;

                default:
                  cLog::log (LOGERROR, "analyseThread - unrecognised sample_fmt " + dec (context->sample_fmt));
                }
              //}}}
              if (mSong.addDecoderFrame (uint32_t(avPacket.data - mStreamFirst), avPacket.size, powers, int(mStreamLast - mStreamFirst))) {
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
        stream += skipped + avPacket.size;
        }

      // done
      free (samples);
      av_frame_free (&avFrame);
      if (context)
        avcodec_close (context);

      // report analyse time
      auto doneTime = (float)duration_cast<milliseconds>(system_clock::now() - time).count();
      cLog::log (LOGINFO, "last took " + dec(doneTime) + "ms");

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
        auto streamPos = mSong.getPlayFrameStreamPos();
        uint8_t* stream = mStreamFirst + streamPos;

        bool aac;
        bool tag;
        int skipped;
        if (parseFrame (stream, mStreamLast, avPacket.data, avPacket.size, aac, tag, skipped)) {
          if (!tag && (mStreamAac == mStreamAac)) {
            auto ret = avcodec_send_packet (context, &avPacket);
            while (ret >= 0) {
              ret = avcodec_receive_frame (context, avFrame);
              if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF || ret < 0)
                break;
              if (avFrame->nb_samples > 0) {
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
                    //{{{  32bit float planar, copy planar channel to interleaved
                    for (auto channel = 0; channel < avFrame->channels; channel++) {
                      auto srcPtr = (float*)avFrame->data[channel];
                      auto dstPtr = (short*)(samples) + channel;
                      for (auto i = 0; i < avFrame->nb_samples; i++) {
                        *dstPtr = (short)(*srcPtr++ * 0x8000);
                        dstPtr += avFrame->channels;
                        }
                      }
                    break;
                    //}}}
                  default:
                    cLog::log (LOGERROR, "playThread - unrecognised sample_fmt " + dec (context->sample_fmt));
                  }
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

    cLog::log (LOGINFO, "exit");

    mPlayDoneSem.notifyAll();

    CoUninitialize();
    }
  //}}}

  //{{{
  static size_t header (void* ptr, size_t size, size_t nmemb, void* reference) {

    if (nmemb > 2) {
      // knock out cr lf
      auto bytePtr = (uint8_t*)ptr;
      bytePtr[nmemb-1] = 0;
      bytePtr[nmemb-2] = 0;
      cLog::log (LOGINFO, "%d %d %x %s", size, nmemb, reference, bytePtr);
      }

    return nmemb;
    }
  //}}}
  //{{{
  static size_t body (void* ptr, size_t size, size_t nmemb, void* reference) {

    cLog::log (LOGINFO2, "body size:%d", nmemb);

    cAppWindow* appWindow = (cAppWindow*)reference;
    memcpy (appWindow->mStreamLast, ptr, nmemb);
    appWindow->mStreamLast += nmemb;

    return nmemb;
    }
  //}}}

  //{{{  vars
  cFileList* mFileList;
  cSong mSong;

  uint8_t* mStreamFirst = nullptr;
  uint8_t* mStreamLast = nullptr;
  bool mStreamAac = false;

  cJpegImageView* mJpegImageView = nullptr;

  bool mChanged = false;
  bool mPlaying = true;
  cSemaphore mPlayDoneSem;

  cVolumeBox* mVolumeBox = nullptr;

  CURL* mCurl = nullptr;
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
    string url = "http://stream.wqxr.org/js-stream.aac";
    //string url = "http://tx.planetradio.co.uk/icecast.php?i=jazzhigh.aac";
    cLog::log (LOGNOTICE, "curl test" + url);
    appWindow.runStream ("httpWindow", 800, 500, url);
    }
  else {
    string fileName = "C:/Users/colin/Music/Elton John";
    wstring wstr(args[1]);
    fileName = string(wstr.begin(), wstr.end());
    cLog::log (LOGNOTICE, "mp3Window" + fileName);
    appWindow.run ("mp3Window", 800, 500, fileName);
    }

  CoUninitialize();
  }
//}}}
