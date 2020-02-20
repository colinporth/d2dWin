// cSong.cpp - singleton class
#pragma once
//{{{  includes
#include "stdafx.h"

#include "cSong.h"

using namespace std;
using namespace chrono;
//}}}

constexpr static int kSilentWindowFrames = 10;
constexpr static float kMinPowerValue = 0.25f;

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

// ****** check for insert at same frame ******************

  // sum of squares channel power
  auto powerValues = (float*)malloc (mNumChannels * 4);
  memset (powerValues, 0, mNumChannels * 4);

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

  // locked ???
  kiss_fftr (fftrConfig, timeBuf, freqBuf);

  auto freqValues = (float*)malloc (kMaxFreq * 4);
  for (auto freq = 0; freq < kMaxFreq; freq++) {
    freqValues[freq] = sqrt ((freqBuf[freq].r * freqBuf[freq].r) + (freqBuf[freq].i * freqBuf[freq].i));
    mMaxFreqValue = max (mMaxFreqValue, freqValues[freq]);
    mMaxFreqValues[freq] = max (mMaxFreqValues[freq], freqValues[freq]);
    }

  // dodgy juicing of maxFreqValue * 4 to max Luma
  float scale = 1024.f / mMaxFreqValue;
  auto lumaValues = (uint8_t*)malloc (kMaxFreq);
  for (auto freq = 0; freq < kMaxFreq; freq++) {
    auto value = freqValues[freq] * scale;
    lumaValues[kMaxFreq - freq - 1] = value > 255 ? 255 : uint8_t(value);
    }

  unique_lock<shared_mutex> lock (mSharedMutex);

  // totalFrames can be a changing estimate for file, or increasing value for streaming
  mTotalFrames = totalFrames;
  mFrameMap.insert (map<int,cFrame*>::value_type (
    frame, new cFrame (mapped, stream, frameLen, powerValues, peakValues, freqValues, lumaValues)));

  // calc silent window
  auto frameNum = getLastFrame();
  auto framePtr = getFramePtr (frameNum);
  if (framePtr && framePtr->isSilent()) {
    auto window = kSilentWindowFrames;
    auto windowFrame = frameNum - 1;
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
int cSong::getHlsSeqNum (system_clock::time_point now, int minMs, int& frame) {

  // get playFrame seqNumOffset from baseFrame, cope with -v offset correctly
  int frameOffset = mPlayFrame - mHlsBaseFrame;
  int seqNumOffset = (frameOffset >= 0)  ? (frameOffset / mHlsFramesPerChunk) :
                                           -((mHlsFramesPerChunk - 1 - frameOffset) / mHlsFramesPerChunk);

  frame = mHlsBaseFrame + (seqNumOffset * mHlsFramesPerChunk);

  // loop until first unloaded frame on or past playFrame, return
  // - else return no load
  while (true) {
    if (!getFramePtr (frame)) {
      // seqNum frame not loaded
      auto seqNumTimePoint = mHlsBaseTimePoint + milliseconds (seqNumOffset * 6400);
      if ((int)(duration_cast<milliseconds>(now - seqNumTimePoint)).count() > minMs)
        // now is 10secs past chunk timePoint, it should be available
        return mHlsBaseSeqNum + seqNumOffset;
      else {
        // too early
        frame = 0;
        return 0;
        }
      }
    seqNumOffset++;
    frame += mHlsFramesPerChunk;
    }
  }
//}}}

// sets
//{{{
void cSong::setPlayFrame (int frame) {
  mPlayFrame = min (max (frame, 0), getLastFrame());
  }
//}}}
//{{{
void cSong::setTitle (const string& title) {

  //if (!hasFrames())
  //  mFrames.back()->setTitle (title);
  }
//}}}

//{{{
void cSong::setHlsBase (int baseSeqNum, system_clock::time_point baseTimePoint) {
// set baseSeqNum, baseTimePoint and baseFrame (sinceMidnight)

  unique_lock<shared_mutex> lock (mSharedMutex);

  mHlsBaseSeqNum = baseSeqNum;
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
void cSong::setHlsLoad (eHlsLoad hlsLoad, int seqNum) {
// latch failed till success, might elaborate later

  switch (hlsLoad) {
    case eHlsLoading:
      if (seqNum != eHlsFailedSeqNum) {
        mHlsLoad = eHlsLoading;
        eHlsFailedSeqNum = 0;
        }
      break;

    case eHlsFailed:
      mHlsLoad = eHlsFailed;
      eHlsFailedSeqNum = seqNum;
      break;

    case eHlsIdle:
      mHlsLoad = eHlsIdle;
      eHlsFailedSeqNum = 0;
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

  // reset maxValue
  mMaxPowerValue = kMinPowerValue;
  mMaxPeakValue = kMinPowerValue;

  mMaxFreqValue = 0.f;
  for (int i = 0; i < kMaxFreq; i++)
    mMaxFreqValues[i] = 0.f;
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
