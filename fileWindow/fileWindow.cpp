// fileWindow.cpp
//{{{  includes
#include "stdafx.h"

#include <winsock2.h>
#include <WS2tcpip.h>
#pragma comment(lib,"ws2_32.lib")

#include "../../shared/net/cWinSockHttp.h"
#include "../../shared/utils/date.h"
#include "../../shared/utils/format.h"

#include "../common/cJpegImage.h"

#include "../common/box/cCalendarBox.h"
#include "../common/box/cClockBox.h"

#include "../common/box/cLogBox.h"
#include "../common/cJpegImageView.h"

#include "../common/box/cValueBox.h"
#include "../common/box/cFloatBox.h"
#include "../common/box/cWindowBox.h"

using namespace chrono;
using namespace concurrency;
//}}}

class cAppWindow : public cD2dWindow {
public:
  //{{{
  void run (const string& title, int width, int height, const string& fileName) {

    initialise (title, width, height, false);

    string pathName = "";
    mImage.setFile (pathName, fileName);
    mImage.loadInfo();

    add (new cCalendarBox (this, 190.f,150.f, mTimePoint), -190.f,0.f);
    add (new cClockBox (this, 40.f, mTimePoint), -82.f,150.f);

    add (new cLogBox (this, 200.f,0.f, true), -200.f,0.f);
    add (new cJpegImageView (this, 0.f,0.f, &mImage));
    add (new cJpegImageView (this, 0.f,0.f, &mCompressImage));

    add (new cValueBox (this, 100.f,kTextHeight, "quality", 1.f,100.f, mQuality, mQualityChanged), 0.f,-kTextHeight);
    add (new cFloatBox (this, 50.f,kTextHeight, mRenderTime), -50.f,-kTextHeight);
    add (new cWindowBox (this, 60.f,24.f), -60.f,0);

    thread([=]() { changeThread(); }).detach();

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

      case  ' ': {
        compressAndLoad();
        break;
        }
      case 0x21: break; // page up
      case 0x22: break; // page down
      case 0x25: mQuality -= 1.f; mQualityChanged = true; break; // left arrow
      case 0x27: mQuality += 1.f; mQualityChanged = true; break; // right arrow
      case 0x26: mQuality -= 5.f; mQualityChanged = true; break; // up arrow
      case 0x28: mQuality += 5.f; mQualityChanged = true; break; // down arrow

      case  'F': toggleFullScreen(); break;
      default  : printf ("key %x\n", key);
      }

    return false;
    }
  //}}}
private:
  //{{{
  void compressAndLoad() {

    int bufLen;
    auto buf = mImage.compressImage (bufLen, mQuality);
    mCompressImage.setBuf (buf, bufLen);
    mCompressImage.loadInfo();
    mCompressImage.releaseImage();

    changed();
    }
  //}}}
  //{{{
  void changeThread() {

    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::log (LOGNOTICE, "changeThread - start");

    while (true) {
      if (mQualityChanged) {
        mQualityChanged = false;
        compressAndLoad();
        }
      else
        Sleep (100);
      }

    cLog::log (LOGNOTICE, "changeThread - exit");
    CoUninitialize();
    }
  //}}}

  cJpegImage mImage;
  cJpegImage mCompressImage;

  float mQuality = 70.f;
  bool mQualityChanged = false;
  };

//{{{
int __stdcall WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::init (LOGINFO3, true);

  int numArgs;
  auto args = CommandLineToArgvW (GetCommandLineW(), &numArgs);

  string arg;
  if (numArgs > 1) {
    // get fileName from commandLine
    wstring wstr (args[1]);
    arg = string (wstr.begin(), wstr.end());
    }
  cLog::log (LOGNOTICE, "arg - " + arg);

  WSADATA wsaData;
  if (WSAStartup (MAKEWORD(2,2), &wsaData)) {
    //{{{  error exit
    cLog::log (LOGERROR, "WSAStartup failed");
    exit (0);
    }
    //}}}

  cAppWindow appWindow;
  appWindow.run ("fileWindow", 1920/2, 600, arg);

  CoUninitialize();
  }
//}}}
