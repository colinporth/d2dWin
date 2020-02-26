// cSong.h
#pragma once
//{{{  includes
#include "cAudioDecode.h"

#include "../../shared/kissFft/kiss_fft.h"
#include "../../shared/kissFft/kiss_fftr.h"
//}}}

class cSong {
public:
  //{{{
  class cFrame {
  public:
    static constexpr float kQuietThreshold = 0.01f;
    //{{{
    cFrame (bool alloced, uint8_t* ptr, uint32_t len,
            float* powerValues, float* peakValues, uint8_t* freqValues, uint8_t* lumaValues) :
        mPtr(ptr), mLen(len), mAlloced(alloced),
        mPowerValues(powerValues), mPeakValues(peakValues),
        mFreqValues(freqValues), mFreqLuma(lumaValues), mMuted(false), mSilence(false) {}
    //}}}
    //{{{
    ~cFrame() {

      if (mAlloced)
        free (mPtr);

      free (mPowerValues);
      free (mPeakValues);
      free (mFreqValues);
      free (mFreqLuma);
      }
    //}}}

    // gets
    uint8_t* getPtr() { return mPtr; }
    int getLen() { return mLen; }

    float* getPowerValues() { return mPowerValues;  }
    float* getPeakValues() { return mPeakValues;  }
    uint8_t* getFreqValues() { return mFreqValues; }
    uint8_t* getFreqLuma() { return mFreqLuma; }

    bool isQuiet() { return mPeakValues[0] + mPeakValues[1] < kQuietThreshold; }

    bool isMuted() { return mMuted; }
    bool isSilence() { return mSilence; }
    void setSilence (bool silence) { mSilence = silence; }

    bool hasTitle() { return !mTitle.empty(); }
    std::string getTitle() { return mTitle; }

  private:
    // vars
    uint8_t* mPtr;
    uint32_t mLen;
    bool mAlloced;

    float* mPowerValues;
    float* mPeakValues;
    uint8_t* mFreqValues;
    uint8_t* mFreqLuma;

    bool mMuted;
    bool mSilence;
    std::string mTitle;
    };
  //}}}
  //{{{
  class cSelect {
  public:
    //{{{
    class cSelectItem {
    public:
      enum eType { eLoop, eMute };

      cSelectItem(eType type, int firstFrame, int lastFrame, const std::string& title) :
        mType(type), mFirstFrame(firstFrame), mLastFrame(lastFrame), mTitle(title) {}

      eType getType() { return mType; }
      int getFirstFrame() { return mFirstFrame; }
      int getLastFrame() { return mLastFrame; }
      bool getMark() { return getFirstFrame() == getLastFrame(); }
      std::string getTitle() { return mTitle; }
      //{{{
      bool inRange (int frame) {
      // ignore 1 frame select range
        return (mFirstFrame != mLastFrame) && (frame >= mFirstFrame) && (frame <= mLastFrame);
        }
      //}}}

      void setType (eType type) { mType = type; }
      void setFirstFrame (int frame) { mFirstFrame = frame; }
      void setLastFrame (int frame) { mLastFrame = frame; }
      void setTitle (const std::string& title) { mTitle = title; }

    private:
      eType mType;
      int mFirstFrame = 0;
      int mLastFrame = 0;
      std::string mTitle;
      };
    //}}}

    // gets
    bool empty() { return mItems.empty(); }
    int getFirstFrame() { return empty() ? 0 : mItems.front().getFirstFrame(); }
    int getLastFrame() { return empty() ? 0 : mItems.back().getLastFrame(); }
    //{{{
    bool inRange (int frame) {

      for (auto &item : mItems)
        if (item.inRange (frame))
          return true;

      return false;
      }
    //}}}
    //{{{
    int constrainToRange (int frame, int constrainedFrame) {
    // if frame in a select range return frame constrained to it

      for (auto &item : mItems)
        if (item.inRange (frame))
          if (constrainedFrame > item.getLastFrame())
            return item.getFirstFrame();
          else if (constrainedFrame < item.getFirstFrame())
            return item.getFirstFrame();
          else
            return constrainedFrame;

      return constrainedFrame;
      }
    //}}}
    std::vector<cSelectItem>& getItems() { return mItems; }

    // actions
    //{{{
    void clearAll() {

      mItems.clear();

      mEdit = eEditNone;
      mEditFrame = 0;
      }
    //}}}
    //{{{
    void addMark (int frame, const std::string& title = "") {
      mItems.push_back (cSelectItem (cSelectItem::eLoop, frame, frame, title));
      mEdit = eEditLast;
      mEditFrame = frame;
      }
    //}}}
    //{{{
    void start (int frame) {

      mEditFrame = frame;

      mItemNum = 0;
      for (auto &item : mItems) {
        // pick from select range
        if (abs(frame - item.getLastFrame()) < 2) {
          mEdit = eEditLast;
          return;
          }
        else if (abs(frame - item.getFirstFrame()) < 2) {
          mEdit = eEditFirst;
          return;
          }
        else if (item.inRange (frame)) {
          mEdit = eEditRange;
          return;
          }
        mItemNum++;
        }

      // add new select item
      mItems.push_back (cSelectItem (cSelectItem::eLoop, frame, frame, ""));
      mEdit = eEditLast;
      }
    //}}}
    //{{{
    void move (int frame) {

      if (mItemNum < mItems.size()) {
        switch (mEdit) {
          case eEditFirst:
            mItems[mItemNum].setFirstFrame (frame);
            if (mItems[mItemNum].getFirstFrame() > mItems[mItemNum].getLastFrame()) {
              mItems[mItemNum].setLastFrame (frame);
              mItems[mItemNum].setFirstFrame (mItems[mItemNum].getLastFrame());
              }
            break;

          case eEditLast:
            mItems[mItemNum].setLastFrame (frame);
            if (mItems[mItemNum].getLastFrame() < mItems[mItemNum].getFirstFrame()) {
              mItems[mItemNum].setFirstFrame (frame);
              mItems[mItemNum].setLastFrame (mItems[mItemNum].getFirstFrame());
              }
            break;

          case eEditRange:
            mItems[mItemNum].setFirstFrame (mItems[mItemNum].getFirstFrame() + frame - mEditFrame);
            mItems[mItemNum].setLastFrame (mItems[mItemNum].getLastFrame() + frame - mEditFrame);
            mEditFrame = frame;
            break;

          default:
            cLog::log (LOGERROR, "moving invalid select");
          }
        }
      }
    //}}}
    //{{{
    void end() {
      mEdit = eEditNone;
      mEditFrame = 0;
      }
    //}}}

  private:
    enum eEdit { eEditNone, eEditFirst, eEditLast, eEditRange };

    eEdit mEdit = eEditNone;
    int mEditFrame = 0;
    std::vector<cSelectItem> mItems;
    int mItemNum = 0;
    };
  //}}}
  virtual ~cSong();

  void init (cAudioDecode::eFrameType frameType, int numChannels, int samplesPerFrame, int sampleRate);
  void addFrame (int frame, bool mapped, uint8_t* stream, int frameLen, int totalFrames, float* samples);
  void clear();

  enum eHlsLoad { eHlsIdle, eHlsLoading, eHlsFailed };
  //{{{  gets
  std::shared_mutex& getSharedMutex() { return mSharedMutex; }
  int getId() { return mId; }

  cAudioDecode::eFrameType getFrameType() { return mFrameType; }
  bool hasSamples() { return mFrameType == cAudioDecode::eWav; }

  int getNumChannels() { return mNumChannels; }
  int getNumSampleBytes() { return mNumChannels * sizeof(float); }
  int getSampleRate() { return mSampleRate; }
  int getSamplesPerFrame() { return mSamplesPerFrame; }

  int getFirstFrame() { return mFrameMap.empty() ? 0 : mFrameMap.begin()->first; }
  int getLastFrame() { return mFrameMap.empty() ? 0 : mFrameMap.rbegin()->first;  }
  int getNumFrames() { return mFrameMap.empty() ? 0 : (mFrameMap.rbegin()->first - mFrameMap.begin()->first + 1); }
  int getTotalFrames() { return mTotalFrames; }
  int getPlayFrame() { return mPlayFrame; }

  //{{{
  cFrame* getFramePtr (int frame) {
    auto it = mFrameMap.find (frame);
    return (it == mFrameMap.end()) ? nullptr : it->second;
    }
  //}}}
  cSelect& getSelect() { return mSelect; }

  // max nums for early allocations
  int getMaxNumSamplesPerFrame() { return kMaxNumSamplesPerFrame; }
  int getMaxNumSampleBytes() { return kMaxNumChannels * sizeof(float); }
  int getMaxNumFrameSamplesBytes() { return getMaxNumSamplesPerFrame() * getMaxNumSampleBytes(); }

  // max values for ui
  float getMaxPowerValue() { return mMaxPowerValue; }
  float getMaxPeakValue() { return mMaxPeakValue; }
  float getMaxFreqValue() { return mMaxFreqValue; }
  int getNumFreqBytes() { return kMaxFreqBytes; }

  // info
  int getBitrate() { return mBitrate; }
  std::string getChan() { return mChan; }

  // hls
  bool hasHlsBase() { return mHlsBaseValid; }
  eHlsLoad getHlsLoad() { return mHlsLoad; }
  int getHlsLoadChunkNum (std::chrono::system_clock::time_point now, std::chrono::seconds secs, int preload);
  int getHlsFrameFromChunkNum (int chunkNum) { return mHlsBaseFrame + (chunkNum - mHlsBaseChunkNum) * mHlsFramesPerChunk; }
  //}}}
  //{{{  sets
  void setSampleRate (int sampleRate) { mSampleRate = sampleRate; }
  void setSamplesPerFrame (int samplePerFrame) { mSamplesPerFrame = samplePerFrame; }

  // playFrame
  void setPlayFrame (int frame);
  void incPlaySec (int secs, bool useSelectRange);
  void incPlayFrame (int frames, bool useSelectRange);

  // stream
  void setChan (const std::string& chan) { mChan = chan; }
  //{{{
  void setBitrate (int bitrate) {
    mBitrate = bitrate;
    mHlsFramesPerChunk = (bitrate >= 128000) ? 300 : 150;
    }
  //}}}

  // hls
  void setHlsBase (int chunkNum, std::chrono::system_clock::time_point timePoint);
  void setHlsLoad (eHlsLoad hlsLoad, int chunkNum);
  //}}}

  // actions
  void prevSilencePlayFrame();
  void nextSilencePlayFrame();

private:
  void clearFrames();

  void checkSilenceWindow (int frame);
  int skipPrev (int fromFrame, bool silence);
  int skipNext (int fromFrame, bool silence);

  constexpr static int kMaxNumChannels = 2;           // arbitrary chan max
  constexpr static int kMaxNumSamplesPerFrame = 2048; // arbitrary frame max
  constexpr static int kMaxFreq = (kMaxNumSamplesPerFrame / 2) + 1; // fft max
  constexpr static int kMaxFreqBytes = 512; // arbitrary graphics max

  // vars
  std::map<int,cFrame*> mFrameMap;
  std::shared_mutex mSharedMutex;

  cAudioDecode::eFrameType mFrameType = cAudioDecode::eUnknown;

  int mId = 0;
  int mNumChannels = kMaxNumChannels;
  int mSampleRate = 0;
  int mSamplesPerFrame = 0;

  int mPlayFrame = 0;
  int mTotalFrames = 0;
  cSelect mSelect;

  //{{{  max stuff for ui
  float mMaxPowerValue = 0.f;
  float mMaxPeakValue = 0.f;
  float mMaxFreqValue = 0.f;
  //}}}
  //{{{  hls vars
  std::string mChan;
  int mBitrate = 0;

  bool mHlsBaseValid = false;
  int mHlsBaseChunkNum = 0;
  int mHlsBaseFrame = 0;
  std::chrono::system_clock::time_point mHlsBaseTimePoint;

  int mHlsFramesPerChunk = 0;
  int eHlsFailedChunkNum = 0;
  eHlsLoad mHlsLoad = eHlsIdle;
  //}}}
  //{{{  fft vars
  kiss_fftr_cfg fftrConfig;
  kiss_fft_scalar timeBuf[kMaxNumSamplesPerFrame];
  kiss_fft_cpx freqBuf[kMaxFreq];
  //}}}
  };
