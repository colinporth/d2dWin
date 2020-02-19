// cSong.cpp - singleton class
#pragma once
//{{{  includes
#include "stdafx.h"

#include "cSong.h"

using namespace std;
using namespace chrono;
//}}}

//auto epgItemIt = mEpgItemMap.find (startTime);
//if (epgItemIt != mEpgItemMap.end())
//  if (title == epgItemIt->second->getTitleString())
//    if (epgItemIt->second->getRecord())

//auto epgItemIt = mEpgItemMap.find (startTime);
//if (epgItemIt == mEpgItemMap.end())
//  mEpgItemMap.insert (
//    std::map<std::chrono::system_clock::time_point,cEpgItem*>::value_type (
//      startTime, new cEpgItem (false, record, startTime, duration, title, description)));

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

  // ??? inside lock ????
  kiss_fftr (fftrConfig, timeBuf, freqBuf);

  auto freqValues = (float*)malloc (kMaxFreq * 4);
  for (auto freq = 0; freq < kMaxFreq; freq++) {
    freqValues[freq] = sqrt ((freqBuf[freq].r * freqBuf[freq].r) + (freqBuf[freq].i * freqBuf[freq].i));
    mMaxFreqValue = max (mMaxFreqValue, freqValues[freq]);
    mMaxFreqValues[freq] = max (mMaxFreqValues[freq], freqValues[freq]);
    }

  // dodgy juicing of maxFreqValue * 4 to max Luma
  float scale = 1024.f / mMaxFreqValue;
  auto lumaValues = (uint8_t*)malloc (kMaxSpectrum);
  for (auto freq = 0; freq < kMaxSpectrum; freq++) {
    auto value = freqValues[freq] * scale;
    lumaValues[kMaxSpectrum - freq - 1] = value > 255 ? 255 : uint8_t(value);
    }

  unique_lock<shared_mutex> lock (mSharedMutex);

  // totalFrames can be a changing estimate for file, or increasing value for streaming
  mTotalFrames = totalFrames;
  //mFrames.push_back (new cFrame (mapped, stream, frameLen, powerValues, peakValues, freqValues, lumaValues));
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
      if (!getFramePtr (windowFrame)->isSilentThreshold()) {
        getFramePtr (frameNum)->setSilent (false);
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
int cSong::getPlayFrame() {

  if (mPlayFrame <= getLastFrame())
    return mPlayFrame;

  return getLastFrame();
  }
//}}}

//{{{
uint64_t cSong::getHlsBaseFrame() {
  return mHlsBaseFrame;
  }
//}}}
//{{{
int cSong::getHlsOffsetMs (system_clock::time_point now) {
  auto basedMs = duration_cast<milliseconds>(now - getHlsBaseTimePoint());
  return int (basedMs.count()) - (getHlsBasedSeqNum() * 6400);
  }
//}}}
//{{{
int cSong::getHlsSeqNumGet (system_clock::time_point now, int minMs, int maxMs) {

  auto hlsOffset = getHlsOffsetMs (now);
  if ((hlsOffset > minMs) && (hlsOffset < maxMs))
    return mHlsBaseSeqNum + mHlsSeqNum;
  else
    return 0;
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
void cSong::setHlsBase (int startSeqNum, system_clock::time_point startTimePoint) {

  mStreaming = true;

  mHlsBaseSeqNum = startSeqNum;
  mHlsBaseTimePoint = startTimePoint;
  mHlsBaseDatePoint = date::floor<date::days>(mHlsBaseTimePoint);

  uint64_t msSinceMidnight = (duration_cast<milliseconds>(mHlsBaseTimePoint - mHlsBaseDatePoint)).count();
  mHlsBaseFrame = (msSinceMidnight * mSampleRate) / mSamplesPerFrame / 1000;

  mHlsSeqNum = 0;

  // should calculate
  mHlsFramesPerChunk = mHlsBitrate >= 128000 ? 300 : 150;

  mHasHlsBase = true;
  }
//}}}

// incs
//{{{
bool cSong::incPlayFrame (int frames) {

  int newFrame = mPlayFrame + frames;
  if (!mHasHlsBase || (newFrame >= 0)) {
    // simple case, clip to playFrame 0
    setPlayFrame (newFrame);
    return false;
    }
  else {
    // allow new chunks before 0
    const int framesPerChunk = mHlsFramesPerChunk;
    int chunks = (-newFrame + (mHlsFramesPerChunk-1)) / framesPerChunk;
    int frameInChunk = framesPerChunk + (newFrame % framesPerChunk);

    cLog::log (LOGINFO, "back to %d, chunks:%d frame:%d", newFrame, chunks, frameInChunk);

    mHlsSeqNum = 0;
    mHlsBaseSeqNum = mHlsBaseSeqNum - chunks;
    mHlsBaseTimePoint = mHlsBaseTimePoint - milliseconds (chunks * 6400);
    clearFrames();
    mPlayFrame = frameInChunk;

    //return false;
    return true;
    }
  }
//}}}
//{{{
bool cSong::incPlaySec (int secs) {
  return incPlayFrame ((secs * mSampleRate) / mSamplesPerFrame);
  }
//}}}

//{{{
void cSong::nextHlsSeqNum() {
  mHlsSeqNum++;
  mHlsLate = 0;
  mHlsLoading = false;
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
