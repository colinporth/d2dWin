// cYuvFrame.h
#pragma once

class cAudFrame  {
public:
  cAudFrame() {}
  //{{{
  ~cAudFrame() {
    free (mSamples);
    }
  //}}}

  //{{{
  void set (int64_t pts, int64_t ptsWidth, int64_t pesPts, int channels, int numSamples) {

    // invalidate while we set it up
    mLoaded = false;

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

    mLoaded = true;
    }
  //}}}
  //{{{
  void invalidate() {

    mLoaded = false;
    mPts = 0;
    mChannels = 0;
    mNumSamples = 0;
    }
  //}}}

  bool mLoaded = false;

  int64_t mPts = -1;
  int64_t mPtsEnd = -1;
  int64_t mPesPts = -1;

  int mChannels = 0;
  int mNumSamples = 0;

  int mNumSampleBytes = 0;
  int16_t* mSamples = nullptr;

  // simple static allocation of channels power, max 6
  float mPower[6] = { 0,0,0,0,0,0 };
  };
