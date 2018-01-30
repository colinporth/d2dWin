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

    int frequency = atoi (param.c_str());
    if (frequency) {
      // launch dvb
      mDvb = new cDvb();
      if (mDvb->createGraph (frequency * 1000)) {
        mDvbTs = new cDumpTransportStream (mRoot, false);

        add (new cIntBgndBox (this, 120.f, kTextHeight, "signal ", mSignal), -120.f, 0.f);
        add (new cUInt64BgndBox (this, 120.f, kTextHeight, "pkt ", mDvbTs->mPackets), -242.f,0.f);
        add (new cUInt64BgndBox (this, 120.f, kTextHeight, "dis ", mDvbTs->mDiscontinuity), -364.f,0.f);
        add (new cTsEpgBox (this, getWidth()/2.f,0.f, mDvbTs))->setTimedOn();

        thread ([=]() { dvbGrabThread (mDvb); }).detach();
        thread ([=]() { dvbSignalThread (mDvb); }).detach();
        }
      }

    // launch file player
    mFileList = getFiles (frequency || param.empty() ? mRoot : param, "*.ts");
    add (new cListBox (this, frequency ? -getWidth()/2.f : 0.f, 0.f, mFileList, mFileIndex, mFileIndexChanged),
         frequency ? getWidth()/2.f : 0.f);
    thread ([=]() { fileWatchThread(); }).detach();

    if (!mFileList.empty())
      mPlayFocus = addFront (new cPlayView (this, 0.f,0.f, mFileList[mFileIndex]), 0.f,0.f);

    thread threadHandle = thread ([=](){ fileSelectThread(); });
    SetThreadPriority (threadHandle.native_handle(), THREAD_PRIORITY_BELOW_NORMAL);
    threadHandle.detach();

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
          if (mPlayFocus) {
            removeBox (mPlayFocus);
            delete mPlayFocus;
            mPlayFocus = nullptr;
            }

          mPlayFocus = new cPlayView (this, 0.f,0.f, mFileList[mFileIndex]);
          addFront (mPlayFocus, 0.f, 0.f);
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
      while (!mFileIndexChanged)
        Sleep (1);

      if (mFileList.empty())
        Sleep (100);
      else {
        if (mPlayFocus) {
          removeBox (mPlayFocus);
          delete mPlayFocus;
          mPlayFocus = nullptr;
          }

        mPlayFocus = new cPlayView (this, 0.f,0.f, mFileList[mFileIndex]);
        addFront (mPlayFocus, 0.f, 0.f);
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

  cBox* mPlayFocus;

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
