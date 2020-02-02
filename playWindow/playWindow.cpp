// playWindow.cpp
//{{{  includes
#include "stdafx.h"

#include "../../shared/net/cWinSockHttp.h"

// should be in stdafx.h
#include "../../shared/utils/cFileList.h"
#include "../boxes/cFileListBox.h"

#include "../../shared/decoders/audioParser.h"
#include "../../shared/utils/cSong.h"
#include "../boxes/cSongBoxes.h"

#include "audioWASAPI.h"

using namespace std;
//}}}

class cAppWindow : public cD2dWindow {
public:
  cAppWindow() : mPlayDoneSem("playDone") {}
  virtual ~cAppWindow() {}
  //{{{
  void run (bool streaming, const string& title, int width, int height, const string& name) {

    initialise (title, width, height, false);

    mJpegImageView = new cJpegImageView (this, 0.f,-220.f, false, false, mSong.mImage);
    add (mJpegImageView);

    add (new cCalendarBox (this, 190.f,150.f, mTimePoint), -190.f,0.f);
    add (new cClockBox (this, 40.f, mTimePoint), -135.f,35.f);

    add (new cLogBox (this, 200.f,-200.f, true), 0.f,-200.f)->setPin (false);

    add (new cSongBox (this, 0,450.f,100.f, mSong), 0.f,0.f);
    add (new cSongLensBox (this, 0,100.f, mSong), 0.f,-150.f);
    add (new cSongTimeBox (this, 600.f,50.f, mSong), -600.f,-50.f);
    add (new cSongFreqBox (this, 0,150.f, mSong), 0.f,-150.f);

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
      thread ([=]() { hlsThread (3, 96000); }).detach();
      //thread ([=]() { httpThread (name); }).detach();
      }

    if (streaming || !mFileList->empty())
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

  // shoutcast http curl
  //{{{
  static int httpTrace (CURL* handle, curl_infotype type, char* ptr, size_t size, cAppWindow* appWindow) {

    switch (type) {
      case CURLINFO_TEXT:
        cLog::log (LOGINFO1, "CURL %s", ptr);
        break;


      case CURLINFO_HEADER_OUT:
        cLog::log (LOGINFO1, "HOUT %s", ptr);
        break;

      case CURLINFO_HEADER_IN:
        cLog::log (LOGINFO1, "H_IN %s", ptr);
        break;


      case CURLINFO_DATA_OUT:
        cLog::log (LOGINFO1, "DOUT %d bytes", size);
        break;

      case CURLINFO_DATA_IN:
        cLog::log (LOGINFO1, "D_IN %d bytes", size);
        break;


      case CURLINFO_SSL_DATA_OUT:
        cLog::log (LOGINFO1, "SOUT %d bytes", size);
        break;

      case CURLINFO_SSL_DATA_IN:
        cLog::log (LOGINFO1, "SDIN %d bytes", size);
        break;


      default:
        return 0;
      }

    return 0;
    }
  //}}}
  //{{{
  static size_t httpHeader (const char* ptr, size_t size, size_t numItems, cAppWindow* appWindow) {

    auto len = numItems * size;
    cLog::log (LOGINFO2, "len:%d  %s", len, ptr);

    string str (ptr);
    string searchStr ("icy-metaint:");

    auto searchStrPos = str.find (searchStr);
    if (searchStrPos != string::npos) {
      auto numStr = str.substr (searchStrPos + searchStr.size(), str.size() - searchStrPos - searchStr.size());
      auto num = stoi (numStr);
      cLog::log (LOGINFO, "httpHeader - found %s value:%d", searchStr.c_str(), num);

      appWindow->mIcySkipLen = num;
      }

    return len;
    }
  //}}}
  //{{{
  static size_t httpBody (uint8_t* ptr, size_t size, size_t numItems, cAppWindow* appWindow) {

    auto len = numItems * size;

    if ((appWindow->mIcyInfoCount >= appWindow->mIcyInfoLen)  &&
        (appWindow->mIcySkipCount + len <= appWindow->mIcySkipLen)) {

      cLog::log (LOGINFO1, "body simple copy len:%d", len);

      // simple copy of whole body, no metaInfo
      memcpy (appWindow->mStreamLast, ptr, len);
      appWindow->mStreamLast += len;
      appWindow->mIcySkipCount += (int)len;
      }

    else {
      cLog::log (LOGINFO1, "body split copy len:%d info:%d:%d skip:%d:%d ",
                            len,
                            appWindow->mIcyInfoCount, appWindow->mIcyInfoLen,
                            appWindow->mIcySkipCount, appWindow->mIcySkipLen);

      // dumb copy for metaInfo straddling body, could be much better
      for (int i = 0; i < len; i++) {
        if (appWindow->mIcyInfoCount < appWindow->mIcyInfoLen) {
          appWindow->mIcyInfo [appWindow->mIcyInfoCount] = ptr[i];
          appWindow->mIcyInfoCount++;
          if (appWindow->mIcyInfoCount >= appWindow->mIcyInfoLen)
            appWindow->addIcyInfo (appWindow->mIcyInfo);
          }
        else if (appWindow->mIcySkipCount >= appWindow->mIcySkipLen) {
          appWindow->mIcyInfoLen = ptr[i] * 16;
          appWindow->mIcyInfoCount = 0;
          appWindow->mIcySkipCount = 0;
          cLog::log (LOGINFO1, "body icyInfo len:", ptr[i] * 16);
          }
        else {
          appWindow->mIcySkipCount++;
          *appWindow->mStreamLast = ptr[i];
          appWindow->mStreamLast++;
          }
        }
      }

    appWindow->mStreamSem.notifyAll();
    return len;
    }
  //}}}
  //{{{
  void addIcyInfo (string icyInfo) {
  // called by httpThread

    mIcyStr = icyInfo;
    cLog::log (LOGINFO1, "addIcyInfo " + mIcyStr);

    string searchStr = "StreamTitle=\'";
    auto searchStrPos = mIcyStr.find (searchStr);
    if (searchStrPos != string::npos) {
      auto searchEndPos = mIcyStr.find ("\';", searchStrPos + searchStr.size());
      if (searchEndPos != string::npos) {
        string titleStr = mIcyStr.substr (searchStrPos + searchStr.size(), searchEndPos - searchStrPos - searchStr.size());
        if (titleStr != mTitleStr) {
          cLog::log (LOGINFO1, "addIcyInfo found title = " + titleStr);
          mSong.setTitle (titleStr);
          mTitleStr = titleStr;
          }
        }
      }

    mUrlStr = "no url";
    searchStr = "StreamUrl=\'";
    searchStrPos = mIcyStr.find (searchStr);
    if (searchStrPos != string::npos) {
      auto searchEndPos = mIcyStr.find ('\'', searchStrPos + searchStr.size());
      if (searchEndPos != string::npos) {
        mUrlStr = mIcyStr.substr (searchStrPos + searchStr.size(), searchEndPos - searchStrPos - searchStr.size());
        cLog::log (LOGINFO1, "addIcyInfo found url = " + mUrlStr);
        }
      }
    }
  //}}}
  //{{{
  void httpThread (const char* url) {

    cLog::setThreadName ("http");

    auto curl = curl_easy_init();
    if (curl) {
      curl_easy_setopt (curl, CURLOPT_DEBUGFUNCTION, httpTrace);
      curl_easy_setopt (curl, CURLOPT_DEBUGDATA, this);
      curl_easy_setopt (curl, CURLOPT_VERBOSE, 1L);

      curl_easy_setopt (curl, CURLOPT_FOLLOWLOCATION, 1L);
      curl_easy_setopt (curl, CURLOPT_URL, url);

      struct curl_slist* slist = NULL;
      slist = curl_slist_append (slist, "Icy-MetaData: 1");
      curl_easy_setopt (curl, CURLOPT_HTTPHEADER, slist);

      curl_easy_setopt (curl, CURLOPT_HEADERFUNCTION, httpHeader);
      curl_easy_setopt (curl, CURLOPT_HEADERDATA, this);

      curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, httpBody);
      curl_easy_setopt (curl, CURLOPT_WRITEDATA, this);

      curl_easy_perform (curl);

      curl_slist_free_all (slist);
      }

    // never gets here
    curl_easy_cleanup (curl);

    cLog::log (LOGINFO, "exit");
    }
  //}}}

  // hls http
  //{{{
  void hlsThread (int chan, int bitrate) {

    const static int kBitrates [] = { 48000, 96000, 128000, 320000 };
    const static string kHost = "as-hls-uk-live.bbcfmt.hs.llnwd.net";
    const static string kPathNames[] = { "none",
                                         "bbc_radio_one",
                                         "bbc_radio_two",
                                         "bbc_radio_three",
                                         "bbc_radio_fourfm",
                                         "bbc_radio_five_live",
                                         "bbc_6music" };

    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::setThreadName ("hls ");

    const string channelPath = "pool_904/live/uk/" +
                               kPathNames[chan] + '/' +
                               kPathNames[chan] + ".isml/" +
                               kPathNames[chan] + "-audio=" + dec(bitrate);
    cWinSockHttp http;
    string redirectedHost = http.getRedirectable (kHost, channelPath + ".norewind.m3u8");
    if (http.getContent()) {
      cLog::log (LOGINFO, redirectedHost);

      // point to #EXT-X-MEDIA-SEQUENCE: sequence num strauto curl = curl_easy_init();
      char* kExtSeq = "#EXT-X-MEDIA-SEQUENCE:";
      const auto extSeq = strstr ((char*)http.getContent(), kExtSeq) + strlen (kExtSeq);
      char* extSeqEnd = strchr (extSeq, '\n');
      *extSeqEnd = '\0';
      uint32_t startSeqNum = atoi (extSeq) + 3;

      // point to #EXT-X-PROGRAM-DATE-TIME dateTime str
      const auto kExtDateTime = "#EXT-X-PROGRAM-DATE-TIME:";
      const auto extDateTime = strstr (extSeqEnd + 1, kExtDateTime) + strlen (kExtDateTime);
      const auto extDateTimeEnd =  strstr (extDateTime + 1, "\n");
      const auto extDateTimeString = string (extDateTime, size_t(extDateTimeEnd - extDateTime));
      cLog::log (LOGNOTICE, kExtDateTime + extDateTimeString);
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

      auto seqNum = startSeqNum;
      while (!getExit())
        if (http.get (redirectedHost, channelPath + '-' + dec(seqNum) + ".ts") == 200) {
          //{{{  parse aacFrames from transportStream packets
          auto ts = http.getContent();
          while ((ts < http.getContent() + http.getContentSize()) && (*ts++ == 0x47)) {
            // ts packet start
            auto payStart = ts[0] & 0x40;
            auto pid = ((ts[0] & 0x1F) << 8) | ts[1];
            auto headerBytes = (ts[2] & 0x20) ? 4 + ts[3] : 3;
            ts += headerBytes;
            auto tsBodyBytes = 187 - headerBytes;
            if (pid == 34) {
              if (payStart && !ts[0] && !ts[1] && (ts[2] == 1) && (ts[3] == 0xC0)) {
                int pesHeaderBytes = 9 + ts[8];
                ts += pesHeaderBytes;
                tsBodyBytes -= pesHeaderBytes;
                }
              // copy ts payload  into stream aacFrames
              memcpy (mStreamLast, ts, tsBodyBytes);
              mStreamLast += tsBodyBytes;  // could signal partial aac frame
              }
            ts += tsBodyBytes;
            }
          mStreamSem.notifyAll();

          http.freeContent();
          seqNum++;
          }
          //}}}
        else
          Sleep (6400);
      }

    cLog::log (LOGINFO, "exit");
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
      int streamSampleRate;
      auto audioFrameType = parseAudioFrames (mStreamFirst, mStreamLast, streamSampleRate);

      bool songDone = false;
      auto stream = mStreamFirst;
      if (audioFrameType == eWav) {
        //{{{  float 32bit interleaved wav uses mapped stream directly
        auto frameSamples = 1024;
        mSong.init (audioFrameType, 2, frameSamples, streamSampleRate);

        int skip;
        int sampleRate;
        eAudioFrameType frameType;
        uint8_t* data = nullptr;
        int dataSize = 0;
        parseAudioFrame (stream, mStreamLast, data, dataSize, frameType, skip, sampleRate);

        auto frameSampleBytes = frameSamples * 2 * 4;
        while (!getExit() && !mSongChanged && !songDone) {
          if (mSong.addFrame (uint32_t(data - mStreamFirst), frameSampleBytes, dataSize, frameSamples, (float*)data))
            thread ([=](){ playThread (false); }).detach();

          data += frameSampleBytes;
          changed();

          songDone = (data + frameSampleBytes) > mStreamLast;
          }
        }
        //}}}
      else {
        mSong.init (audioFrameType, 2, audioFrameType == eMp3 ? 1152 : 2048, streamSampleRate);

        //{{{  add jpeg if available
        int jpegLen = 0;
        auto jpegBuf = parseId3Tag (mStreamFirst, mStreamLast, jpegLen);
        if (jpegBuf) {
          mSong.mImage = new cJpegImage();
          mSong.mImage->setBuf (jpegBuf, jpegLen);
          mJpegImageView->setImage (mSong.mImage);
          }
        //}}}
        auto samples = (float*)malloc (mSong.getSamplesPerFrame() * mSong.getNumSampleBytes());

        auto codec = avcodec_find_decoder (mSong.getAudioFrameType() == eAac ? AV_CODEC_ID_AAC : AV_CODEC_ID_MP3);
        auto context = avcodec_alloc_context3 (codec);
        avcodec_open2 (context, codec, NULL);

        AVPacket avPacket;
        av_init_packet (&avPacket);
        auto avFrame = av_frame_alloc();

        while (!getExit() && !mSongChanged && !songDone) {
          int skip;
          int sampleRate;
          eAudioFrameType frameType;
          while (parseAudioFrame (stream, mStreamLast, avPacket.data, avPacket.size, frameType, skip, sampleRate)) {
            if (frameType == mSong.getAudioFrameType()) {
              auto ret = avcodec_send_packet (context, &avPacket);
              while (ret >= 0) {
                ret = avcodec_receive_frame (context, avFrame);
                if ((ret == AVERROR_EOF) || (ret < 0))
                  break;
                if ((ret != AVERROR(EAGAIN)) && (avFrame->nb_samples > 0)) {
                  if (avFrame->nb_samples != mSong.getSamplesPerFrame()) // fixup mSamplesPerFrame
                    mSong.setSamplesPerFrame (avFrame->nb_samples);
                  if (avFrame->sample_rate > mSong.getSampleRate()) // fixup aac-sbr sample rate
                    mSong.setSampleRate (avFrame->sample_rate);

                  //{{{  covert planar avFrame->data to interleaved float samples
                  switch (context->sample_fmt) {
                    case AV_SAMPLE_FMT_S16P: // 16bit signed planar
                      for (auto channel = 0; channel < avFrame->channels; channel++) {
                        auto srcPtr = (int16_t*)avFrame->data[channel];
                        auto dstPtr = (float*)(samples) + channel;
                        for (auto sample = 0; sample < avFrame->nb_samples; sample++) {
                          *dstPtr = *srcPtr++ / float(0x8000);
                          dstPtr += avFrame->channels;
                          }
                        }
                      break;

                    case AV_SAMPLE_FMT_FLTP: // 32bit float planar
                      for (auto channel = 0; channel < avFrame->channels; channel++) {
                        auto srcPtr = (float*)avFrame->data[channel];
                        auto dstPtr = (float*)(samples) + channel;
                        for (auto sample = 0; sample < avFrame->nb_samples; sample++) {
                          *dstPtr = *srcPtr++;
                          dstPtr += avFrame->channels;
                          }
                        }
                      break;

                    default:
                      cLog::log (LOGERROR, "playThread - unrecognised sample_fmt " + dec (context->sample_fmt));
                    }
                  //}}}
                  if (mSong.addFrame (uint32_t(avPacket.data - mStreamFirst), avPacket.size, int(mStreamLast - mStreamFirst),
                                      avFrame->nb_samples, samples))
                    thread ([=](){ playThread (streaming); }).detach();
                  changed();
                  }
                }
              }
            stream += skip + avPacket.size;
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
        av_frame_free (&avFrame);
        if (context)
          avcodec_close (context);
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

    setExit();
    }
  //}}}
  //{{{
  void playThread (bool streaming) {

    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::setThreadName ("play");

    auto device = getDefaultAudioOutputDevice();
    if (device) {
      device->setSampleRate (mSong.getSampleRate());
      SetThreadPriority (GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
      device->start();

      if (mSong.getAudioFrameType() == eWav) {
        //{{{  float 32 bit interleaved wav uses mapped stream directly
        while (!getExit() && !mSongChanged && (mSong.mPlayFrame <= mSong.getLastFrame()))
          if (mPlaying) {
            cLog::log (LOGINFO2, "process wav frame:%d", mSong.getPlayFrame());
            device->process ([&](float*& srcSamples, int& numSrcSamples,
                                 int numDstSamplesLeft, int numDstSamples) mutable noexcept {
              // load srcSamples callback lambda
              cLog::log (LOGINFO3, " - callback wav src:%d dst:%d:%d",
                         mSong.getSamplesPerFrame(), numDstSamplesLeft, numDstSamples);
              srcSamples = (float*)(mStreamFirst + mSong.getPlayFrameStreamIndex());
              numSrcSamples = mSong.getSamplesPerFrame();

              mSong.incPlayFrame (1);
              changed();
              });
            }
          else
            Sleep (10);

        device->stop();
        }
        //}}}
      else {
        //{{{  init decoder
        auto codec = avcodec_find_decoder (mSong.getAudioFrameType() == eAac ? AV_CODEC_ID_AAC : AV_CODEC_ID_MP3);
        auto context = avcodec_alloc_context3 (codec);
        avcodec_open2 (context, codec, NULL);

        AVPacket avPacket;
        av_init_packet (&avPacket);
        auto avFrame = av_frame_alloc();
        //}}}
        auto samples = (float*)malloc (mSong.getMaxSamplesPerFrame() * mSong.getNumSampleBytes());
        while (!getExit() && !mSongChanged &&
               (streaming || (mSong.mPlayFrame <= mSong.getLastFrame())))
          if (mPlaying) {
            cLog::log (LOGINFO2, "process for frame:%d", mSong.getPlayFrame());
            device->process ([&](float*& srcSamples, int& numSrcSamples,
                                 int numDstSamplesLeft, int numDstSamples) mutable noexcept {
              // load srcSamples callback lambda
              cLog::log (LOGINFO3, " - callback for src:%d dst:%d:%d",
                         mSong.getSamplesPerFrame(), numDstSamplesLeft, numDstSamples);

              int skip;
              int sampleRate;
              eAudioFrameType frameType;
              if (parseAudioFrame (mStreamFirst + mSong.getPlayFrameStreamIndex(), mStreamLast,
                                   avPacket.data, avPacket.size, frameType, skip, sampleRate))
                if (frameType == mSong.getAudioFrameType()) {
                  auto ret = avcodec_send_packet (context, &avPacket);
                  while (ret >= 0) {
                    ret = avcodec_receive_frame (context, avFrame);
                    if ((ret == AVERROR_EOF) || (ret < 0))
                      break;
                    if ((ret != AVERROR(EAGAIN)) && (avFrame->nb_samples > 0)) {
                      //{{{  covert planar avFrame->data to interleaved float samples
                      switch (context->sample_fmt) {
                        case AV_SAMPLE_FMT_S16P: // 16bit signed planar
                          for (auto channel = 0; channel < avFrame->channels; channel++) {
                            auto srcPtr = (int16_t*)avFrame->data[channel];
                            auto dstPtr = (float*)(samples) + channel;
                            for (auto sample = 0; sample < avFrame->nb_samples; sample++) {
                              *dstPtr = *srcPtr++ / float(0x8000);
                              dstPtr += avFrame->channels;
                              }
                            }
                          break;

                        case AV_SAMPLE_FMT_FLTP: // 32bit float planar
                          for (auto channel = 0; channel < avFrame->channels; channel++) {
                            auto srcPtr = (float*)avFrame->data[channel];
                            auto dstPtr = (float*)(samples) + channel;
                            for (auto sample = 0; sample < avFrame->nb_samples; sample++) {
                              *dstPtr = *srcPtr++;
                              dstPtr += avFrame->channels;
                              }
                            }
                          break;

                        default:
                          cLog::log (LOGERROR, "playThread - unrecognised sample_fmt " + dec (context->sample_fmt));
                        }
                      }
                      //}}}
                    }
                  }
              srcSamples = samples;
              numSrcSamples = mSong.getSamplesPerFrame();

              mSong.incPlayFrame (1);
              });
            changed();
            }
          else
            Sleep (10);

        device->stop();
        free (samples);
        //{{{  free decoder
        av_frame_free (&avFrame);

        if (context)
          avcodec_close (context);
        //}}}
        }
      }

    mPlayDoneSem.notifyAll();

    cLog::log (LOGINFO, "exit");
    CoUninitialize();
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
  cSemaphore mPlayDoneSem;

  cVolumeBox* mVolumeBox = nullptr;

  // icyMeta parsed into
  string mIcyStr;
  string mTitleStr;
  string mUrlStr;

  int mIcySkipCount = 0;
  int mIcySkipLen = 0;
  int mIcyInfoCount = 0;
  int mIcyInfoLen = 0;
  char mIcyInfo[255] = {0};
  //}}}
  };

// main
int __stdcall WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::init (LOGINFO, true, "", "playWindow");
  av_log_set_level (AV_LOG_VERBOSE);
  av_log_set_callback (cLog::avLogCallback);

  WSADATA wsaData;
  if (WSAStartup (MAKEWORD(2,2), &wsaData))
    exit (0);

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
