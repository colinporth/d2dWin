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
#include <stdint.h>
#include <string.h>

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

CURL* curl;
cBipBuffer mBipBuffer;
//{{{
static size_t header (void* ptr, size_t size, size_t nmemb, void* stream) {

  if (nmemb > 2) {
    // knock out cr lf
    auto bytePtr = (uint8_t*)ptr;
    bytePtr[nmemb-1] = 0;
    bytePtr[nmemb-2] = 0;
    cLog::log (LOGINFO, "%d %d %x %s", size, nmemb, stream, bytePtr);
    }

  return nmemb;
  }
//}}}
//{{{
static size_t body (void* ptr, size_t size, size_t nmemb, void* stream) {

  //cLog::log (LOGINFO, "body %d %d %x", size, nmemb, stream);

  int bytesAllocated = 0;
  auto toPtr = mBipBuffer.reserve ((int)nmemb, bytesAllocated);
  if (bytesAllocated > 0) {
    memcpy (toPtr, ptr, bytesAllocated);
    mBipBuffer.commit (bytesAllocated);
    }
  else
    cLog::log (LOGINFO, "mBipBuffer full");

  return nmemb;
  }
//}}}

//{{{
void httpThread (const char* url) {

 CoInitializeEx (NULL, COINIT_MULTITHREADED);

  //curl_easy_setopt (curl, CURLOPT_VERBOSE, 1L);
  curl_easy_setopt (curl, CURLOPT_URL, url);
  curl_easy_setopt (curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt (curl, CURLOPT_HEADERFUNCTION, header);
  curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, body);

  curl_easy_perform (curl);
  curl_easy_cleanup (curl);

  CoUninitialize();
  }
//}}}
//{{{
void readThread() {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::setThreadName ("read");

  while (mBipBuffer.getCommittedSize() == 0) {
    //{{{  wait for body data
    cLog::log (LOGINFO, "waiting for body");
    Sleep (200);
    }
    //}}}

  char* contentType = NULL;
  bool ok = (curl_easy_getinfo (curl, CURLINFO_CONTENT_TYPE, &contentType) == 0);
  bool aac = ok && contentType && (strcmp (contentType, "audio/aacp") == 0);

  AVCodecID streamType = aac ? AV_CODEC_ID_AAC : AV_CODEC_ID_MP3;
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

  cWinAudio audio (2, 44100);

  while (true) {
    int srcSize = 0;
    auto srcPtr = mBipBuffer.getContiguousBlock (srcSize);
    if (srcSize == 0) {
      cLog::log (LOGINFO, "waiting for more body data");
      Sleep (100);
      }
    else {
      cLog::log (LOGINFO, "body %d %x", srcSize, srcPtr);
      //{{{  ffmpeg decode and play block
      auto bytesLeft = srcSize;
      while (bytesLeft) {
        auto bytesUsed = av_parser_parse2 (parser, context, &avPacket.data, &avPacket.size, srcPtr, bytesLeft, 0, 0, AV_NOPTS_VALUE);
        srcPtr += bytesUsed;
        srcSize -= bytesUsed;
        if (avPacket.size) {
          auto ret = avcodec_send_packet (context, &avPacket);
          while (ret >= 0) {
            ret = avcodec_receive_frame (context, avFrame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF || ret < 0)
              break;

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
      mBipBuffer.decommitBlock (srcSize);
      }
    }

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
  const char* url = argc > 1 ? argv[1] : "http://stream.wqxr.org/wqxr.aac";
  cLog::log (LOGNOTICE, "curl test %s", url);

  avcodec_register_all();
  mBipBuffer.allocateBuffer (8192 * 1024);

  WSADATA wsaData;
  if (WSAStartup (MAKEWORD(2,2), &wsaData))
    exit (0);

  curl = curl_easy_init();
  if (curl) {
    thread ([=]() { httpThread (url); }).detach();
    thread ([=]() { readThread(); }).detach();
    }
  else
    cLog::log (LOGERROR, "curl_easy_init error");

  while (true)
    Sleep (1000);

  CoUninitialize();
  }
