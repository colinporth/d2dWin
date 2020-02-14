// cSong.cpp
#pragma once
//{{{  includes
#include "stdafx.h"

#include "cAudioDecode.h"

#include "../../shared/kissFft/kiss_fft.h"
#include "../../shared/kissFft/kiss_fftr.h"

#include "cSong.h"

#include "concurrent_vector.h"
//}}}

//{{{
cSong::~cSong() {

  mFrames.clear();
  setJpegImage (nullptr);
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
void cSong::setJpegImage (cJpegImage* jpegImage) {
  auto temp = mJpegImage;
  mJpegImage = jpegImage;;
  delete temp;
  }
//}}}
//{{{
void cSong::setTitle (const std::string& title) {

  if (!mFrames.empty())
    mFrames.back()->setTitle (title);
  }
//}}}
//{{{
void cSong::setPlayFrame (int frame) {
  mPlayFrame = std::min (std::max (frame, 0), getLastFrame());
  }
//}}}

void cSong::incPlayFrame (int frames) { setPlayFrame (mPlayFrame + frames); }
void cSong::incPlaySec (int secs) { incPlayFrame (secs * mSampleRate / mSamplesPerFrame); }

//{{{
void cSong::init (cAudioDecode::eFrameType frameType, int numChannels, int samplesPerFrame, int sampleRate) {

  mId++;

  mFrameType = frameType;
  mNumChannels = numChannels;
  mSampleRate = sampleRate;
  mSamplesPerFrame = samplesPerFrame;

  mFrames.clear();

  mPlayFrame = 0;
  mTotalFrames = 0;

  setJpegImage (nullptr);

  mMaxPowerValue = kMinPowerValue;
  mMaxFreqValue = 0.f;
  for (int i = 0; i < kMaxFreq; i++)
    mMaxFreqValues[i] = 0.f;

  fftrConfig = kiss_fftr_alloc (samplesPerFrame, 0, 0, 0);
  }
//}}}
//{{{
bool cSong::addFrame (uint8_t* stream, int frameLen, int estimatedTotalFrames, int samplesPerFrame, float* samples) {
// return true if enough frames added to start playing, streamLen only used to estimate totalFrames

  mSamplesPerFrame = samplesPerFrame;
  mTotalFrames = estimatedTotalFrames;

  // sum of squares channel power
  auto powerValues = (float*)malloc (mNumChannels * 4);
  memset (powerValues, 0, mNumChannels * 4);
  for (int sample = 0; sample < samplesPerFrame; sample++) {
    timeBuf[sample] = 0;
    for (auto chan = 0; chan < mNumChannels; chan++) {
      auto value = *samples++;
      timeBuf[sample] += value;
      powerValues[chan] += value * value;
      }
    }
  for (auto chan = 0; chan < mNumChannels; chan++) {
    powerValues[chan] = sqrtf (powerValues[chan] / samplesPerFrame);
    mMaxPowerValue = std::max (mMaxPowerValue, powerValues[chan]);
    }

  kiss_fftr (fftrConfig, timeBuf, freqBuf);

  auto freqValues = (float*)malloc (kMaxFreq * 4);
  for (auto freq = 0; freq < kMaxFreq; freq++) {
    freqValues[freq] = sqrt ((freqBuf[freq].r * freqBuf[freq].r) + (freqBuf[freq].i * freqBuf[freq].i));
    mMaxFreqValue = std::max (mMaxFreqValue, freqValues[freq]);
    mMaxFreqValues[freq] = std::max (mMaxFreqValues[freq], freqValues[freq]);
    }

  // dodgy juicing of maxFreqValue * 4 to max Luma
  float scale = 1024.f / mMaxFreqValue;
  auto lumaValues = (uint8_t*)malloc (kMaxSpectrum);
  for (auto freq = 0; freq < kMaxSpectrum; freq++) {
    auto value = freqValues[freq] * scale;
    lumaValues[kMaxSpectrum - freq - 1] = value > 255 ? 255 : uint8_t(value);
    }

  mFrames.push_back (new cFrame (stream, frameLen, powerValues, freqValues, lumaValues));

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

  // return true if first frame, used to launch player
  return frameNum == 0;
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

//{{{
int cSong::skipPrev (int fromFrame, bool silent) {

  for (auto frame = fromFrame-1; frame >= 0; frame--)
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
