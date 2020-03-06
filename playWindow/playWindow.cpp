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

class cAppWindow : public cD2dWindow {
public:
  //{{{
  void run (const string& title, int width, int height, const vector<string>& names) {

    init (title, width, height, false);
    add (new cCalendarBox (this, 190.f,150.f), -190.f,0.f);
    add (new cClockBox (this, 40.f), -135.f,35.f);
    add (new cSongBox (this, 0.f,0.f, mSong));

    if (names.empty()) {
      //{{{  hls radio 1..6
      add (new cBmpBox (this, 40.f,40.f, 1, r1x80, [&](cBmpBox* box, int index){
        mSong.clear(); mSong.setChan ("bbc_radio_one"); mSongChanged = true; } ));
      addRight (new cBmpBox (this, 40.f,40.f, 2, r2x80, [&](cBmpBox* box, int index){
        mSong.clear(); mSong.setChan ("bbc_radio_two"); mSongChanged = true; } ));
      addRight (new cBmpBox (this, 40.f,40.f, 3, r3x80, [&](cBmpBox* box, int index){
        mSong.clear(); mSong.setChan ("bbc_radio_three"); mSongChanged = true; } ));
      addRight (new cBmpBox (this, 40.f,40.f, 4, r4x80, [&](cBmpBox* box, int index){
        mSong.clear(); mSong.setChan ("bbc_radio_fourfm"); mSongChanged = true; } ));
      addRight (new cBmpBox (this, 40.f,40.f, 5, r5x80, [&](cBmpBox* box, int index){
        mSong.clear(); mSong.setChan ("bbc_radio_five_live"); mSongChanged = true; } ));
      addRight (new cBmpBox (this, 40.f,40.f, 6, r6x80, [&](cBmpBox* box, int index){
        mSong.clear(); mSong.setChan ("bbc_6music"); mSongChanged = true; } ));

      mBitrateStr = "48k aacHE";
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

      thread ([=](){ hlsThread ("as-hls-uk-live.bbcfmt.hs.llnwd.net", "bbc_radio_fourfm", 48000); }).detach();
      }
      //}}}
    else {
      cUrl url;
      url.parse (names[0]);
      if (url.getScheme() == "http") {
        //{{{  shoutcast
        mShoutCast.push_back (names[0]);
        mShoutCast.push_back ("http://stream.wqxr.org/wqxr.aac");
        mShoutCast.push_back ("http://stream.wqxr.org/js-stream.aac");
        mShoutCast.push_back ("http://tx.planetradio.co.uk/icecast.php?i=jazzhigh.aac");
        mShoutCast.push_back ("http://us4.internet-radio.com:8266/");
        mShoutCast.push_back ("http://tx.planetradio.co.uk/icecast.php?i=countryhits.aac");
        mShoutCast.push_back ("http://live-absolute.sharp-stream.com/absoluteclassicrockhigh.aac");
        mShoutCast.push_back ("http://media-ice.musicradio.com:80/SmoothCountry");

        add (new cStringListBox (this, 500.f, 300.f, mShoutCast, [&](cStringListBox* box, const std::string& string){
          auto listBox = (cStringListBox*)box;
          mSong.clear();
          mUrl = string;
          mSongChanged = true;
          cLog::log (LOGINFO, "listBox" + string);
          }
          ))->setPin (true);

        //mDebugStr = "shoutcast " + url.getHost() + " channel " + url.getPath();
        add (new cTitleBox (this, 500.f,20.f, mDebugStr));

        thread ([=](){ icyThread (names[0]); }).detach();
        }
        //}}}
      else {
        //{{{  filelist
        mFileList = new cFileList (names, "*.aac;*.mp3;*.wav");

        if (!mFileList->empty()) {

          add (new cFileListBox (this, 0.f,-220.f, mFileList, [&](cFileListBox* box, int index){
            mSong.clear(); mSongChanged = true; }))->setPin (true);

          mJpegImageView = new cJpegImageView (this, 0.f,-220.f, false, false, nullptr);
          addFront (mJpegImageView);

          thread([=](){ mFileList->watchThread(); }).detach();
          thread ([=](){ fileThread(); }).detach();
          }
        }
        //}}}
      }

    mLogBox = add (new cLogBox (this, 20.f));
    add (new cWindowBox (this, 60.f,24.f), -60.f,0.f)->setPin (false);

    // loop till quit
    messagePump();

    delete mFileList;
    delete mJpegImageView;
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

      case 0x26: if (mFileList && mFileList->prevIndex()) changed(); break; // up arrow
      case 0x28: if (mFileList && mFileList->nextIndex()) changed(); break; // down arrow

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
  void addIcyInfo (int frame, const string& icyInfo) {

    cLog::log (LOGINFO, "addIcyInfo " + icyInfo);

    string icysearchStr = "StreamTitle=\'";
    string searchStr = "StreamTitle=\'";
    auto searchStrPos = icyInfo.find (searchStr);
    if (searchStrPos != string::npos) {
      auto searchEndPos = icyInfo.find ("\';", searchStrPos + searchStr.size());
      if (searchEndPos != string::npos) {
        string titleStr = icyInfo.substr (searchStrPos + searchStr.size(), searchEndPos - searchStrPos - searchStr.size());
        if (titleStr != mLastTitleStr) {
          cLog::log (LOGINFO1, "addIcyInfo found title = " + titleStr);
          mSong.getSelect().addMark (frame, titleStr);
          mLastTitleStr = titleStr;
          }
        }
      }

    string urlStr = "no url";
    searchStr = "StreamUrl=\'";
    searchStrPos = icyInfo.find (searchStr);
    if (searchStrPos != string::npos) {
      auto searchEndPos = icyInfo.find ('\'', searchStrPos + searchStr.size());
      if (searchEndPos != string::npos) {
        urlStr = icyInfo.substr (searchStrPos + searchStr.size(), searchEndPos - searchStrPos - searchStr.size());
        cLog::log (LOGINFO1, "addIcyInfo found url = " + urlStr);
        }
      }
    }
  //}}}

  //{{{
  void hlsThread (const string& host, const string& chan, int bitrate) {
  // hls chunk http load and analyse thread, single thread helps chan change and jumping backwards
  // - host is redirected, assumes bbc radio aac, 48000 sampleaRate

    constexpr int kHlsPreload = 10; // about a minute

    cLog::setThreadName ("hls ");

    mSong.setChan (chan);
    mSong.setBitrate (bitrate);

    while (!getExit()) {
      // loop till exit
      const string path = "pool_904/live/uk/" + mSong.getChan() +
                          "/" + mSong.getChan() + ".isml/" + mSong.getChan() +
                          "-audio=" + dec(mSong.getBitrate());
      cWinSockHttp http;
      auto redirectedHost = http.getRedirect (host, path + ".norewind.m3u8");
      if (http.getContent()) {
        //{{{  hls m3u8 ok, parse it for baseChunkNum, baseTimePoint
        int mediaSequence = stoi (getTaggedValue (http.getContent(), "#EXT-X-MEDIA-SEQUENCE:"));

        istringstream inputStream (getTaggedValue (http.getContent(), "#EXT-X-PROGRAM-DATE-TIME:"));
        system_clock::time_point programDateTimePoint;
        inputStream >> date::parse ("%FT%T", programDateTimePoint);

        programDateTimePoint -= 37s;

        http.freeContent();
        //}}}
        mSong.init (cAudioDecode::eAac, 2, 48000, mSong.getBitrate() >= 128000 ? 1024 : 2048);
        mSong.setHlsBase (mediaSequence, programDateTimePoint);
        cAudioDecode decode (cAudioDecode::eAac);

        auto player = thread ([=](){ playThread (true); });

        mSongChanged = false;
        while (!getExit() && !mSongChanged) {
          auto chunkNum = mSong.getHlsLoadChunkNum (getNow(), 12s, kHlsPreload);
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

    cLog::log (LOGINFO, "exit");
    }
  //}}}
  //{{{
  void icyThread (const string& url) {

    cLog::setThreadName ("icy ");

    mUrl = url;
    while (!getExit()) {
      int icySkipCount = 0;
      int icySkipLen = 0;
      int icyInfoCount = 0;
      int icyInfoLen = 0;
      char icyInfo[255] = { 0 };

      uint8_t bufferFirst[4096];
      uint8_t* bufferEnd = bufferFirst;
      uint8_t* buffer = bufferFirst;

      int frameNum = -1;
      cAudioDecode decode (cAudioDecode::eAac);

      thread player;
      mSongChanged = false;

      cWinSockHttp http;
      cUrl parsedUrl;
      parsedUrl.parse (mUrl);
      http.get (parsedUrl.getHost(), parsedUrl.getPath(), "Icy-MetaData: 1",
        //{{{  headerCallback lambda
        [&](const string& key, const string& value) noexcept {
          if (key == "icy-metaint")
            icySkipLen = stoi (value);
          },
        //}}}
        //{{{  dataCallback lambda
        [&] (const uint8_t* data, int length) noexcept {
        // return false to exit

          // cLog::log (LOGINFO, "callback %d", length);
          if ((icyInfoCount >= icyInfoLen) && (icySkipCount + length <= icySkipLen)) {
            //{{{  simple copy of whole body, no metaInfo
            //cLog::log (LOGINFO1, "body simple copy len:%d", length);

            memcpy (bufferEnd, data, length);

            bufferEnd += length;
            icySkipCount += length;
            }
            //}}}
          else {
            //{{{  dumb copy for metaInfo straddling body, could be much better
            //cLog::log (LOGINFO1, "body split copy length:%d info:%d:%d skip:%d:%d ",
                                  //length, icyInfoCount, icyInfoLen, icySkipCount, icySkipLen);
            for (int i = 0; i < length; i++) {
              if (icyInfoCount < icyInfoLen) {
                icyInfo [icyInfoCount] = data[i];
                icyInfoCount++;
                if (icyInfoCount >= icyInfoLen)
                  addIcyInfo (frameNum, icyInfo);
                }
              else if (icySkipCount >= icySkipLen) {
                icyInfoLen = data[i] * 16;
                icyInfoCount = 0;
                icySkipCount = 0;
                //cLog::log (LOGINFO1, "body icyInfo len:", data[i] * 16);
                }
              else {
                icySkipCount++;
                *bufferEnd = data[i];
                bufferEnd++;
                }
              }

            return !getExit() && !mSongChanged;
            }
            //}}}

          if (frameNum == -1) {
            // enough data to determine frameType and sampleRate (wrong for aac sbr)
            frameNum = 0;
            int sampleRate;
            auto frameType = cAudioDecode::parseSomeFrames (bufferFirst, bufferEnd, sampleRate);
            mSong.init (frameType, 2, 44100, (frameType == cAudioDecode::eMp3) ? 1152 : 2048);
            }

          while (decode.parseFrame (buffer, bufferEnd)) {
            if (decode.getFrameType() == mSong.getFrameType()) {
              auto samples = decode.decodeFrame (frameNum);
              if (samples) {
                mSong.setFixups (decode.getNumChannels(), decode.getSampleRate(), decode.getNumSamples());
                mSong.addFrame (frameNum++, samples, true, mSong.getNumFrames()+1);
                changed();
                if (frameNum == 1) // launch player after first frame
                  player = thread ([=](){ playThread (true); });
                }
              }
            buffer += decode.getNextFrameOffset();
            }

          if ((buffer > bufferFirst) && (buffer < bufferEnd)) {
            // shuffle down last partial frame
            auto bufferLeft = int(bufferEnd - buffer);
            memcpy (bufferFirst, buffer, bufferLeft);
            bufferEnd = bufferFirst + bufferLeft;
            buffer = bufferFirst;
            }

          return !getExit() && !mSongChanged;
          }
        //}}}
        );

      cLog::log (LOGINFO, "icyThread songChanged");
      player.join();
      }

    cLog::log (LOGINFO, "exit");
    }
  //}}}
  //{{{
  void fileThread() {

    cLog::setThreadName ("file");

    while (!getExit()) {
      HANDLE fileHandle = CreateFile (mFileList->getCurFileItem().getFullName().c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
      HANDLE mapping = CreateFileMapping (fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
      auto fileMapFirst = (uint8_t*)MapViewOfFile (mapping, FILE_MAP_READ, 0, 0, 0);
      auto fileMapSize = GetFileSize (fileHandle, NULL);
      auto fileMapEnd = fileMapFirst + fileMapSize;

      // sampleRate for aac-sbr wrong in header, fixup later, use a maxValue for samples alloc
      int sampleRate;
      auto frameType = cAudioDecode::parseSomeFrames (fileMapFirst, fileMapEnd, sampleRate);
      if (cAudioDecode::mJpegPtr) // should delete old jpegImage, but we have memory to waste
        mJpegImageView->setImage (new cJpegImage (cAudioDecode::mJpegPtr, cAudioDecode::mJpegLen));

      auto player = thread ([=](){ playThread (false); });

      int frameNum = 0;
      bool songDone = false;
      auto fileMapPtr = fileMapFirst;
      if (frameType == cAudioDecode::eWav) {
        //{{{  float 32bit interleaved wav uses file map directly
        auto frameSamples = 1024;
        mSong.init (frameType, 2, sampleRate, frameSamples);

        cAudioDecode decode (cAudioDecode::eWav);
        decode.parseFrame (fileMapPtr, fileMapEnd);

        auto samples = decode.getFramePtr();
        auto frameSampleBytes = frameSamples * 2 * 4;
        while (!getExit() && !mSongChanged && ((samples + frameSampleBytes) <= fileMapEnd)) {
          mSong.addFrame (frameNum++, (float*)samples, false, fileMapSize / frameSampleBytes);
          samples += frameSampleBytes;
          changed();
          }
        }
        //}}}
      else {
        // hold onto decoded samples
        mSong.init (frameType, 2, sampleRate, (frameType == cAudioDecode::eMp3) ? 1152 : 2048);

        cAudioDecode decode (frameType);
        while (!getExit() && !mSongChanged && decode.parseFrame (fileMapPtr, fileMapEnd)) {
          if (decode.getFrameType() == mSong.getFrameType()) {
            auto samples = decode.decodeFrame (frameNum);
            if (samples) {
              mSong.setFixups (decode.getNumChannels(), decode.getSampleRate(), decode.getNumSamples());
              int numFrames = mSong.getNumFrames();
              int totalFrames = (numFrames > 0) ? int(fileMapEnd - fileMapFirst) / (int(decode.getFramePtr() - fileMapFirst) / numFrames) : 0;
              mSong.addFrame (frameNum++, samples, true, totalFrames+1);
              changed();
              }
            }
          fileMapPtr += decode.getNextFrameOffset();
          }
        cLog::log (LOGINFO, "song analysed");
        }

      // wait for play to end or abort
      player.join();
      //{{{  next file
      UnmapViewOfFile (fileMapFirst);
      CloseHandle (fileHandle);

      if (mSongChanged) // use changed fileIndex
        mSongChanged = false;
      else if (!mFileList->nextIndex())
        setExit();
      //}}}
      }

    cLog::log (LOGINFO, "exit");
    }
  //}}}
  //{{{
  void playThread (bool streaming) {
  // launched and lifetime per song

    cLog::setThreadName ("play");
    float silence [2048*2] = { 0.f };

    SetThreadPriority (GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

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
          if (mPlaying && framePtr && framePtr->getSamples())
            srcSamples = framePtr->getSamples();
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

  cFileList* mFileList = nullptr;
  cJpegImageView* mJpegImageView = nullptr;

  string mLastTitleStr;
  vector<string> mShoutCast;
  string mUrl;

  string mDebugStr;
  cBox* mLogBox = nullptr;
  //}}}
  };

// main
int __stdcall WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
  cLog::init (LOGINFO, true, "", "playWindow");

  int numArgs;
  auto args = CommandLineToArgvW (GetCommandLineW(), &numArgs);

  vector<string> names;
  for (int i = 1; i < numArgs; i++)
    names.push_back (wcharToString (args[i]));

  cAppWindow appWindow;
  appWindow.run ("playWindow", 800, 420, names);

  return 0;
  }
