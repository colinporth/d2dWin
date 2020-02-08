// cAudioDecode.h
#pragma once
//{{{  includes
extern "C" {
  #include "libavcodec/avcodec.h"
  }
//}}}

class cAudioDecode {
public:
  enum eFrameType { eUnknown, eMp3, eAac, eId3Tag, eWav } ;

  cAudioDecode() {}
  cAudioDecode (eFrameType frameType);
  cAudioDecode (uint8_t* framePtr, int frameLen);
  ~cAudioDecode();

  // gets
  eFrameType getFrameType() { return mFrameType; }
  int getSampleRate() { return mSampleRate; }
  int getNumSamples() { return mNumSamples; }

  uint8_t* getFramePtr() { return mFramePtr; }
  int getFrameLen() { return mFrameLen; }
  int getNextFrameOffset() { return mFrameLen + mSkip; }

  void setFrame (uint8_t* framePtr, int frameLen);

  bool parseFrame (uint8_t* framePtr, uint8_t* frameLast);
  int frameToSamples (float* samples);

  // statics not quite right but being here picks up eFrameType
  static eFrameType parseSomeFrames (uint8_t* framePtr, uint8_t* frameLast, int& sampleRate);
  static eFrameType parseAllFrames (uint8_t* framePtr, uint8_t* frameLast, int& sampleRate);
  inline static uint8_t* mJpegPtr = nullptr;
  inline static int mJpegLen = 0;

private:
  static bool parseId3Tag (uint8_t* framePtr, uint8_t* frameLast);

  // vars
  uint8_t* mFramePtr = nullptr;
  int mFrameLen = 0;
  int mSkip = 0;

  eFrameType mFrameType = eUnknown;
  int mSampleRate = 0;
  int mNumSamples = 0;

  AVCodecContext* mContext = nullptr;
  AVPacket mAvPacket;
  AVFrame* mAvFrame = nullptr;
  };
