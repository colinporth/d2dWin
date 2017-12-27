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

#include "../common/box/cValueBox.h"
#include "../common/cFloatBox.h"
#include "../common/cLogBox.h"
#include "../common/cWindowBox.h"
#include "../common/cVolumeBox.h"
#include "../common/box/cBmpBox.h"
#include "../common/cClockBox.h"
#include "../common/box/cDateBox.h"
#include "../common/cCalendarBox.h"
#include "../common/box/cHlsPeakBox.h"
#include "../common/box/cHlsDotsBox.h"

//using namespace chrono;
//}}}

class cAppWindow : public cHls, public cD2dWindow, public cWinAudio {
public:
  cAppWindow() : cHls(4, kDefaultBitrate, getDaylightSeconds()) {}
  //{{{
  void run (const string& title, int width, int height, int chan) {

    cLog::log (LOGINFO, "hlsWindow " + dec(chan) + " " + dec(kDefaultBitrate));

    initialise (title, width, height, false);
    add (new cCalendarBox (this, 190.f,160.f, mTimePoint), -190.f - 24.f,0);
    add (new cHlsDotsBox (this, 18.f,60.f, this), -24.f, 0);
    add (new cHlsPeakBox (this, 0,0, this));

    add (new cLogBox (this, 200.f,0, true), 0,200.f);

    add (new cBmpBox (this, 60.f, 60.f, r1x80, 1, mChan, mChanChanged));
    add (new cBmpBox (this, 60.f, 60.f, r2x80, 2, mChan, mChanChanged), 61.f,0);
    add (new cBmpBox (this, 60.f, 60.f, r3x80, 3, mChan, mChanChanged), 122.f,0);
    add (new cBmpBox (this, 60.f, 60.f, r4x80, 4, mChan, mChanChanged), 183.f,0);
    add (new cBmpBox (this, 60.f, 60.f, r5x80, 5, mChan, mChanChanged), 244.f,0);
    add (new cBmpBox (this, 60.f, 60.f, r6x80, 6, mChan, mChanChanged), 305.f,0);
    //add (new cClockBox (this, 40.f, mTimePoint), -82.f,-82.f);

    add (new cVolumeBox (this, 12.f,0), -12.f,0);
    add (new cFloatBox (this, 50.f,20.f, mRenderTime), 0,-20.f);
    add (new cWindowBox (this, 60.f,24.f), -60.f,0);

    // launch loaderThread
    thread ([=]() {
      //{{{  loader
      CoInitializeEx (NULL, COINIT_MULTITHREADED);
      cWinSockHttp http;
      loader (http);
      CoUninitialize();
      }
      //}}}
      ).detach();

    // launch playerThread, high priority
    auto playerThread = thread ([=]() {
      //{{{  player
      CoInitializeEx (NULL, COINIT_MULTITHREADED);
      player (this, this);
      CoUninitialize();
      }
      //}}}
      );
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

      case  ' ': togglePlay(); break;

      case 0x21:
        //{{{  page up
         {
          incPlaySeconds (-60);
          mLoadSem.notify();
          changed();
          }
        break;
        //}}}
      case 0x22:
        //{{{  page down
         {
          incPlaySeconds (60);
          mLoadSem.notify();
          changed();
          }
        break;
        //}}}
      case 0x25:
       //{{{  left arrow
        {
         incPlaySeconds (-1);
         mLoadSem.notify();
         changed();
         }
       break;
       //}}}
      case 0x27:
        //{{{  right arrow
         {
          incPlaySeconds (1);
          mLoadSem.notify();
          changed();
          }
        break;
        //}}}
      case 0x26:
        //{{{  up arrow
        if (mChan > 1) {
          mChan--;
          mChanChanged = true;
          }
        break;
        //}}}
      case 0x28:
        //{{{  down arrow
        if (mChan < 6) {
          mChan++;
          mChanChanged = true;
          }
        break;
        //}}}
      case 0x23: break; // home
      case 0x24: break; // end

      case  '1':
      case  '2':
      case  '3':
      case  '4':
      case  '5':
      case  '6': mChan = key - '0'; mChanChanged = true; break;

      case  'F': toggleFullScreen(); break;

      default  : printf ("key %x\n", key);
      }

    return false;
    }
  //}}}
  };

//{{{
int __stdcall WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::init (LOGINFO1, true);

  int numArgs;
  auto args = CommandLineToArgvW (GetCommandLineW(), &numArgs);
  if (numArgs > 1) {
    // get fileName from commandLine
    wstring wstr (args[1]);
    auto fileName = string (wstr.begin(), wstr.end());
    }
  int chan = 4;

  WSADATA wsaData;
  if (WSAStartup (MAKEWORD(2,2), &wsaData)) {
    //{{{  error exit
    cLog::log (LOGERROR, "WSAStartup failed");
    exit (0);
    }
    //}}}

  cAppWindow appWindow;
  appWindow.run ("hlsWindow", 600, 340, chan);

  CoUninitialize();
  }
//}}}
