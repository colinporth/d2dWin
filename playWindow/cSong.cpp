// cSong.cpp
#pragma once
//{{{  includes
#include "stdafx.h"

#include "cSong.h"

using namespace std;
using namespace chrono;
//}}}

constexpr static float kMinPowerValue = 0.25f;
constexpr static float kMinPeakValue = 0.25f;
constexpr static float kMinFreqValue = 256.f;
constexpr static int kSilenceWindowFrames = 4;

//{{{
cSong::~cSong() {

  clearFrames();
  }
//}}}

//{{{
void cSong::init (cAudioDecode::eFrameType frameType, int numChannels, int samplesPerFrame, int sampleRate) {

  unique_lock<shared_mutex> lock (mSharedMutex);

  // reset frame type
  mFrameType = frameType;
  mNumChannels = numChannels;
  mSampleRate = sampleRate;
  mSamplesPerFrame = samplesPerFrame;
  mHlsBaseValid = false;

  clearFrames();

  // ??? should deallocate ???
  fftrConfig = kiss_fftr_alloc (mSamplesPerFrame, 0, 0, 0);
  }
//}}}
//{{{
void cSong::addFrame (int frame, bool mapped, uint8_t* stream, int frameLen, int totalFrames, float* samples) {
// return true if enough frames added to start playing, streamLen only used to estimate totalFrames

  // sum of squares channel power
  auto powerValues = (float*)malloc (mNumChannels * 4);
  memset (powerValues, 0, mNumChannels * 4);

  // peak
  auto peakValues = (float*)malloc (mNumChannels * 4);
  memset (peakValues, 0, mNumChannels * 4);

  for (int sample = 0; sample < mSamplesPerFrame; sample++) {
    timeBuf[sample] = 0;
    for (auto channel = 0; channel < mNumChannels; channel++) {
      auto value = *samples++;
      timeBuf[sample] += value;
      powerValues[channel] += value * value;
      peakValues[channel] = max (abs(peakValues[channel]), value);
      }
    }

  for (auto channel = 0; channel < mNumChannels; channel++) {
    powerValues[channel] = sqrtf (powerValues[channel] / mSamplesPerFrame);
    mMaxPowerValue = max (mMaxPowerValue, powerValues[channel]);
    mMaxPeakValue = max (mMaxPeakValue, peakValues[channel]);
    }

  // ??? lock against init fftrConfig recalc???
  kiss_fftr (fftrConfig, timeBuf, freqBuf);

  auto freqValues = (uint8_t*)malloc (getNumFreqBytes());
  auto lumaValues = (uint8_t*)malloc (getNumFreqBytes());
  float freqScale = 255.f / mMaxFreqValue;

  auto freqBufPtr = freqBuf;
  auto freqValuesPtr = freqValues;
  auto lumaValuesPtr = lumaValues + getNumFreqBytes() - 1;
  for (auto i = 0; i < getNumFreqBytes(); i++) {
    auto value = sqrt (((*freqBufPtr).r * (*freqBufPtr).r) + ((*freqBufPtr).i * (*freqBufPtr).i));
    mMaxFreqValue = max (mMaxFreqValue, value);

    // freq scaled to byte, only used for display
    value *= freqScale;
    *freqValuesPtr++ = value > 255 ? 255 : uint8_t(value);

    // luma crushed, reversed index, for quicker copyMem to bitmap later
    value *= 4.f;
    *lumaValuesPtr-- = value > 255 ? 255 : uint8_t(value);

    freqBufPtr++;
    }

  unique_lock<shared_mutex> lock (mSharedMutex);

  // totalFrames can be a changing estimate for file, or increasing value for streaming
  mFrameMap.insert (map<int,cFrame*>::value_type (
    frame, new cFrame (mapped, stream, frameLen, powerValues, peakValues, freqValues, lumaValues)));
  mTotalFrames = totalFrames;

  checkSilenceWindow (frame);
  }
//}}}
//{{{
void cSong::clear() {

  unique_lock<shared_mutex> lock (mSharedMutex);

  mFrameType = cAudioDecode::eUnknown;
  mNumChannels = 0;
  mSampleRate = 0;
  mSamplesPerFrame = 0;
  mHlsBaseValid = false;

  clearFrames();

  // ??? should deallocate ???
  //fftrConfig = kiss_fftr_alloc (mSamplesPerFrame, 0, 0, 0);
  }
//}}}

// gets
//{{{
int cSong::getHlsLoadChunkNum (system_clock::time_point now, chrono::seconds secs, int preload) {

  // get offsets of playFrame from baseFrame, handle -v offsets correctly
  int frameOffset = mPlayFrame - mHlsBaseFrame;
  int chunkNumOffset = (frameOffset >= 0)  ? (frameOffset / mHlsFramesPerChunk) :
                                             -((mHlsFramesPerChunk - 1 - frameOffset) / mHlsFramesPerChunk);

  // loop until chunkNum with unloaded frame, chunkNum not available yet, or preload ahead of playFrame loaded
  int loaded = 0;
  while ((loaded < preload) && ((now - (mHlsBaseTimePoint + (chunkNumOffset * 6400ms))) > secs))
    // chunkNum chunk should be available
    if (!getFramePtr (mHlsBaseFrame + (chunkNumOffset * mHlsFramesPerChunk)))
      // not loaded, return chunkNum to load
      return mHlsBaseChunkNum + chunkNumOffset;
    else {
      // already loaded, next
      loaded++;
      chunkNumOffset++;
      }

  // return 0, no chunkNum available to load
  return 0;
  }
//}}}

// playsFrame
//{{{
void cSong::setPlayFrame (int frame) {

  if (hasHlsBase())
    mPlayFrame = min (frame, getLastFrame()+1);
  else
    mPlayFrame = min (max (frame, 0), getLastFrame()+1);
  }
//}}}
//{{{
void cSong::incPlayFrame (int frames, bool constrainToRange) {

  int playFrame = mPlayFrame + frames;
  if (constrainToRange)
    playFrame = mSelect.constrainToRange (mPlayFrame, playFrame);

  setPlayFrame (playFrame);
  }
//}}}
//{{{
void cSong::incPlaySec (int secs, bool useSelectRange) {
  incPlayFrame ((secs * mSampleRate) / mSamplesPerFrame, useSelectRange);
  }
//}}}

// hls
//{{{
void cSong::setHlsBase (int chunkNum, system_clock::time_point timePoint) {
// set baseChunkNum, baseTimePoint and baseFrame (sinceMidnight)

  unique_lock<shared_mutex> lock (mSharedMutex);

  mHlsBaseChunkNum = chunkNum;
  mHlsBaseTimePoint = timePoint;

  // calc hlsBaseFrame
  auto midnightTimePoint = date::floor<date::days>(timePoint);
  uint64_t msSinceMidnight = duration_cast<milliseconds>(timePoint - midnightTimePoint).count();
  mHlsBaseFrame = int((msSinceMidnight * mSampleRate) / mSamplesPerFrame / 1000);

  mPlayFrame = mHlsBaseFrame;

  mHlsBaseValid = true;
  }
//}}}
//{{{
void cSong::setHlsLoad (eHlsLoad hlsLoad, int chunkNum) {
// latch failed till success, might elaborate later

  switch (hlsLoad) {
    case eHlsLoading:
      if (chunkNum != eHlsFailedChunkNum) {
        mHlsLoad = eHlsLoading;
        eHlsFailedChunkNum = 0;
        }
      break;

    case eHlsFailed:
      mHlsLoad = eHlsFailed;
      eHlsFailedChunkNum = chunkNum;
      break;

    case eHlsIdle:
      mHlsLoad = eHlsIdle;
      eHlsFailedChunkNum = 0;
      break;
    }
  }
//}}}

// actions
//{{{
void cSong::prevSilencePlayFrame() {
  mPlayFrame = skipPrev (mPlayFrame, false);
  mPlayFrame = skipPrev (mPlayFrame, true);
  mPlayFrame = skipPrev (mPlayFrame, false);
  }
//}}}
//{{{
void cSong::nextSilencePlayFrame() {
  mPlayFrame = skipNext (mPlayFrame, true);
  mPlayFrame = skipNext (mPlayFrame, false);
  mPlayFrame = skipNext (mPlayFrame, true);
  }
//}}}

// private
//{{{
void cSong::clearFrames() {

  // new id for any cache
  mId++;

  // reset frames
  mPlayFrame = 0;
  mTotalFrames = 0;
  mSelect.clearAll();

  for (auto frame : mFrameMap)
    delete (frame.second);
  mFrameMap.clear();

  // reset maxValues
  mMaxPowerValue = kMinPowerValue;
  mMaxPeakValue = kMinPeakValue;
  mMaxFreqValue = kMinFreqValue;
  }
//}}}
//{{{
void cSong::checkSilenceWindow (int frame) {

  // walk backwards looking for continuous loaded quiet frames
  auto windowSize = 0;
  while (true) {
    auto framePtr = getFramePtr (frame);
    if (framePtr && framePtr->isQuiet()) {
      windowSize++;
      frame--;
      }
    else
      break;
    };

  if (windowSize > kSilenceWindowFrames) {
    // walk forward setting silence for continuous loaded quiet frames
    while (true) {
      auto framePtr = getFramePtr (++frame);
      if (framePtr && framePtr->isQuiet())
        framePtr->setSilence (true);
      else
        break;
      }
    }
  }
//}}}

//{{{
int cSong::skipPrev (int fromFrame, bool silence) {

  for (int frame = fromFrame-1; frame >= getFirstFrame(); frame--) {
    auto framePtr = getFramePtr (frame);
    if (framePtr && (framePtr->isSilence() ^ silence))
      return frame;
    }

  return fromFrame;
  }
//}}}
//{{{
int cSong::skipNext (int fromFrame, bool silence) {

  for (int frame = fromFrame; frame <= getLastFrame(); frame++) {
    auto framePtr = getFramePtr (frame);
    if (framePtr && (framePtr->isSilence() ^ silence))
      return frame;
    }

  return fromFrame;
  }
//}}}
