// pdfWindow.cpp
//{{{  includes
#include "stdafx.h"

#include "../../shared/utils/resolve.h"

// mupdf
#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include "../boxes/cFloatBox.h"
#include "../boxes/cLogBox.h"
#include "../boxes/cWindowBox.h"
#include "../boxes/cClockBox.h"
#include "../boxes/cCalendarBox.h"

using namespace concurrency;
//}}}
//{{{  const
const int kFullScreen = false;
const int kThumbThreads = 2;
//}}}

class cAppWindow : public cD2dWindow {
public:
  cAppWindow() : mFileScannedSem("fileScanned") {}
  //{{{
  void run (const string& title, int width, int height, string name) {

    initialise (title, width, height, kFullScreen);
    add (new cClockBox (this, 50.f, mTimePoint), -110.f,-120.f);
    add (new cCalendarBox (this, 190.f,150.f, mTimePoint), -190.f-120.f,-150.f);
    add (new cLogBox (this, 200.f,0.f, true), -200.f,0);

    //add (new cImageSetView (this, 0.f,0.f, mImageSet));
    //mJpegImageView = new cJpegImageView (this, 0.f,0.f, nullptr);
    //add (mJpegImageView);

    add (new cWindowBox (this, 60.f,24.f), -60.f,0.f);
    add (new cFloatBox (this, 50.f, kLineHeight, mRenderTime), 0.f,-kLineHeight);

    if (name.find (".lnk") <= name.size()) {
      string fullName;
      if (resolveShortcut (name.c_str(), fullName))
        name = fullName;
      }
    //thread ([=]() { filesThread (name); } ).detach();

    //for (auto i = 0; i < kThumbThreads; i++)
    //  thread ([=]() { thumbsThread (i); } ).detach();

    messagePump();
    };
  //}}}

protected:
  //{{{
  bool onKey (int key) {

    switch (key) {
      case 0x00: break;
      case 0x10: changed(); break; // shift
      case 0x11: break; // control

      case 0x1B: return true; // escape abort

      case  ' ': break; // space bar

      case 0x21: break; // page up
      case 0x22: break; // page down

      case 0x23: changed(); break;   // end
      case 0x24: changed();  break; // home

      case 0x25: changed();  break;    // left arrow
      case 0x26: changed();  break; // up arrow
      case 0x27: changed();  break;    // right arrow
      case 0x28: changed();  break; // down arrow

      case 'F':  toggleFullScreen(); break;

      default: cLog::log (LOGERROR, "unused key %x", key);
      }

    return false;
    }
  //}}}
  //{{{
  bool onKeyUp (int key) {

    switch (key) {
      case 0x10: changed(); break; // shift
      default: break;
      }

    return false;
    }
  //}}}

private:
  // vars
  cSemaphore mFileScannedSem;
  };

//{{{
int __stdcall WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::init (LOGINFO1, true);

  int numArgs;
  auto args = CommandLineToArgvW (GetCommandLineW(), &numArgs);
  string rootDirName;
  if (numArgs > 1) {
    wstring wstr(args[1]);
    rootDirName = string(wstr.begin(), wstr.end());
    cLog::log (LOGINFO, "pdfWindow resolved " + rootDirName);
    }
  else
    rootDirName = "C:/Users/colin/Pictures";

  cAppWindow window;
  window.run ("pdfWindow", 1920/2, 1080/2, rootDirName);

  CoUninitialize();
  }
//}}}
