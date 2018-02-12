// test.cpp - minimal console
//{{{  includes
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <wrl.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <fcntl.h>
#include <io.h>

#include <string>
#include <thread>
#include <mutex>

#include "../../shared/utils/utils.h"
#include "../../shared/utils/cLog.h"
#include "../../shared/utils/cSemaphore.h"

using namespace std;
//}}}

int main (int argc, char* argv[]) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::init (LOGINFO, false);

  cLog::log (LOGINFO, "hello colin");

// 48000*2*2 * 256 = 49.152Mhz
// - PLLI2S_VCO: VCO_344M
// - I2S_CLK = PLLI2S_VCO / PLLI2SQ = 344/7 = 49.142 Mhz
// - I2S_CLK1 = I2S_CLK / PLLI2SDIVQ = 49.142/1 = 49.142 Mhz

  float minDiff = 9990999.f;
  int minQ = 0;
  int minN = 0;
  for (auto n = 50; n < 433; n++) {
    for (auto q = 2; q < 16; q++) {
      float diff = 49.152f - (float(n) / float(q));
      if (fabs(diff) < minDiff && diff < 0.f) {
        minDiff = fabs(diff);
        minQ = q;
        minN = n;
        }
      }
    }
  cLog::log (LOGNOTICE, dec(minQ) + " " + dec(minN) + " " + frac(minDiff,6,4, ' ') + " " + frac(float(minN) / float(minQ),6,4, ' '));

  while (true)
    Sleep (1000);

  CoUninitialize();
  }
