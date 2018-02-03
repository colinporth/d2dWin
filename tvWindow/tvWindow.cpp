// tvWindow.cpp
//{{{  includes
#include "stdafx.h"

#include "../../shared/dvb/cWinDvb.h"
#include "../../shared/utils/cFileList.h"

#include "../boxes/cClockBox.h"
#include "../boxes/cFileListBox.h"
#include "../boxes/cIntBox.h"
#include "../boxes/cTsEpgBox.h"
#include "../boxes/cLogBox.h"
#include "../boxes/cWindowBox.h"

#include "cPlayView.h"
//}}}

class cAppWindow : public cD2dWindow {
public:
  //{{{
  void run (string title, int width, int height, const string& param) {

    initialise (title, width, height, false);
    add (new cLogBox (this, 200.f,-200.f, true), 0.f,-200.f);
    add (new cClockBox (this, 40.f, mTimePoint), -84.f,2.f);

    int frequency = atoi (param.c_str());
    if (frequency) {
      mDvb = new cDvb (mTvRoot);
      if (mDvb->createGraph (frequency * 1000)) {
        thread ([=]() { mDvb->grabThread(); }).detach();
        thread ([=]() { mDvb->signalThread(); }).detach();

        // turn these into a dvb monitor widget
        add (new cTuneBox (this, 40.f, kLineHeight, mDvb, 650, "itv"));
        add (new cTuneBox (this, 40.f, kLineHeight, mDvb, 674, "bbc"), 42.f,0.f);
        add (new cTuneBox (this, 40.f, kLineHeight, mDvb, 706, "hd"), 84.f,0.f);
        add (new cIntBgndBox (this, 120.f, kLineHeight, "signal ", mDvb->mSignal), 126.f,0.f);
        add (new cUInt64BgndBox (this, 120.f, kLineHeight, "pkt ", mDvb->mPackets), 248.f,0.f);
        add (new cUInt64BgndBox (this, 120.f, kLineHeight, "dis ", mDvb->mDiscontinuity), 370.f,0.f);
        add (new cTsEpgBox (this, getWidth()/2.f,-kLineHeight, mDvb), 0.f,kLineHeight)->setTimedOn();
        }
      }

    // launch file player
    mFileList = new cFileList (frequency || param.empty() ? mTvRoot : param, "*.ts");
    thread([=]() { mFileList->watchThread(); }).detach();

    auto boxWidth = frequency ? getWidth()/2.f : 0.f;
    add (new cAppFileListBox (this, -boxWidth,0.f, mFileList), boxWidth,frequency ? kLineHeight : 0.f);

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
  class cTuneBox : public cBox {
  public:
    //{{{
    cTuneBox (cD2dWindow* window, float width, float height, cDvb* dvb, int frequency, const string& title) :
        cBox ("tune", window, width, height), mDvb(dvb), mFrequency(frequency), mTitle(title) {
      mPin = true;
      }
    //}}}

    //{{{
    bool onDown (bool right, cPoint pos)  {

      mDvb->stop();
      mDvb->tune (mFrequency * 1000);
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
    cDvb* mDvb;
    string mTitle;
    int mFrequency;
    };
  //}}}
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
  cDvb* mDvb = nullptr;

  string mTvRoot = "c:/tv";
  cFileList* mFileList;

  cBox* mPlayFocus;
  //}}}
  };

// main
int __stdcall WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::init (LOGINFO, true); // false, "C:/Users/colin/Desktop");

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
