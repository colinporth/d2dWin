// tvGrab.cpp - minimal console dvb grab
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

#include "../common/cDvb.h"

#pragma comment(lib,"common.lib")

using namespace std;
//}}}

//{{{
void dvbGrabThread (cDvb* dvb, cTransportStream* ts) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::setThreadName ("grab");

  int64_t streamPos = 0;
  while (true) {
    auto blockSize = 0;
    auto ptr = dvb->getBlock (blockSize);
    if (blockSize) {
      streamPos += ts->demux (ptr, blockSize, streamPos, false, -1);
      dvb->releaseBlock (blockSize);
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
void dvbSignalThread (cDvb* dvb, cTransportStream* ts) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::setThreadName ("sign");

  while (true)
    cLog::log (LOGINFO, dec(dvb->getSignal()) +
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
    auto mDvb = new cDvb ("c:/tv");
    mDvb->createGraph (frequency * 1000, fileName + "/tune.ts");
    cLog::log (LOGERROR, "not finished");

    //if (mDvb->run()) {
    //  cLog::log (LOGNOTICE, "created dvb- dump - tuned to " + dec(frequency) + "mhz");
    //  thread ([=]() { dvbSignalThread (mDvb, nullptr); }).detach();
    //  }
    //else
    //  cLog::log (LOGERROR, "failed to create dvb graph");
    }
    //}}}
  else {
    //{{{  use sampleGrabber filter
    auto mTs = new cDumpTransportStream (fileName, true);
    cLog::log (LOGNOTICE, "Created dumpTransportStream");

    auto mDvb = new cDvb("c:/tv");
    mDvb->createGraph (frequency * 1000);
    cLog::log (LOGERROR, "not finished");
    //if (mDvb->run()) {
    //  cLog::log (LOGNOTICE, "Created dvb- sampleGrabber - tuned to " + dec(frequency) + "mhz");
    //  thread ([=]() { dvbGrabThread (mDvb, mTs); }).detach();
    //  thread ([=]() { dvbSignalThread (mDvb, mTs); }).detach();
    //  }
    //else
    //  cLog::log (LOGERROR, "failed to create dvb graph");
    }
    //}}}

  while (true)
    Sleep (1000);

  CoUninitialize();
  }
