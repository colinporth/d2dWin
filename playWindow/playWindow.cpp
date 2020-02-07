// playWindow.cpp
//{{{  includes
#include "stdafx.h"

// should be in stdafx.h
#include "../../shared/utils/cFileList.h"
#include "../boxes/cFileListBox.h"

#include "cSong.h"
#include "cSongBoxes.h"

#include "cAudioDecode.h"
#include "audioWASAPI.h"

using namespace std;
//}}}

class cAppWindow : public cD2dWindow {
public:
  //{{{
  void run (bool streaming, const string& title, int width, int height, const string& name) {

    initialise (title, width, height, false);

    mJpegImageView = new cJpegImageView (this, 0.f,-220.f, false, false, mSong.mImage);
    add (mJpegImageView);

    add (new cCalendarBox (this, 190.f,150.f, mTimePoint), -190.f,0.f);
    add (new cClockBox (this, 40.f, mTimePoint), -135.f,35.f);
    add (new cSongBox (this, 0.f,0.f, mSong), 0.f,0.f);
    add (new cLogBox (this, 200.f,-200.f, true), 0.f,-200.f)->setPin (false);

    mFileList = new cFileList (name, "*.aac;*.mp3;*.wav");
    thread([=]() { mFileList->watchThread(); }).detach();
    add (new cAppFileListBox (this, 0.f,-220.f, mFileList))->setPin (true);

    mVolumeBox = new cVolumeBox (this, 12.f,0.f, nullptr);
    add (mVolumeBox, -12.f,0.f);

    // last box
    add (new cWindowBox (this, 60.f,24.f), -60.f,0.f)->setPin (false);

    if (streaming) {
      // allocate simnple big buffer for stream
      mStreamFirst = (uint8_t*)malloc (200000000);
      mStreamLast = mStreamFirst;

      //{{{
      const static string kPathNames[] = { "none",
                                           "bbc_radio_one",
                                           "bbc_radio_two",
                                           "bbc_radio_three",
                                           "bbc_radio_fourfm",
                                           "bbc_radio_five_live",
                                           "bbc_6music" };
      //}}}
      const static int kBitrates [] = { 48000, 96000, 128000, 320000 };
      thread ([=]() { hlsThread ("as-hls-uk-live.bbcfmt.hs.llnwd.net", "bbc_radio_fourfm", 48000); }).detach();
      //thread ([=]() { icyThread (name); }).detach();
      //thread ([=](){ analyseThread (streaming); }).detach();
      }
    else if (streaming || !mFileList->empty())
      thread ([=](){ analyseThread (streaming); }).detach();

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
      case  'F': toggleFullScreen(); break;

      case  ' ': mPlaying = !mPlaying; break;

      case 0x21: mSong.prevSilence(); changed(); break;; // page up
      case 0x22: mSong.nextSilence(); changed(); break;; // page down

      case 0x25: mSong.incPlaySec (getControl() ? -10 : -1);  changed(); break; // left arrow  - 1 sec
      case 0x27: mSong.incPlaySec (getControl() ?  10 :  1);  changed(); break; // right arrow  + 1 sec

      case 0x24: mSong.setPlayFrame (0); changed(); break; // home
      case 0x23: mSong.setPlayFrame (mSong.getTotalFrames() -1); changed(); break; // end

      case 0x26: if (mFileList->prevIndex()) changed(); break; // up arrow
      case 0x28: if (mFileList->nextIndex()) changed(); break; // down arrow
      case 0x0d: mSongChanged = true; changed(); break; // enter - play file

      default  : cLog::log (LOGINFO, "key %x", key);
      }

    return false;
    }
  //}}}

private:
  //{{{
  class cAppFileListBox : public cFileListBox {
  public:
    cAppFileListBox (cD2dWindow* window, float width, float height, cFileList* fileList) :
      cFileListBox (window, width, height, fileList) {}

    void onHit() {
      (dynamic_cast<cAppWindow*>(getWindow()))->mSongChanged = true;
      }
    };
  //}}}

  //{{{
  void addIcyInfo (const string& icyInfo) {
  // called by httpThread

    cLog::log (LOGINFO1, "addIcyInfo " + icyInfo);

    string icysearchStr = "StreamTitle=\'";
    string searchStr = "StreamTitle=\'";
    auto searchStrPos = icyInfo.find (searchStr);
    if (searchStrPos != string::npos) {
      auto searchEndPos = icyInfo.find ("\';", searchStrPos + searchStr.size());
      if (searchEndPos != string::npos) {
        string titleStr = icyInfo.substr (searchStrPos + searchStr.size(), searchEndPos - searchStrPos - searchStr.size());
        if (titleStr != mLastTitleStr) {
          cLog::log (LOGINFO1, "addIcyInfo found title = " + titleStr);
          mSong.setTitle (titleStr);
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
  static uint8_t* extractAacFramesFromTs (uint8_t* stream, uint8_t* ts, int tsLen) {
  // extract aacFrames to stream from chunk tsPackets, ts and stream can be same buffer

    auto tsDone = ts + tsLen - 188;
    while ((ts <= tsDone) && (*ts++ == 0x47)) {
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

        // copy ts payload aacFrames into stream
        memcpy (stream, ts, tsBodyBytes);
        stream += tsBodyBytes;
        }

      ts += tsBodyBytes;
      }

    return stream;
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

    cHttp::cUrl parsedUrl;
    parsedUrl.parse (url);

    cWinSockHttp http;
    http.get (parsedUrl.getHost(), parsedUrl.getPath(), "Icy-MetaData: 1",
      //{{{  headerCallback lambda
      [&](const string& key, const string& value) noexcept {
        if (key == "icy-metaint")
          icySkipLen = stoi (value);
        },
      //}}}
      //{{{  dataCallback lambda
      [&](const uint8_t* data, int length) noexcept {
        // cLog::log (LOGINFO, "callback %d", length);
        if ((icyInfoCount >= icyInfoLen)  &&
            (icySkipCount + length <= icySkipLen)) {
          // simple copy of whole body, no metaInfo
          cLog::log (LOGINFO1, "body simple copy len:%d", length);
          memcpy (mStreamLast, data, length);
          mStreamLast += length;
          icySkipCount += length;
          }
        else {
          // dumb copy for metaInfo straddling body, could be much better
          cLog::log (LOGINFO1, "body split copy length:%d info:%d:%d skip:%d:%d ",
                                length, icyInfoCount, icyInfoLen, icySkipCount, icySkipLen);
          for (int i = 0; i < length; i++) {
            if (icyInfoCount < icyInfoLen) {
              icyInfo [icyInfoCount] = data[i];
              icyInfoCount++;
              if (icyInfoCount >= icyInfoLen)
                addIcyInfo (icyInfo);
              }
            else if (icySkipCount >= icySkipLen) {
              icyInfoLen = data[i] * 16;
              icyInfoCount = 0;
              icySkipCount = 0;
              cLog::log (LOGINFO1, "body icyInfo len:", data[i] * 16);
              }
            else {
              icySkipCount++;
              *mStreamLast = data[i];
              mStreamLast++;
              }
            }
          }

        mStreamSem.notifyAll();
        }
      //}}}
      );

    cLog::log (LOGINFO, "exit");
    }
  //}}}
  //{{{
  void hlsThread (string host, const string& chan, int bitrate) {
  // hls chunk http load and analyse thread, single thread helps channel change and jumping backwards

  //  hls const
    const string kM3u8Suffix = ".norewind.m3u8";
    cLog::setThreadName ("hls ");

    mSong.init (cAudioDecode::eAac, 2, bitrate <= 96000 ? 2048 : 1024, 48000);

    cWinSockHttp http;
    string path = "pool_904/live/uk/" + chan + '/' + chan + ".isml/" + chan + "-audio=" + dec(bitrate);
    host = http.getRedirect (host, path + ".norewind.m3u8");
    if (http.getContent()) {
      //{{{  parse m3u8 for startSeqNum, startDatePoint, startTimePoint
      // point to #EXT-X-MEDIA-SEQUENCE: sequence num
      char* kExtSeq = "#EXT-X-MEDIA-SEQUENCE:";
      const auto extSeq = strstr ((char*)http.getContent(), kExtSeq) + strlen (kExtSeq);
      char* extSeqEnd = strchr (extSeq, '\n');
      *extSeqEnd = '\0';
      uint32_t startSeqNum = atoi (extSeq) + 3;

      // point to #EXT-X-PROGRAM-DATE-TIME: dateTime str
      const auto kExtDateTime = "#EXT-X-PROGRAM-DATE-TIME:";
      const auto extDateTime = strstr (extSeqEnd + 1, kExtDateTime) + strlen (kExtDateTime);
      const auto extDateTimeEnd =  strstr (extDateTime + 1, "\n");
      const auto extDateTimeString = string (extDateTime, size_t(extDateTimeEnd - extDateTime));
      cLog::log (LOGINFO, kExtDateTime + extDateTimeString);
      http.freeContent();

      // parse ISO time format from string
      chrono::system_clock::time_point startTimePoint;
      istringstream inputStream (extDateTimeString);
      inputStream >> date::parse ("%FT%T", startTimePoint);

      chrono::system_clock::time_point startDatePoint;
      startDatePoint = date::floor<date::days>(startTimePoint);

      // 6.4 secsPerChunk, 300/150 framesPerChunk, 1024/2048 samplesPerFrame
      const float kSamplesPerSecond = 48000.f;
      const float kSamplesPerFrame = (bitrate <= 96000) ? 2048.f : 1024.f;
      const float kFramesPerSecond = kSamplesPerSecond / kSamplesPerFrame;
      const float kStartTimeSecondsOffset = 17.f;

      const auto seconds = chrono::duration_cast<chrono::seconds>(startTimePoint - startDatePoint);
      uint32_t startFrame = uint32_t((uint32_t(seconds.count()) - kStartTimeSecondsOffset) * kFramesPerSecond);
      //}}}

      cAudioDecode decode (cAudioDecode::eAac);
      float* samples = (float*)malloc (mSong.getMaxSamplesPerFrame() * mSong.getNumSampleBytes());

      auto stream = mStreamFirst;
      auto seqNum = startSeqNum;
      while (!getExit()) {
        // get hls seqNum chunk
        if (http.get (host, path + '-' + dec(seqNum) + ".ts") == 200) {
          mStreamLast = extractAacFramesFromTs (mStreamLast, http.getContent(), http.getContentSize());
          http.freeContent();

          while (decode.parseFrame (stream, mStreamLast)) {
            //  add aacFrame from stream to song
            auto numSamples = decode.frameToSamples (samples);
            if (numSamples) {
              // frame fixup aacHE sampleRate, samplesPerFrame
              mSong.setSampleRate (decode.mSampleRate);
              mSong.setSamplesPerFrame (decode.mNumSamples);
              if (mSong.addFrame (decode.mFramePtr, decode.mFrameLen, mSong.getNumFrames()+1, numSamples, samples))
                thread ([=](){ playThread (true); }).detach();
              changed();
              }
            stream += decode.mSkip + decode.mFrameLen;
            }
          seqNum++;
          }
        else // wait for next hls chunk
          Sleep (1000);
        }
      free (samples);
      }

    mPlayDoneSem.wait();
    cLog::log (LOGINFO, "exit");
    setExit();
    }
  //}}}
  //{{{
  void analyseThread (bool streaming) {

    cLog::setThreadName ("anal");

    while (!getExit()) {
      HANDLE fileHandle = 0;
      HANDLE mapping = 0;

      if (streaming) {
        //{{{  wait for a bit of stream
        while (streaming && (mStreamLast - mStreamFirst) < 1440)
          mStreamSem.wait();
        }
        //}}}
      else {
        //{{{  openFile mapping to stream
        fileHandle = CreateFile (mFileList->getCurFileItem().getFullName().c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        mapping = CreateFileMapping (fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);

        mStreamFirst = (uint8_t*)MapViewOfFile (mapping, FILE_MAP_READ, 0, 0, 0);
        mStreamLast = mStreamFirst + GetFileSize (fileHandle, NULL);
        }
        //}}}

      // sampleRate for aac-sbr wrong in header, fixup later, use a maxValue for samples alloc
      int sampleRate;
      auto frameType = cAudioDecode::parseFrames (mStreamFirst, mStreamLast, sampleRate);
      if (cAudioDecode::mJpegPtr) {
        //{{{  add jpeg
        mSong.mImage = new cJpegImage();
        mSong.mImage->setBuf (cAudioDecode::mJpegPtr, cAudioDecode::mJpegLen);
        mJpegImageView->setImage (mSong.mImage);
        }
        //}}}

      bool songDone = false;
      auto stream = mStreamFirst;
      if (frameType == cAudioDecode::eWav) {
        //{{{  float 32bit interleaved wav uses mapped stream directly
        auto frameSamples = 1024;
        mSong.init (frameType, 2, frameSamples, sampleRate);

        cAudioDecode decode (cAudioDecode::eWav);
        decode.parseFrame (stream, mStreamLast);
        auto data = decode.mFramePtr;

        auto frameSampleBytes = frameSamples * 2 * 4;
        while (!getExit() && !mSongChanged && !songDone) {
          int totalFrames = int(mStreamLast - mStreamFirst) / frameSampleBytes;
          if (mSong.addFrame (data, frameSampleBytes, totalFrames, frameSamples, (float*)data))
            thread ([=](){ playThread (false); }).detach();

          data += frameSampleBytes;
          changed();

          songDone = (data + frameSampleBytes) > mStreamLast;
          }
        }
        //}}}
      else {
        mSong.init (frameType, 2, (frameType == cAudioDecode::eMp3) ? 1152 : 2048, sampleRate);

        cAudioDecode decode (frameType);
        auto samples = (float*)malloc (mSong.getSamplesPerFrame() * mSong.getNumSampleBytes());

        while (!getExit() && !mSongChanged && !songDone) {
          while (decode.parseFrame (stream, mStreamLast)) {
            if (decode.mFrameType == mSong.getFrameType()) {
              auto numSamples = decode.frameToSamples (samples);
              if (numSamples) {
                // frame fixup aacHE sampleRate, samplesPerFrame
                mSong.setSampleRate (decode.mSampleRate);
                mSong.setSamplesPerFrame (decode.mNumSamples);
                int numFrames = mSong.getNumFrames();
                int totalFrames = (numFrames > 0) ? int(mStreamLast - mStreamFirst) / (int(decode.mFramePtr - mStreamFirst) / numFrames) : 0;
                if (mSong.addFrame (decode.mFramePtr, decode.mFrameLen, totalFrames+1, numSamples, samples))
                  thread ([=](){ playThread (streaming); }).detach();
                changed();
                }
              }
            stream += decode.mSkip + decode.mFrameLen;
            }
          if (streaming)
            mStreamSem.wait();
          else {
            cLog::log (LOGINFO, "song done");
            songDone = true;
            }
          }
        // done
        free (samples);
        }

      // wait for play to end or abort
      mPlayDoneSem.wait();
      if (!streaming && fileHandle) {
        //{{{  next file
        UnmapViewOfFile (mStreamFirst);
        CloseHandle (fileHandle);

        if (mSongChanged) // use changed fileIndex
          mSongChanged = false;
        else if (!mFileList->nextIndex())
          break;
        }
        //}}}
      }

    cLog::log (LOGINFO, "exit");
    setExit();
    }
  //}}}
  //{{{
  void playThread (bool streaming) {

    cLog::setThreadName ("play");

    auto device = getDefaultAudioOutputDevice();
    if (device) {
      device->setSampleRate (mSong.getSampleRate());
      SetThreadPriority (GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

      cAudioDecode decode (mSong.getFrameType());
      float* samples = (mSong.getFrameType() == cAudioDecode::eWav) ?
                         nullptr : (float*)malloc (mSong.getMaxSamplesPerFrame() * mSong.getNumSampleBytes());

      device->start();
      while (!getExit() && !mSongChanged &&
             (streaming || (mSong.mPlayFrame <= mSong.getLastFrame())))
        if (mPlaying) {
          //{{{
          cLog::log (LOGINFO2, "process for frame:%d", mSong.getPlayFrame());
          //}}}
          device->process ([&](float*& srcSamples, int& numSrcSamples,
                               int numDstSamplesLeft, int numDstSamples) mutable noexcept {
            // lambda callback - load srcSamples
            //{{{
            cLog::log (LOGINFO3, " - callback for src:%d dst:%d:%d",
                       mSong.getSamplesPerFrame(), numDstSamplesLeft, numDstSamples);
            //}}}
            if (mSong.isFramePtrSamples())
              srcSamples = (float*)mSong.getPlayFramePtr();
            else {
              decode.setFrame (mSong.getPlayFramePtr(), mSong.getPlayFrameLen());
              decode.frameToSamples (samples);
              srcSamples = samples;
              }
            numSrcSamples = mSong.getSamplesPerFrame();
            mSong.incPlayFrame (1);
            });

          changed();
          }
        else
          Sleep (10);

      device->stop();
      free (samples);
      }

    mPlayDoneSem.notifyAll();
    cLog::log (LOGINFO, "exit");
    }
  //}}}

  //{{{  vars
  cFileList* mFileList;

  cSong mSong;
  uint8_t* mStreamFirst = nullptr;
  uint8_t* mStreamLast = nullptr;
  uint8_t* mStreamTemp = nullptr;
  cSemaphore mStreamSem;

  cJpegImageView* mJpegImageView = nullptr;

  bool mSongChanged = false;
  bool mPlaying = true;
  cSemaphore mPlayDoneSem = "playDone";

  cVolumeBox* mVolumeBox = nullptr;
  string mLastTitleStr;
  //}}}
  };

// main
//{{{
int __stdcall WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::init (LOGINFO, true, "", "playWindow");
  av_log_set_level (AV_LOG_VERBOSE);
  av_log_set_callback (cLog::avLogCallback);

  cAppWindow appWindow;

  int numArgs;
  auto args = CommandLineToArgvW (GetCommandLineW(), &numArgs);
  if (numArgs > 1)
    appWindow.run (false, "playWindow", 800, 600, wcharToString (args[1]));
  else {
    //{{{  urls
    //const string url = "http://stream.wqxr.org/wqxr.aac";
    //const string url = "http://tx.planetradio.co.uk/icecast.php?i=jazzhigh.aac";
    //const string url = "http://us4.internet-radio.com:8266/";
    //const string url = "http://tx.planetradio.co.uk/icecast.php?i=countryhits.aac";
    //const string url = "http://live-absolute.sharp-stream.com/absoluteclassicrockhigh.aac";
    //const string url = "http://media-ice.musicradio.com:80/SmoothCountry";
    //}}}
    const string url = "http://stream.wqxr.org/js-stream.aac";
    appWindow.run (true, "playWindow " + url, 800, 600, url);
    }

  CoUninitialize();
  return 0;
  }
//}}}
