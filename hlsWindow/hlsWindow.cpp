// hlsWindow.cpp
//{{{  includes
#include "stdafx.h"

#include <winsock2.h>
#include <WS2tcpip.h>
#pragma comment(lib,"ws2_32.lib")

#include "../../shared/net/cWinSockHttp.h"
#include "../../shared/utils/cWinAudio.h"
#include "../../shared/hls/hls.h"

#include "../../shared/hls/r1x80.h"
#include "../../shared/hls/r2x80.h"
#include "../../shared/hls/r3x80.h"
#include "../../shared/hls/r4x80.h"
#include "../../shared/hls/r5x80.h"
#include "../../shared/hls/r6x80.h"

#include "../boxes/cValueBox.h"
#include "../boxes/cFloatBox.h"
#include "../boxes/cLogBox.h"
#include "../boxes/cWindowBox.h"
#include "../boxes/cVolumeBox.h"
#include "../boxes/cBmpBox.h"
#include "../boxes/cClockBox.h"
#include "../boxes/cDateBox.h"
#include "../boxes/cCalendarBox.h"
#include "../boxes/cHlsPeakBox.h"
#include "../boxes/cHlsDotsBox.h"

using namespace std;
//}}}

class cAppWindow : public cHls, public cD2dWindow {
public:
  cAppWindow (int chan, int bitrate) : cHls(chan, bitrate, getDayLightSeconds()) {}
  //{{{
  void run (const string& title, int width, int height) {

    init (title, width, height, false);
    add (new cCalendarBox (this, 190.f,160.f), -190.f - 24.f,0);
    add (new cHlsDotsBox (this, 18.f,60.f, this), -24.f, 0);
    add (new cHlsPeakBox (this, 0,0, this));

    add (new cLogBox (this, 20.f));
    //{{{  add chan1 to 6 bmp boxes
    add (new cBmpBox (this, 60.f, 60.f, r1x80, [&](cBox* box) mutable noexcept {
      mChan = 1;
      mChanChanged = true;
      } ));
    addRight (new cBmpBox (this, 60.f, 60.f, r2x80, [&](cBox* box) mutable noexcept {
      mChan = 2;
      mChanChanged = true;
      } ));
    addRight (new cBmpBox (this, 60.f, 60.f, r3x80, [&](cBox* box) mutable noexcept {
      mChan = 3;
      mChanChanged = true;
      } ));
    addRight (new cBmpBox (this, 60.f, 60.f, r4x80, [&](cBox* box) mutable noexcept {
      mChan = 4;
      mChanChanged = true;
      } ));
    addRight (new cBmpBox (this, 60.f, 60.f, r5x80, [&](cBox* box) mutable noexcept {
      mChan = 5;
      mChanChanged = true;
      } ));
    addRight (new cBmpBox (this, 60.f, 60.f, r6x80, [&](cBox* box) mutable noexcept {
      mChan = 6;
      mChanChanged = true;
      } ));
    //}}}
    //add (new cClockBox (this, 40.f, mTimePoint), -82.f,-82.f);

    mVolumeBox = new cVolumeBox (this, 12.f,0, nullptr);
    add (mVolumeBox, -12.f,0);
    add (new cWindowBox (this, 60.f,24.f), -60.f,0);

    // launch loaderThread
    thread ([=]() {
      CoInitializeEx (NULL, COINIT_MULTITHREADED);
      cWinSockHttp http;
      loader (http);
      CoUninitialize();
      }).detach();

    // launch playerThread, high priority
    thread ([=]() {
      CoInitializeEx (NULL, COINIT_MULTITHREADED);
      SetThreadPriority (GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
      cWinAudio audio (2, 48000);
      mVolumeBox->setAudio (&audio);
      player (audio, this);
      CoUninitialize();
      }).detach();

    // loop till quit
    messagePump();
    }
  //}}}

protected:
  //{{{
  bool onKey (int key) {

    switch (key) {
      case 0x00: break;
      case 0x1B: return true;
      case  'F': toggleFullScreen(); break;

      case  ' ': togglePlay(); break;

      case 0x21: incPlaySeconds (-60); mLoadSem.notify(); changed(); break; // page up
      case 0x22: incPlaySeconds (+60); mLoadSem.notify(); changed(); break; // page down
      case 0x25: incPlaySeconds (-1); mLoadSem.notify(); changed(); break;  // left arrow
      case 0x27: incPlaySeconds (+1); mLoadSem.notify(); changed(); break;  // right arrow

      case 0x26: if (mChan > 1) { mChan--; mChanChanged = true; } break;    // up arrow
      case 0x28: if (mChan < 6) { mChan++; mChanChanged = true; } break;    // down arrow

      case  '1':
      case  '2':
      case  '3':
      case  '4':
      case  '5':
      case  '6': mChan = key - '0'; mChanChanged = true; break;

      default  : printf ("key %x\n", key);
      }

    return false;
    }
  //}}}

private:
  cVolumeBox* mVolumeBox;
  };


int __stdcall WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);

  cLog::init (LOGINFO1, true);
  cLog::log (LOGNOTICE, "hlsWindow");

  cAppWindow appWindow (kDefaultChan, kDefaultBitrate);
  appWindow.run ("hlsWindow", 600, 340);

  CoUninitialize();
  return 0;
  }
