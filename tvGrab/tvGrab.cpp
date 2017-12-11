// tvGrab.cpp - minimal console bda grab
//{{{  includes
#include "stdafx.h"

#include "../common/cBda.h"
#include "../../shared/decoders/cTransportStream.h"

using namespace std;
//}}}
const bool kDump = false;

//{{{
class cDumpTransportStream : public cTransportStream {
public:
  //{{{
  cDumpTransportStream() {
    memset (ts,0,188);
    }
  //}}}
  virtual ~cDumpTransportStream() {}

protected:
  void startProgram (int vidPid, int audPid, const string& name, time_t startTime) {

    cLog::log (LOGNOTICE, "startProgram " + dec(vidPid) + " " + dec(audPid) + " " + name);
    if (vidPid == 101 && audPid == 102) {
      mVidPid = vidPid;
      mAudPid = audPid;
      mProgFile = CreateFile ("c:/videos/prog.ts",
                              GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
      writePat (mProgFile, 0x1234, 1, 32); // tsid, sid, pgmPid
      writePmt (mProgFile, 1, 32); // sid, pgmPid
      }
    }

  void packet (int pid, uint8_t* tsPtr) {
    if ((pid == mVidPid) || (pid == mAudPid)) {
      DWORD numBytesUsed;
      WriteFile (mProgFile, tsPtr, 188, &numBytesUsed, NULL);
      if (numBytesUsed != 188)
        cLog::log (LOGERROR, "writePacket " + dec(numBytesUsed));
      }
    }

private:
  //{{{
  void writePat (HANDLE file, int transportStreamId, int serviceId, int pgmPid) {

    cLog::log (LOGINFO, "writePat");

    int pid = PID_PAT;
    int continuityCount = 0;

    // tsHeader
    auto tsPtr = ts;
    *tsPtr++ = 0x47; // sync byte
    *tsPtr++ = 0x40 | ((pid & 0x1f00) >> 8);           // payload_unit_start_indicator + pid upper
    *tsPtr++ = pid & 0xff;                             // pid lower
    *tsPtr++ = 0x10 | continuityCount;                 // no adaptControls + cont count
    *tsPtr++ = 0;                                      // pointerField

    // tsSection start
    auto sectionLength = 5+4 + 4;
    *tsPtr++ = TID_PAT;                                // PAT tid
    *tsPtr++ = 0xb0 | ((sectionLength & 0x0F00) >> 8); // Pat sectionLength upper
    *tsPtr++ = sectionLength & 0x0FF;                  // Pat sectionLength lower

    // sectionLength from here to end of crc
    *tsPtr++ = (transportStreamId & 0xFF00) >> 8;      // transportStreamId
    *tsPtr++ = transportStreamId & 0x00FF;
    *tsPtr++ = 0xc1;                                   // For simplicity, we'll have a version_id of 0
    *tsPtr++ = 0x00;                                   // First section number = 0
    *tsPtr++ = 0x00;                                   // last section number = 0

    *tsPtr++ = (serviceId & 0xFF00) >> 8;              // first section serviceId
    *tsPtr++ = serviceId & 0x00FF;
    *tsPtr++ = 0xE0 | ((pgmPid & 0x1F00) >> 8);       // first section pgmPid
    *tsPtr++ = pgmPid & 0x00FF;
    // tsSection end

    // tsSection crc, calc from tsSection start to here
    auto crc32 = crc32Block (0xffffffff, ts+5, 3 + 5+4);
    *tsPtr++ = (crc32 & 0xff000000) >> 24;
    *tsPtr++ = (crc32 & 0x00ff0000) >> 16;
    *tsPtr++ = (crc32 & 0x0000ff00) >>  8;
    *tsPtr++ = crc32 & 0x000000ff;

    // write
    DWORD numBytesUsed;
    WriteFile (file, ts, 188, &numBytesUsed, NULL);
    if (numBytesUsed != 188)
      cLog::log (LOGERROR, "writePat " + dec(numBytesUsed));
    }
  //}}}
  //{{{
  void writePmt (HANDLE file, int serviceId, int pgmPid) {

    cLog::log (LOGINFO, "writePmt");

    int pid = pgmPid;
    int continuityCount = 0;

    int pcrPid = 101;
    int vidPid = 101;
    int vidStreamType = 2;
    int audPid = 102;
    int audStreamType = 3;
    int esInfoLen = 0;

    auto tsPtr = ts;
    *tsPtr++ = 0x47; // sync byte
    *tsPtr++ = 0x40 | ((pid & 0x1f00) >> 8);        // payload_unit_start_indicator + pid upper
    *tsPtr++ = pid & 0xff;                          // pid lower
    *tsPtr++ = 0x10 | continuityCount;                 // no adaptControls + cont count
    *tsPtr++ = 0;                                      // pointerField

    int sectionLength = 23; // 5+4 + program_info_length + esStreams * (5 + ES_info_length) + 4
    *tsPtr++ = TID_PMT;
    *tsPtr++ = 0xb0 | ((sectionLength & 0x0F00) >> 8);
    *tsPtr++ = sectionLength & 0x0FF;

    *tsPtr++ = (serviceId & 0xFF00) >> 8;
    *tsPtr++ = serviceId & 0x00FF;
    *tsPtr++ = 0xc1; // version_id of 0
    *tsPtr++ = 0x00; // first section number = 0
    *tsPtr++ = 0x00; // last section number = 0

    *tsPtr++ = 0xE0 | ((pcrPid & 0x1F00) >> 8);
    *tsPtr++ = pcrPid & 0x00FF;

    *tsPtr++ = 0xF0;
    *tsPtr++ = 0; // program_info_length;

    // vid es
    *tsPtr++ = vidStreamType; // elementary stream_type
    *tsPtr++ = 0xE0 | ((vidPid & 0x1F00) >> 8); // elementary_PID
    *tsPtr++ = vidPid & 0x00FF;
    *tsPtr++ = ((esInfoLen & 0xFF00) >> 8) | 0xF0;  // ES_info_length
    *tsPtr++ = esInfoLen & 0x00FF;

    // aud es
    *tsPtr++ = audStreamType; // elementary stream_type
    *tsPtr++ = 0xE0 | ((audPid & 0x1F00) >> 8); // elementary_PID
    *tsPtr++ = audPid & 0x00FF;
    *tsPtr++ = ((esInfoLen & 0xFF00) >> 8) | 0xF0;  // ES_info_length
    *tsPtr++ = esInfoLen & 0x00FF;

    auto crc32 = crc32Block (0xffffffff, ts+5, 27-5);
    *tsPtr++ = (crc32 & 0xff000000) >> 24;
    *tsPtr++ = (crc32 & 0x00ff0000) >> 16;
    *tsPtr++ = (crc32 & 0x0000ff00) >>  8;
    *tsPtr++ = crc32 & 0x000000ff;

    DWORD numBytesUsed;
    WriteFile (file, ts, 188, &numBytesUsed, NULL);
    if (numBytesUsed != 188)
      cLog::log (LOGERROR, "writePat " + dec(numBytesUsed));
    }
  //}}}

  int mVidPid = -1;
  int mAudPid = -1;

  uint8_t ts[188];
  HANDLE mProgFile = 0;
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

      DWORD numBytesUsed;
      WriteFile (file, ptr, blockSize, &numBytesUsed, NULL);
      if (numBytesUsed != blockSize)
        cLog::log (LOGERROR, "WriteFile only used " + dec(numBytesUsed) + " of " + dec(blockSize));
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

  auto frequency = (argc >= 2) ? 674 : atoi(argv[1]);
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
    auto mFile = CreateFile (mFileName.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    cLog::log (LOGNOTICE, "Created file " + mFileName);

    auto mTs = new cDumpTransportStream();
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
