// tvWindow.cpp
//{{{  includes
#include "stdafx.h"

#include "../../shared/dvb/cDumpTransportStream.h"
#include "../common/cDvb.h"

#include "../common/box/cTsEpgBox.h"
#include "../common/box/cToggleBox.h"
#include "../common/box/cIntBox.h"
#include "../common/box/cValueBox.h"
#include "../common/box/cLogBox.h"
#include "../common/box/cWindowBox.h"
#include "../common/box/cListBox.h"

#include "cPlayView.h"
//}}}

class cAppWindow : public cD2dWindow {
public:
  //{{{
  void run (string title, int width, int height, const string& param) {

    // init d2dWindow, boxes
    initialise (title, width, height, false);
    add (new cLogBox (this, 200.f,-200.f, true), 0.f,-200.f);
    //add (new cFramesDebugBox (this, 0.f,getHeight()/4.f), 0.f,kTextHeight);

    // launch file player
    thread threadHandle;
    int frequency = atoi (param.c_str());
    if (param.empty() || frequency) {
      mFileList = getFiles (mRoot, "*.ts");
      add (new cListBox (this, getWidth()/2.f, 0.f, mFileList, mFileIndex, mFileIndexChanged));
      thread ([=]() { fileWatchThread(); }).detach();
      threadHandle = thread ([=](){ fileSelectThread(); });
      SetThreadPriority (threadHandle.native_handle(), THREAD_PRIORITY_BELOW_NORMAL);
      threadHandle.detach();
      }
    else {
      mPlayFocus = new cPlayView (this, 0.f,0.f, param);
      addFront (mPlayFocus, 0.f, 0.f);
      }

    if (frequency) {
      // launch dvb
      mDvb = new cDvb();
      if (mDvb->createGraph (frequency * 1000)) {
        mDvbTs = new cDumpTransportStream (mRoot, false);

        add (new cIntBgndBox (this, 120.f, kTextHeight, "signal ", mSignal), -120.f, 0.f);
        add (new cUInt64BgndBox (this, 120.f, kTextHeight, "pkt ", mDvbTs->mPackets), -242.f,0.f);
        add (new cUInt64BgndBox (this, 120.f, kTextHeight, "dis ", mDvbTs->mDiscontinuity), -364.f,0.f);
        add (new cTsEpgBox (this, getWidth()/2.f,0.f, mDvbTs), getWidth()/2.f,0.f)->setTimedOn();

        thread ([=]() { dvbGrabThread (mDvb); }).detach();
        thread ([=]() { dvbSignalThread (mDvb); }).detach();
        }
      }

    // exit, maximise box
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
      case 0x1B: return true;
      case  'F': toggleFullScreen(); break ;
      //{{{
      case 0x26: // up arrow - prev file
        if (!mFileList.empty() && (mFileIndex > 0)) {
          mFileIndex = mFileIndex--;
          changed();
          }
        break;
      //}}}
      //{{{
      case 0x28: // down arrow - next file
        if (!mFileList.empty() && mFileIndex < mFileList.size()-1) {
          mFileIndex = mFileIndex++;
          changed();
          }
        break;
      //}}}
      //{{{
      case 0x0d: // enter - play file
        if (!mFileList.empty()) {
          cLog::log (LOGINFO, "enter key");
          mFileIndexChanged = true;
          }
        break;
      //}}}
      default: if (mPlayFocus) mPlayFocus->onKey (key);
      }

    return false;
    }
  //}}}

private:
  //{{{
  void fileWatchThread() {

    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::setThreadName ("watc");

    // Watch the directory for file creation and deletion.
    HANDLE dwChangeHandle = FindFirstChangeNotification (
      mRoot.c_str(), TRUE, FILE_NOTIFY_CHANGE_FILE_NAME| FILE_NOTIFY_CHANGE_DIR_NAME);
    if (dwChangeHandle == INVALID_HANDLE_VALUE)
     cLog::log (LOGERROR, "FindFirstChangeNotification function failed");

    while (!getExit()) {
      cLog::log (LOGINFO, "Waiting for notification");
      if (WaitForSingleObject (dwChangeHandle, INFINITE) == WAIT_OBJECT_0) {
        // A file was created, renamed, or deleted in the directory.
        mFileList = getFiles (mRoot, "*.ts");
        cLog::log (LOGINFO, "fileWatch changed " + dec(mFileList.size()));
        if (FindNextChangeNotification (dwChangeHandle) == FALSE)
          cLog::log (LOGERROR, "FindNextChangeNotification function failed");
        }
      else
        cLog::log (LOGERROR, "No changes in the timeout period");
      }

    cLog::log (LOGNOTICE, "exit");
    CoUninitialize();
    }
  //}}}
  //{{{
  void fileSelectThread() {

    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::setThreadName ("file");

    while (!getExit()) {
      mFileIndexChanged = false;

      if (mFileList.empty())
        Sleep (100);
      else {
        auto playerView = new cPlayView (this, 0.f,0.f, mFileList[mFileIndex]);
        addFront (playerView, 0.f, 0.f);
        mPlayFocus = playerView;

        while (!mFileIndexChanged)
          Sleep (1);

        mPlayFocus = nullptr;
        removeBox (playerView);
        delete playerView;
        }
      }

    cLog::log (LOGNOTICE, "exit");
    CoUninitialize();
    }
  //}}}

  //{{{
  void dvbGrabThread (cDvb* dvb) {

    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::setThreadName ("grab");

    dvb->run();

    int64_t streamPos = 0;
    auto blockSize = 0;
    while (true) {
      auto ptr = dvb->getBlock (blockSize);
      if (blockSize) {
        streamPos += mDvbTs->demux (ptr, blockSize, streamPos, false, -1);
        dvb->releaseBlock (blockSize);
        changed();
        }
      else
        Sleep (1);
      }

    cLog::log (LOGNOTICE, "exit");
    CoUninitialize();
    }
  //}}}
  //{{{
  void dvbSignalThread (cDvb* dvb) {

    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::setThreadName ("sign");

    while (true) {
      mSignal = dvb->getSignal();
      Sleep (100);
      }

    cLog::log (LOGNOTICE, "exit");
    CoUninitialize();
    }
  //}}}

  //{{{  vars
  string mRoot = "c:/tv";

  cPlayView* mPlayFocus;

  vector <string> mFileList;
  int mFileIndex = 0;
  bool mFileIndexChanged = false;

  cDumpTransportStream* mDvbTs;
  cDvb* mDvb = nullptr;
  int mSignal = 0;
  //}}}
  };

//{{{
int __stdcall WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);

  cLog::init (LOGINFO, true);
  //cLog::init (LOGINFO, false, "C:/Users/colin/Desktop");

  string param;
  int numArgs;
  auto args = CommandLineToArgvW (GetCommandLineW(), &numArgs);
  if (numArgs > 1) {
    // get fileName from commandLine
    wstring wstr (args[1]);
    param = string (wstr.begin(), wstr.end());
    }

  cAppWindow appWindow;
  appWindow.run ("tvWindow", 1920/2, 1080/2, param);

  CoUninitialize();
  }
//}}}
