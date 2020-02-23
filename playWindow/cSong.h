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

    // set
    void setTitle (const std::string& title) { mTitle = title; }

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
  virtual ~cSong();

  void init (cAudioDecode::eFrameType frameType, int numChannels, int samplesPerFrame, int sampleRate);
  void addFrame (int frame, bool mapped, uint8_t* stream, int frameLen, int totalFrames, float* samples);

  enum eHlsLoad { eHlsIdle, eHlsLoading, eHlsFailed} ;
  //{{{  gets
  std::shared_mutex& getSharedMutex() { return mSharedMutex; }
  bool getStreaming() { return mStreaming; }
  int getId() { return mId; }

  cAudioDecode::eFrameType getFrameType() { return mFrameType; }
  bool hasSamples() { return mFrameType == cAudioDecode::eWav; }

  int getNumChannels() { return mNumChannels; }
  int getNumSampleBytes() { return mNumChannels * sizeof(float); }

  int getSampleRate() { return mSampleRate; }
  int getSamplesPerFrame() { return mSamplesPerFrame; }

  int getMaxSamplesPerFrame() { return kMaxSamplesPerFrame; }
  int getMaxSamplesBytes() { return getMaxSamplesPerFrame() * getNumSampleBytes(); }

  float getMaxPowerValue() { return mMaxPowerValue; }
  float getMaxPeakValue() { return mMaxPeakValue; }
  float getMaxFreqValue() { return mMaxFreqValue; }

  int getNumFreqBytes() { return kMaxFreqBytes; }

  int getFirstFrame() { return mFrameMap.empty() ? 0 : mFrameMap.begin()->first; }
  int getLastFrame() { return mFrameMap.empty() ? 0 : mFrameMap.rbegin()->first;  }
  int getNumFrames() { return mFrameMap.empty() ? 0 : (mFrameMap.rbegin()->first - mFrameMap.begin()->first + 1); }
  int getTotalFrames() { return mTotalFrames; }
  int getPlayFrame() { return mPlayFrame; }

  cFrame* getFramePtr (int frame) {
    auto it = mFrameMap.find (frame);
    return (it == mFrameMap.end()) ? nullptr : it->second; }

  // selects
  bool hasSelect() { return mSelectValid; }
  int getSelectFirstFrame() { return mSelectFirstFrame; }
  int getSelectLastFrame() { return mSelectLastFrame; }

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

  void setPlayFrame (int frame);

  // streaming
  void setTitle (const std::string& title);
  void setStreaming() { mStreaming = true; }

  // hls
  void setChan (const std::string& chan) { mChan = chan; }
  //{{{
  void setBitrate (int bitrate) {
    mBitrate = bitrate;
    mHlsFramesPerChunk = bitrate >= 128000 ? 300 : 150;
    }
  //}}}

  void setHlsBase (int baseChunkNum, std::chrono::system_clock::time_point baseTimePoint);

  void setHlsLoad (eHlsLoad hlsLoad, int chunkNum);
  //}}}

  // incs
  void incPlaySec (int secs, bool useSelectRange);
  void incPlayFrame (int frames, bool useSelectRange);

  void clearSelect();
  void markSelect();
  void startSelect (int frame);
  void moveSelect (int frame);
  void endSelect (int frame);

  // actions
  void prevSilencePlayFrame();
  void nextSilencePlayFrame();

private:
  bool inSelectRange (int frame);
  void clearFrames();

  void checkSilenceWindow (int frame);
  int skipPrev (int fromFrame, bool silence);
  int skipNext (int fromFrame, bool silence);

  constexpr static int kMaxSamplesPerFrame = 2048; // arbitrary frame max
  constexpr static int kMaxFreq = (kMaxSamplesPerFrame / 2) + 1; // fft max
  constexpr static int kMaxFreqBytes = 512; // arbitrary graphics max

  // vars
  std::map<int,cFrame*> mFrameMap;
  std::shared_mutex mSharedMutex;

  cAudioDecode::eFrameType mFrameType = cAudioDecode::eUnknown;
  bool mStreaming = false;

  int mId = 0;
  int mNumChannels = 0;
  int mSampleRate = 0;
  int mSamplesPerFrame = 0;

  int mPlayFrame = 0;
  int mTotalFrames = 0;

  bool mSelectValid = false;
  int mSelectFirstFrame = 0;
  int mSelectLastFrame = 0;
  enum eSelectEdit { eSelectEditNone, eSelectEditFirst, eSelectEditLast, eSelectEditRange };
  eSelectEdit mSelectEdit = eSelectEditNone;
  int mSelectEditFrame = 0;

  //{{{  max stuff for ui values
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
  kiss_fft_scalar timeBuf[kMaxSamplesPerFrame];
  kiss_fft_cpx freqBuf[kMaxFreq];
  //}}}
  };
