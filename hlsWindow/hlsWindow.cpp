// playWindow.cpp
//{{{  includes
#include "stdafx.h"

#include "cSong.h"
#include "cSongBox.h"

#include "audioWASAPI.h"
#include "../../shared/decoders/cAudioDecode.h"

#include "../../shared/hls/r1x80.h"
#include "../../shared/hls/r2x80.h"
#include "../../shared/hls/r3x80.h"
#include "../../shared/hls/r4x80.h"
#include "../../shared/hls/r5x80.h"
#include "../../shared/hls/r6x80.h"

using namespace std;
using namespace chrono;
//}}}
//vs-hls-uk-live.akamaized.net/pool_902/live/uk/bbc_one_hd/bbc_one_hd.isml/bbc_one_hd-pa4%3d128000-video%3d827008.m3u8

const string kHost = "vs-hls-uk-live.akamaized.net";
const vector <string> kChannels = { "bbc_one_hd" };
const int kBitRate = 128000;

class cAppWindow : public cD2dWindow {
public:
  //{{{
  void run (const string& title, int width, int height, const vector<string>& names) {

    init (title, width, height, false);
    add (new cCalendarBox (this, 190.f,150.f), -190.f,0.f);
    add (new cClockBox (this, 40.f), -135.f,35.f);
    add (new cSongBox (this, 0.f,0.f, mSong));

    mBitrateStr = "128k 827008";
    addRight (new cTitleBox (this, 60.f,20.f, mBitrateStr, [&](cTitleBox* box){
      //{{{  lambda
      mSong.clear();
      switch (mSong.getBitrate()) {
        case 48000:
          mSong.setBitrate (96000);
          mBitrateStr = "96k aacHE";
          break;
        case 96000:
          mSong.setBitrate (128000);
          mBitrateStr = "128k aac";
          break;
        case 128000:
          mSong.setBitrate (320000);
          mBitrateStr = "320k aac";
          break;
        case 320000:
          mSong.setBitrate (48000);
          mBitrateStr = "48k aacHE";
          break;
        }
      mSongChanged = true;
      }
      //}}}
      ), 4.f);

    add (new cTitleBox (this, 500.f,20.f, mDebugStr), 0.f,40.f);

    // startup radio4
    thread ([=](){ hlsThread (kHost, kChannels[0], kBitRate); }).detach();

    mLogBox = add (new cLogBox (this, 20.f));
    add (new cWindowBox (this, 60.f,24.f), -60.f,0.f)->setPin (false);

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

      case 'M' : mSong.getSelect().addMark (mSong.getPlayFrame()); changed(); break;

      case ' ' : mPlaying = !mPlaying; break;

      case 0x21: mSong.prevSilencePlayFrame(); changed(); break;; // page up
      case 0x22: mSong.nextSilencePlayFrame(); changed(); break;; // page down

      case 0x25: mSong.incPlaySec (getShift() ? -300 : getControl() ? -10 : -1, false);  changed(); break; // left arrow  - 1 sec
      case 0x27: mSong.incPlaySec (getShift() ? 300 : getControl() ?  10 :  1, false);  changed(); break; // right arrow  + 1 sec

      case 0x24: mSong.setPlayFrame (
        mSong.getSelect().empty() ? mSong.getFirstFrame() : mSong.getSelect().getFirstFrame()); changed(); break; // home
      case 0x23: mSong.setPlayFrame (
        mSong.getSelect().empty() ? mSong.getLastFrame() : mSong.getSelect().getLastFrame()); changed(); break; // end

      case 0x2e: mSong.getSelect().clearAll(); changed(); break;; // delete select

      case 0x0d: mSongChanged = true; break; // enter

      // crude chan,bitrate change
      case '1' : mSong.clear(); mSong.setChan ("bbc_radio_one"); mSongChanged = true; break;
      case '2' : mSong.clear(); mSong.setChan ("bbc_radio_two"); mSongChanged = true; break;
      case '3' : mSong.clear(); mSong.setChan ("bbc_radio_three"); mSongChanged = true; break;
      case '4' : mSong.clear(); mSong.setChan ("bbc_radio_fourfm"); mSongChanged = true;  break;
      case '5' : mSong.clear(); mSong.setChan ("bbc_radio_five_live"); mSongChanged = true; break;
      case '6' : mSong.clear(); mSong.setChan ("bbc_6music"); mSongChanged = true; break;
      case '7' : mSong.clear(); mSong.setBitrate (48000); mBitrateStr = "48k aacHE"; mSongChanged = true; break;
      case '8' : mSong.clear(); mSong.setBitrate (96000); mBitrateStr = "96k aacHE"; mSongChanged = true; break;
      case '9' : mSong.clear(); mSong.setBitrate (128000); mBitrateStr = "128k aac"; mSongChanged = true; break;
      case '0' : mSong.clear(); mSong.setBitrate (320000); mBitrateStr = "320k aac"; mSongChanged = true; break;

      default  : cLog::log (LOGINFO, "key %x", key); changed(); break;
      }

    return false;
    }
  //}}}

private:
  //{{{
  static string getTaggedValue (uint8_t* buffer, char* tag) {

    const char* tagPtr = strstr ((char*)buffer, tag);
    const char* valuePtr = tagPtr + strlen (tag);
    const char* endPtr = strchr (valuePtr, '\n');

    const string valueString = string (valuePtr, endPtr - valuePtr);
    cLog::log (LOGINFO, string(tagPtr, endPtr - tagPtr) + " value: " + valueString);

    return valueString;
    }
  //}}}
  //{{{
  static uint8_t* extractAacFramesFromTs (uint8_t* ts, int tsLen) {
  // extract aacFrames from ts packets, pack back into ts, gets smaller ts gets stripped

    auto aacFramesPtr = ts;

    auto tsEnd = ts + tsLen;
    while ((ts < tsEnd) && (*ts++ == 0x47)) {
      // ts packet start
      auto payStart = ts[0] & 0x40;
      auto pid = ((ts[0] & 0x1F) << 8) | ts[1];
      auto headerBytes = (ts[2] & 0x20) ? 4 + ts[3] : 3;
      ts += headerBytes;
      auto tsBodyBytes = 187 - headerBytes;

      if (pid == 34) {
        if (payStart &&
            !ts[0] && !ts[1] && (ts[2] == 1) && (ts[3] == 0xC0)) {
          int pesHeaderBytes = 9 + ts[8];
          ts += pesHeaderBytes;
          tsBodyBytes -= pesHeaderBytes;
          }

        // copy ts payload aacFrames back into buffer
        memcpy (aacFramesPtr, ts, tsBodyBytes);
        aacFramesPtr += tsBodyBytes;
        }

      ts += tsBodyBytes;
      }

    return aacFramesPtr;
    }
  //}}}

  //{{{
  void hlsThread (const string& host, const string& chan, int bitrate) {
  // hls chunk http load and analyse thread, single thread helps chan change and jumping backwards

    constexpr int kHlsPreload = 10; // about a minute
    cLog::setThreadName ("hls ");

    mSong.setChan (chan);
    mSong.setBitrate (bitrate);
    while (!getExit()) {
      const string path = "pool_902/live/uk/" + mSong.getChan() +
                          "/" + mSong.getChan() + ".isml/" + mSong.getChan() +
                          "-pa4=" + dec(mSong.getBitrate()) +
                          "-video=827008";
      cPlatformHttp http;
      auto redirectedHost = http.getRedirect (host, path + ".m3u8");
      if (http.getContent()) {
        //{{{  hls m3u8 ok, parse it for baseChunkNum, baseTimePoint
        int mediaSequence = stoi (getTaggedValue (http.getContent(), "#EXT-X-MEDIA-SEQUENCE:"));

        istringstream inputStream (getTaggedValue (http.getContent(), "#EXT-X-PROGRAM-DATE-TIME:"));
        system_clock::time_point programDateTimePoint;
        inputStream >> date::parse ("%FT%T", programDateTimePoint);

        http.freeContent();
        //}}}
        mSong.init (cAudioDecode::eAac, 2, 48000, mSong.getBitrate() >= 128000 ? 1024 : 2048);
        mSong.setHlsBase (mediaSequence, programDateTimePoint, -37s);
        cAudioDecode decode (cAudioDecode::eAac);

        thread player;
        bool firstTime = true;
        mSongChanged = false;
        while (!getExit() && !mSongChanged) {
          auto chunkNum = mSong.getHlsLoadChunkNum (getNowRaw(), 12s, kHlsPreload);
          if (chunkNum) {
            // get hls chunkNum chunk
            mSong.setHlsLoad (cSong::eHlsLoading, chunkNum);
            if (http.get (redirectedHost, path + '-' + dec(chunkNum) + ".ts") == 200) {
              cLog::log (LOGINFO1, "got " + dec(chunkNum) +
                                   " at " + date::format ("%T", floor<seconds>(getNow())));
              int seqFrameNum = mSong.getHlsFrameFromChunkNum (chunkNum);
              auto aacFrames = http.getContent();
              auto aacFramesEnd = extractAacFramesFromTs (aacFrames, http.getContentSize());
              while (decode.parseFrame (aacFrames, aacFramesEnd)) {
                auto samples = decode.decodeFrame (seqFrameNum);
                if (samples) {
                  mSong.setFixups (decode.getNumChannels(), decode.getSampleRate(), decode.getNumSamples());
                  mSong.addFrame (seqFrameNum++, samples, true, mSong.getNumFrames());
                  changed();
                  if (firstTime) {
                    firstTime = false;
                    //player = thread ([=](){ playThread (true); });
                    }
                  }
                aacFrames += decode.getNextFrameOffset();
                }
              http.freeContent();
              mSong.setHlsLoad (cSong::eHlsIdle, chunkNum);
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
        //player.join();
        }
      }
    cLog::log (LOGINFO, "exit");
    }
  //}}}
  //{{{
  void playThread (bool streaming) {

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

          auto framePtr = mSong.getFramePtr (mSong.getPlayFrame());
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

  string mBitrateStr;
  string mUrl;

  string mDebugStr;
  cBox* mLogBox = nullptr;
  //}}}
  };

// main
//int WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
int main (int argc, char** argv) {

  cLog::init (LOGINFO, false, "", "hlsWindow");
  vector<string> names;

  //int numArgs;
  //auto args = CommandLineToArgvW (GetCommandLineW(), &numArgs);
  //for (int i = 1; i < numArgs; i++)
  //  names.push_back (wcharToString (args[i]));

  cAppWindow appWindow;
  appWindow.run ("hlsWindow", 800, 420, names);

  return 0;
  }
