// tvGrab.cpp - minimal console bda grab
//{{{  includes
#include "stdafx.h"

#include "../common/cBda.h"
#include "../../shared/decoders/cTransportStream.h"

using namespace std;
//}}}
const bool kDump = false;
const bool kGrabAll = false;

//{{{
class cDumpTransportStream : public cTransportStream {
public:
  cDumpTransportStream (const string& rootName) : mRootName(rootName){}
  virtual ~cDumpTransportStream() {}

protected:
  //{{{
  void startProgram (cService* service, const string& name, time_t startTime) {

    cLog::log (LOGNOTICE, "startProgram " + name);

    auto recordFileIt = mRecordFileMap.find (service->getSid());
    if (recordFileIt == mRecordFileMap.end()) // create new cRecordFile for this cService
      recordFileIt = mRecordFileMap.insert (
        map<int,cRecordFile>::value_type (
          service->getSid(), cRecordFile (service->getVidPid(), service->getAudPid()))).first;

    auto validFileName = validString (service->getNameString() + " - " + name, "<>:\\|?*""/");
    recordFileIt->second.createFile (mRootName + "/" + validFileName + ".ts", service);
    }
  //}}}
  //{{{
  void pesPacket (cPidInfo* pidInfo, uint8_t* ts) {
  // save pes packet

    auto recordFileIt = mRecordFileMap.find (pidInfo->mSid);
    if (recordFileIt != mRecordFileMap.end())
      recordFileIt->second.writePes (pidInfo->mPid, ts);
    }
  //}}}

private:
  string mRootName;
  map<int,cRecordFile> mRecordFileMap;
  };
//}}}

//{{{
void bdaGrabThread (cBda* bda, cTransportStream* ts, HANDLE file) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::log (LOGNOTICE, "bdaGrabThread - start");

  int64_t streamPos = 0;
  while (true) {
    auto blockSize = 0;
    auto ptr = bda->getContiguousBlock (blockSize);
    if (blockSize) {
      auto bytesUsed = ts->demux (ptr, blockSize, streamPos, false, -1);
      streamPos += bytesUsed;

      if (kGrabAll) {
        DWORD numBytesUsed;
        WriteFile (file, ptr, blockSize, &numBytesUsed, NULL);
        if (numBytesUsed != blockSize)
          cLog::log (LOGERROR, "WriteFile only used " + dec(numBytesUsed) + " of " + dec(blockSize));
        }

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
void bdaQualityThread (cBda* bda, cTransportStream* ts) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::log (LOGNOTICE, "bdaQualityThread - start");

  int lastStrength = 0;
  int64_t lastDiscontinuity = 0;
  while (true) {
    auto strength = bda->getSignalStrength();
    auto discontinuity = ts->getDiscontinuity();
    auto packets = ts->getPackets();
    if ((abs(strength - lastStrength) > 2.f) || (discontinuity != lastDiscontinuity)) {
      cLog::log (LOGINFO, "strength " + dec(strength) +
                          " " + dec(discontinuity) +
                          " " + dec(packets));
      lastStrength = strength;
      lastDiscontinuity = discontinuity;
      }
    Sleep (200);
    }

  cLog::log (LOGNOTICE, "bdaQualityThread - exit");
  CoUninitialize();
  }
//}}}
//{{{
void bdaStrengthThread (cBda* bda) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::log (LOGNOTICE, "bdaStrengthThread - start");

  float lastStrength = 0.f;
  while (true) {
    auto strength = bda->getSignalStrength();
    cLog::log (LOGINFO, "strength " + dec(strength));
    Sleep (100);
    }

  cLog::log (LOGNOTICE, "bdaStrengthThread - exit");
  CoUninitialize();
  }
//}}}

int main (int argc, char* argv[]) {

  auto hr = CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::init ("tvGrab", LOGINFO, false);
  if (hr != S_OK)
    cLog::log (LOGERROR, "CoInitializeEx " + dec(hr));

  auto frequency = (argc >= 2) ? atoi(argv[1]) : 674;
  string mFileName = (argc >= 3) ? argv[2] : "c:/videos/tune.ts";

  if (kDump) {
    //{{{  use dump.ax filter
    auto mBda = new cBda();
    if (mBda->createGraph (frequency * 1000, mFileName)) {
      cLog::log (LOGNOTICE, "created bda- dump - tuned to " + dec(frequency) + "mhz");
      thread ([=]() { bdaStrengthThread (mBda); }).detach();
      }
    else
      cLog::log (LOGERROR, "failed to create bda graph");
    }
    //}}}
  else {
    //{{{  use sampleGrabber
    HANDLE mFile = 0;
    if (kGrabAll) {
      mFile = CreateFile (mFileName.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
      cLog::log (LOGNOTICE, "Created file " + mFileName);
      }

    auto mTs = new cDumpTransportStream ("c:/videos");
    cLog::log (LOGNOTICE, "Created ts");

    auto mBda = new cBda();
    if (mBda->createGraph (frequency * 1000)) {
      cLog::log (LOGNOTICE, "Created bda- sampleGrabber - tuned to " + dec(frequency) + "mhz");
      thread ([=]() { bdaGrabThread (mBda, mTs, mFile); }).detach();
      thread ([=]() { bdaQualityThread (mBda, mTs); }).detach();
      }
    else
      cLog::log (LOGERROR, "failed to create bda graph");
    }
    //}}}

  while (true)
    Sleep (1000);

  CoUninitialize();
  }
