// cSong.cpp - singleton class
#pragma once
//{{{  includes
#include "stdafx.h"

#include "cSong.h"

using namespace std;
//}}}

//{{{
cSong::~cSong() {

  for (auto frame : mFrames)
    delete (frame);

  mFrames.clear();
  }
//}}}

// gets
//{{{
int cSong::getPlayFrame() {
  if (mPlayFrame < mFrames.size())
    return mPlayFrame;
  else if (!mFrames.empty())
    return (int)mFrames.size() - 1;
  else // startup case
    return 0;
  }
//}}}
//{{{
uint8_t* cSong::getPlayFramePtr() {

  if (mPlayFrame < mFrames.size())
    return mFrames[mPlayFrame]->getPtr();
  else if (!mFrames.empty())
    return mFrames[mFrames.size()-1]->getPtr();

  return nullptr;
  }
//}}}
//{{{
int cSong::getPlayFrameLen() {

  if (mPlayFrame < mFrames.size())
    return mFrames[mPlayFrame]->getLen();
  else if (!mFrames.empty())
    return mFrames[mFrames.size()-1]->getLen();

   return 0;
  }
//}}}

// sets
//{{{
void cSong::setTitle (const string& title) {

  if (!mFrames.empty())
    mFrames.back()->setTitle (title);
  }
//}}}
//{{{
void cSong::setPlayFrame (int frame) {
  mPlayFrame = min (max (frame, 0), getLastFrame());
  }
//}}}

void cSong::incPlayFrame (int frames) { setPlayFrame (mPlayFrame + frames); }
void cSong::incPlaySec (int secs) { incPlayFrame (secs * mSampleRate / mSamplesPerFrame); }

//{{{
void cSong::init (cAudioDecode::eFrameType frameType, int numChannels, int samplesPerFrame, int sampleRate) {

  lock_guard<mutex> lockGuard (mMutex);

  // new id for any cache
  mId++;

  // reset frame type
  mFrameType = frameType;
  mNumChannels = numChannels;
  mSamplesPerFrame = samplesPerFrame;
  mSampleRate = sampleRate;

  // reset frames
  mPlayFrame = 0;
  mTotalFrames = 0;

  for (auto frame : mFrames)
    delete (frame);
  mFrames.clear();

  // reset maxValue
  mMaxPowerValue = kMinPowerValue;
  mMaxPeakValue = kMinPowerValue;

  mMaxFreqValue = 0.f;
  for (int i = 0; i < kMaxFreq; i++)
    mMaxFreqValues[i] = 0.f;

  fftrConfig = kiss_fftr_alloc (samplesPerFrame, 0, 0, 0);
  }
//}}}
//{{{
void cSong::addFrame (bool mapped, uint8_t* stream, int frameLen, int totalFrames, int samplesPerFrame, float* samples) {
// return true if enough frames added to start playing, streamLen only used to estimate totalFrames

  // sum of squares channel power
  auto powerValues = (float*)malloc (mNumChannels * 4);
  memset (powerValues, 0, mNumChannels * 4);

  auto peakValues = (float*)malloc (mNumChannels * 4);
  memset (peakValues, 0, mNumChannels * 4);

  for (int sample = 0; sample < samplesPerFrame; sample++) {
    timeBuf[sample] = 0;
    for (auto chan = 0; chan < mNumChannels; chan++) {
      auto value = *samples++;
      timeBuf[sample] += value;
      powerValues[chan] += value * value;
      peakValues[chan] = max (abs(peakValues[chan]), value);
      }
    }

  for (auto chan = 0; chan < mNumChannels; chan++) {
    powerValues[chan] = sqrtf (powerValues[chan] / samplesPerFrame);
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

  lock_guard<mutex> lockGuard (mMutex);

  // totalFrames can be a changing estimate for file, or increasing value for streaming
  mTotalFrames = totalFrames;
  mSamplesPerFrame = samplesPerFrame;
  mFrames.push_back (new cFrame (mapped, stream, frameLen, powerValues, peakValues, freqValues, lumaValues));

  // calc silent window
  auto frameNum = getLastFrame();
  if (mFrames[frameNum]->isSilent()) {
    auto window = kSilentWindowFrames;
    auto windowFrame = frameNum - 1;
    while ((window >= 0) && (windowFrame >= 0)) {
      // walk backwards looking for no silence
      if (!mFrames[windowFrame]->isSilentThreshold()) {
        mFrames[frameNum]->setSilent (false);
        break;
        }
      windowFrame--;
      window--;
      }
    }
  }
//}}}

//{{{
void cSong::prevSilence() {
  mPlayFrame = skipPrev (mPlayFrame, false);
  mPlayFrame = skipPrev (mPlayFrame, true);
  mPlayFrame = skipPrev (mPlayFrame, false);
  }
//}}}
//{{{
void cSong::nextSilence() {
  mPlayFrame = skipNext (mPlayFrame, true);
  mPlayFrame = skipNext (mPlayFrame, false);
  mPlayFrame = skipNext (mPlayFrame, true);
  }
//}}}

// private
//{{{
int cSong::skipPrev (int fromFrame, bool silent) {

  for (int frame = fromFrame-1; frame >= 0; frame--)
    if (mFrames[frame]->isSilent() ^ silent)
      return frame;

  return fromFrame;
  }
//}}}
//{{{
int cSong::skipNext (int fromFrame, bool silent) {

  for (int frame = fromFrame; frame < getNumFrames(); frame++)
    if (mFrames[frame]->isSilent() ^ silent)
      return frame;

  return fromFrame;
  }
//}}}
