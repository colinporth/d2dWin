// tvGrabWindow.cpp
//{{{  includes
#include "stdafx.h"

#include "../common/cBda.h"
#include "../../shared/decoders/cTransportStream.h"

#include "../common/cTransportStreamBox.h"
#include "../common/cFloatBox.h"
#include "../common/cLogBox.h"
#include "../common/cWindowBox.h"

using namespace std;
//}}}
const int kDefaultFrequency = 706000;

class cAppWindow : public cD2dWindow {
public:
  //{{{
  void run (const string& title, int width, int height, const string& param) {

    mTs = new cTransportStream();

    initialise (title, width, height, false);
    addBox (new cTransportStreamBox (this, 0,-200, mTs));
    addBox (new cLogBox (this, 200.f,0, true), 0,200.f);
    addBox (new cWindowBox (this, 60.f,24.f), -60.f,0.f);

    auto frequency = param.empty() ? kDefaultFrequency : atoi(param.c_str());
    if (frequency)
      thread ([=]() { bdaThread (frequency); }).detach();
    else
      thread ([=]() { fileThread (param); }).detach();

    messagePump();
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
  void bdaThread (int frequency) {

    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::log (LOGNOTICE, "bdaThread - start");

    auto bda = new cBda (128*240*188);

    // *** possible problem with box def here ***
    addBox (new cFloatBox (this, 150.f, kTextHeight, "strength ", bda->mSignalStrength), 0.f,-kTextHeight);

    char fileName[100];
    sprintf (fileName, "c:/videos/tune.ts");
    auto file = CreateFile (fileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);

    bda->createGraph (frequency);
    cLog::log (LOGNOTICE, "tvGrabWindow %d", frequency);

    int64_t streamPos = 0;
    auto blockSize = 0;
    while (true) {
      auto ptr = bda->getContiguousBlock (blockSize);
      if (blockSize) {
        auto bytesUsed = mTs->demux (ptr, blockSize, streamPos, false, -1);
        streamPos += bytesUsed;
        changed();

        DWORD numBytesUsed;
        WriteFile (file, ptr, blockSize, &numBytesUsed, NULL);
        if (numBytesUsed != blockSize)
          cLog::log (LOGERROR, "WriteFile only used " + dec(numBytesUsed) + " of " + dec(blockSize));

        bda->decommitBlock (blockSize);
        }
      else
        Sleep (1);
      }

    cLog::log (LOGNOTICE, "bdaThread - exit");
    CoUninitialize();
    }
  //}}}
  //{{{
  void fileThread (const string& fileName) {

    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::log (LOGNOTICE, "fileThread - start");

    auto fileHandle = CreateFile (fileName.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (fileHandle != INVALID_HANDLE_VALUE) {
      auto fileMappingHandle = CreateFileMapping (fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
      auto streamBuf = (uint8_t*)MapViewOfFile (fileMappingHandle, FILE_MAP_READ, 0, 0, 0);
      int64_t streamSize;
      GetFileSizeEx (fileHandle, (PLARGE_INTEGER)(&streamSize));

      setChangeCountDown (20);
      auto analyseStreamPos = mTs->demux (streamBuf, streamSize, 0, false, -1);
      cLog::log (LOGINFO, "analyseThread %d of %d", analyseStreamPos, streamSize);
      changed();

      UnmapViewOfFile (streamBuf);
      CloseHandle (fileMappingHandle);
      CloseHandle (fileHandle);
      }
    else
      cLog::log (LOGERROR, "CreateFile - failed " + fileName);

    cLog::log (LOGNOTICE, "fileThread - exit");
    CoUninitialize();
    }
  //}}}

  string mFileName;
  cTransportStream* mTs = nullptr;
  };

//{{{
int __stdcall WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::init ("tvGrabWindow", LOGINFO1, true);

  string param;
  int numArgs;
  auto args = CommandLineToArgvW (GetCommandLineW(), &numArgs);
  if (numArgs > 1) {
    // get fileName from commandLine
    wstring wstr (args[1]);
    param = string (wstr.begin(), wstr.end());
    }

  cAppWindow appWindow;
  appWindow.run ("tvGrabWindow", 1050, 900, param);
  CoUninitialize();
  }
//}}}
