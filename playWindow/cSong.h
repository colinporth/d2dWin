// cSong.h
#pragma once
//{{{  includes
#include "cAudioDecode.h"

#include "concurrent_vector.h"
//}}}

class cSong {
public:
  constexpr static int kMaxSamplesPerFrame = 2048;
  constexpr static int kMaxFreq = (kMaxSamplesPerFrame / 2) + 1;
  constexpr static int kMaxSpectrum = kMaxFreq;
  constexpr static float kMinPowerValue = 0.25f;
  constexpr static int kSilentWindowFrames = 10;

  //{{{
  class cFrame {
  public:
    static constexpr float kSilentThreshold = 0.05f;
    //{{{
    cFrame (uint8_t* ptr, uint32_t len,
            float* powerValues, float* peakPowerValues,
            float* freqValues, uint8_t* lumaValues) :
          mPtr(ptr), mLen(len),
          mPowerValues(powerValues), mPeakPowerValues(peakPowerValues),
          mFreqValues(freqValues), mFreqLuma(lumaValues) {

      mSilent = isSilentThreshold();
      }
    //}}}
    //{{{
    ~cFrame() {
      // !!! ownership of mPtr still be decided, don't delete for now !!!!
      free (mPowerValues);
      free (mPeakPowerValues);
      free (mFreqValues);
      free (mFreqLuma);
      }
    //}}}

    // gets
    uint8_t* getPtr() { return mPtr; }
    int getLen() { return mLen; }

    float* getPowerValues() { return mPowerValues;  }
    float* getPeakPowerValues() { return mPeakPowerValues;  }
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

    float* mPowerValues;
    float* mPeakPowerValues;
    float* mFreqValues;
    uint8_t* mFreqLuma;

    bool mSilent;
    std::string mTitle;
    };
  //}}}
  //{{{
  class cFrameChunk {
  public:
    cFrameChunk (int frame) : mFirstFrame(frame), mLastFrame(frame) {}

    // gets
    int getFirstFrame() { return mFirstFrame; }
    int getLastFrame() { return mLastFrame; }
    int getNumFrames() { return mLastFrame - mFirstFrame; }

    bool hasTitle() { return !mTitle.empty(); }
    std::string getTitle() { return mTitle; }

    // set
    void setTitle (const std::string& title) { mTitle = title; }

    int mFirstFrame;
    int mLastFrame;

  private:
    std::string mTitle;
    };
  //}}}
  virtual ~cSong();

  //{{{  gets
  cAudioDecode::eFrameType getFrameType() { return mFrameType; }
  bool isFramePtrSamples() { return mFrameType == cAudioDecode::eWav; }

  int getNumChannels() { return mNumChannels; }
  int getNumSampleBytes() { return mNumChannels * sizeof(float); }
  int getSampleRate() { return mSampleRate; }
  int getSamplesPerFrame() { return mSamplesPerFrame; }
  int getMaxSamplesPerFrame() { return kMaxSamplesPerFrame; }

  float getMaxPowerValue() { return mMaxPowerValue; }
  float getMaxPeakPowerValue() { return mMaxPeakPowerValue; }
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
  cJpegImage* getJpegImage() { return mJpegImage; }
  //}}}
  //{{{  sets
  void setSampleRate (int sampleRate) { mSampleRate = sampleRate; }
  void setSamplesPerFrame (int samplePerFrame) { mSamplesPerFrame = samplePerFrame; }

  void setPlayFrame (int frame);
  void incPlayFrame (int frames);
  void incPlaySec (int secs);

  void setTitle (const std::string& title);

  void setJpegImage (cJpegImage* jpegImage);
  void setChan (const std::string& chan) { mChan = chan; }
  void setBitrate (int bitrate) { mBitrate = bitrate; }
  //}}}

  void init (cAudioDecode::eFrameType frameType, int numChannels, int samplesPerFrame, int sampleRate);
  bool addFrame (uint8_t* stream, int frameLen, int estimatedTotalFrames, int samplesPerFrame, float* samples);

  void prevSilence();
  void nextSilence();

  // public var
  concurrency::concurrent_vector<cFrame*> mFrames;
  concurrency::concurrent_vector<cFrameChunk*> mFrameChunks;

private:
  int skipPrev (int fromFrame, bool silent);
  int skipNext (int fromFrame, bool silent);

  // private vars
  cAudioDecode::eFrameType mFrameType = cAudioDecode::eUnknown;

  int mId = 0;
  int mNumChannels = 0;
  int mSampleRate = 0;
  int mSamplesPerFrame = 0;

  int mPlayFrame = 0;
  int mTotalFrames = 0;

  float mMaxPowerValue = 0.f;
  float mMaxPeakPowerValue = 0.f;

  float mMaxFreqValues[kMaxFreq];
  float mMaxFreqValue = 0.f;

  int mBitrate;
  std::string mChan;
  cJpegImage* mJpegImage = nullptr;
  };
