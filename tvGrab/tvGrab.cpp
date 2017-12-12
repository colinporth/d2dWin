// tvGrab.cpp - minimal console bda grab
//{{{  includes
#include "stdafx.h"

#include "../common/cBda.h"
#include "../../shared/decoders/cTransportStream.h"
#include "../../shared/decoders/cDumpTransportStream.h"

using namespace std;
//}}}

//{{{
void bdaGrabThread (cBda* bda, cTransportStream* ts) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::log (LOGNOTICE, "bdaGrabThread - start");

  int64_t streamPos = 0;
  while (true) {
    auto blockSize = 0;
    auto ptr = bda->getContiguousBlock (blockSize);
    if (blockSize) {
      auto bytesUsed = ts->demux (ptr, blockSize, streamPos, false, -1);
      streamPos += bytesUsed;
      bda->decommitBlock (blockSize);
      if (blockSize != 240 * 188)
        cLog::log (LOGINFO, "blocksize " + dec(blockSize,6));
      }
    else
      Sleep (4);
    }

  cLog::log (LOGNOTICE, "bdaGrabThread - exit");
  CoUninitialize();
  }
//}}}
//{{{
void bdaSignalThread (cBda* bda, cTransportStream* ts) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::log (LOGNOTICE, "bdaSignalThread - start");

  while (true)
    cLog::log (LOGINFO, "signal " + dec(bda->getSignalStrength()) +
                        (ts ? (" " + dec(ts->getDiscontinuity()) + " " + dec(ts->getPackets())) : ""));

  cLog::log (LOGNOTICE, "bdaSignalThread - exit");
  CoUninitialize();
  }
//}}}

int main (int argc, char* argv[]) {

  auto hr = CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::init ("tvGrab", LOGINFO, false);
  if (hr != S_OK)
    cLog::log (LOGERROR, "CoInitializeEx " + dec(hr));

  auto frequency = (argc >= 2) ? atoi(argv[1]) : 674;
  string fileName = (argc >= 3) ? argv[2] : "e:/tv";
  bool dumpFilter = (argc >= 4);

  if (dumpFilter) {
    //{{{  use dump.ax filter
    auto mBda = new cBda();
    if (mBda->createGraph (frequency * 1000, fileName + "/tune.ts")) {
      cLog::log (LOGNOTICE, "created bda- dump - tuned to " + dec(frequency) + "mhz");
      thread ([=]() { bdaSignalThread (mBda, nullptr); }).detach();
      }
    else
      cLog::log (LOGERROR, "failed to create bda graph");
    }
    //}}}
  else {
    //{{{  sampleGrabber
    auto mTs = new cDumpTransportStream (fileName);
    cLog::log (LOGNOTICE, "Created dumpTransportStream");

    auto mBda = new cBda();
    if (mBda->createGraph (frequency * 1000)) {
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
