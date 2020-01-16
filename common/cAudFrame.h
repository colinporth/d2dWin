// cYuvFrame.h
#pragma once
#include "iFrame.h"

class cAudFrame : public iFrame {
public:
  static const int kMaxChannels = 6;

  cAudFrame() {}
  virtual ~cAudFrame() { free (mSamples); }

  bool isFirstPesFrame() { return mPts == mPesPts; }

  //{{{
  void set (int64_t pts, int64_t ptsWidth, int64_t pesPts, int channels, int numSamples) {

    // invalidate while we set it up
    mOk = false;

    mPts = pts;
    mPtsEnd = pts + ptsWidth;

    if (channels > kMaxChannels) {
      cLog::log (LOGERROR, "cAudFrame::set - too many channels " + dec(channels));
      channels = kMaxChannels;
      }

    auto numSampleBytes = channels * numSamples * 4;
    if (!mSamples || (numSampleBytes != mNumSampleBytes)) {
      mSamples = (float*)realloc (mSamples, numSampleBytes);
      mNumSampleBytes = numSampleBytes;
      cLog::log (LOGINFO1, "cAudFrame::set - alloc samples " + dec(numSampleBytes));
      }

    // make valid again
    mChannels = channels;
    mNumSamples = numSamples;

    mOk = true;
    }
  //}}}
  //{{{
  void invalidate() {

    mOk = false;

    mPts = 0;

    mChannels = 0;
    mNumSamples = 0;

    for (auto chan = 0; chan < kMaxChannels; chan++)
      mPower[chan] =  0.f;
    }
  //}}}

  int64_t mPesPts = -1;

  int mChannels = 0;
  int mNumSamples = 0;

  int mNumSampleBytes = 0;
  float* mSamples = nullptr;

  float mPower[kMaxChannels] = { 0.f };
  };
