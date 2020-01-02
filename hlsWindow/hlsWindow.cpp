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
//}}}

class cAppWindow : public cHls, public cD2dWindow {
public:
  cAppWindow (int chan, int bitrate) : cHls(chan, bitrate, getDaylightSeconds()) {}
  //{{{
  void run (const std::string& title, int width, int height) {

    initialise (title, width, height, false);
    add (new cCalendarBox (this, 190.f,160.f, mTimePoint), -190.f - 24.f,0);
    add (new cHlsDotsBox (this, 18.f,60.f, this), -24.f, 0);
    add (new cHlsPeakBox (this, 0,0, this));

    add (new cLogBox (this, 200.f,0, true), 0,200.f)->setPin (false);

    add (new cBmpBox (this, 60.f, 60.f, r1x80, 1, mChan, mChanChanged));
    add (new cBmpBox (this, 60.f, 60.f, r2x80, 2, mChan, mChanChanged), 61.f,0);
    add (new cBmpBox (this, 60.f, 60.f, r3x80, 3, mChan, mChanChanged), 122.f,0);
    add (new cBmpBox (this, 60.f, 60.f, r4x80, 4, mChan, mChanChanged), 183.f,0);
    add (new cBmpBox (this, 60.f, 60.f, r5x80, 5, mChan, mChanChanged), 244.f,0);
    add (new cBmpBox (this, 60.f, 60.f, r6x80, 6, mChan, mChanChanged), 305.f,0);
    //add (new cClockBox (this, 40.f, mTimePoint), -82.f,-82.f);

    mVolumeBox = new cVolumeBox (this, 12.f,0, nullptr);
    add (mVolumeBox, -12.f,0);
    add (new cWindowBox (this, 60.f,24.f), -60.f,0);

    // launch loaderThread
    std::thread ([=]() {
      CoInitializeEx (NULL, COINIT_MULTITHREADED);
      cWinSockHttp http;
      loader (http);
      CoUninitialize();
      }).detach();

    // launch playerThread, high priority
    auto playerThread = std::thread ([=]() {
      CoInitializeEx (NULL, COINIT_MULTITHREADED);
      cWinAudio audio (2, 48000);
      mVolumeBox->setAudio (&audio);
      player (audio, this);
      CoUninitialize();
      });
    SetThreadPriority (playerThread.native_handle(), THREAD_PRIORITY_HIGHEST);
    playerThread.detach();

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
  WSADATA wsaData;
  if (WSAStartup (MAKEWORD(2,2), &wsaData))
    exit (0);

  cLog::init (LOGINFO1, true);
  cLog::log (LOGNOTICE, "hlsWindow");

  cAppWindow appWindow (kDefaultChan, kDefaultBitrate);
  appWindow.run ("hlsWindow", 600, 340);

  CoUninitialize();
  return 0;
  }
