// cAudioDecode.h
#pragma once
//extern "C" {
//  #include "libavcodec/avcodec.h"
//  }
#include "../../shared/decoders/minimp3.h"
#include "../../shared/decoders/aacdec.h"

class cAudioDecode {
public:
  enum eFrameType { eUnknown, eId3Tag, eWav, eMp3, eAac } ;

  cAudioDecode() {}
  cAudioDecode (eFrameType frameType);
  cAudioDecode (uint8_t* framePtr, int frameLen);
  ~cAudioDecode();

  // gets
  eFrameType getFrameType() { return mFrameType; }
  int getSampleRate() { return mSampleRate; }

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

  cAacDecoder* mAacDecoder = nullptr;
  mp3dec_t mMp3Dec;
  //AVCodecContext* mContext = nullptr;
  //AVPacket mAvPacket;
  //AVFrame* mAvFrame = nullptr;
  };
