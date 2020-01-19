// playWindow.cpp
//{{{  includes
#include "stdafx.h"

// should be in stdafx.h
#include "../../shared/utils/cFileList.h"
#include "../boxes/cFileListBox.h"

#include "../../shared/decoders/audioParser.h"
#include "../../shared/utils/cSong.h"
#include "../boxes/cSongBoxes.h"

#include "WASAPIRenderer.h"

using namespace std;
//}}}

#include <functiondiscoverykeys.h>
RenderBuffer* renderQueue = NULL;
bool UseConsoleDevice;
bool UseCommunicationsDevice;
bool UseMultimediaDevice;
wchar_t* OutputEndpoint;
//{{{
template <class T> void SafeRelease(T **ppT) {
  if (*ppT) {
    (*ppT)->Release();
    *ppT = NULL;
    }
  }
//}}}
//{{{
LPWSTR GetDeviceName (IMMDeviceCollection *DeviceCollection, UINT DeviceIndex) {

  IMMDevice* device;
  HRESULT hr = DeviceCollection->Item (DeviceIndex, &device);
  if (FAILED (hr)) {
    //{{{
    cLog::log (LOGINFO,"Unable to get device %d: %x\n", DeviceIndex, hr);
    return NULL;
    }
    //}}}

  LPWSTR deviceId;
  hr = device->GetId (&deviceId);
  if (FAILED (hr)) {
    //{{{
    cLog::log (LOGINFO,"Unable to get device %d id: %x\n", DeviceIndex, hr);
    return NULL;
    }
    //}}}

  IPropertyStore* propertyStore;
  hr = device->OpenPropertyStore (STGM_READ, &propertyStore);
  SafeRelease (&device);
  if (FAILED (hr)) {
    //{{{
    cLog::log (LOGINFO,"Unable to open device %d property store: %x\n", DeviceIndex, hr);
    return NULL;
    }
    //}}}

  PROPVARIANT friendlyName;
  PropVariantInit (&friendlyName);
  hr = propertyStore->GetValue (PKEY_Device_FriendlyName, &friendlyName);
  SafeRelease (&propertyStore);

  if (FAILED (hr)) {
    //{{{
    cLog::log (LOGINFO,"Unable to retrieve friendly name for device %d : %x\n", DeviceIndex, hr);
    return NULL;
    }
    //}}}

  wchar_t deviceName[128];
  //hr = StringCbprintf (deviceName, sizeof(deviceName), "%s (%s)", friendlyName.vt != VT_LPWSTR ? "Unknown" : friendlyName.pwszVal, deviceId);
  //if (FAILED (hr)) {
    //{{{
    //cLog::log (LOGINFO,"Unable to format friendly name for device %d : %x\n", DeviceIndex, hr);
    //return NULL;
    // }
    //}}}

  PropVariantClear (&friendlyName);
  CoTaskMemFree (deviceId);

  wchar_t* returnValue = _wcsdup (deviceName);
  if (returnValue == NULL) {
    //{{{
    cLog::log (LOGINFO,"Unable to allocate buffer for return\n");
    return NULL;
    }
    //}}}

  return returnValue;
  }
//}}}
//{{{
bool PickDevice (IMMDevice** DeviceToUse, bool* IsDefaultDevice, ERole* DefaultDeviceRole) {

  HRESULT hr;

  bool retValue = true;
  IMMDeviceEnumerator *deviceEnumerator = NULL;
  IMMDeviceCollection *deviceCollection = NULL;

  *IsDefaultDevice = false;   // Assume we're not using the default device.

  hr = CoCreateInstance (__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&deviceEnumerator));
  if (FAILED(hr)) {
    cLog::log (LOGINFO, "Unable to instantiate device enumerator: %x\n", hr);
    retValue = false;
    goto Exit;
    }

  IMMDevice* device = NULL;
  ERole deviceRole = eConsole;
  deviceRole = eConsole;
  //deviceRole = eCommunications;
  //deviceRole = eMultimedia;
  hr = deviceEnumerator->GetDefaultAudioEndpoint (eRender, deviceRole, &device);
   if (FAILED(hr)) {
    cLog::log (LOGINFO, "Unable to get default device for role %d: %x\n", deviceRole, hr);
    retValue = false;
    goto Exit;
    }

  *IsDefaultDevice = true;
  *DefaultDeviceRole = deviceRole;
  *DeviceToUse = device;
  retValue = true;

Exit:
  SafeRelease (&deviceCollection);
  SafeRelease (&deviceEnumerator);
  return retValue;
  }
//}}}

// main cAppWindow
class cAppWindow : public cD2dWindow {
public:
  cAppWindow() : mPlayDoneSem("playDone") {}
  virtual ~cAppWindow() {}

  //{{{
  void run (bool streaming, const string& title, int width, int height, const string& name) {

    initialise (title, width, height, false);

    add (new cCalendarBox (this, 190.f,150.f, mTimePoint), -190.f,0.f);
    add (new cClockBox (this, 40.f, mTimePoint), -135.f,35.f);

    mJpegImageView = new cJpegImageView (this, 0.f,-220.f, false, false, mSong.mImage);
    add (mJpegImageView);

    add (new cLogBox (this, 200.f,-200.f, true), 0.f,-200.f)->setPin (false);

    add (new cSongFreqBox (this, 0,100.f, mSong), 0,-640.f);
    add (new cSongSpecBox (this, 0,300.f, mSong), 0,-540.f);
    add (new cSongWaveBox (this, 0,100.f, mSong), 0,-220.f);
    add (new cSongLensBox (this, 0,100.f, mSong), 0.f,-120.f);
    add (new cSongTimeBox (this, 600.f,50.f, mSong), -600.f,-50.f);

    mFileList = new cFileList (name, "*.aac;*.mp3");
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
      thread ([=]() { httpThread (name.c_str()); }).detach();
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
      case 0x23: mSong.setPlayFrame (mSong.mNumFrames-1); changed(); break; // end

      case 0x26: if (mFileList->prevIndex()) changed(); break; // up arrow
      case 0x28: if (mFileList->nextIndex()) changed(); break; // down arrow
      case 0x0d: mChanged = true; changed(); break; // enter - play file

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
      (dynamic_cast<cAppWindow*>(getWindow()))->mChanged = true;
      }
    };
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
  static int httpTrace (CURL* handle, curl_infotype type, char* ptr, size_t size, cAppWindow* appWindow) {

    switch (type) {
      case CURLINFO_TEXT:
        cLog::log (LOGINFO, "TEXT %s", ptr);
        break;

      case CURLINFO_HEADER_OUT:
        cLog::log (LOGINFO, "HEADER_OUT %s", ptr);
        break;

      case CURLINFO_DATA_OUT:
        cLog::log (LOGINFO, "DATA_OUT %s", ptr);
        break;

      case CURLINFO_SSL_DATA_OUT:
        cLog::log (LOGINFO, "SSL_DATA_OUT %s", ptr);
        break;

      case CURLINFO_HEADER_IN:
        cLog::log (LOGINFO, "HEADER_IN %s", ptr);
        break;

      case CURLINFO_DATA_IN:
        //cLog::log (LOGINFO, "DATA_IN size:%d", ptr);
        break;

      case CURLINFO_SSL_DATA_IN:
        cLog::log (LOGINFO, "SSL_DATA_IN size:%d", ptr);
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
  void httpThread (const char* url) {

    cLog::setThreadName ("http");

    auto curl = curl_easy_init();
    if (curl) {
      //curl_easy_setopt (curl, CURLOPT_DEBUGFUNCTION, httpTrace);
      //curl_easy_setopt (curl, CURLOPT_DEBUGDATA, this);
      //curl_easy_setopt (curl, CURLOPT_VERBOSE, 1L);

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
    }
  //}}}
  //{{{
  void analyseThread (bool streaming) {

    cLog::setThreadName ("anal");

    while (!getExit()) {
      HANDLE fileHandle = 0;
      HANDLE mapping = 0;

      // wait for a bit of stream
      while (streaming && (mStreamLast - mStreamFirst) < 1440)
        mStreamSem.wait();
      if (!streaming) {
        //{{{  open file mapped onto stream
        fileHandle = CreateFile (mFileList->getCurFileItem().getFullName().c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        mapping = CreateFileMapping (fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);

        mStreamFirst = (uint8_t*)MapViewOfFile (mapping, FILE_MAP_READ, 0, 0, 0);
        mStreamLast = mStreamFirst + GetFileSize (fileHandle, NULL);
        }
        //}}}

      // sampleRate for aac-sbr wrong in header, fixup later
      int streamSampleRate;
      auto audioFrameType = parseAudioFrames (mStreamFirst, mStreamLast, streamSampleRate);
      mSong.init ("stream", audioFrameType, audioFrameType == eAac ? 2048 : 1152, streamSampleRate);
      //{{{  addd jpeg if available
      int jpegLen = 0;
      auto jpegBuf = parseId3Tag (mStreamFirst, mStreamLast, jpegLen);
      if (jpegBuf) {
        mSong.mImage = new cJpegImage();
        mSong.mImage->setBuf (jpegBuf, jpegLen);
        mJpegImageView->setImage (mSong.mImage);
        }
      //}}}

      auto codec = avcodec_find_decoder (mSong.mAudioFrameType == eAac ? AV_CODEC_ID_AAC : AV_CODEC_ID_MP3);
      auto context = avcodec_alloc_context3 (codec);
      avcodec_open2 (context, codec, NULL);

      AVPacket avPacket;
      av_init_packet (&avPacket);
      auto avFrame = av_frame_alloc();
      auto samples = (float*)malloc (mSong.getSamplesSize());

      auto stream = mStreamFirst;
      while (!getExit() && !mChanged) {
        int skip;
        int sampleRate;
        eAudioFrameType frameType;
        while (parseAudioFrame (stream, mStreamLast, avPacket.data, avPacket.size, frameType, skip, sampleRate)) {
          if (frameType == mSong.mAudioFrameType) {
            auto ret = avcodec_send_packet (context, &avPacket);
            while (ret >= 0) {
              ret = avcodec_receive_frame (context, avFrame);
              if ((ret == AVERROR_EOF) || (ret < 0))
                break;
              if ((ret != AVERROR(EAGAIN)) && (avFrame->nb_samples > 0)) {
                if (avFrame->nb_samples != mSong.mSamplesPerFrame)
                  //{{{  update mSamplesPerFrame
                  mSong.mSamplesPerFrame = avFrame->nb_samples;
                  //}}}
                if (avFrame->sample_rate > mSong.mSampleRate)
                  //{{{  fixup aac-sbr sample rate
                  mSong.mSampleRate = avFrame->sample_rate;
                  //}}}
                //{{{  covert planar avFrame->data to interleaved int16_t samples
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
                if (mSong.addFrame (uint32_t(avPacket.data - mStreamFirst), avPacket.size,
                                    avFrame->nb_samples, samples, int(mStreamLast - mStreamFirst)) == kPlayFrameThreshold) {
                  //{{{  launch playThread
                  auto threadHandle = thread ([=](){ playThread (streaming); });
                  SetThreadPriority (threadHandle.native_handle(), THREAD_PRIORITY_TIME_CRITICAL);
                  threadHandle.detach();
                  }
                  //}}}
                changed();
                }
              }
            }
          stream += skip + avPacket.size;
          }
        if (streaming)
          mStreamSem.wait();
        }

      // done
      free (samples);
      av_frame_free (&avFrame);
      if (context)
        avcodec_close (context);

      // wait for play to end or abort
      mPlayDoneSem.wait();
      if (!streaming && fileHandle) {
        //{{{  next file
        UnmapViewOfFile (mStreamFirst);
        CloseHandle (fileHandle);

        if (mChanged) // use changed fileIndex
          mChanged = false;
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

    auto codec = avcodec_find_decoder (mSong.mAudioFrameType == eAac ? AV_CODEC_ID_AAC : AV_CODEC_ID_MP3);
    auto context = avcodec_alloc_context3 (codec);
    avcodec_open2 (context, codec, NULL);

    AVPacket avPacket;
    av_init_packet (&avPacket);
    auto avFrame = av_frame_alloc();
    auto samples = (float*)malloc (mSong.getSamplesSize());

    cWinAudio32 audio (mSong.mChannels, mSong.mSampleRate);
    mVolumeBox->setAudio (&audio);

    while (!getExit() && !mChanged && (streaming || (mSong.mPlayFrame < mSong.getNumParsedFrames()-1)))
      if (!mPlaying)
        audio.play (mSong.mChannels, nullptr, mSong.mSamplesPerFrame, 1.f);
      else {
        int skip;
        int sampleRate;
        eAudioFrameType frameType;
        if (parseAudioFrame (mStreamFirst + mSong.getPlayFrameStreamIndex(), mStreamLast,
                             avPacket.data, avPacket.size, frameType, skip, sampleRate)) {
          if (frameType == mSong.mAudioFrameType) {
            auto ret = avcodec_send_packet (context, &avPacket);
            while (ret >= 0) {
              ret = avcodec_receive_frame (context, avFrame);
              if ((ret == AVERROR_EOF) || (ret < 0))
                break;
              if ((ret != AVERROR(EAGAIN)) && (avFrame->nb_samples > 0)) {
                //{{{  covert planar avFrame->data to interleaved int16_t samples
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
                audio.play (avFrame->channels, samples, avFrame->nb_samples, 1.f);
                mSong.incPlayFrame (1);
                changed();
                }
              }
            }
          }
        }

    // done
    mVolumeBox->setAudio (nullptr);

    free (samples);
    av_frame_free (&avFrame);
    if (context)
      avcodec_close (context);

    mPlayDoneSem.notifyAll();

    cLog::log (LOGINFO, "exit");
    CoUninitialize();
    }
  //}}}

  const int kPlayFrameThreshold = 10; // 10 frames analyse before play, about half second
  //{{{  vars
  cFileList* mFileList;

  cSong mSong;
  uint8_t* mStreamFirst = nullptr;
  uint8_t* mStreamLast = nullptr;
  cSemaphore mStreamSem;

  cJpegImageView* mJpegImageView = nullptr;

  bool mChanged = false;
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
//{{{
int __stdcall WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);

  cLog::init (LOGINFO, true, "", "playWindow");

  av_log_set_level (AV_LOG_VERBOSE);
  av_log_set_callback (cLog::avLogCallback);

  int numArgs;
  auto args = CommandLineToArgvW (GetCommandLineW(), &numArgs);

  // WASAPI renederer test
  int TargetLatency = 30;
  int TargetFrequency = 880;
  int TargetDurationInSec = 2;
  IMMDevice* device = NULL;
  bool isDefaultDevice;
  ERole role;
  if (PickDevice (&device, &isDefaultDevice, &role)) {
    CWASAPIRenderer* renderer = new (std::nothrow) CWASAPIRenderer (device, isDefaultDevice, role);
    if (renderer) {
      if (renderer->Initialize (TargetLatency)) {
        UINT32 renderBufferSizeInBytes = (renderer->BufferSizePerPeriod()  * renderer->getFrameSize());
        size_t renderDataLength = (renderer->getSamplesPerSecond() * TargetDurationInSec * renderer->getFrameSize()) + (renderBufferSizeInBytes-1);
        size_t renderBufferCount = renderDataLength / (renderBufferSizeInBytes);
        RenderBuffer* renderBuffer = new (std::nothrow) RenderBuffer();
        if (renderBuffer == NULL) {
         //{{{  exit
         printf("Unable to allocate render buffer\n");
         return -1;
         }
         //}}}
        renderBuffer->_BufferLength = renderBufferSizeInBytes;
        renderBuffer->_Buffer = new BYTE[renderBufferSizeInBytes];
        if (renderBuffer->_Buffer == NULL) {
          //{{{  exit
          printf("Unable to allocate render buffer\n");
          return -1;
          }
          //}}}
        if (renderer->Start (renderQueue)) {
          }
        }
      }
    }

  cAppWindow appWindow;
  if (numArgs == 1) {
    //{{{  urls
    //const string url = "http://stream.wqxr.org/wqxr.aac";
    //const string url = "http://tx.planetradio.co.uk/icecast.php?i=jazzhigh.aac";
    //const string url = "http://us4.internet-radio.com:8266/";
    //const string url = "http://tx.planetradio.co.uk/icecast.php?i=countryhits.aac";
    //const string url = "http://live-absolute.sharp-stream.com/absoluteclassicrockhigh.aac";
    //const string url = "http://media-ice.musicradio.com:80/SmoothCountry";
    //}}}
    const string url = "http://stream.wqxr.org/js-stream.aac";
    appWindow.run (true, "playWindow " + url, 800, 800, url);
    }
  else {
    wstring wstr (args[1]);

    #pragma warning(push)
      #pragma warning(disable: 4244)
      string fileName = string (wstr.begin(), wstr.end());
    #pragma warning(pop)

    //string fileName = "C:/Users/colin/Music/Elton John";
    appWindow.run (false, "playWindow" + fileName, 800, 800, fileName);
    }

  //if (renderer) {
  //  renderer->Stop();
  //  renderer->Shutdown();
  //  }
  //SafeRelease (&renderer);

  CoUninitialize();
  }
//}}}
