// cPlayView.cpp
//{{{  includes
#include "stdafx.h"

#include "cPlayView.h"

#include "../../shared/utils/cWinAudio32.h"
#include "../boxes/cAudFrameBox.h"

#ifdef _DEBUG
  #pragma comment (lib,"libmfx_d.lib")
#else
  #pragma comment (lib,"libmfx.lib")
#endif

using namespace std;
using namespace concurrency;
//}}}
//{{{  const
const int kMaxAudFrames = 120; // just over 2sec
const int kMaxVidFrames = 30;  // just over 1sec

const int kChunkSize = 2048*188;
//}}}

//{{{
cPlayView::cPlayView (cD2dWindow* window, float width, float height, const string& fileName) :
    cView("playerView", window, width, height),
    mFileName(fileName), mFirstVidPtsSem("firstVidPts") {

  mAudio = new cWinAudio32 (2, 48000);

  // create transportStreams
  mAnalTs = new cAnalTransportStream();
  mAudTs = new cAudTransportStream (kMaxAudFrames);
  mVidTs = new cVidTransportStream (kMaxVidFrames);

  mTimecodeBox = window->add (new cTimecodeBox (window, 600.f,60.f, this), -600.f,-60.f)->setPin (true);
  mProgressBox = window->add (new cProgressBox (window, 0.f,6.f, this), 0.f,-6.f);
  mAudFrameBox = window->add (new cAudFrameBox (window, 82.f,240.0f, mPlayAudFrame, mAudio), -84.f,-240.f-6.0f);

  thread ([=](){ analyserThread(); }).detach();
  thread ([=]() { audThread(); }).detach();
  thread ([=]() { vidThread(); }).detach();
  thread ([=](){ playThread(); }).detach();
  }
//}}}
//{{{
cPlayView::~cPlayView() {

  mAbort = true;
  while (!mAnalAborted || !mVidAborted || !mAudAborted || !mPlayAborted) {
    mPlayPtsSem.notifyAll();
    Sleep (1);
    }

  mWindow->removeBox (mTimecodeBox);
  mWindow->removeBox (mProgressBox);
  mWindow->removeBox (mAudFrameBox);

  delete mAudTs;
  delete mVidTs;
  delete mAnalTs;
  }
//}}}

//{{{
bool cPlayView::onKey (int key) {

  switch (key) {
    //{{{
    case  ' ': // space - toggle play
      togglePlay();
      break;
    //}}}
    //{{{
    case 0x24: // home
      setPlayPts (0);
      mWindow->changed();
      break;
    //}}}
    //{{{
    case 0x23: // end
      setEnd();
      mWindow->changed();
      break;
    //}}}
    //{{{
    case 0x21: // page up
      incPlayPts (-90000*60);
      mWindow->changed();
      break;
    //}}}
    //{{{
    case 0x22: // page down
      incPlayPts (90000*60);
      mWindow->changed();
      break;
    //}}}
    //{{{
    case 0x25: // left arrow
      incPlayPts (-90000*10);
      mWindow->changed();
      break;
    //}}}
    //{{{
    case 0x27: // right arrow
      incPlayPts (90000*10);
      mWindow->changed();
      break;
    //}}}

    case '<':
    case ',':
      //{{{  single step prev
      incPlayPts (-90000/25);
      setPause();
      mWindow->changed();
      break;
      //}}}
    case '>':
    case '.':
      //{{{  single step next
      incPlayPts (90000/25);
      setPause();
      mWindow->changed();
      break;
      //}}}

    case  '0': key += 10; // nasty trick to wrap '0' as channel 10
    case  '1':
    case  '2':
    case  '3':
    case  '4':
    case  '5':
    case  '6':
    case  '7':
    case  '8':
    case  '9': setService (key - '1'); mWindow->changed(); break;
    }

  return false;
  }
//}}}
//{{{
bool cPlayView::onWheel (int delta, cPoint pos) {

  if (mWindow->getControl())
    return cView::onWheel (delta, pos);
  else {
    mPlaying = ePause;
    incPlayPts (-int64_t(delta * (90000/25) / 120));
    mWindow->changed();
    }

  return true;
  }
//}}}
//{{{
bool cPlayView::onMove (bool right, cPoint pos, cPoint inc) {

  const int kAudSamplesPerAacFrame = 1152;

  if (mWindow->getControl())
    return cView::onMove (right, pos, inc);
  else {
    mPlaying = ePause;
    incPlayPts (int64_t (-inc.x * kAudSamplesPerAacFrame * 48 / 90 / 8));
    mWindow->changed();
    return true;
    }
  }
//}}}
//{{{
bool cPlayView::onUp (bool right, bool mouseMoved, cPoint pos) {

  if (!mouseMoved)
    togglePlay();

  return false;
  }
//}}}
//{{{
void cPlayView::onDraw (ID2D1DeviceContext* dc) {

  auto vidframe = (cVidFrame*)mVidTs->findNearestFrame (getPlayPts());
  if (vidframe && (vidframe->getPts() != mBitmapPts)) {
    mBitmap = vidframe->makeBitmap (dc, mBitmap);
    mBitmapPts = vidframe->getPts();
    }

  dc->SetTransform (mView2d.mTransform);
  if (mBitmap)
    dc->DrawBitmap (mBitmap, cRect(getSize()));

  dc->SetTransform (Matrix3x2F::Identity());
  }
//}}}

// private sets
//{{{
void cPlayView::setService (int index) {

  auto service = mAnalTs->getService (index);
  if (service) {
    mAudTs->setPid (service->getAudPid());
    mVidTs->setPid (service->getVidPid());
    }
  }
//}}}
//{{{
void cPlayView::setPlayPts (int64_t playPts) {
// mPlayPts is offset from mBasePts

  if (playPts < 0)
    mPlayPts = 0;
  else if (playPts > getLengthPts())
    mPlayPts = getLengthPts();
  else
    mPlayPts = playPts;

  mPlayPtsSem.notifyAll();
  }
//}}}
//{{{
void cPlayView::togglePlay() {
  switch (mPlaying) {
    case ePause: mPlaying = ePlay; break;
    case eScrub: mPlaying = ePlay; break;
    case ePlay:  mPlaying = ePause; break;
    }
  }
//}}}

//{{{
void cPlayView::analyserThread() {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::setThreadName ("anal");
  SetThreadPriority (GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);

  const auto fileChunkBuffer = (uint8_t*)malloc (kChunkSize);
  auto file = _open (mFileName.c_str(), _O_RDONLY | _O_BINARY);
  //{{{  get streamSize
  struct _stat64 buf;
  _fstat64 (file, &buf);
  mStreamSize = buf.st_size ;
  //}}}
  //{{{  parse front of stream until first service
  int64_t streamPos = 0;

  cService* service = nullptr;
  while (!service) {
    auto chunkBytesLeft = _read (file, fileChunkBuffer, kChunkSize);
    auto chunkPtr = fileChunkBuffer;
    while (chunkBytesLeft >= 188) {
      auto bytesUsed = mAnalTs->demux (chunkPtr, chunkBytesLeft, streamPos, false, -1);
      streamPos += bytesUsed;
      chunkPtr += bytesUsed;
      chunkBytesLeft -= (int)bytesUsed;
      }

    service = mAnalTs->getService (0);
    }

  // select first service
  mVidTs->setPid (service->getVidPid());
  mAudTs->setPid (service->getAudPid());
  cLog::log (LOGNOTICE, "first service - vidPid:" + dec(service->getVidPid()) +
                        " audPid:" + dec(service->getAudPid()));
  mAnalTs->clearPosCounts();
  //}}}
  //{{{  parse end of stream for initial lastPts
  streamPos = mStreamSize - (kChunkSize * 8);
  _lseeki64 (file, streamPos, SEEK_SET);

  while (streamPos < mStreamSize) {
    auto chunkBytesLeft = _read (file, fileChunkBuffer, kChunkSize);
    auto chunkPtr = fileChunkBuffer;
    while (chunkBytesLeft >= 188) {
      auto bytesUsed = mAnalTs->demux (chunkPtr, chunkBytesLeft, streamPos, chunkPtr == fileChunkBuffer, -1);
      streamPos += bytesUsed;
      chunkPtr += bytesUsed;
      chunkBytesLeft -= (int)bytesUsed;
      }
    }

  mAnalTs->clearPosCounts();
  //}}}

  // analyse stream from start, chase tail, update progress bar
  mStreamPos = 0;
  _lseeki64 (file, mStreamPos, SEEK_SET);

  int sameStreamSizeCount = 0;
  int firstVidSignalCount = 0;
  while (!getAbort() && (sameStreamSizeCount < 10)) {
    int64_t bytesToRead = mStreamSize - mStreamPos;
    if (bytesToRead > kChunkSize) // trim to kChunkSize
      bytesToRead = kChunkSize;
    if (bytesToRead >= 188) { // at least 1 packet to read, trim read to packet boundary
      bytesToRead -= bytesToRead % 188;
      auto chunkBytesLeft = _read (file, fileChunkBuffer, (unsigned int)bytesToRead);
      if (chunkBytesLeft >= 188) {
        auto chunkPtr = fileChunkBuffer;
        while (chunkBytesLeft >= 188) {
          auto bytesUsed = mAnalTs->demux (chunkPtr, chunkBytesLeft, mStreamPos, mStreamPos == 0, -1);
          mStreamPos += bytesUsed;
          chunkPtr += bytesUsed;
          chunkBytesLeft -= (int)bytesUsed;
          mWindow->changed();

          if ((firstVidSignalCount < 3) &&
              (mAnalTs->getFirstPts (service->getAudPid()) != -1) &&
              (mAnalTs->getFirstPts (service->getVidPid()) != -1)) {
            firstVidSignalCount++;
            mFirstVidPtsSem.notifyAll();
            }
          }
        }
      }
    else {
      //{{{  check fileSize changed
      while (!getAbort()) {
        // check size
        _fstat64 (file, &buf);
        if (buf.st_size > mStreamSize) {
          // bigger
          cLog::log (LOGINFO, "fileSize now " + dec(mStreamSize));
          mStreamSize = buf.st_size;
          sameStreamSizeCount = 0;
          break;
          }

        else {
          // same
          sameStreamSizeCount++;
          if (sameStreamSizeCount >= 10) {
            cLog::log (LOGINFO, "fileSize sameCount expired " + dec(sameStreamSizeCount));
            break;
            }
          }

        // wait with abort
        int i = 0;
        while (!getAbort() && (i++ < 1000))
          Sleep (1);
        }
      }
      //}}}
    }
  _close (file);
  free (fileChunkBuffer);

  cLog::log (LOGINFO, "exit");
  CoUninitialize();
  mAnalAborted = true;
  }
//}}}
//{{{
void cPlayView::audThread() {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::setThreadName ("aud ");

  const auto fileChunkBuffer = (uint8_t*)malloc (kChunkSize);
  mFirstVidPtsSem.wait();

  bool skip = false;
  int64_t lastJumpStreamPos = -1;

  int64_t streamPos = 0;
  auto file = _open (mFileName.c_str(), _O_RDONLY | _O_BINARY);
  while (!getAbort()) {
    auto chunkPtr = fileChunkBuffer;
    auto chunkBytesLeft = _read (file, fileChunkBuffer, kChunkSize);
    if (chunkBytesLeft < 188) {
      // end of file
      while (!getAbort() && mAudTs->isLoaded (getPlayPts(), 1))
        mPlayPtsSem.wait();
      //{{{  jump to pts in stream
      auto jumpStreamPos = mAnalTs->findStreamPos (mAudTs->getPid(), getPlayPts());
      streamPos = jumpStreamPos;
      _lseeki64 (file, streamPos, SEEK_SET);
      lastJumpStreamPos = jumpStreamPos;

      chunkBytesLeft = 0;
      skip = true;
      //}}}
      }
    else {
      while (chunkBytesLeft >= 188) {
        //{{{  demux up to a frame
        // decode a frame
        auto bytesUsed = mAudTs->demux (chunkPtr, chunkBytesLeft, streamPos, skip, mAudTs->getPid());
        streamPos += bytesUsed;
        chunkPtr += bytesUsed;
        chunkBytesLeft -= (int)bytesUsed;
        skip = false;
        //}}}
        mWindow->changed();

        while (!getAbort() && mAudTs->isLoaded (getPlayPts(), kMaxAudFrames/2))
          mPlayPtsSem.wait();

        bool loaded = mAudTs->isLoaded (getPlayPts(), 1);
        if (!loaded || (getPlayPts() > mAudTs->getLastLoadedPts() + 100000)) {
          auto jumpStreamPos = mAnalTs->findStreamPos (mAudTs->getPid(), getPlayPts());
          if (jumpStreamPos != lastJumpStreamPos) {
            //{{{  jump to jumpStreamPos, unless same as last, wait for rest of GOP or chunk to demux
            cLog::log (LOGINFO, "jump playPts:" + getPtsString(getPlayPts()) +
                       (loaded ? " after ":" notLoaded ") + getPtsString(mAudTs->getLastLoadedPts()));

            _lseeki64 (file, jumpStreamPos, SEEK_SET);

            streamPos = jumpStreamPos;
            lastJumpStreamPos = jumpStreamPos;
            chunkBytesLeft = 0;
            skip = true;

            break;
            }
            //}}}
          }
        }
      }
    }

  _close (file);
  free (fileChunkBuffer);

  cLog::log (LOGINFO, "exit");
  CoUninitialize();
  mAudAborted = true;
  }
//}}}
//{{{
void cPlayView::vidThread() {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::setThreadName ("vid ");

  const auto fileChunkBuffer = (uint8_t*)malloc (kChunkSize);
  mFirstVidPtsSem.wait();

  bool skip = false;
  int64_t lastJumpStreamPos = -1;

  int64_t streamPos = 0;
  auto file = _open (mFileName.c_str(), _O_RDONLY | _O_BINARY);
  while (!getAbort()) {
    auto chunkPtr = fileChunkBuffer;
    auto chunkBytesLeft = _read (file, fileChunkBuffer, kChunkSize);
    if (chunkBytesLeft < 188) {
      // end of file
      while (!getAbort() && mVidTs->isLoaded (getPlayPts(), 1))
        mPlayPtsSem.wait();
      //{{{  jump to pts in stream
      auto jumpStreamPos = mAnalTs->findStreamPos (mVidTs->getPid(), getPlayPts());
      streamPos = jumpStreamPos;
      _lseeki64 (file, streamPos, SEEK_SET);
      lastJumpStreamPos = jumpStreamPos;

      chunkBytesLeft = 0;
      skip = true;
      //}}}
      }
    else {
      while (chunkBytesLeft >= 188) {
        //{{{  demux up to a frame
        auto bytesUsed = mVidTs->demux (chunkPtr, chunkBytesLeft, streamPos, skip, mVidTs->getPid());
        streamPos += bytesUsed;
        chunkPtr += bytesUsed;
        chunkBytesLeft -= (int)bytesUsed;
        skip = false;
        //}}}
        mWindow->changed();

        while (!getAbort() && mVidTs->isLoaded (getPlayPts(), 4))
          mPlayPtsSem.wait();

        bool loaded = mVidTs->isLoaded (getPlayPts(), 1);
        if (!loaded || (getPlayPts() > mVidTs->getLastLoadedPts() + 100000)) {
          auto jumpStreamPos = mAnalTs->findStreamPos (mVidTs->getPid(), getPlayPts());
          if (jumpStreamPos != lastJumpStreamPos) {
            //{{{  jump to jumpStreamPos, unless same as last, wait for rest of GOP or chunk to demux
            cLog::log (LOGINFO, "jump playPts:" + getPtsString(getPlayPts()) +
                       (loaded ? " after ":" notLoaded ") + getPtsString(mVidTs->getLastLoadedPts()));

            _lseeki64 (file, jumpStreamPos, SEEK_SET);

            streamPos = jumpStreamPos;
            lastJumpStreamPos = jumpStreamPos;
            chunkBytesLeft = 0;
            skip = true;

            break;
            }
            //}}}
          }
        }
      }
    }

  _close (file);
  free (fileChunkBuffer);

  cLog::log (LOGINFO, "exit");
  CoUninitialize();
  mVidAborted = true;
  }
//}}}
//{{{
void cPlayView::playThread() {

  const int kAudSamplesPerUnknownFrame = 1024;

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::setThreadName ("play");
  SetThreadPriority (GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

  mFirstVidPtsSem.wait();
  mPlayPts = mAnalTs->getFirstPts (mVidTs->getPid()) - mAnalTs->getBasePts();

  while (!getAbort()) {
    auto pts = getPlayPts();
    mPlayAudFrame = (cAudFrame*)mAudTs->findFrame (pts);

    // play using frame where available, else play silence
    mAudio->play (mPlayAudFrame ? mPlayAudFrame->mChannels : mAudio->getSrcChannels(),
                  mPlayAudFrame && (mPlaying != ePause) ? mPlayAudFrame->mSamples : nullptr,
                  mPlayAudFrame ? mPlayAudFrame->mNumSamples : kAudSamplesPerUnknownFrame, 1.f);

    if ((mPlaying == ePlay) && (mPlayPts < getLengthPts()))
      incPlayPts (int64_t (((mPlayAudFrame ? mPlayAudFrame->mNumSamples : kAudSamplesPerUnknownFrame) * 90) / 48));
    if (mPlayPts > getLengthPts())
      mPlayPts = getLengthPts();
    mWindow->changed();

    mPlayPtsSem.notifyAll();
    }

  cLog::log (LOGINFO, "exit");
  CoUninitialize();
  mPlayAborted = true;
  }
//}}}
