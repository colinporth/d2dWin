// tvGrabWindow.cpp
//{{{  includes
#include "stdafx.h"

#include "../common/cBda.h"

#include "../../shared/dvb/cDumpTransportStream.h"

#include "../common/box/cTransportStreamBox.h"
#include "../common/box/cIntBox.h"
#include "../common/box/cLogBox.h"
#include "../common/box/cWindowBox.h"
//}}}

class cAppWindow : public cD2dWindow {
public:
  //{{{
  class cTuneBox : public cD2dWindow::cBox {
  public:
    //{{{
    cTuneBox (cD2dWindow* window, float width, float height, const string& title, int frequency) :
        cBox ("tune", window, width, height), mTitle(title), mFrequency(frequency) {
      mPin = true;
      }
    //}}}

    //{{{
    bool onDown (bool right, cPoint pos)  {
      auto appWindow = dynamic_cast<cAppWindow*>(mWindow);
      appWindow->retune (mFrequency);
      return true;
      }
    //}}}
    //{{{
    void onDraw (ID2D1DeviceContext* dc) {

      IDWriteTextLayout* textLayout;
      mWindow->getDwriteFactory()->CreateTextLayout (
        strToWstr(mTitle).data(), (uint32_t)mTitle.size(), mWindow->getTextFormat(),
        getSize().x, getSize().y, &textLayout);

      dc->FillRectangle (mRect, mWindow->getGreyBrush());
      dc->DrawTextLayout (getTL(2.f), textLayout, mWindow->getBlackBrush());
      dc->DrawTextLayout (getTL(), textLayout, mWindow->getWhiteBrush());

      textLayout->Release();
      }
    //}}}

  private:
    string mTitle;
    int mFrequency;
    };
  //}}}
  //{{{
  void run (const string& title, int width, int height, const string& param, const string& rootName) {

    mTs = new cDumpTransportStream (rootName);

    initialise (title, width, height, false);
    add (new cTuneBox (this, 40.f, kTextHeight, "bbc", 674));
    add (new cTuneBox (this, 40.f, kTextHeight, "itv", 650), 42.f,0.f);
    add (new cTuneBox (this, 40.f, kTextHeight, "hd", 706), 84.f,0.f);
    add (new cIntBgndBox (this, 120.f, kTextHeight, "signal ", mSignal), 126.f,0.f);
    add (new cUInt64BgndBox (this, 120.f, kTextHeight, "pkt ", mTs->mPackets), 248.f,0.f);
    add (new cUInt64BgndBox (this, 120.f, kTextHeight, "dis ", mTs->mDiscontinuity), 370.f,0.f);

    add (new cTransportStreamBox (this, 0,-kTextHeight, mTs), 0.f, kTextHeight);
    add (new cLogBox (this, 200.f,0, true), 0,200.f);
    add (new cWindowBox (this, 60.f,24.f), -60.f,0.f);

    auto frequency = param.empty() ? 674 : atoi(param.c_str());
    if (frequency) {
      mBda = new cBda();
      thread ([=]() { bdaGrabThread (mBda, frequency*1000); }).detach();
      thread ([=]() { bdaSignalThread (mBda); }).detach();
      }
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
  void bdaGrabThread (cBda* bda, int frequency) {

    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::setThreadName ("grab");

    bda->createGraph (frequency);
    bda->run();

    int64_t streamPos = 0;
    auto blockSize = 0;
    while (true) {
      auto ptr = bda->getBlock (blockSize);
      if (blockSize) {
        streamPos += mTs->demux (ptr, blockSize, streamPos, false, -1);
        bda->releaseBlock (blockSize);
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
  void bdaSignalThread (cBda* bda) {

    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::setThreadName ("sign");

    while (true)
      mSignal = bda->getSignal();

    cLog::log (LOGNOTICE, "exit");
    CoUninitialize();
    }
  //}}}
  //{{{
  void fileThread (const string& fileName) {

    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::setThreadName ("file");

    auto fileHandle = CreateFile (fileName.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (fileHandle != INVALID_HANDLE_VALUE) {
      auto fileMappingHandle = CreateFileMapping (fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
      auto streamBuf = (uint8_t*)MapViewOfFile (fileMappingHandle, FILE_MAP_READ, 0, 0, 0);
      int64_t streamSize;
      GetFileSizeEx (fileHandle, (PLARGE_INTEGER)(&streamSize));

      setChangeCountDown (20);
      auto analyseStreamPos = mTs->demux (streamBuf, streamSize, 0, false, -1);
      cLog::log (LOGINFO, "%d of %d", analyseStreamPos, streamSize);
      changed();

      UnmapViewOfFile (streamBuf);
      CloseHandle (fileMappingHandle);
      CloseHandle (fileHandle);
      }
    else
      cLog::log (LOGERROR, "CreateFile - failed " + fileName);

    cLog::log (LOGNOTICE, "exit");
    CoUninitialize();
    }
  //}}}
  //{{{
  void retune (int frequency) {
    mBda->stop();
    mTs->clear();
    mBda->tune (frequency * 1000);
    mBda->run();
    }
  //}}}

  string mFileName;
  HANDLE mFile = 0;

  cTransportStream* mTs = nullptr;
  cBda* mBda = nullptr;
  int mSignal = 0;
  };

//{{{
int __stdcall WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::init (LOGINFO, true);

  string param;
  int numArgs;
  auto args = CommandLineToArgvW (GetCommandLineW(), &numArgs);
  if (numArgs > 1) {
    // get fileName from commandLine
    wstring wstr (args[1]);
    param = string (wstr.begin(), wstr.end());
    }

  cAppWindow appWindow;
  string rootName = "c:/tv";
  appWindow.run ("tvGrabWindow", 1050, 900, param, rootName);
  CoUninitialize();
  }
//}}}
