 // hlsWindow.cpp
//{{{  includes
#include "stdafx.h"
#include <shellapi.h>

#include "../../shared/utils/cSong.h"

#include "../../shared/decoders/cAudioDecode.h"
#include "../../shared/audio/audioWASAPI.h"

#include "../../shared/net/cWinSockHttp.h"

#include "../common/cD2dWindow.h"
#include "../boxes/cClockBox.h"
#include "../boxes/cSongBox.h"
#include "../boxes/cLogBox.h"
#include "../boxes/cWindowBox.h"

#include "../../shared/utils/cVideoDecode.h"

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

class cAppWindow : public cD2dWindow {
public:
  //{{{
  void run (const string& title, int width, int height, int channelNum, int audBitrate, int vidBitrate) {

    init (title, width, height, false);

    mVideoDecode = new cMfxVideoDecode();
    //mVideoDecode = new cFFmpegVideoDecode();

    add (new cVideoDecodeBox (this, 0.f,0.f, mVideoDecode), 0.f,0.f);

    add (new cClockBox (this, 40.f), -135.f,35.f);
    add (new cSongBox (this, 0.f,0.f, mSong));
    mLogBox = add (new cLogBox (this, 20.f));
    add (new cWindowBox (this, 60.f,24.f), -60.f,0.f)->setPin (false);

    // startup
    thread ([=](){ hlsThread (kHost, kChannels[channelNum], audBitrate, vidBitrate); }).detach();

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
        mSong.getSelect().addMark (mSong.getPlayFrame());
        changed();
        break;
      //}}}
      //{{{
      case 0x21: // page up - back one hour
        mSong.incPlaySec (-60*60, false);
        //mVideoDecode.setPlayPts (framePtr->getPts());
        mVideoDecode->clear();
        changed();
        break;
      //}}}
      //{{{
      case 0x22: // page down - forward one hour
        mSong.incPlaySec (60*60, false);
        //mVideoDecode.setPlayPts (framePtr->getPts());
        mVideoDecode->clear();
        changed();
        break;
      //}}}
      //{{{
      case 0x25: // left  arrow -1s, ctrl -10s, shift -5m
        mSong.incPlaySec (-(getShift() ? 300 : getControl() ? 10 : 1), false);
        //mVideoDecode.setPlayPts (framePtr->getPts());
        mVideoDecode->clear();
        changed();
        break;
      //}}}
      //{{{
      case 0x27: // right arrow +1s, ctrl +10s, shift +5m
        mSong.incPlaySec (getShift() ? 300 : getControl() ?  10 :  1, false);
        if (getShift() || getControl())
          mVideoDecode->clear();
        //mVideoDecode.setPlayPts (framePtr->getPts());
        changed();
        break;
      //}}}
      //{{{
      case 0x24: // home
        mSong.setPlayFrame (
        mSong.getSelect().empty() ? mSong.getFirstFrame() : mSong.getSelect().getFirstFrame());
        changed();
        break;
      //}}}
      //{{{
      case 0x23: // end
        mSong.setPlayFrame (
        mSong.getSelect().empty() ? mSong.getLastFrame() : mSong.getSelect().getLastFrame());
        changed();
        break;
      //}}}
      //{{{
      case 0x2e: // delete select
        mSong.getSelect().clearAll(); changed();
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

private:
  //{{{
  static uint64_t getPts (uint8_t* ts) {
  // return 33 bits of pts,dts

    if ((ts[0] & 0x01) && (ts[2] & 0x01) && (ts[4] & 0x01)) {
      // valid marker bits
      uint64_t pts = ts[0] & 0x0E;
      pts = (pts << 7) | ts[1];
      pts = (pts << 8) | (ts[2] & 0xFE);
      pts = (pts << 7) | ts[3];
      pts = (pts << 7) | (ts[4] >> 1);
      return pts;
      }
    else {
      cLog::log (LOGERROR, "getPts marker bits - %02x %02x %02x %02x 0x02", ts[0], ts[1], ts[2], ts[3], ts[4]);
      return 0;
      }
    }
  //}}}
  //{{{
  static string getTagValue (uint8_t* buffer, const char* tag) {

    const char* tagPtr = strstr ((char*)buffer, tag);
    const char* valuePtr = tagPtr + strlen (tag);
    const char* endPtr = strchr (valuePtr, '\n');

    return string (valuePtr, endPtr - valuePtr);
    }
  //}}}

  //{{{
  void hlsThread (const string& host, const string& channel, int audBitrate, int vidBitrate) {
  // hls http chunk load and decode thread

    cLog::setThreadName ("hls ");

    constexpr int kHlsPreload = 2;

    uint8_t* pesBuffer = nullptr;
    int pesBufferLen = 0;

    mSong.setChannel (channel);
    mSong.setBitrate (audBitrate, audBitrate < 128000 ? 180 : 360); // audBitrate, audioFrames per chunk

    while (!getExit()) {
      const string path = "pool_902/live/uk/" + mSong.getChannel() +
                          "/" + mSong.getChannel() +
                          ".isml/" + mSong.getChannel() +
                          (mSong.getBitrate() < 128000 ? "-pa3=" : "-pa4=") + dec(mSong.getBitrate()) +
                          "-video=" + dec(vidBitrate);
      cPlatformHttp http;
      string redirectedHost = http.getRedirect (host, path + ".m3u8");
      if (http.getContent()) {
        //{{{  got .m3u8, parse for mediaSequence, programDateTimePoint
        int mediaSequence = stoi (getTagValue (http.getContent(), "#EXT-X-MEDIA-SEQUENCE:"));

        istringstream inputStream (getTagValue (http.getContent(), "#EXT-X-PROGRAM-DATE-TIME:"));
        system_clock::time_point programDateTimePoint;
        inputStream >> date::parse ("%FT%T", programDateTimePoint);

        http.freeContent();
        //}}}

        mSong.init (cAudioDecode::eAac, 2, 48000, mSong.getBitrate() < 128000 ? 2048 : 1024); // samplesPerFrame
        mSong.setHlsBase (mediaSequence, programDateTimePoint, -37s, (2*60*60) - 30);
        cAudioDecode audioDecode (cAudioDecode::eAac);

        thread player;
        bool firstTime = true;
        bool firstPts = true;
        mSongChanged = false;
        while (!getExit() && !mSongChanged) {
          int chunkNum = mSong.getHlsLoadChunkNum (system_clock::now(), 12s, kHlsPreload);
          if (chunkNum) {
            // get hls chunkNum chunk
            mSong.setHlsLoad (cSong::eHlsLoading, chunkNum);
            if (http.get (redirectedHost, path + '-' + dec(chunkNum) + ".ts") == 200) {
              //{{{  process audio first
              int seqFrameNum = mSong.getHlsFrameFromChunkNum (chunkNum);

              uint64_t pts = 0;

              // parse ts packets
              uint8_t* ts = http.getContent();
              uint8_t* tsEnd = ts + http.getContentSize();
              while ((ts < tsEnd) && (*ts++ == 0x47)) {
                auto payStart = ts[0] & 0x40;
                auto pid = ((ts[0] & 0x1F) << 8) | ts[1];
                auto headerBytes = (ts[2] & 0x20) ? 4 + ts[3] : 3;
                ts += headerBytes;
                auto tsBodyBytes = 187 - headerBytes;

                if (pid == 34) {
                  // audio pid
                  if (payStart && !ts[0] && !ts[1] && (ts[2] == 1) && (ts[3] == 0xC0)) {
                    if (pesBufferLen) {
                      //{{{  process prev audioPes
                      uint8_t* pesBufferPtr = pesBuffer;
                      uint8_t* pesBufferEnd = pesBuffer + pesBufferLen;

                      while (audioDecode.parseFrame (pesBufferPtr, pesBufferEnd)) {
                        // several aacFrames per audio pes
                        float* samples = audioDecode.decodeFrame (seqFrameNum);
                        if (samples) {
                          if (firstTime)
                            mSong.setFixups (audioDecode.getNumChannels(), audioDecode.getSampleRate(), audioDecode.getNumSamples());
                          mSong.addAudioFrame (seqFrameNum++, samples, true, mSong.getNumFrames(), nullptr, pts);
                          pts += (audioDecode.getNumSamples() * 90) / 48;
                          changed();

                          if (firstTime) {
                            // launch player
                            firstTime = false;
                            player = thread ([=](){ playThread (true); });
                            }
                          }

                        pesBufferPtr += audioDecode.getNextFrameOffset();
                        }

                      pesBufferLen = 0;
                      }
                      //}}}

                    if (ts[7] & 0x80)
                      pts = getPts (ts+9);

                    // skip header
                    int pesHeaderBytes = 9 + ts[8];
                    ts += pesHeaderBytes;
                    tsBodyBytes -= pesHeaderBytes;
                    }

                  // copy ts payload into pesBuffer
                  pesBuffer = (uint8_t*)realloc (pesBuffer, pesBufferLen + tsBodyBytes);
                  memcpy (pesBuffer + pesBufferLen, ts, tsBodyBytes);
                  pesBufferLen += tsBodyBytes;
                  }

                ts += tsBodyBytes;
                }

              if (pesBufferLen) {
                //{{{  process last audioPes
                uint8_t* pesBufferPtr = pesBuffer;
                uint8_t* pesBufferEnd = pesBuffer + pesBufferLen;

                while (audioDecode.parseFrame (pesBufferPtr, pesBufferEnd)) {
                  // several aacFrames per audio pes
                  float* samples = audioDecode.decodeFrame (seqFrameNum);
                  if (samples) {
                    mSong.addAudioFrame (seqFrameNum++, samples, true, mSong.getNumFrames(), nullptr, pts);
                    pts += (audioDecode.getNumSamples() * 90) / 48;
                    changed();
                    }

                  pesBufferPtr += audioDecode.getNextFrameOffset();
                  }

                pesBufferLen = 0;
                }
                //}}}
              //}}}
              mSong.setHlsLoad (cSong::eHlsIdle, chunkNum);
              //{{{  process video second, may block waiting for free videoFrames
              // parse ts packets
              ts = http.getContent();
              while ((ts < tsEnd) && (*ts++ == 0x47)) {
                auto payStart = ts[0] & 0x40;
                auto pid = ((ts[0] & 0x1F) << 8) | ts[1];
                auto headerBytes = (ts[2] & 0x20) ? 4 + ts[3] : 3;
                ts += headerBytes;
                auto tsBodyBytes = 187 - headerBytes;

                if (pid == 33) {
                  //  video pid
                  if (payStart && !ts[0] && !ts[1] && (ts[2] == 1) && (ts[3] == 0xe0)) {
                    if (pesBufferLen) {
                      // process prev videoPes
                      mVideoDecode->decode (firstPts, pts, pesBuffer, pesBufferLen);
                      firstPts = false;
                      pesBufferLen = 0;
                      }

                    if (ts[7] & 0x80)
                      pts = getPts (ts+9);

                    // skip header
                    int pesHeaderBytes = 9 + ts[8];
                    ts += pesHeaderBytes;
                    tsBodyBytes -= pesHeaderBytes;
                    }

                  // copy ts payload into pesBuffer
                  pesBuffer = (uint8_t*)realloc (pesBuffer, pesBufferLen + tsBodyBytes);
                  memcpy (pesBuffer + pesBufferLen, ts, tsBodyBytes);
                  pesBufferLen += tsBodyBytes;
                  }

                ts += tsBodyBytes;
                }

              if (pesBufferLen) {
                // process last videoPes
                mVideoDecode->decode (firstPts, pts, pesBuffer, pesBufferLen);
                pesBufferLen = 0;
                }
              //}}}
              http.freeContent();
              }
            else {
              //{{{  failed to load expected available chunk, back off for 250ms
              mSong.setHlsLoad (cSong::eHlsFailed, chunkNum);
              changed();

              cLog::log (LOGERROR, "late " + dec(chunkNum));
              this_thread::sleep_for (250ms);
              }
              //}}}
            }
          else // no chunk available, back off for 100ms
            this_thread::sleep_for (100ms);
          }
        player.join();
        }
      }

    free (pesBuffer);
    cLog::log (LOGINFO, "exit");
    }
  //}}}
  //{{{
  void playThread (bool streaming) {
  // audio player thread, video just follows play pts

    cLog::setThreadName ("play");
    SetThreadPriority (GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    float silence [2048*2] = { 0.f };
    float samples [2048*2] = { 0.f };

    auto device = getDefaultAudioOutputDevice();
    if (device) {
      cLog::log (LOGINFO, "device @ %d", mSong.getSampleRate());
      device->setSampleRate (mSong.getSampleRate());
      cAudioDecode decode (mSong.getFrameType());

      device->start();
      while (!getExit() && !mSongChanged) {
        device->process ([&](float*& srcSamples, int& numSrcSamples) mutable noexcept {
          // lambda callback - load srcSamples
          shared_lock<shared_mutex> lock (mSong.getSharedMutex());

          auto framePtr = mSong.getAudioFramePtr (mSong.getPlayFrame());
          if (mPlaying && framePtr && framePtr->getSamples()) {
            if (mSong.getNumChannels() == 1) {
              //{{{  mono to stereo
              auto src = framePtr->getSamples();
              auto dst = samples;
              for (int i = 0; i < mSong.getSamplesPerFrame(); i++) {
                *dst++ = *src;
                *dst++ = *src++;
                }
              }
              //}}}
            else
              memcpy (samples, framePtr->getSamples(), mSong.getSamplesPerFrame() * mSong.getNumChannels() * sizeof(float));
            srcSamples = samples;
            }
          else
            srcSamples = silence;
          numSrcSamples = mSong.getSamplesPerFrame();

          if (mPlaying && framePtr) {
            if (mVideoDecode)
              mVideoDecode->setPlayPts (framePtr->getPts());
            mSong.incPlayFrame (1, true);
            changed();
            }
          });

        if (!streaming && (mSong.getPlayFrame() > mSong.getLastFrame()))
          break;
        }

      device->stop();
      }

    cLog::log (LOGINFO, "exit");
    }
  //}}}

  //{{{  vars
  cSong mSong;
  bool mSongChanged = false;
  bool mPlaying = true;
  cBox* mLogBox = nullptr;

  cVideoDecode* mVideoDecode = nullptr;
  //}}}
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
  appWindow.run ("hlsWindow", 800, 420, channelNum, kAudBitrate, kVidBitrate);
  return 0;
  }
