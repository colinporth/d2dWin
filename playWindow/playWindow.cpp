// playWindow.cpp
//{{{  includes
#include "stdafx.h"

// should be in stdafx.h
#include "../../shared/utils/cFileList.h"
#include "../boxes/cFileListBox.h"

#include "cSong.h"
#include "cSongBox.h"

#include "audioWASAPI.h"
#include "cAudioDecode.h"

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
  void run (const string& title, int width, int height, const string& name) {

    init (title + " " + name, width, height, false);
    add (new cCalendarBox (this, 190.f,150.f), -190.f,0.f);
    add (new cClockBox (this, 40.f), -135.f,35.f);
    add (new cSongBox (this, 0.f,0.f, mSong));

    if (name.empty()) {
      //{{{  hls radio 1..6
      add (new cBmpBox (this, 40.f,40.f, r1x80,
           [&](cBox* box){ mSong.setChan ("bbc_radio_one"); mChanged = true; } ));

      addRight (new cBmpBox (this, 40.f,40.f, r2x80,
                [&](cBox* box){ mSong.setChan ("bbc_radio_two"); mChanged = true; } ));

      addRight (new cBmpBox (this, 40.f,40.f, r3x80,
                [&](cBox* box){ mSong.setChan ("bbc_radio_three"); mChanged = true; } ));

      addRight (new cBmpBox (this, 40.f,40.f, r4x80,
                [&](cBox* box){ mSong.setChan ("bbc_radio_fourfm"); mChanged = true; } ));

      addRight (new cBmpBox (this, 40.f,40.f, r5x80,
                [&](cBox* box){ mSong.setChan ("bbc_radio_five_live"); mChanged = true; } ));

      addRight (new cBmpBox (this, 40.f,40.f, r6x80,
                [&](cBox* box){ mSong.setChan ("bbc_6music"); mChanged = true; } ));

      add (new cTitleBox (this, 500.f,20.f, mDebugStr), 0.f,40.f);

      thread ([=]() { hlsThread ("as-hls-uk-live.bbcfmt.hs.llnwd.net", "bbc_radio_fourfm", 48000); }).detach();
      }
      //}}}
    else {
      cUrl url;
      url.parse (name);
      if (url.getScheme() == "http") {
        //{{{  shoutcast
        //{{{  urls
        //const string url = "http://stream.wqxr.org/wqxr.aac";
        //const string url = "http://tx.planetradio.co.uk/icecast.php?i=jazzhigh.aac";
        //const string url = "http://us4.internet-radio.com:8266/";
        //const string url = "http://tx.planetradio.co.uk/icecast.php?i=countryhits.aac";
        //const string url = "http://live-absolute.sharp-stream.com/absoluteclassicrockhigh.aac";
        //const string url = "http://media-ice.musicradio.com:80/SmoothCountry";
        //}}}
        mDebugStr = "shoutcast " + url.getHost() + " channel " + url.getPath();
        add (new cTitleBox (this, 500.f,20.f, mDebugStr));

        thread ([=](){ icyThread (name); }).detach();
        }
        //}}}
      else {
        //{{{  filelist
        mFileList = new cFileList (name, "*.aac;*.mp3;*.wav");

        if (!mFileList->empty()) {

          add (new cFileListBox (this, 0.f,-220.f, mFileList,
               [&](cBox* box){ mChanged = true; }))->setPin (true);

          mJpegImageView = new cJpegImageView (this, 0.f,-220.f, false, false, nullptr);
          addFront (mJpegImageView);

          thread([=](){ mFileList->watchThread(); }).detach();
          thread ([=](){ fileThread(); }).detach();
          }
        }
        //}}}
      }
    thread ([=](){ playThread(); }).detach();

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

    shared_lock<shared_mutex> lock (mSong.getSharedMutex());

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

      case 0x26: if (mFileList->prevIndex()) changed(); break; // up arrow
      case 0x28: if (mFileList->nextIndex()) changed(); break; // down arrow

      case 0x2e: mSong.getSelect().clear(); changed(); break;; // delete

      case 0x0d: mChanged = true; changed(); break; // enter - play file

      // crude chan,bitrate change
      case '1' : mSong.setChan ("bbc_radio_one"); mChanged = true; break;
      case '2' : mSong.setChan ("bbc_radio_two"); mChanged = true; break;
      case '3' : mSong.setChan ("bbc_radio_three"); mChanged = true; break;
      case '4' : mSong.setChan ("bbc_radio_fourfm"); mChanged = true;  break;
      case '5' : mSong.setChan ("bbc_radio_five_live"); mChanged = true; break;
      case '6' : mSong.setChan ("bbc_6music"); mChanged = true; break;
      case '7' : mSong.setBitrate (48000); mChanged = true; break;
      case '8' : mSong.setBitrate (96000); mChanged = true; break;
      case '9' : mSong.setBitrate (128000); mChanged = true; break;
      case '0' : mSong.setBitrate (320000); mChanged = true; break;

      default  : cLog::log (LOGINFO, "key %x", key); mChanged = true; break;
      }

    return false;
    }
  //}}}

private:
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
      const string path = "pool_904/live/uk/" + mSong.getChan() +
                          "/" + mSong.getChan() + ".isml/" + mSong.getChan() +
                          "-audio=" + dec(mSong.getBitrate());
      cWinSockHttp http;
      auto redirectedHost = http.getRedirect (host, path + ".norewind.m3u8");
      if (http.getContent()) {
        //{{{  parse m3u8 for baseChunkNum, baseTimePoint
        // point to #EXT-X-MEDIA-SEQUENCE: sequence num
        char* kExtSeq = "#EXT-X-MEDIA-SEQUENCE:";
        const auto extSeq = strstr ((char*)http.getContent(), kExtSeq) + strlen (kExtSeq);
        char* extSeqEnd = strchr (extSeq, '\n');
        *extSeqEnd = '\0';
        int baseChunkNum = atoi (extSeq) + 3;

        // point to #EXT-X-PROGRAM-DATE-TIME: dateTime str
        const auto kExtDateTime = "#EXT-X-PROGRAM-DATE-TIME:";
        const auto extDateTime = strstr (extSeqEnd + 1, kExtDateTime) + strlen (kExtDateTime);
        const auto extDateTimeEnd =  strstr (extDateTime + 1, "\n");
        const auto extDateTimeString = string (extDateTime, size_t(extDateTimeEnd - extDateTime));
        cLog::log (LOGINFO, kExtDateTime + extDateTimeString);

        // parse ISO time format from string
        istringstream inputStream (extDateTimeString);
        system_clock::time_point baseTimePoint;
        inputStream >> date::parse ("%FT%T", baseTimePoint);
        baseTimePoint -= 17s;

        http.freeContent();
        //}}}
        mSong.init (cAudioDecode::eAac, 2, mSong.getBitrate() >= 128000 ? 1024 : 2048, 48000);
        mSong.setHlsBase (baseChunkNum, baseTimePoint);
        cAudioDecode decode (cAudioDecode::eAac);
        float* samples = (float*)malloc (mSong.getMaxSamplesPerFrame() * mSong.getNumSampleBytes());

        mChanged = false;
        while (!getExit() && !mChanged) {
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
                // add aacFrame from aacFrames to song
                auto numSamples = decode.frameToSamples (samples);
                if (numSamples) {
                  // copy single aacFrame to aacFrame, add to song which owns it
                  int aacFrameLen = decode.getFrameLen();
                  auto aacFrame = (uint8_t*)malloc (aacFrameLen);
                  memcpy (aacFrame, decode.getFramePtr(), aacFrameLen);

                  // frame fixup aacHE sampleRate, samplesPerFrame, !!! total estimate not right !!!
                  mSong.setSampleRate (decode.getSampleRate());
                  mSong.setSamplesPerFrame (numSamples);
                  mSong.addFrame (seqFrameNum++, true, aacFrame, aacFrameLen, mSong.getNumFrames(), samples);
                  changed();
                  }
                aacFrames += decode.getNextFrameOffset();
                }
              http.freeContent();
              mSong.setHlsLoad (cSong::eHlsIdle, chunkNum);
              }
            else {
              //{{{  failed, back off for 250ms
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
        free (samples);
        }
      }

    cLog::log (LOGINFO, "exit");
    setExit();
    }
  //}}}
  //{{{
  void icyThread (const string& url) {

    cLog::setThreadName ("icy ");

    int icySkipCount = 0;
    int icySkipLen = 0;
    int icyInfoCount = 0;
    int icyInfoLen = 0;
    char icyInfo[255] = { 0 };

    uint8_t bufferFirst[2048];
    uint8_t* bufferEnd = bufferFirst;
    uint8_t* buffer = bufferFirst;

    int frameNum = -1;
    cAudioDecode decode (cAudioDecode::eAac);
    float* samples = nullptr;

    cUrl parsedUrl;
    parsedUrl.parse (url);

    cWinSockHttp http;
    http.get (parsedUrl.getHost(), parsedUrl.getPath(), "Icy-MetaData: 1",
      //{{{  lambda headerCallback
      [&](const string& key, const string& value) noexcept {
        if (key == "icy-metaint")
          icySkipLen = stoi (value);
        },
      //}}}
      //{{{  lambda dataCallback
      [&](const uint8_t* data, int length) noexcept {
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
          }
          //}}}

        if (frameNum < 0) {
          frameNum = 0;
          int sampleRate;
          auto frameType = cAudioDecode::parseSomeFrames (bufferFirst, bufferEnd, sampleRate);
          mSong.init (frameType, 2, (frameType == cAudioDecode::eMp3) ? 1152 : 2048, sampleRate);
          mSong.setStreaming();
          samples = (float*)malloc (mSong.getSamplesPerFrame() * mSong.getNumSampleBytes());
          }

        while (decode.parseFrame (buffer, bufferEnd)) {
          if (decode.getFrameType() == mSong.getFrameType()) {
            auto numSamples = decode.frameToSamples (samples);
            if (numSamples) {
              int framelen = decode.getFrameLen();
              auto frame = (uint8_t*)malloc (framelen);
              memcpy (frame, decode.getFramePtr(), framelen);

              // frame fixup aacHE sampleRate, samplesPerFrame
              mSong.setSampleRate (decode.getSampleRate());
              mSong.setSamplesPerFrame (numSamples);
              mSong.addFrame (frameNum++, true, frame, framelen, mSong.getNumFrames()+1, samples);
              changed();
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
        }
      //}}}
      );

    // never exits
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

      int frameNum = 0;
      bool songDone = false;
      auto fileMapPtr = fileMapFirst;
      if (frameType == cAudioDecode::eWav) {
        //{{{  float 32bit interleaved wav uses file map directly
        auto frameSamples = 1024;
        mSong.init (frameType, 2, frameSamples, sampleRate);

        cAudioDecode decode (cAudioDecode::eWav);
        decode.parseFrame (fileMapPtr, fileMapEnd);
        auto data = decode.getFramePtr();

        auto frameSampleBytes = frameSamples * 2 * 4;
        while (!getExit() && !mChanged && !songDone) {
          mSong.addFrame (frameNum++, false, data, frameSampleBytes, fileMapSize / frameSampleBytes, (float*)data);
          data += frameSampleBytes;
          changed();
          songDone = (data + frameSampleBytes) > fileMapEnd;
          }
        }
        //}}}
      else {
        mSong.init (frameType, 2, (frameType == cAudioDecode::eMp3) ? 1152 : 2048, sampleRate);

        cAudioDecode decode (frameType);
        auto samples = (float*)malloc (mSong.getSamplesPerFrame() * mSong.getNumSampleBytes());

        while (!getExit() && !mChanged && !songDone) {
          while (decode.parseFrame (fileMapPtr, fileMapEnd)) {
            if (decode.getFrameType() == mSong.getFrameType()) {
              auto numSamples = decode.frameToSamples (samples);
              if (numSamples) {
                // frame fixup aacHE sampleRate, samplesPerFrame
                mSong.setSampleRate (decode.getSampleRate());
                mSong.setSamplesPerFrame (numSamples);
                int numFrames = mSong.getNumFrames();
                int totalFrames = (numFrames > 0) ? int(fileMapEnd - fileMapFirst) / (int(decode.getFramePtr() - fileMapFirst) / numFrames) : 0;
                mSong.addFrame (frameNum++, false, decode.getFramePtr(), decode.getFrameLen(), totalFrames+1, samples);
                changed();
                }
              }
            fileMapPtr += decode.getNextFrameOffset();
            }
          cLog::log (LOGINFO, "song done");
          songDone = true;
          }
        // done
        free (samples);
        }

      // wait for play to end or abort
      mPlayDoneSem.wait();
      //{{{  next file
      UnmapViewOfFile (fileMapFirst);
      CloseHandle (fileHandle);

      if (mChanged) // use changed fileIndex
        mChanged = false;
      else if (!mFileList->nextIndex())
        break;
      //}}}
      }

    cLog::log (LOGINFO, "exit");
    setExit();
    }
  //}}}
  //{{{
  void playThread() {

    cLog::setThreadName ("play");

    // wait for valid sampleRate to decalre audioDevice
    while (!mSong.getSampleRate())
      this_thread::sleep_for (10ms);

    SetThreadPriority (GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    while (!getExit()) {
      // loop for filelist, new song may change sampleRate
      auto device = getDefaultAudioOutputDevice();
      if (device) {
        device->setSampleRate (mSong.getSampleRate());

        cAudioDecode decode (mSong.getFrameType());
        float* samples = mSong.hasSamples() ? nullptr : (float*)malloc (mSong.getMaxSamplesBytes());

        device->start();
        while (!getExit() && (mSong.getStreaming() || (mSong.getPlayFrame() <= mSong.getLastFrame()))) {
          auto framePtr = mSong.getFramePtr (mSong.getPlayFrame());
          if (mPlaying && framePtr && framePtr->getPtr()) {
            device->process ([&](float*& srcSamples, int& numSrcSamples) mutable noexcept {
              // lambda callback - load srcSamples
              shared_lock<shared_mutex> lock (mSong.getSharedMutex());
              if (mSong.hasSamples())
                srcSamples = (float*)framePtr->getPtr();
              else {
                decode.setFrame (framePtr->getPtr(), framePtr->getLen());
                decode.frameToSamples (samples);
                srcSamples = samples;
                }
              numSrcSamples = mSong.getSamplesPerFrame();
              mSong.incPlayFrame (1, true);
              changed();
              });
            }
          else
            this_thread::sleep_for (100ms);
          }

        device->stop();
        free (samples);
        }

      mPlayDoneSem.notifyAll();
      }

    cLog::log (LOGINFO, "exit");
    }
  //}}}

  //{{{  vars
  cSong mSong;

  bool mChanged = false;
  bool mPlaying = true;
  cSemaphore mPlayDoneSem = "playDone";

  cFileList* mFileList = nullptr;
  cJpegImageView* mJpegImageView = nullptr;
  string mLastTitleStr;

  string mDebugStr;
  cBox* mLogBox = nullptr;
  //}}}
  };

// main
int __stdcall WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::init (LOGINFO, true, "", "playWindow");
  av_log_set_level (AV_LOG_VERBOSE);
  av_log_set_callback (cLog::avLogCallback);

  int numArgs;
  auto args = CommandLineToArgvW (GetCommandLineW(), &numArgs);

  cAppWindow appWindow;
  appWindow.run ("playWindow", 800, 420, (numArgs > 1) ? wcharToString (args[1]) : "");

  CoUninitialize();
  return 0;
  }
