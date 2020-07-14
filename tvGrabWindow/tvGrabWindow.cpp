// tvGrabWindow.cpp
//{{{  includes
#include "stdafx.h"

#include "../../shared/dvb/cDvb.h"

#include "../boxes/cTitleBox.h"
#include "../boxes/cTsEpgBox.h"
#include "../boxes/cTsPidBox.h"

#include "../boxes/cLogBox.h"
#include "../boxes/cWindowBox.h"

using namespace std;
//}}}

class cAppWindow : public cD2dWindow {
public:
  //{{{
  void run (const string& title, int width, int height, const string& param, const string& rootName) {

    init (title, width, height, false);
    setChangeCountDown (4);

    auto frequency = param.empty() ? 626 : atoi (param.c_str());
    if (frequency) {
      mDvb = new cDvb (frequency * 1000, rootName, false, "all", "");
      add (new cTsEpgBox (this, getHeight()/2.f, 0.f, mDvb), 0.f,0.f);
      add (new cTsPidBox (this, -getHeight()/2.f,0.f, mDvb), getHeight()/2.f,0.f);

      add (new cTitleBox (this, 60.f,24.f, mDvb->mErrorStr), -200.f,0.f);
      add (new cTitleBox (this, 60.f,24.f, mDvb->mSignalStr), -130.f,0.f);

      thread ([=]() {
        //{{{  grabthread
        CoInitializeEx (NULL, COINIT_MULTITHREADED);
        mDvb->grabThread();
        CoUninitialize();
        //}}}
        }).detach();
      }

    else {
      mDumpTs = new cDumpTransportStream (rootName, false);
      add (new cTsEpgBox (this, getHeight()/2.f, 0.f, mDumpTs), 0.f,0.f);
      add (new cTsPidBox (this, -getHeight()/2.f, 0.f, mDumpTs), getHeight()/2.f,0.f);
      thread ([=]() { fileThread (param, mDumpTs); }).detach();
      }

    add (new cLogBox (this, 20.f));
    add (new cWindowBox (this, 60.f,24.f), -60.f,0.f)->setPin (false);

    messagePump();

    delete mDvb;
    delete mDumpTs;
    }
  //}}}

protected:
  //{{{
  bool onKey (int key) {

    switch (key) {
      case 0x00 : break;
      case 0x1B : return true;

      default   : printf ("key %x\n", key);
      }
    return false;
    }
  //}}}

private:
  //{{{
  void fileThread (const string& fileName, cDumpTransportStream* ts) {

    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::setThreadName ("file");

    auto fileHandle = CreateFile (fileName.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (fileHandle != INVALID_HANDLE_VALUE) {
      auto fileMappingHandle = CreateFileMapping (fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
      auto streamBuf = (uint8_t*)MapViewOfFile (fileMappingHandle, FILE_MAP_READ, 0, 0, 0);
      int64_t streamSize;
      GetFileSizeEx (fileHandle, (PLARGE_INTEGER)(&streamSize));

      int64_t streamPos = 0;
      int64_t chunkSize = 240*188*4;
      auto streamPtr = streamBuf;
      while (streamPos < streamSize) {
        if (streamSize - streamPos < chunkSize)
          chunkSize = streamSize - streamPos;
        streamPos += ts->demux (streamPtr, chunkSize, streamPos, false, -1);
        streamPtr += chunkSize;
        changed();
        //Sleep (10);
        }

      cLog::log (LOGINFO, "%d of %d", streamPos, streamSize);
      changed();

      UnmapViewOfFile (streamBuf);
      CloseHandle (fileMappingHandle);
      CloseHandle (fileHandle);
      }
    else
      cLog::log (LOGERROR, "CreateFile - failed " + fileName);

    cLog::log (LOGINFO, "exit");
    CoUninitialize();
    }
  //}}}

  // vars
  cDvb* mDvb = nullptr;
  cDumpTransportStream* mDumpTs = nullptr;
  };


// main
int __stdcall WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);

  cLog::init (LOGINFO, true);

  int numArgs;
  auto args = CommandLineToArgvW (GetCommandLineW(), &numArgs);

  string param = (numArgs > 1) ? wcharToString (args[1]) : "";

  cAppWindow appWindow;
  string rootName = "/tv";
  appWindow.run ("tvGrabWindow", 1050, 900, param, rootName);

  CoUninitialize();
  }
