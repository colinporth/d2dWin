// tvWindow.cpp
//{{{  includes
#include "stdafx.h"
#include "../../shared/utils/resolve.h"

#include "../../shared/utils/cFileList.h"

#include "../boxes/cClockBox.h"
#include "../boxes/cFileListBox.h"
#include "../boxes/cLogBox.h"
#include "../boxes/cWindowBox.h"

#include "cPlayView.h"

using namespace std;
//}}}
const string kTvRoot = "/tv";

class cAppWindow : public cD2dWindow {
public:
  //{{{
  void run (string title, int width, int height, const string& root) {

    init (title, width, height, false);
    add (new cClockBox (this, 40.f), -84.f,2.f);

    // fileList
    mFileList = new cFileList (root.empty() ? kTvRoot : root, "*.ts");
    thread([=]() { mFileList->watchThread(); }).detach();

    add (new cFileListBox (this, 0.f,0.f, mFileList, [&](cFileListBox* box, int index) { selectFileItem(); }));

    // launch file player
    if (!mFileList->empty())
      mPlayFocus = addFront (new cPlayView (this, 0.f,0.f, mFileList->getCurFileItem().getFullName()));

    add (new cLogBox (this, 20.f));
    add (new cWindowBox (this, 60.f,24.f), -60.f,0.f)->setPin (false);

    // loop till exit
    messagePump();

    // cleanup
    delete mFileList;
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
  cFileList* mFileList = nullptr;

  cBox* mPlayFocus;
  //}}}
  };

// main
int __stdcall WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::init (LOGINFO, true, "", "tvWindow"); // false, "C:/Users/colin/Desktop");

  int numArgs;
  auto args = CommandLineToArgvW (GetCommandLineW(), &numArgs);

  cAppWindow appWindow;
  appWindow.run ("tvWindow", 1920/2, 1080/2, (numArgs > 1) ? wcharToString (args[1]) : "");

  CoUninitialize();
  }
