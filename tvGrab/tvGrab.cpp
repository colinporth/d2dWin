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

#include "../../shared/dvb/cWinDvb.h"

#pragma comment(lib,"common.lib")

using namespace std;
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
    if (mDvb->createGraph (frequency * 1000, fileName + "/tune.ts")) {
      cLog::log (LOGNOTICE, "created dvb- dump - tuned to " + dec(frequency) + "mhz");
      thread ([=]() { mDvb->signalThread(); }).detach();
      }
    else
      cLog::log (LOGERROR, "failed to create dvb graph");
    }
    //}}}
  else {
    //{{{  use sampleGrabber filter
    auto mTs = new cDumpTransportStream (fileName, true);
    cLog::log (LOGNOTICE, "Created dumpTransportStream");

    auto mDvb = new cDvb("c:/tv");
    if (mDvb->createGraph (frequency * 1000)) {
      cLog::log (LOGNOTICE, "Created dvb- sampleGrabber - tuned to " + dec(frequency) + "mhz");
      thread ([=]() { mDvb->grabThread(); }).detach();
      thread ([=]() { mDvb->signalThread(); }).detach();
      }
    else
      cLog::log (LOGERROR, "failed to create dvb graph");
    }
    //}}}

  while (true)
    Sleep (1000);

  CoUninitialize();
  }
