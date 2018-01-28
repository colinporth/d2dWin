// cYuvFrame.h
#pragma once
#include "iFrame.h"

class cAudFrame : public iFrame {
public:
  cAudFrame() {}
  //{{{
  virtual ~cAudFrame() {
    freeResources();
    }
  //}}}

  bool isFirstPesFrame() { return mPts == mPesPts; }

  //{{{
  void set (int64_t pts, int64_t ptsWidth, int64_t pesPts, int channels, int numSamples) {

    // invalidate while we set it up
    mOk = false;

    mPts = pts;
    mPtsEnd = pts + ptsWidth;

    if (channels > 6) {
      cLog::log (LOGERROR, "cAudFrame::set - too many channels " + dec(channels));
      channels = 6;
      }

    auto numSampleBytes = channels * numSamples * 2;
    if (!mSamples || (numSampleBytes != mNumSampleBytes)) {
      mSamples = (int16_t*)realloc (mSamples, numSampleBytes);
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
    }
  //}}}
  //{{{
  void freeResources() {
    free (mSamples);
    }
  //}}}

  int64_t mPesPts = -1;

  int mChannels = 0;
  int mNumSamples = 0;

  int mNumSampleBytes = 0;
  int16_t* mSamples = nullptr;

  // simple static allocation of channels power, max 6
  float mPower[6] = { 0,0,0,0,0,0 };
  };
