// tvGrab.cpp - simplest posible console bda grab
//{{{  includes
#include "stdafx.h"

#include "../common/cBda.h"
#include "../../shared/decoders/cTransportStream.h"

using namespace std;
//}}}

//{{{
void bdaCaptureThread (cBda* bda, cTransportStream* ts, HANDLE file) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::log (LOGNOTICE, "bdaCaptureThread - start");

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

  cLog::log (LOGNOTICE, "bdaCaptureThread - exit");
  CoUninitialize();
  }
//}}}
//{{{
void bdaStrengthDiscontinuityThread (cBda* bda, cTransportStream* ts) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::log (LOGNOTICE, "bdaStrengthDiscontinuityThread - start");

  float lastStrength = 0.f;
  int64_t lastDiscontinuity = 0;
  while (true) {
    auto strength = bda->getSignalStrength();
    auto discontinuity = ts->getDiscontinuity();
    if ((abs(strength - lastStrength) > 2.f) || (discontinuity != lastDiscontinuity)) {
      cLog::log (LOGINFO, "strength " + dec(strength) + " " + dec(discontinuity));
      lastStrength = strength;
      lastDiscontinuity = discontinuity;
      }
    Sleep (200);
    }

  cLog::log (LOGNOTICE, "bdaStrengthDiscontinuityThread - exit");
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

int main(int argc, char** argv) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::init ("tvGrab", LOGINFO, false);

  auto frequency = argc == 1 ? 674 : atoi(argv[1]);
  string mFileName = "c:/videos/tune.ts";

  if (true) {
    auto mBda = new cBda();
    mBda->createGraph (frequency * 1000, mFileName);
    cLog::log (LOGNOTICE, "created bda- dump - tuned to " + dec(frequency) + "mhz");

    thread ([=]() { bdaStrengthThread (mBda); }).detach();

    while (true) {
      Sleep (200);
      }
    }

  else {
    auto mFile = CreateFile (mFileName.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    cLog::log (LOGNOTICE, "created file " + mFileName);

    auto mTs = new cTransportStream();
    cLog::log (LOGNOTICE, "created ts");

    auto mBda = new cBda();
    mBda->createGraph (frequency * 1000);
    cLog::log (LOGNOTICE, "created bda- sampleGrabber - tuned to " + dec(frequency) + "mhz");

    thread ([=]() { bdaCaptureThread (mBda, mTs, mFile); }).detach();
    thread ([=]() { bdaStrengthDiscontinuityThread (mBda, mTs); }).detach();

    while (true) {
      Sleep (200);
      }
    }

  CoUninitialize();
  }
