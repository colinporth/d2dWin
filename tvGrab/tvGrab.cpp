// tvGrab.cpp - minimal console bda grab
//{{{  includes
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <wrl.h>

#include <string>
#include <thread>
#include <mutex>

#include "../../shared/utils/utils.h"
#include "../../shared/utils/cLog.h"
#include "../../shared/utils/cSemaphore.h"

#include "../../shared/dvb/cDumpTransportStream.h"

#include "../common/cBda.h"

#pragma comment(lib,"common.lib")

using namespace std;
//}}}

//{{{
void bdaGrabThread (cBda* bda, cTransportStream* ts) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::setThreadName ("grab");

  int64_t streamPos = 0;
  while (true) {
    auto blockSize = 0;
    auto ptr = bda->getBlock (blockSize);
    if (blockSize) {
      streamPos += ts->demux (ptr, blockSize, streamPos, false, -1);
      bda->releaseBlock (blockSize);
      if (blockSize != 240 * 188)
        cLog::log (LOGINFO, "blocksize " + dec(blockSize));
      }
    else
      Sleep (4);
    }

  cLog::log (LOGNOTICE, "exit");
  CoUninitialize();
  }
//}}}
//{{{
void bdaSignalThread (cBda* bda, cTransportStream* ts) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::setThreadName ("sign");

  while (true)
    cLog::log (LOGINFO, dec(bda->getSignal()) +
                        (ts ? (" " + dec(ts->getDiscontinuity()) +
                               " " + dec(ts->getPackets())) : ""));

  cLog::log (LOGNOTICE, "exit");
  CoUninitialize();
  }
//}}}

int main (int argc, char* argv[]) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::init (LOGINFO, false);

  // params, defaults
  int frequency =   (argc >= 2) ? atoi(argv[1]) : 674;
  string fileName = (argc >= 3) ? argv[2] : "c:/tv";
  bool dumpFilter = (argc >= 4);

  if (dumpFilter) {
    //{{{  use dump filter
    auto mBda = new cBda();
    mBda->createGraph (frequency * 1000, fileName + "/tune.ts");
    if (mBda->run()) {
      cLog::log (LOGNOTICE, "created bda- dump - tuned to " + dec(frequency) + "mhz");
      thread ([=]() { bdaSignalThread (mBda, nullptr); }).detach();
      }
    else
      cLog::log (LOGERROR, "failed to create bda graph");
    }
    //}}}
  else {
    //{{{  use sampleGrabber filter
    auto mTs = new cDumpTransportStream (fileName, true);
    cLog::log (LOGNOTICE, "Created dumpTransportStream");

    auto mBda = new cBda();
    mBda->createGraph (frequency * 1000);
    if (mBda->run()) {
      cLog::log (LOGNOTICE, "Created bda- sampleGrabber - tuned to " + dec(frequency) + "mhz");
      thread ([=]() { bdaGrabThread (mBda, mTs); }).detach();
      thread ([=]() { bdaSignalThread (mBda, mTs); }).detach();
      }
    else
      cLog::log (LOGERROR, "failed to create bda graph");
    }
    //}}}

  while (true)
    Sleep (1000);

  CoUninitialize();
  }
