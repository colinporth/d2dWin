 // hlsWindow.cpp
//{{{  includes
#define _CRT_SECURE_NO_WARNINGS
#define _SILENCE_CXX17_UNCAUGHT_EXCEPTION_DEPRECATION_WARNING
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <wrl.h>

#include <d3d11.h>
#include <d3d11_1.h>
#include <d2d1.h>
#include <d2d1_1.h>
#include <d2d1helper.h>
#include <DXGI1_2.h>
#include <dwrite.h>

#include <shellapi.h>

#include "../common/cD2dWindow.h"
#include "../boxes/cClockBox.h"
#include "../boxes/cSongBox.h"
#include "../boxes/cLogBox.h"
#include "../boxes/cWindowBox.h"

#include "../../shared/utils/cVideoDecode.h"
#include "../../shared/utils/cLoaderPlayer.h"

using namespace std;
using namespace chrono;
//}}}
//{{{  channels
constexpr int kDefaultChannelNum = 3;
constexpr int kAudBitrate = 128000; //  96000  128000
constexpr int kVidBitrate = 827008; // 827008 1604032 2812032 5070016

const string kHost = "vs-hls-uk-live.akamaized.net";
const vector <string> kChannels = { "bbc_one_hd",          "bbc_two_hd",          "bbc_four_hd", // pa4
                                    "bbc_news_channel_hd", "bbc_one_scotland_hd", "s4cpbs",      // pa4
                                    "bbc_one_south_west",  "bbc_parliament" };                   // pa3
//}}}

//{{{
class cVideoDecodeBox : public cD2dWindow::cView {
public:
  cVideoDecodeBox (cD2dWindow* window, float width, float height, cVideoDecode* videoDecode)
    : cView("videoDecode", window, width, height), mVideoDecode(videoDecode) {}
  virtual ~cVideoDecodeBox() {}

  void onDraw (ID2D1DeviceContext* dc) {

    auto frame = mVideoDecode->findPlayFrame();
    if (frame) {
      if (frame->getPts() != mPts) { // new Frame, update bitmap
        mPts = frame->getPts();
        if (mBitmap)  {
          auto pixelSize = mBitmap->GetPixelSize();
          if ((pixelSize.width != mVideoDecode->getWidth()) || (pixelSize.height != mVideoDecode->getHeight())) {
            // bitmap size changed, remove and recreate
            mBitmap->Release();
            mBitmap = nullptr;
            }
          }

        if (!mBitmap)
          dc->CreateBitmap (D2D1::SizeU(mVideoDecode->getWidth(), mVideoDecode->getHeight()),
                            { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE, 0,0 },
                            &mBitmap);
        mBitmap->CopyFromMemory (&D2D1::RectU(0,0, mVideoDecode->getWidth(),mVideoDecode->getHeight()),
                                 frame->get32(), mVideoDecode->getWidth() * 4);
        }
      }

    if (mBitmap) {
      dc->SetTransform (mView2d.mTransform);
      dc->DrawBitmap (mBitmap, cRect(getSize()));
      dc->SetTransform (D2D1::Matrix3x2F::Identity());
      }

    // info string
    string str = dec(mVideoDecode->getWidth()) +
                 "x" + dec(mVideoDecode->getHeight()) +
                 " " + dec(mVideoDecode->getFramePoolSize());

    IDWriteTextLayout* textLayout;
    mWindow->getDwriteFactory()->CreateTextLayout (
      std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
      mWindow->getTextFormat(), getWidth(), getHeight(), &textLayout);
    dc->DrawTextLayout (getTL(), textLayout, mWindow->getWhiteBrush());
    textLayout->Release();
    }

private:
  cVideoDecode* mVideoDecode;
  ID2D1Bitmap* mBitmap = nullptr;
  uint64_t mPts = 0;
  };
//}}}

class cAppWindow : public cD2dWindow, public cLoaderPlayer {
public:
  //{{{
  void run (const string& title, int width, int height, int channelNum, int audBitrate, int vidBitrate) {

    cD2dWindow::init (title, width, height, false);
    setChangeCountDown (0); // refresh evry frame

    cLoaderPlayer::initialise (false, kHost, "pool_902/live/uk/", kChannels[channelNum],
                               audBitrate, vidBitrate,
                               true, true, true, true); // use mfx decoder
    if (getVideoDecode())
      add (new cVideoDecodeBox (this, 0.f,0.f, getVideoDecode()), 0.f,0.f);
    add (new cClockBox (this, 40.f), -135.f,35.f);
    add (new cSongBox (this, 0.f,0.f, mSong));
    mLogBox = add (new cLogBox (this, 20.f));
    add (new cWindowBox (this, 60.f,24.f), -60.f,0.f)->setPin (false);

    // startup
    thread ([=](){ hlsLoaderThread(); }).detach();

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

      case 'F' : toggleFullScreen(); break;
      case 'L' : mLogBox->togglePin(); break;
      case ' ' : mPlaying = !mPlaying; break;

      //{{{
      case 'M':  // mark
        mSong->getSelect().addMark (mSong->getPlayFrame());
        changed();
        break;

      //}}}
      //{{{
      case 0x21: {// page up - back one hour
        mSong->incPlaySec (-60*60, false);
        auto framePtr = mSong->getAudioFramePtr(mSong->getPlayFrame());
        if (framePtr) {
          getVideoDecode()->setPlayPts (framePtr->getPts());
          getVideoDecode()->clear(framePtr->getPts());
          }
        changed();
        break;
        }
      //}}}
      //{{{
      case 0x22: {// page down - forward one hour
        mSong->incPlaySec (60*60, false);
        auto framePtr = mSong->getAudioFramePtr(mSong->getPlayFrame());
        if (framePtr) {
          getVideoDecode()->setPlayPts(framePtr->getPts());
          getVideoDecode()->clear(framePtr->getPts());
          }
        changed();
        break;
        }
      //}}}
      //{{{
      case 0x25: {// left  arrow -1s, ctrl -10s, shift -5m
        mSong->incPlaySec (-(getShift() ? 300 : getControl() ? 10 : 1), false);
        auto framePtr = mSong->getAudioFramePtr(mSong->getPlayFrame());
        if (framePtr) {
          getVideoDecode()->setPlayPts(framePtr->getPts());
          getVideoDecode()->clear(framePtr->getPts());
          }
        changed();
        break;
        }
      //}}}
      //{{{
      case 0x27: {// right arrow +1s, ctrl +10s, shift +5m
        mSong->incPlaySec (getShift() ? 300 : getControl() ?  10 :  1, false);
        auto framePtr = mSong->getAudioFramePtr(mSong->getPlayFrame());
        if (framePtr) {
          getVideoDecode()->setPlayPts(framePtr->getPts());
          getVideoDecode()->clear(framePtr->getPts());
        }
        changed();
        break;
        }
      //}}}
      //{{{
      case 0x24: // home
        mSong->setPlayFrame (
        mSong->getSelect().empty() ? mSong->getFirstFrame() : mSong->getSelect().getFirstFrame());
        changed();
        break;
      //}}}
      //{{{
      case 0x23: // end
        mSong->setPlayFrame (
        mSong->getSelect().empty() ? mSong->getLastFrame() : mSong->getSelect().getLastFrame());
        changed();
        break;
      //}}}
      //{{{
      case 0x2e: // delete select
        mSong->getSelect().clearAll(); changed();
        break;
      //}}}
      //{{{
      default:
        cLog::log (LOGINFO, "key %x", key);
        changed();
        break;
      //}}}
      }

    return false;
    }
  //}}}
  cBox* mLogBox = nullptr;
  };

// main
int WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

  cLog::init (LOGINFO, true, "", "hlsWindow");

  int numArgs;
  auto args = CommandLineToArgvW (GetCommandLineW(), &numArgs);
  vector<string> names;
  for (int i = 1; i < numArgs; i++)
    names.push_back (wcharToString (args[i]));

  int channelNum = names.empty() ? kDefaultChannelNum : stoi (names[0]);

  cAppWindow appWindow;
  appWindow.run ("hlsWindow", 800, 450, channelNum, kAudBitrate, kVidBitrate);
  return 0;
  }
