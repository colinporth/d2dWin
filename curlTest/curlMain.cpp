// curlMain.cpp
//{{{  includes
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include <shellapi.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <shlguid.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <io.h>

#include <thread>

#include "../../shared/utils/cLog.h"
#include "../../shared/utils/cWinAudio.h"
#include "../../shared/utils/cBipBuffer.h"

#define CURL_STATICLIB
#include "../../curl/include/curl/curl.h"

extern "C" {
  #include <libavcodec/avcodec.h>
  #include <libavformat/avformat.h>
  }

using namespace std;
//}}}

cBipBuffer mBipBuffer;
CURL* curl;
bool mAac = false;

//{{{
static size_t headerData (void* ptr, size_t size, size_t nmemb, void* stream) {

  cLog::log (LOGINFO, "header %d %d %x %s", size, nmemb, stream, ptr);
  return nmemb;
  }
//}}}
//{{{
static size_t writeData (void* ptr, size_t size, size_t nmemb, void* stream) {

  //cLog::log (LOGINFO, "body %d %d %x", size, nmemb, stream);

  int bytesAllocated = 0;
  auto toPtr = mBipBuffer.reserve ((int)nmemb, bytesAllocated);
  if (bytesAllocated > 0) {
    memcpy (toPtr, ptr, bytesAllocated);
    mBipBuffer.commit (bytesAllocated);
    }
  else
    cLog::log (LOGINFO, "mBipBuffer full");

  char* contentType = NULL;
  auto res = curl_easy_getinfo (curl, CURLINFO_CONTENT_TYPE, &contentType);
  if (!res && contentType)
    mAac = strcmp (contentType, "audio/aacp") == 0;

  return nmemb;
  }
//}}}
//{{{
void readThread() {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::setThreadName ("read");

  while (mBipBuffer.getCommittedSize() == 0) {
    cLog::log (LOGINFO, "reader waiting for first data");
    Sleep (100);
    }

  AVCodecID streamType = mAac ? AV_CODEC_ID_AAC : AV_CODEC_ID_MP3;
  auto parser = av_parser_init (streamType);
  auto codec = avcodec_find_decoder (streamType);
  auto context = avcodec_alloc_context3 (codec);
  avcodec_open2 (context, codec, NULL);

  AVPacket avPacket;
  av_init_packet (&avPacket);
  avPacket.data = 0;
  avPacket.size = 0;

  auto avFrame = av_frame_alloc();

  auto samples = (int16_t*)malloc (2048 * 2 * 2);
  memset (samples, 0, 2048 * 2 * 2);

  cWinAudio audio;
  audio.open (2, 44100);

  while (true) {
    int blockSize = 0;
    auto ptr = mBipBuffer.getContiguousBlock (blockSize);
    if (blockSize == 0) {
      cLog::log (LOGINFO, "reader waiting");
      Sleep (100);
      }
    else {
      cLog::log (LOGINFO, "read %d %x", blockSize, ptr);
      //{{{  ffmpeg decode and play block
      auto srcPtr = ptr;
      auto srcSize = blockSize;

      while (srcSize) {
        auto bytesUsed = av_parser_parse2 (parser, context, &avPacket.data, &avPacket.size,
                                           srcPtr, (int)srcSize, 0, 0, AV_NOPTS_VALUE);
        //cLog::log (LOGINFO, "av_parser_parse2 %d %d", bytesUsed, avPacket.size);

        srcPtr += bytesUsed;
        srcSize -= bytesUsed;
        if (avPacket.size) {
          auto ret = avcodec_send_packet (context, &avPacket);
          while (ret >= 0) {
            ret = avcodec_receive_frame (context, avFrame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF || ret < 0)
              break;

            //cLog::log (LOGINFO, "avcodec_receive_frame %d %d", avFrame->channels, avFrame->nb_samples);

            //frame->set (interpolatedPts, avFrame->nb_samples*90/48, pidInfo->mPts, avFrame->channels, avFrame->nb_samples);
            switch (context->sample_fmt) {
              case AV_SAMPLE_FMT_S16P:
                //{{{  16bit signed planar, copy planar to interleaved, calc channel power
                for (auto channel = 0; channel < avFrame->channels; channel++) {
                  auto srcPtr = (short*)avFrame->data[channel];
                  auto dstPtr = (short*)(samples) + channel;
                  for (auto i = 0; i < avFrame->nb_samples; i++) {
                    auto sample = *srcPtr++;
                    *dstPtr = sample;
                    dstPtr += avFrame->channels;
                    }
                  }

                break;
                //}}}
              case AV_SAMPLE_FMT_FLTP:
                //{{{  32bit float planar, copy planar channel to interleaved, calc channel power
                for (auto channel = 0; channel < avFrame->channels; channel++) {
                  auto srcPtr = (float*)avFrame->data[channel];
                  auto dstPtr = (short*)(samples) + channel;
                  for (auto i = 0; i < avFrame->nb_samples; i++) {
                    auto sample = (short)(*srcPtr++ * 0x8000);
                    *dstPtr = sample;
                    dstPtr += avFrame->channels;
                    }
                  }

                break;
                //}}}
              default:;
              }
            audio.play (avFrame->channels, (int16_t*)samples, avFrame->nb_samples, 1.f);
            }
          }
        }
      //}}}
      mBipBuffer.decommitBlock (blockSize);
      }
    }

  audio.close();

  free (samples);
  av_frame_free (&avFrame);
  if (context)
    avcodec_close (context);
  if (parser)
    av_parser_close (parser);


  CoUninitialize();
  }
//}}}

int main (int argc, char *argv[]) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);

  cLog::init (LOGINFO, false, "");
  cLog::log (LOGNOTICE, "curl test");

  const char* url = argc > 1 ? argv[1] : "http://us4.internet-radio.com:8266/";
  cLog::log (LOGNOTICE, url);

  mBipBuffer.allocateBuffer (8192 * 1024);
  avcodec_register_all();

  WSADATA wsaData;
  if (WSAStartup (MAKEWORD(2,2), &wsaData))
    exit (0);

  curl = curl_easy_init();
  if (curl) {
    //curl_easy_setopt (curl, CURLOPT_URL, "http://www.example.com");
    //curl_easy_setopt (curl, CURLOPT_URL, "http://stream.wqxr.org/js-stream.aac");
    curl_easy_setopt (curl, CURLOPT_VERBOSE, 1L);

    curl_easy_setopt (curl, CURLOPT_URL, url);
    curl_easy_setopt (curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt (curl, CURLOPT_HEADERFUNCTION, headerData);
    curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, writeData);

    thread ([=](){ readThread(); }).detach();

    CURLcode res = curl_easy_perform (curl);

    curl_easy_cleanup (curl);
    }

  CoUninitialize();
  }
