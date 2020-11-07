// playWindow.cpp
//{{{  includes
#include "stdafx.h"

#include "../../shared/audio/audioWASAPI.h"
#include "../../shared/utils/cSong.h"
#include "../../shared/decoders/cAudioParser.h"
#include "../../shared/decoders/cAacDecoder.h"
#include "../../shared/decoders/cMp3Decoder.h"
#include "../../shared/net/cWinSockHttp.h"

#include "../../shared/resources/r1.h"
#include "../../shared/resources/r2.h"
#include "../../shared/resources/r3.h"
#include "../../shared/resources/r4.h"
#include "../../shared/resources/r5.h"
#include "../../shared/resources/r6.h"

#include "../common/cD2dWindow.h"
#include "../boxes/cLogBox.h"
#include "../boxes/cWindowBox.h"
#include "../boxes/cClockBox.h"
#include "../boxes/cCalendarBox.h"
#include "../boxes/cBmpBox.h"
#include "../boxes/cTitleBox.h"

#include "../boxes/cListBox.h"
#include "../boxes/cStringListBox.h"
#include "../../shared/utils/cFileList.h"
#include "../boxes/cFileListBox.h"

#include "../common/cJpegImage.h"
#include "../boxes/cJpegImageView.h"

#include "../boxes/cSongBox.h"

using namespace std;
using namespace chrono;
//}}}
//{{{  channels
const string kHost = "as-hls-uk-live.bbcfmt.s.llnwi.net";
const vector <string> kChannels = { "bbc_radio_one",    "bbc_radio_two",       "bbc_radio_three",
                                    "bbc_radio_fourfm", "bbc_radio_five_live", "bbc_6music" };

// proper link to m3u8, fix it to use it one day
// const string kLink = "a.files.bbci.co.uk/media/live/manifesto/audio/simulcast/hls/uk/sbr_med/llnw/";
// + channel.m3u8"
// - returns
//    #EXTM3U
//    #EXT-X-VERSION:3
//    #EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=135680,CODECS="mp4a.40.2"
//    http://as-hls-uk-live.bbcfmt.s.llnwi.net/pool_904/live/uk/bbc_radio_three/bbc_radio_three.isml/bbc_radio_three-audio%3d128000.norewind.m3u8
//}}}

namespace {
  //{{{
  iAudioDecoder* createAudioDecoder (eAudioFrameType frameType) {

    switch (frameType) {
      case eAudioFrameType::eMp3:
        cLog::log (LOGINFO, "createAudioDecoder ffmpeg mp3");
        return new cMp3Decoder();
        break;

      case eAudioFrameType::eAacAdts:
        cLog::log (LOGINFO, "createAudioDecoder ffmpeg aacAdts");
        return new cAacDecoder();
        break;

      default:
        cLog::log (LOGERROR, "createAudioDecoder frameType:%d", frameType);
      }

    return nullptr;
    }
  //}}}
  //{{{
  string getTagValue (uint8_t* buffer, char* tag) {

    const char* tagPtr = strstr ((char*)buffer, tag);
    const char* valuePtr = tagPtr + strlen (tag);
    const char* endPtr = strchr (valuePtr, '\n');

    const string valueString = string (valuePtr, endPtr - valuePtr);
    cLog::log (LOGINFO, string(tagPtr, endPtr - tagPtr) + " value: " + valueString);

    return valueString;
    }
  //}}}
  //{{{
  uint8_t* extractAacFramesFromTs (uint8_t* ts, int tsLen) {
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
  }

class cAppWindow : public cD2dWindow {
public:
  //{{{
  ~cAppWindow() {
    delete mSong;
    }
  //}}}
  //{{{
  void run (const string& title, int width, int height, const vector<string>& names) {

    init (title, width, height, false);
    setChangeCountDown (0); // refresh evry frame

    add (new cCalendarBox (this, 190.f,150.f), -190.f,0.f);
    add (new cClockBox (this, 40.f), -135.f,35.f);

    string radioChannelName;
    if (names.empty())
      radioChannelName = kChannels[3];
    else {
      for (auto channelName : kChannels) {
        if (names[0] == channelName) {
          radioChannelName = names[0];
          break;
          }
        }
      }

    if (radioChannelName.empty()) {
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
          mSong->clear();
          mUrl = string;
          mSong->setChanged (true);
          cLog::log (LOGINFO, "listBox" + string);
          }
          ))->setPin (true);

        //mDebugStr = "shoutcast " + url.getHost() + " channel " + url.getPath();
        add (new cTitleBox (this, 500.f,20.f, mDebugStr));

        //thread ([=](){ icyThread (names[0]); }).detach();
        }
        //}}}
      else {
        //{{{  filelist
        mFileList = new cFileList (names, "*.aac;*.mp3;*.wav");

        if (!mFileList->empty()) {
          add (new cFileListBox (this, 0.f,-200.f, mFileList, [&](cFileListBox* box, int index){
            mSong->clear(); mSong->setChanged (true); }))->setPin (true);

          mJpegImageView = new cJpegImageView (this, 0.f,-220.f, false, false, nullptr);
          addFront (mJpegImageView);

          //thread([=](){ mFileList->watchThread(); }).detach();
          //thread ([=](){ fileThread(); }).detach();
          }
        }
        //}}}
      }
    else {
      //{{{  add radio 1..6 with action lambda
      add (new cBmpBox (this, 40.f,40.f, 1, r1, [&](cBmpBox* box, int index) {
        } ));
      addRight (new cBmpBox (this, 40.f,40.f, 2, r2, [&](cBmpBox* box, int index) {
        } ));
      addRight (new cBmpBox (this, 40.f,40.f, 3, r3, [&](cBmpBox* box, int index) {
        } ));
      addRight (new cBmpBox (this, 40.f,40.f, 4, r4, [&](cBmpBox* box, int index) {
        } ));
      addRight (new cBmpBox (this, 40.f,40.f, 5, r5, [&](cBmpBox* box, int index) {
        } ));
      addRight (new cBmpBox (this, 40.f,40.f, 6, r6,[&](cBmpBox* box, int index) {
        } ));

      constexpr int kBitRate = 128000;
      add (new cTitleBox (this, 500.f,20.f, mDebugStr), 0.f,40.f);

      thread ([=](){ hlsThread (kHost, radioChannelName, kBitRate); }).detach();
      }
      //}}}

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

      case 'M' : mSong->getSelect().addMark (mSong->getPlayFrame()); break;

      case ' ' : mPlaying = !mPlaying; break;

      case 0x21: mSong->prevSilencePlayFrame(); break;; // page up
      case 0x22: mSong->nextSilencePlayFrame(); break;; // page down

      case 0x25: mSong->incPlaySec (getShift() ? -300 : getControl() ? -10 : -1, false); break; // left arrow  - 1 sec
      case 0x27: mSong->incPlaySec (getShift() ? 300 : getControl() ?  10 :  1, false); break; // right arrow  + 1 sec

      case 0x24: mSong->setPlayFrame (
        mSong->getSelect().empty() ? mSong->getFirstFrame() : mSong->getSelect().getFirstFrame()); break; // home
      case 0x23: mSong->setPlayFrame (
        mSong->getSelect().empty() ? mSong->getLastFrame() : mSong->getSelect().getLastFrame()); break; // end

      case 0x26: if (mFileList && mFileList->prevIndex()) break; // up arrow
      case 0x28: if (mFileList && mFileList->nextIndex()) break; // down arrow

      case 0x2e: mSong->getSelect().clearAll(); break;; // delete select

      case 0x0d: mSong->setChanged (true); break; // enter

      // crude chan,bitrate change
      case '1' : break;
      case '2' : break;
      case '3' : break;
      case '4' : break;
      case '5' : break;
      case '6' : break;
      case '7' : break;
      case '8' : break;
      case '9' : break;
      case '0' : break;

      default  : cLog::log (LOGINFO, "key %x", key); break;
      }

    return false;
    }
  //}}}
private:
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
          mSong->getSelect().addMark (frame, titleStr);
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
  void hlsThread (const string& host, const string& channel, int bitrate) {
  // hls chunk http load and analyse thread, single thread helps chan change and jumping backwards

    cLog::setThreadName ("hls ");

    cHlsSong* hlsSong = new cHlsSong (bitrate < 128000 ? 150 : 300 );
    hlsSong->initialise (eAudioFrameType::eAacAdts, 2, 48000, bitrate < 128000 ? 2048 : 1024, 1000);
    auto audioDecoder = createAudioDecoder (eAudioFrameType::eAacAdts);

    mSong = hlsSong;
    add (new cSongBox (this, 0.f,0.f, mSong));

    while (!getExit()) {
      const string path = "pool_904/live/uk/" + channel + "/" + channel + ".isml/" + channel + "-audio=" + dec(bitrate);
      cPlatformHttp http;
      auto redirectedHost = http.getRedirect (host, path + ".norewind.m3u8");
      if (http.getContent()) {
        //{{{  hls m3u8 ok, parse it for baseChunkNum, baseTimePoint
        int extXMediaSequence = stoi (getTagValue (http.getContent(), "#EXT-X-MEDIA-SEQUENCE:"));
        istringstream inputStream (getTagValue (http.getContent(), "#EXT-X-PROGRAM-DATE-TIME:"));

        system_clock::time_point extXProgramDateTimePoint;
        inputStream >> date::parse ("%FT%T", extXProgramDateTimePoint);

        http.freeContent();

        hlsSong->setBase (extXMediaSequence, 0, extXProgramDateTimePoint, -37s);
        //}}}

        thread player;
        mSong->setChanged (false);;
        while (!getExit() && !mSong->getChanged()) {
          if (hlsSong->getLoadChunk(mChunkNum, mFrameNum, 2)) {
            // get hls chunkNum chunk
            if (http.get (redirectedHost, path + '-' + dec(mChunkNum) + ".ts") == 200) {
              cLog::log (LOGINFO1, "got " + dec(mChunkNum) +
                                   " at " + date::format ("%T", floor<seconds>(getNow())));
              auto aacFrames = http.getContent();
              auto aacFramesEnd = extractAacFramesFromTs (aacFrames, http.getContentSize());
              int frameSize;
              while (cAudioParser::parseFrame (aacFrames, aacFramesEnd, frameSize)) {
                auto samples = audioDecoder->decodeFrame (aacFrames, frameSize, mFrameNum);
                if (samples) {
                  mSong->addFrame (true, mFrameNum++, samples, true, mSong->getNumFrames(), 0);
                  if (!player.joinable())
                    player = thread ([=](){ playThread (true); });
                  }
                aacFrames += frameSize;
                }
              http.freeContent();
              }
            else {
              //{{{  failed to load expected available chunk, back off for 250ms
              cLog::log (LOGERROR, "late " + dec(mChunkNum));
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

    delete hlsSong;
    delete audioDecoder;

    cLog::log (LOGINFO, "exit");
    }
  //}}}
  //{{{
  //void icyThread (const string& url) {

    //cLog::setThreadName ("icy ");

    //mUrl = url;
    //while (!getExit()) {
      //int icySkipCount = 0;
      //int icySkipLen = 0;
      //int icyInfoCount = 0;
      //int icyInfoLen = 0;
      //char icyInfo[255] = { 0 };

      //uint8_t bufferFirst[4096];
      //uint8_t* bufferEnd = bufferFirst;
      //uint8_t* buffer = bufferFirst;

      //int frameNum = -1;
      //cAudioDecode decode (cAudioDecode::eAac);

      //thread player;
      //mSong->setChanged (false);

      //cPlatformHttp http;
      //cUrl parsedUrl;
      //parsedUrl.parse (mUrl);
      //http.get (parsedUrl.getHost(), parsedUrl.getPath(), "Icy-MetaData: 1",
        //{{{  headerCallback lambda
        //[&](const string& key, const string& value) noexcept {
          //if (key == "icy-metaint")
            //icySkipLen = stoi (value);
          //},
        //}}}
        //{{{  dataCallback lambda
        //[&] (const uint8_t* data, int length) noexcept {
        //// return false to exit

          //// cLog::log (LOGINFO, "callback %d", length);
          //if ((icyInfoCount >= icyInfoLen) && (icySkipCount + length <= icySkipLen)) {
            //{{{  simple copy of whole body, no metaInfo
            ////cLog::log (LOGINFO1, "body simple copy len:%d", length);

            //memcpy (bufferEnd, data, length);

            //bufferEnd += length;
            //icySkipCount += length;
            //}
            //}}}
          //else {
            //{{{  dumb copy for metaInfo straddling body, could be much better
            ////cLog::log (LOGINFO1, "body split copy length:%d info:%d:%d skip:%d:%d ",
                                  ////length, icyInfoCount, icyInfoLen, icySkipCount, icySkipLen);
            //for (int i = 0; i < length; i++) {
              //if (icyInfoCount < icyInfoLen) {
                //icyInfo [icyInfoCount] = data[i];
                //icyInfoCount++;
                //if (icyInfoCount >= icyInfoLen)
                  //addIcyInfo (frameNum, icyInfo);
                //}
              //else if (icySkipCount >= icySkipLen) {
                //icyInfoLen = data[i] * 16;
                //icyInfoCount = 0;
                //icySkipCount = 0;
                ////cLog::log (LOGINFO1, "body icyInfo len:", data[i] * 16);
                //}
              //else {
                //icySkipCount++;
                //*bufferEnd = data[i];
                //bufferEnd++;
                //}
              //}

            //return !getExit() && !mSong->getChanged();
            //}
            //}}}

          //if (frameNum == -1) {
            //// enough data to determine frameType and sampleRate (wrong for aac sbr)
            //frameNum = 0;
            //int sampleRate;
            //auto frameType = cAudioDecode::parseSomeFrames (bufferFirst, bufferEnd, sampleRate);
            //mSong->initialise (frameType, 2, 44100, (frameType == cAudioDecode::eMp3) ? 1152 : 2048, 3000);
            //}

          //while (decode.parseFrame (buffer, bufferEnd)) {
            //if (decode.getFrameType() == mSong->getFrameType()) {
              //auto samples = decode.decodeFrame (frameNum);
              //if (samples) {
                //mSong->setFixups (decode.getNumChannels(), decode.getSampleRate(), decode.getNumSamplesPerFrame());
                //mSong->addFrame (true, frameNum++, samples, true, mSong->getNumFrames()+1, 0);
                //if (frameNum == 1) // launch player after first frame
                  //player = thread ([=](){ playThread (true); });
                //}
              //}
            //buffer += decode.getNextFrameOffset();
            //}

          //if ((buffer > bufferFirst) && (buffer < bufferEnd)) {
            //// shuffle down last partial frame
            //auto bufferLeft = int(bufferEnd - buffer);
            //memcpy (bufferFirst, buffer, bufferLeft);
            //bufferEnd = bufferFirst + bufferLeft;
            //buffer = bufferFirst;
            //}

          //return !getExit() && !mSong->getChanged();
          //}
        //}}}
        //);

      //cLog::log (LOGINFO, "icyThread songChanged");
      //player.join();
      //}

    //cLog::log (LOGINFO, "exit");
    //}
  //}}}
  //{{{
  //void fileThread() {

    //cLog::setThreadName ("file");

    //while (!getExit()) {
      //if (cAudioDecode::mJpegPtr) {
        //{{{  delete any jpegImage
        //mJpegImageView->setImage (nullptr);

        //delete (cAudioDecode::mJpegPtr);
        //cAudioDecode::mJpegPtr = nullptr;
        //}
        //}}}

      //HANDLE fileHandle = CreateFile (mFileList->getCurFileItem().getFullName().c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
      //HANDLE mapping = CreateFileMapping (fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
      //auto fileMapFirst = (uint8_t*)MapViewOfFile (mapping, FILE_MAP_READ, 0, 0, 0);
      //auto fileMapSize = GetFileSize (fileHandle, NULL);
      //auto fileMapEnd = fileMapFirst + fileMapSize;

      //// sampleRate for aac-sbr wrong in header, fixup later, use a maxValue for samples alloc
      //int sampleRate;
      //auto frameType = cAudioDecode::parseSomeFrames (fileMapFirst, fileMapEnd, sampleRate);
      //if (cAudioDecode::mJpegPtr) // should delete old jpegImage, but we have memory to waste
        //mJpegImageView->setImage (new cJpegImage (cAudioDecode::mJpegPtr, cAudioDecode::mJpegLen));

      //thread player;

      //int frameNum = 0;
      //bool songDone = false;
      //auto fileMapPtr = fileMapFirst;
      //cAudioDecode decode (frameType);

      //if (frameType == cAudioDecode::eWav) {
        //{{{  parse wav
        //auto frameSamples = 1024;
        //mSong->initialise (frameType, 2, sampleRate, frameSamples);
        //decode.parseFrame (fileMapPtr, fileMapEnd);
        //auto samples = decode.getFramePtr();
        //while (!getExit() && !mSong->getChanged() && ((samples + (frameSamples * 2 * sizeof(float))) <= fileMapEnd)) {
          //mSong->addFrame (true, frameNum++, (float*)samples, false, fileMapSize / (frameSamples * 2 * sizeof(float)), 0);
          //samples += frameSamples * 2 * sizeof(float);
          //if (frameNum == 1)
            //player = thread ([=](){ playThread (false); });
          //}
        //}
        //}}}
      //else {
        //{{{  parse coded
        //mSong->initialise (frameType, 2, sampleRate, (frameType == cAudioDecode::eMp3) ? 1152 : 2048);
        //while (!getExit() && !mSong->getChanged() && decode.parseFrame (fileMapPtr, fileMapEnd)) {
          //if (decode.getFrameType() == mSong->getFrameType()) {
            //auto samples = decode.decodeFrame (frameNum);
            //if (samples) {
              //int numFrames = mSong->getNumFrames();
              //int totalFrames = (numFrames > 0) ? int(fileMapEnd - fileMapFirst) / (int(decode.getFramePtr() - fileMapFirst) / numFrames) : 0;
              //mSong->setFixups (decode.getNumChannels(), decode.getSampleRate(), decode.getNumSamplesPerFrame());
              //mSong->addFrame (true, frameNum++, samples, true, totalFrames+1, 0);
              //if (frameNum == 1)
                //player = thread ([=](){ playThread (false); });
              //}
            //}
          //fileMapPtr += decode.getNextFrameOffset();
          //}
        //}
        //}}}
      //cLog::log (LOGINFO, "loaded");

      //// wait for play to end or abort
      //player.join();
      //{{{  next file
      //UnmapViewOfFile (fileMapFirst);
      //CloseHandle (fileHandle);

      //if (mSong->getChanged()) // use changed fileIndex
        //mSong->setChanged (false);
      //else if (!mFileList->nextIndex())
        //setExit();
      //}}}
      //}

    //cLog::log (LOGINFO, "exit");
    //}
  //}}}
  //{{{
  void playThread (bool streaming) {

    cLog::setThreadName ("play");
    SetThreadPriority (GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    float silence [2048*2] = { 0.f };
    float samples [2048*2] = { 0.f };

    auto device = getDefaultAudioOutputDevice();
    if (device) {
      cLog::log (LOGINFO, "device @ %d", mSong->getSampleRate());
      device->setSampleRate (mSong->getSampleRate());
      //cAudioDecode decode (mSong->getFrameType());

      device->start();
      while (!getExit() && !mSong->getChanged()) {
        device->process ([&](float*& srcSamples, int& numSrcSamples) mutable noexcept {
          // lambda callback - load srcSamples
          shared_lock<shared_mutex> lock (mSong->getSharedMutex());

          auto framePtr = mSong->getFramePtr (mSong->getPlayFrame());
          if (mPlaying && framePtr && framePtr->getSamples()) {
            if (mSong->getNumChannels() == 1) {
              //{{{  mono to stereo
              auto src = framePtr->getSamples();
              auto dst = samples;
              for (int i = 0; i < mSong->getSamplesPerFrame(); i++) {
                *dst++ = *src;
                *dst++ = *src++;
                }
              }
              //}}}
            else
              memcpy (samples, framePtr->getSamples(), mSong->getSamplesPerFrame() * mSong->getNumChannels() * sizeof(float));
            srcSamples = samples;
            }
          else
            srcSamples = silence;
          numSrcSamples = mSong->getSamplesPerFrame();

          if (mPlaying && framePtr)
            mSong->incPlayFrame (1, true);
          });

        if (!streaming && (mSong->getPlayFrame() > mSong->getLastFrame()))
          break;
        }

      device->stop();
      }

    cLog::log (LOGINFO, "exit");
    }
  //}}}
  //{{{  vars
  cSong* mSong;
  bool mPlaying = true;

  string mBitrateStr;

  cFileList* mFileList = nullptr;
  cJpegImageView* mJpegImageView = nullptr;

  int mChunkNum;
  int mFrameNum;

  string mLastTitleStr;
  vector<string> mShoutCast;
  string mUrl;

  string mDebugStr;
  cBox* mLogBox = nullptr;
  //}}}
  };

// main
int WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

  cLog::init (LOGINFO, true, "", "playWindow");

  int numArgs;
  auto args = CommandLineToArgvW (GetCommandLineW(), &numArgs);
  vector <string> names;
  for (int i = 1; i < numArgs; i++)
    names.push_back (wcharToString (args[i]));

  cAppWindow appWindow;
  appWindow.run ("playWindow", 800, 420, names);

  return 0;
  }
