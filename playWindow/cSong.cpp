// cSong.cpp - singleton class
#pragma once
//{{{  includes
#include "stdafx.h"

#include "cSong.h"

using namespace std;
using namespace chrono;
//}}}

//
constexpr static float kMinPowerValue = 0.25f;
constexpr static float kMinFreqValue = 256.f;
constexpr static int kSilentWindowFrames = 4;

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
  mSamplesPerFrame = samplesPerFrame;
  mSampleRate = sampleRate;

  clearFrames();
  mHasHlsBase = false;

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
    for (auto chan = 0; chan < mNumChannels; chan++) {
      auto value = *samples++;
      timeBuf[sample] += value;
      powerValues[chan] += value * value;
      peakValues[chan] = max (abs(peakValues[chan]), value);
      }
    }

  for (auto chan = 0; chan < mNumChannels; chan++) {
    powerValues[chan] = sqrtf (powerValues[chan] / mSamplesPerFrame);
    mMaxPowerValue = max (mMaxPowerValue, powerValues[chan]);
    mMaxPeakValue = max (mMaxPeakValue, peakValues[chan]);
    }

  // ??? lock against init fftrConfig recalc???
  kiss_fftr (fftrConfig, timeBuf, freqBuf);

  float freqScale = 255.f / mMaxFreqValue;
  auto freqBufPtr = freqBuf;
  auto freqValues = (uint8_t*)malloc (getNumFreqBytes());
  auto freqValuesPtr = freqValues;
  auto lumaValues = (uint8_t*)malloc (getNumFreqBytes());
  auto lumaValuesPtr = lumaValues + getNumFreqBytes() - 1;
  for (auto i = 0; i < getNumFreqBytes(); i++) {
    auto value = sqrt (((*freqBufPtr).r * (*freqBufPtr).r) + ((*freqBufPtr).i * (*freqBufPtr).i));
    mMaxFreqValue = max (mMaxFreqValue, value);

    // freq scaled to byte, only used for display
    value *= freqScale;
    *freqValuesPtr++ = value > 255 ? 255 : uint8_t(value);

    // luma crushed, reversed index for quick copyMem to bitmap later
    value *= 4.f;
    *lumaValuesPtr-- = value > 255 ? 255 : uint8_t(value);

    freqBufPtr++;
    }

  unique_lock<shared_mutex> lock (mSharedMutex);

  // totalFrames can be a changing estimate for file, or increasing value for streaming
  mTotalFrames = totalFrames;
  mFrameMap.insert (map<int,cFrame*>::value_type (
    frame, new cFrame (mapped, stream, frameLen, powerValues, peakValues, freqValues, lumaValues)));

  // calc silent window
  auto framePtr = getFramePtr (frame);
  if (framePtr && framePtr->isSilent()) {
    auto window = kSilentWindowFrames;
    auto windowFrame = frame - 1;
    while ((window >= 0) && (windowFrame >= 0)) {
      // walk backwards looking for no silence
      auto windowFramePtr = getFramePtr (windowFrame);
      if (windowFramePtr && !windowFramePtr->isSilentThreshold()) {
        framePtr->setSilent (false);
        break;
        }
      windowFrame--;
      window--;
      }
    }
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

// sets
//{{{
void cSong::setPlayFrame (int frame) {
  mPlayFrame = min (max (frame, 0), getLastFrame()+1);
  }
//}}}
//{{{
void cSong::setTitle (const string& title) {

  //if (!hasFrames())
  //  mFrames.back()->setTitle (title);
  }
//}}}

//{{{
void cSong::setHlsBase (int baseChunkNum, system_clock::time_point baseTimePoint) {
// set baseChunkNum, baseTimePoint and baseFrame (sinceMidnight)

  unique_lock<shared_mutex> lock (mSharedMutex);

  mHlsBaseChunkNum = baseChunkNum;
  mHlsBaseTimePoint = baseTimePoint;

  // calc hlsBaseFrame
  auto midnightTimePoint = date::floor<date::days>(baseTimePoint);
  uint64_t msSinceMidnight = duration_cast<milliseconds>(baseTimePoint - midnightTimePoint).count();
  mHlsBaseFrame = int((msSinceMidnight * mSampleRate) / mSamplesPerFrame / 1000);

  mPlayFrame = mHlsBaseFrame;

  mStreaming = true;
  mHasHlsBase = true;
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

// incs
//{{{
void cSong::incPlayFrame (int frames) {

  int playFrame = mPlayFrame + frames;
  if (!mHasHlsBase || (playFrame >= 0))
    // simple case, clip to playFrame 0
    setPlayFrame (playFrame);
  else
    mPlayFrame += frames;
  }
//}}}
//{{{
void cSong::incPlaySec (int secs) {
  incPlayFrame ((secs * mSampleRate) / mSamplesPerFrame);
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

  for (auto frame : mFrameMap)
    delete (frame.second);
  mFrameMap.clear();

  // reset maxValues
  mMaxPowerValue = kMinPowerValue;
  mMaxPeakValue = kMinPowerValue;

  mMaxFreqValue = kMinFreqValue;
  }
//}}}
//{{{
int cSong::skipPrev (int fromFrame, bool silent) {

  for (int frame = fromFrame-1; frame >= 0; frame--)
    if (getFramePtr (frame)->isSilent() ^ silent)
      return frame;

  return fromFrame;
  }
//}}}
//{{{
int cSong::skipNext (int fromFrame, bool silent) {

  for (int frame = fromFrame; frame < getNumFrames(); frame++)
    if (getFramePtr (frame)->isSilent() ^ silent)
      return frame;

  return fromFrame;
  }
//}}}
