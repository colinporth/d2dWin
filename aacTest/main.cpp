// aactest main.cpp
//{{{  includes
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

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
#include "../../shared/teensyAac/cAacDecoder.h"

#pragma comment(lib,"common.lib")

using namespace std;
//}}}


int main (int argc, char *argv[]) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);

  cLog::init (LOGINFO, false, "");
  cLog::log (LOGNOTICE, "aac test");

  const char* filename = argc > 1 ? argv[1] : "C:\\Users\\colin\\Desktop\\test.aac";
  auto fileHandle = CreateFile (filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
  auto streamBuf = (uint8_t*)MapViewOfFile (CreateFileMapping (fileHandle, NULL, PAGE_READONLY, 0, 0, NULL), FILE_MAP_READ, 0, 0, 0);
  auto streamLen = (int)GetFileSize (fileHandle, NULL);

  auto samples = (int16_t*)malloc (2048 * 2 * 2);
  memset (samples, 0, 1024 * 2 * 2);

  // int framesPerAacFrame = bitrate <= 96000 ? 2 : 1;
  cAacDecoder aacDecoder;

  cWinAudio audio;
  audio.audOpen (2, 44100/2);

  uint8_t* srcPtr = streamBuf;
  int bytesLeft = streamLen;
  while (true) {
    int frameLen = aacDecoder.AACDecode (&srcPtr, &bytesLeft, samples);
    cLog::log (LOGINFO, "br:%d ch:%d rate:%d p:%d f:%d sbr:%d tns:%d pns:%d frame:%d",
      aacDecoder.bitRate, aacDecoder.nChans, aacDecoder.sampRate, aacDecoder.profile,
      aacDecoder.format, aacDecoder.sbrEnabled, aacDecoder.tnsUsed, aacDecoder.pnsUsed, aacDecoder.frameCount);

    //cLog::log (LOGINFO, "frameLen %d %d %d", int(srcPtr - streamBuf), (int)bytesLeft, frameLen);
    audio.audPlay (2, samples, 1024, 1.f);
    }

  audio.audClose();

  free (samples);

  UnmapViewOfFile (streamBuf);
  CloseHandle (fileHandle);

  CoUninitialize();
  }
