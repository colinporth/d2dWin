// cSong.h
#pragma once
//{{{  includes
#include "cAudioDecode.h"

#include "../../shared/kissFft/kiss_fft.h"
#include "../../shared/kissFft/kiss_fftr.h"
//}}}

class cSong {
public:
  constexpr static float kMinPowerValue = 0.25f;
  constexpr static int kSilentWindowFrames = 10;
  constexpr static int kMaxSamplesPerFrame = 2048;
  constexpr static int kMaxFreq = (kMaxSamplesPerFrame / 2) + 1;
  constexpr static int kMaxSpectrum = kMaxFreq;

  //{{{
  class cFrame {
  public:
    static constexpr float kSilentThreshold = 0.05f;
    //{{{
    cFrame (bool alloced, uint8_t* ptr, uint32_t len,
            float* powerValues, float* peakValues, float* freqValues, uint8_t* lumaValues) :
        mPtr(ptr), mLen(len), mAlloced(alloced),
        mPowerValues(powerValues), mPeakValues(peakValues), mFreqValues(freqValues), mFreqLuma(lumaValues) {

      mSilent = isSilentThreshold();
      }
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
    float* getFreqValues() { return mFreqValues; }
    uint8_t* getFreqLuma() { return mFreqLuma; }

    bool isSilent() { return mSilent; }
    bool isSilentThreshold() { return mPowerValues[0] + mPowerValues[1] < kSilentThreshold; }
    void setSilent (bool silent) { mSilent = silent; }

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
    float* mFreqValues;
    uint8_t* mFreqLuma;

    bool mSilent;
    std::string mTitle;
    };
  //}}}
  virtual ~cSong();

  void init (cAudioDecode::eFrameType frameType, int numChannels, int samplesPerFrame, int sampleRate);
  void addFrame (bool mapped, uint8_t* stream, int frameLen, int totalFrames, int samplesPerFrame, float* samples);

  //{{{  gets
  std::mutex& getMutex() { return mMutex; }

  cAudioDecode::eFrameType getFrameType() { return mFrameType; }

  bool hasSomeFrames() { return !mFrames.empty(); }
  bool isFramePtrSamples() { return mFrameType == cAudioDecode::eWav; }

  int getNumChannels() { return mNumChannels; }
  int getNumSampleBytes() { return mNumChannels * sizeof(float); }
  int getSampleRate() { return mSampleRate; }
  int getSamplesPerFrame() { return mSamplesPerFrame; }
  int getMaxSamplesPerFrame() { return kMaxSamplesPerFrame; }

  float getMaxPowerValue() { return mMaxPowerValue; }
  float getMaxPeakValue() { return mMaxPeakValue; }
  float getMaxFreqValue() { return mMaxFreqValue; }
  int getNumFreq() { return kMaxFreq; }
  int getNumFreqLuma() { return kMaxSpectrum; }

  int getNumFrames() { return (int)mFrames.size(); }
  int getLastFrame() { return getNumFrames() - 1;  }
  int getTotalFrames() { return mTotalFrames; }

  int getId() { return mId; }

  int getPlayFrame();
  uint8_t* getPlayFramePtr();
  int getPlayFrameLen();

  // optional info
  int getBitrate() { return mBitrate; }
  std::string getChan() { return mChan; }

  bool hasBaseTime() { return mHasBaseTime; }
  std::chrono::system_clock::time_point getBaseTimePoint() { return mBaseTimePoint; }

  int getSeqNum() { return mBaseSeqNum + mSeqNum; }
  int getBaseSeqNum() { return mBaseSeqNum; }
  int getBasedSeqNum() { return mSeqNum; }

  int getHlsOffsetMs (std::chrono::system_clock::time_point now);
  //}}}
  //{{{  sets
  void setSampleRate (int sampleRate) { mSampleRate = sampleRate; }
  void setSamplesPerFrame (int samplePerFrame) { mSamplesPerFrame = samplePerFrame; }

  void setPlayFrame (int frame);

  void setTitle (const std::string& title);

  void setChan (const std::string& chan) { mChan = chan; }
  void setBitrate (int bitrate) { mBitrate = bitrate; }

  void setBase (int startSeqNum, std::chrono::system_clock::time_point startTimePoint);
  //}}}

  // incs
  bool incPlayFrame (int frames);
  bool incPlaySec (int secs);
  void incSeqNum();

  void prevSilencePlayFrame();
  void nextSilencePlayFrame();

  std::vector<cFrame*> mFrames;

private:
  void clearFrames (int playFrane);
  int skipPrev (int fromFrame, bool silent);
  int skipNext (int fromFrame, bool silent);

  //{{{  private vars
  std::mutex mMutex;
  cAudioDecode::eFrameType mFrameType = cAudioDecode::eUnknown;

  int mId = 0;
  int mNumChannels = 0;
  int mSampleRate = 0;
  int mSamplesPerFrame = 0;

  int mPlayFrame = 0;
  int mTotalFrames = 0;

  float mMaxPowerValue = 0.f;
  float mMaxPeakValue = 0.f;

  float mMaxFreqValues[kMaxFreq];
  float mMaxFreqValue = 0.f;

  int mBitrate;
  std::string mChan;

  bool mHasBaseTime = false;
  int mBaseSeqNum = 0;
  int mSeqNum = 0;
  std::chrono::system_clock::time_point mBaseTimePoint;

  kiss_fftr_cfg fftrConfig;
  kiss_fft_scalar timeBuf[cSong::kMaxSamplesPerFrame];
  kiss_fft_cpx freqBuf[cSong::kMaxFreq];
  //}}}
  };
