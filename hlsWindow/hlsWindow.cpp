// playWindow.cpp
//{{{  includes
#include "stdafx.h"

#include "../../shared/audio/audioWASAPI.h"
#include "../../shared/decoders/cSong.h"
#include "../../shared/decoders/cAudioDecode.h"

#include "../boxes/cSongBox.h"

using namespace std;
using namespace chrono;
//}}}
//vs-hls-uk-live.akamaized.net/pool_902/live/uk/bbc_one_hd/bbc_one_hd.isml/bbc_one_hd-pa4%3d128000-video%3d827008.m3u8
//vs-hls-uk-live.akamaized.net/pool_902/live/uk/bbc_four_hd/bbc_four_hd.isml/bbc_four_hd-pa4%3d128000-video%3d5070016.m3u8
//vs-hls-uk-live.akamaized.net/pool_902/live/uk/bbc_one_south_west/bbc_one_south_west.isml/bbc_one_south_west-pa3%3d96000-video%3d1604032.m3u8
const string kHost = "vs-hls-uk-live.akamaized.net";
const vector <string> kChannels = { "bbc_one_hd", "bbc_four_hd", "bbc_one_south_west" };
constexpr int kBitRate = 128000;
//constexpr int kVidBitrate = 827008;
//constexpr int kVidBitrate = 2812032;
constexpr int kVidBitrate = 5070016;

class cAppWindow : public cD2dWindow {
public:
  //{{{
  void run (const string& title, int width, int height, const vector<string>& names) {

    init (title, width, height, false);
    add (new cCalendarBox (this, 190.f,150.f), -190.f,0.f);
    add (new cClockBox (this, 40.f), -135.f,35.f);
    add (new cSongBox (this, 0.f,0.f, mSong));

    // startup
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
  static uint8_t* extractFramesFromTs (uint8_t* ts, int tsLen, uint8_t* vidFramesPtr) {
  // extract aacFrames, vidframes from ts packets
  // - audio put back into ts, gets smaller ts gets stripped
  // - video into supplied buffer

    int audioPesNum = 0;
    int videoPesNum = 0;
    auto aacFramesPtr = ts;

    auto tsEnd = ts + tsLen;
    while ((ts < tsEnd) && (*ts++ == 0x47)) {
      // ts packet start, dumb ts parser
      auto payStart = ts[0] & 0x40;
      auto pid = ((ts[0] & 0x1F) << 8) | ts[1];
      auto headerBytes = (ts[2] & 0x20) ? 4 + ts[3] : 3;
      ts += headerBytes;
      auto tsBodyBytes = 187 - headerBytes;

      if (pid == 33) {
        if (payStart && !ts[0] && !ts[1] && (ts[2] == 1) && (ts[3] == 0xe0)) {
          cLog::log (LOGINFO, "video pid:" + dec(pid) + " " + dec(videoPesNum++));
          int pesHeaderBytes = 9 + ts[8];
          ts += pesHeaderBytes;
          tsBodyBytes -= pesHeaderBytes;
          }

        // copy ts payload into vidFrames buffer
        memcpy (vidFramesPtr, ts, tsBodyBytes);
        vidFramesPtr += tsBodyBytes;
        }

      else if (pid == 34) {
        if (payStart && !ts[0] && !ts[1] && (ts[2] == 1) && (ts[3] == 0xC0)) {
          cLog::log (LOGINFO, "audio pid:" + dec(pid) + " " + dec(audioPesNum++));

          int pesHeaderBytes = 9 + ts[8];
          ts += pesHeaderBytes;
          tsBodyBytes -= pesHeaderBytes;
          }

        // copy ts payload aacFrames back into buffer
        memcpy (aacFramesPtr, ts, tsBodyBytes);
        aacFramesPtr += tsBodyBytes;
        }

      else {
        // other pid
        if (payStart) {
          cLog::log (LOGINFO, "other pid:%d header %x %x %x %x headerBytes:%d",
                              pid, int(ts[0]), int(ts[1]), int(ts[2]), int(ts[3]), headerBytes);
          }
        }

      ts += tsBodyBytes;
      }

    return aacFramesPtr;
    }
  //}}}

  //{{{
  void hlsThread (const string& host, const string& chan, int bitrate) {
  // hls chunk http load and analyse thread, single thread helps chan change and jumping backwards

    mFile = fopen ("C:/Users/colin/Desktop/hls.ts", "wb");
    uint8_t* vidFrames = (uint8_t*)malloc (10000000);

    constexpr int kHlsPreload = 10; // about a minute
    cLog::setThreadName ("hls ");

    mSong.setChan (chan);
    mSong.setBitrate (bitrate, 360);
    while (!getExit()) {
      const string path = "pool_902/live/uk/" + mSong.getChan() +
                          "/" + mSong.getChan() + ".isml/" + mSong.getChan() +
                          "-pa4=" + dec(mSong.getBitrate()) +
                          "-video=" + dec(kVidBitrate);
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
              cLog::log (LOGINFO, "got " + dec(chunkNum) +
                                   " at " + date::format ("%T", floor<seconds>(getNow())) +
                                   " size:" + dec(http.getContentSize()));

              fwrite (http.getContent(), 1, http.getContentSize(), mFile);

              int seqFrameNum = mSong.getHlsFrameFromChunkNum (chunkNum);
              auto aacFrames = http.getContent();
              auto aacFramesEnd = extractFramesFromTs (aacFrames, http.getContentSize(), vidFrames);
              while (decode.parseFrame (aacFrames, aacFramesEnd)) {
                auto samples = decode.decodeFrame (seqFrameNum);
                if (samples) {
                  mSong.setFixups (decode.getNumChannels(), decode.getSampleRate(), decode.getNumSamples());
                  mSong.addFrame (seqFrameNum++, samples, true, mSong.getNumFrames());
                  changed();
                  if (firstTime) {
                    firstTime = false;
                    player = thread ([=](){ playThread (true); });
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
        player.join();
        }
      }

    fclose (mFile);
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
  cBox* mLogBox = nullptr;

  FILE* mFile = nullptr;
  //}}}
  };

int main (int argc, char** argv) {

  cLog::init (LOGINFO, false, "", "hlsWindow");

  cAppWindow appWindow;
  appWindow.run ("hlsWindow", 800, 420, {});

  return 0;
  }
