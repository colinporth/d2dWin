// tvWindow.cpp
//{{{  includes
#include "stdafx.h"

#include "../../shared/dvb/cWinDvb.h"
#include "../../shared/utils/cFileList.h"

#include "../boxes/cClockBox.h"
#include "../boxes/cFileListBox.h"
#include "../boxes/cIntBox.h"
#include "../boxes/cDvbBox.h"
#include "../boxes/cLogBox.h"
#include "../boxes/cWindowBox.h"

#include "cPlayView.h"
//}}}

class cAppWindow : public cD2dWindow {
public:
  //{{{
  void run (string title, int width, int height, const string& rootOrFrequency) {

    initialise (title, width, height, false);
    add (new cLogBox (this, 200.f,-200.f, true), 0.f,-200.f);
    add (new cClockBox (this, 40.f, mTimePoint), -84.f,2.f);

    int frequency = atoi (rootOrFrequency.c_str());
    if (frequency) {
      mDvb = new cDvb (mTvRoot);
      if (mDvb->createGraph (frequency * 1000)) {
        thread ([=]() { mDvb->grabThread(); }).detach();
        thread ([=]() { mDvb->signalThread(); }).detach();
        add (new cDvbBox (this, 480.f,0.f, mDvb))->setTimedOn();
        }
      }

    // fileList
    mFileList = new cFileList (frequency || rootOrFrequency.empty() ? mTvRoot : rootOrFrequency, "*.ts");
    thread([=]() { mFileList->watchThread(); }).detach();
    auto boxWidth = frequency ? 480.f : 0.f;
    add (new cAppFileListBox (this, -boxWidth,0.f, mFileList), frequency ? boxWidth : 0.f, 0.f);

    // launch file player
    if (!mFileList->empty())
      mPlayFocus = addFront (new cPlayView (this, 0.f,0.f, mFileList->getCurFileItem().getFullName()));

    add (new cWindowBox (this, 60.f,24.f), -60.f,0.f);

    // loop till quit
    messagePump();
    }
  //}}}

protected:
  //{{{
  bool onKey (int key) {

    switch (key) {
      case 0x00: break;
      case  'F': toggleFullScreen(); break ;
      //{{{
      case 0x1B: // escape - exit
        if (mPlayFocus) {
          //{{{  exit playView
          removeBox (mPlayFocus);
          delete mPlayFocus;
          mPlayFocus = nullptr;
          }
          //}}}
        else
          return true;
        break;
      //}}}

      case 0x26: if (mFileList->prevIndex()) changed(); break; // up arrow - prev file
      case 0x28: if (mFileList->nextIndex()) changed(); break; // down arrow - next file
      case 0x0d: selectFileItem(); break; // enter - play file

      default: if (mPlayFocus) mPlayFocus->onKey (key);
      }

    return false;
    }
  //}}}

private:
  //{{{
  class cAppFileListBox : public cFileListBox {
  public:
    cAppFileListBox (cD2dWindow* window, float width, float height, cFileList* fileList) :
      cFileListBox (window, width, height, fileList) {}

    void onHit() { (dynamic_cast<cAppWindow*>(getWindow()))->selectFileItem(); }
    };
  //}}}
  //{{{
  void selectFileItem() {

    if (mPlayFocus) {
      removeBox (mPlayFocus);
      delete mPlayFocus;
      }

    if (!mFileList->empty()) {
      mPlayFocus = new cPlayView (this, 0.f,0.f, mFileList->getCurFileItem().getFullName());
      addFront (mPlayFocus, 0.f,0.f);
      }
    }
  //}}}
  //{{{  vars
  string mTvRoot = "/tv";
  cDvb* mDvb = nullptr;
  cFileList* mFileList;
  cBox* mPlayFocus;
  //}}}
  };

//{{{
int __stdcall WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::init (LOGINFO, true); // false, "C:/Users/colin/Desktop");

  int numArgs;
  auto args = CommandLineToArgvW (GetCommandLineW(), &numArgs);

  string rootOrFrequency;
  if (numArgs > 1) {
    // get fileName from commandLine
    wstring wstr (args[1]);
    rootOrFrequency = string (wstr.begin(), wstr.end());
    }

  cAppWindow appWindow;
  appWindow.run ("tvWindow", 1920/2, 1080/2, rootOrFrequency);

  CoUninitialize();
  }
//}}}
