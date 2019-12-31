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

#include "../../shared/utils/cLog.h"
#include "../../shared/utils/cWinAudio.h"

#define CURL_STATICLIB
#include "../../curl/include/curl/curl.h"

extern "C" {
  #include <libavcodec/avcodec.h>
  #include <libavformat/avformat.h>
  }

using namespace std;
//}}}

int main (int argc, char *argv[]) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);

  cLog::init (LOGINFO, false, "");
  cLog::log (LOGNOTICE, "curl test");

  WSADATA wsaData;
  if (WSAStartup (MAKEWORD(2,2), &wsaData))
    exit (0);

  CURL* curl = curl_easy_init();
  if (curl) {
    //curl_easy_setopt (curl, CURLOPT_URL, "http://stream.wqxr.org/wqxr.aac");
    curl_easy_setopt (curl, CURLOPT_URL, "http://www.wqxr.org");
    CURLcode res = curl_easy_perform (curl);
    if (res == CURLE_OK) {
      char* ct;
      /* ask for the content-type */
      res = curl_easy_getinfo (curl, CURLINFO_CONTENT_TYPE, &ct);

      if ((res == CURLE_OK) && ct)
        printf("We received Content-Type: %s\n", ct);
      }

    curl_easy_cleanup (curl);
    }

  const char* filename = argc > 1 ? argv[1] : "C:\\Users\\colin\\Desktop\\test.aac";
  cLog::log (LOGNOTICE, filename);

  // map file
  auto fileHandle = CreateFile (filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
  auto streamBuf = (uint8_t*)MapViewOfFile (CreateFileMapping (fileHandle, NULL, PAGE_READONLY, 0, 0, NULL), FILE_MAP_READ, 0, 0, 0);
  auto streamLen = (int)GetFileSize (fileHandle, NULL);

  auto samples = (int16_t*)malloc (2048 * 2 * 2);
  memset (samples, 0, 2048 * 2 * 2);
  //{{{  ffmpeg
  avcodec_register_all();

  AVCodecID streamType;

  bool aac = true;
  if (aac)
    streamType = AV_CODEC_ID_AAC;
  else
    streamType = AV_CODEC_ID_MP3;

  auto mAudParser = av_parser_init (streamType);
  auto mAudCodec = avcodec_find_decoder (streamType);
  auto mAudContext = avcodec_alloc_context3 (mAudCodec);
  avcodec_open2 (mAudContext, mAudCodec, NULL);

  AVPacket avPacket;
  av_init_packet (&avPacket);
  avPacket.data = streamBuf;
  avPacket.size = 0;

  cWinAudio audio;
  audio.audOpen (2, 44100);

  auto srcPtr = streamBuf;
  auto srcSize = streamLen;

  auto avFrame = av_frame_alloc();
  while (srcSize) {
    auto bytesUsed = av_parser_parse2 (mAudParser, mAudContext, &avPacket.data, &avPacket.size,
                                       srcPtr, (int)srcSize, 0, 0, AV_NOPTS_VALUE);
    cLog::log (LOGINFO, "av_parser_parse2 %d %d", bytesUsed, avPacket.size);

    srcPtr += bytesUsed;
    srcSize -= bytesUsed;
    if (avPacket.size) {
      auto ret = avcodec_send_packet (mAudContext, &avPacket);
      while (ret >= 0) {
        ret = avcodec_receive_frame (mAudContext, avFrame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF || ret < 0)
          break;

        cLog::log (LOGINFO, "avcodec_receive_frame %d %d", avFrame->channels, avFrame->nb_samples);

        //frame->set (interpolatedPts, avFrame->nb_samples*90/48, pidInfo->mPts, avFrame->channels, avFrame->nb_samples);
        switch (mAudContext->sample_fmt) {
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
        audio.audPlay (avFrame->channels, (int16_t*)samples, avFrame->nb_samples, 1.f);
        }
      }
    }
  av_frame_free (&avFrame);

  if (mAudContext)
    avcodec_close (mAudContext);
  if (mAudParser)
    av_parser_close (mAudParser);
  //}}}
  free (samples);

  UnmapViewOfFile (streamBuf);
  CloseHandle (fileHandle);

  CoUninitialize();
  }
