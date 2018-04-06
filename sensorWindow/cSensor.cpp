// cSensor.cpp
//{{{  includes
#include "stdafx.h"

#include "cSensor.h"

#include "../../shared/utils/utils.h"
#include "../../shared/utils/cLog.h"
#include "../../shared/decoders/jpegHeader.h"

#include "../common/usbUtils.h"
#pragma comment (lib,"CyAPI")

#include "bayer.h"

#include "../inc/jpeglib/jpeglib.h"
#pragma comment (lib,"turbojpeg-static")

using namespace chrono;
//}}}
//{{{  const
const int kLoaderQueueSize = 64;
const size_t kMaxBytes = (16384-16) * 0x800;
const int kMaxFrameBytes = 1600*1200*2; // extra buffer for unwrap around
//}}}

// public
//{{{
void cSensor::run() {

  // allocate enough space past buffer end fix wrap around frame
  mBuffer = (uint8_t*)malloc (kMaxBytes + kMaxFrameBytes);
  mBufferEnd = mBuffer + kMaxBytes;

  initUSB();

  mId = readReg (0x3000);
  if (mId == kMt9d112) {
    mSensorTitle = "Mt9d112";
    cLog::log (LOGNOTICE, "MTD112 - read:0x3000 sensorId " + hex(mId));
    setMode (ePreview);
    }

  else {
    writeReg (0xF0, 0);
    mId = readReg (0x00);
    if (mId == kMt9d111) {
      mSensorTitle = "Mt9d111";
      cLog::log (LOGNOTICE, "MT9D111 - page:0xF0 read:0x00 sensorId " + hex(mId));
      setSlowPll();
      setMode (ePreview);
      }
    else
      cLog::log (LOGNOTICE, "unknown - page:0xF0 read:0x00 sensorId " + hex(mId));
    }

  updateTitle();

  thread ([=](){ listenThread(); }).detach();
  }
//}}}

//{{{
ID2D1Bitmap* cSensor::getBitmapFrame (ID2D1DeviceContext* dc, int bayer, bool info) {

  auto rgbFrame = getRgbFrame (bayer, info);

  if (rgbFrame) {
    if (mBitmap) {
      auto pixelSize = mBitmap->GetPixelSize();
      if ((pixelSize.width != getSize().x) || (pixelSize.height != getSize().y)) {
        // new size, release bitmap and create again at new size
        mBitmap->Release();
        mBitmap = nullptr;
        }
      }
    if (!mBitmap) // need new bitmap, first time or new size
      dc->CreateBitmap (SizeU((int)getSize().x, (int)getSize().y),
                        { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE, 0,0 },
                        &mBitmap);

    // fill bitmap
    mBitmap->CopyFromMemory (&RectU (0, 0, (int)getSize().x, (int)getSize().y), rgbFrame, (int)getSize().x*4);

    // free rgbFrame
    free (rgbFrame);
    }

  return mBitmap;
  }
//}}}

//{{{
void cSensor::setSlowPll() {

  switch (mId) {
    case kMt9d111:
      //setPll (16, 1, 1); // 48Mhz
      setPll (18, 1, 2); // 36Mhz
      break;

    case kMt9d112:
      writeReg (0x341E, 0x8F09); // PLL,clk_in control BYPASS PLL
      writeReg (0x341C, 0x0150); // PLL 13:8:n=1, 7:0:m=85 clk = (10mhz/(n+1)) * (m/8) = 50mhz
      Sleep (5);
      writeReg (0x341E, 0x8F09); // PLL,clk_in control: PLL ON, bypass PLL
      writeReg (0x341E, 0x8F08); // PLL,clk_in control: USE PLL
      break;
    }
  }
//}}}
//{{{
void cSensor::setFastPll() {

  switch (mId) {
    case kMt9d111:
      setPll (80, 11, 0); // 80Mhz
      break;

    case kMt9d112:
      writeReg (0x341E, 0x8F09); // PLL,clk_in control BYPASS PLL
      writeReg (0x341C, 0x0180); // PLL 13:8:n=1, 7:0:m=128 clk = (10mhz/(n+1)) * (m/8) = 80mhz
      Sleep (5);
      writeReg (0x341E, 0x8F09); // PLL,clk_in control: PLL ON, bypass PLL
      writeReg (0x341E, 0x8F08); // PLL,clk_in control: USE PLL
      break;
    }
  }
//}}}
//{{{
void cSensor::setPll (int m, int n, int p) {

  mPllm = m;
  mPlln = n;
  mPllp = p;

  if (mId == kMt9d111) {
    cLog::log (LOGINFO, "pll m:%d n:%d p:%d Fpfd(2-13):%d Fvco(110-240):%d Fout(6-80):%d",
                        mPllm, mPlln, mPllp,
                        24000000 / (mPlln+1), (24000000 / (mPlln+1))*mPllm, ((24000000 / (mPlln+1) * mPllm)/ (2*(mPllp+1))));

    //  PLL = (24mhz / (N+1)) * M / 2*(P+1)
    writeReg (0x66, (mPllm << 8) | mPlln);  // PLL Control1    M:N
    writeReg (0x67, 0x0500 | mPllp)      ;  // PLL Control2 0x05:P
    writeReg (0x65, 0xA000);  // Clock CNTRL - PLL ON
    writeReg (0x65, 0x2000);  // Clock CNTRL - USE PLL
    Sleep (100);
    }
  }
//}}}

//{{{
void cSensor::setMode (eMode mode) {

  mLastFramePtr = NULL;

  mMode = mode;
  switch (mode) {
    case ePreview:
      //{{{  previewYuv
      cLog::log (LOGINFO, "setMode previewYuv");

      mWidth = 800;
      mHeight = 600;

      if (mId == kMt9d111) {
        writeReg (0xF0, 1);
        writeReg (0x09, 0x000A); // factory bypass 10 bit sensor
        writeReg (0x97, 0x02); // output format configuration Yuv swap
        writeReg (0xC6, 0xA120); writeReg (0xC8, 0x00); // Sequencer.params.mode - none
        writeReg (0xC6, 0xA103); writeReg (0xC8, 0x01); // Sequencer goto preview A
        }

      else if (mId == kMt9d112) {
        // set A preview mode
        writeReg (0x338c, 0x2795); writeReg (0x3390, 0x0002); // set A output format
        writeReg (0x338C, 0xA120); writeReg (0x3390, 0x0000); // sequencer.params.mode - none
        writeReg (0x338C, 0xA103); writeReg (0x3390, 0x0001); // sequencer.cmd - goto preview mode A
        }
      else
        cLog::log (LOGERROR, "setMode preview - unknown sensor");

      break;
      //}}}
    case ePreviewRgb565:
      //{{{  previewRgb565
      cLog::log (LOGINFO, "setMode previewRgb565");

      mWidth = 800;
      mHeight = 600;

      if (mId == kMt9d111) {
        writeReg (0xF0, 1);
        writeReg (0x09, 0x000A); // factory bypass 10 bit sensor
        writeReg (0x97, 0x0022); // output format configuration - RGB565, swap odd even
        writeReg (0xC6, 0xA120); writeReg (0xC8, 0x00); // Sequencer.params.mode - none
        writeReg (0xC6, 0xA103); writeReg (0xC8, 0x01); // Sequencer goto preview A
        }

      break;
      //}}}

    case eCapture:
      //{{{  captureYuv
      cLog::log (LOGINFO, "setMode captureYuv");

      mWidth = 1600;
      mHeight = 1200;

      if (mId == kMt9d111) {
        writeReg (0xF0, 1);
        writeReg (0x09, 0x000A); // factory bypass 10 bit sensor
        writeReg (0x97, 0x02); // output format configuration Yuv swap
        writeReg (0xC6, 0xA120); writeReg (0xC8, 0x02); // Sequencer.params.mode - capture video
        writeReg (0xC6, 0xA103); writeReg (0xC8, 0x02); // Sequencer goto capture B
        }

      else if (mId == kMt9d112) {
        // set B capture mode
        writeReg (0x338c, 0x2797); writeReg (0x3390, 0x0002); // set B output format
        writeReg (0x338C, 0xA120); writeReg (0x3390, 0x0002); // sequencer.params.mode - capture video
        writeReg (0x338C, 0xA103); writeReg (0x3390, 0x0002); // sequencer.cmd - goto capture mode B
        }
      else
        cLog::log (LOGERROR, "setMode capture - unknown sensor");

      break;
      //}}}
    case eCaptureRgb565:
      //{{{  captureRgb565
      cLog::log (LOGINFO, "setMode captureRgb565");

      mWidth = 1600;
      mHeight = 1200;

      if (mId == kMt9d111) {
        writeReg (0xF0, 1);
        writeReg (0x09, 0x000A); // factory bypass 10 bit sensor
        writeReg (0x97, 0x0022); // output format configuration - RGB565, swap odd even
        writeReg (0xC6, 0xA120); writeReg (0xC8, 0x02); // Sequencer.params.mode - capture video
        writeReg (0xC6, 0xA103); writeReg (0xC8, 0x02); // Sequencer goto capture B
        }

      break;
      //}}}

    case eBayer:
      //{{{  bayer
      cLog::log (LOGINFO, "setMode bayer");

      if (mId == kMt9d111) {
        mWidth = 1608;
        mHeight = 1208;

        writeReg (0xF0, 1);
        writeReg (0x09, 0x0008); // factory bypass 10 bit sensor
        writeReg (0xC6, 0xA120); writeReg (0xC8, 0x02); // Sequencer.params.mode - capture video
        writeReg (0xC6, 0xA103); writeReg (0xC8, 0x02); // Sequencer goto capture B
        }

      else if (mId == kMt9d112) {
        mWidth = 1600;
        mHeight = 1200;

        writeReg (0x338c, 0x2797); writeReg (0x3390, 0x0100); // set B output format - processed bayer mode
        //writeReg (0x338c, 0x2797); writeReg (0x3390, 0x0004); // set B output format - progressive bayer mode
        writeReg (0x338C, 0xA120); writeReg (0x3390, 0x0002); // sequencer.params.mode - capture video
        writeReg (0x338C, 0xA103); writeReg (0x3390, 0x0002); // sequencer.cmd - goto capture mode B
        }
      else
        cLog::log (LOGERROR, "setMode bayer - unknown sensor");

      break;
      //}}}
    case eJpeg:
      //{{{  jpeg
      if (mId == kMt9d111) {
        cLog::log (LOGINFO, "setMode jpeg");

        mWidth = 1600;
        mHeight = 1200;

        //{{{  last data byte status ifp page2 0x02
        // b:0 = 1  transfer done
        // b:1 = 1  output fifo overflow
        // b:2 = 1  spoof oversize error
        // b:3 = 1  reorder buffer error
        // b:5:4    fifo watermark
        // b:7:6    quant table 0 to 2
        //}}}
        writeReg (0xf0, 0x0001);

        // mode_config JPG bypass - shadow ifp page2 0x0a
        writeReg (0xc6, 0x270b); writeReg (0xc8, 0x0010); // 0x0030 to disable B

        //{{{  jpeg.config id=9  0x07
        // b:0 =1  video
        // b:1 =1  handshake on error
        // b:2 =1  enable retry on error
        // b:3 =1  host indicates ready
        // ------
        // b:4 =1  scaled quant
        // b:5 =1  auto select quant
        // b:6:7   quant table id
        //}}}
        writeReg (0xc6, 0xa907); writeReg (0xc8, 0x0031);

        //{{{  mode fifo_config0_B - shadow ifp page2 0x0d
        //   output config  ifp page2  0x0d
        //   b:0 = 1  enable spoof frame
        //   b:1 = 1  enable pixclk between frames
        //   b:2 = 1  enable pixclk during invalid data
        //   b:3 = 1  enable soi/eoi
        //   -------
        //   b:4 = 1  enable soi/eoi during FV
        //   b:5 = 1  enable ignore spoof height
        //   b:6 = 1  enable variable pixclk
        //   b:7 = 1  enable byteswap
        //   -------
        //   b:8 = 1  enable FV on LV
        //   b:9 = 1  enable status inserted after data
        //   b:10 = 1  enable spoof codes
        //}}}
        writeReg (0xc6, 0x2772); writeReg (0xc8, 0x0067);

        //{{{  mode fifo_config1_B - shadow ifp page2 0x0e
        //   b:3:0   pclk1 divisor
        //   b:7:5   pclk1 slew
        //   -----
        //   b:11:8  pclk2 divisor
        //   b:15:13 pclk2 slew
        //}}}
        writeReg (0xc6, 0x2774); writeReg (0xc8, 0x0101);

        //{{{  mode fifo_config2_B - shadow ifp page2 0x0f
        //   b:3:0   pclk3 divisor
        //   b:7:5   pclk3 slew
        //}}}
        writeReg (0xc6, 0x2776); writeReg (0xc8, 0x0001);

        // mode OUTPUT WIDTH HEIGHT B
        writeReg (0xc6, 0x2707); writeReg (0xc8, mWidth);
        writeReg (0xc6, 0x2709); writeReg (0xc8, mHeight);

        // mode SPOOF WIDTH HEIGHT B
        writeReg (0xc6, 0x2779); writeReg (0xc8, mWidth);
        writeReg (0xc6, 0x277b); writeReg (0xc8, mHeight);

        //writeReg (0x09, 0x000A); // factory bypass 10 bit sensor
        writeReg (0xC6, 0xA120); writeReg (0xC8, 0x02); // Sequencer.params.mode - capture video
        writeReg (0xC6, 0xA103); writeReg (0xC8, 0x02); // Sequencer goto capture B

        //writeReg (0xc6, 0xa907); printf ("JPEG_CONFIG %x\n",readReg (0xc8));
        //writeReg (0xc6, 0x2772); printf ("MODE_FIFO_CONF0_B %x\n",readReg (0xc8));
        //writeReg (0xc6, 0x2774); printf ("MODE_FIFO_CONF1_B %x\n",readReg (0xc8));
        //writeReg (0xc6, 0x2776); printf ("MODE_FIFO_CONF2_B %x\n",readReg (0xc8));
        //writeReg (0xc6, 0x2707); printf ("MODE_OUTPUT_WIDTH_B %d\n",readReg (0xc8));
        //writeReg (0xc6, 0x2709); printf ("MODE_OUTPUT_HEIGHT_B %d\n",readReg (0xc8));
        //writeReg (0xc6, 0x2779); printf ("MODE_SPOOF_WIDTH_B %d\n",readReg (0xc8));
        //writeReg (0xc6, 0x277b); printf ("MODE_SPOOF_HEIGHT_B %d\n",readReg (0xc8));
        //writeReg (0xc6, 0xa906); printf ("JPEG_FORMAT %d\n",readReg (0xc8));
        //writeReg (0xc6, 0x2908); printf ("JPEG_RESTART_INT %d\n", readReg (0xc8));
        //writeReg (0xc6, 0xa90a); printf ("JPEG_QSCALE_1 %d\n",readReg (0xc8));
        //writeReg (0xc6, 0xa90b); printf ("JPEG_QSCALE_2 %d\n",readReg (0xc8));
        //writeReg (0xc6, 0xa90c); printf ("JPEG_QSCALE_3 %d\n",readReg (0xc8));
        }
      else
        cLog::log (LOGERROR, "setMode jpeg - unknown sensor");

      break;
      //}}}
    }

  mLastFramePtr = NULL;
  }
//}}}

//{{{
float cSensor::getFocus() {
  return mFocus;
  }
//}}}
//{{{
void cSensor::setFocus (float focus) {
  mFocus = focus;
  mFocus = max (mFocus, 0.f);
  mFocus = min (mFocus, 255.f);
  sendFocus ((int)mFocus);
  };
//}}}

// private:
//{{{
uint8_t cSensor::limitByte (float v) {

  if (v <= 0.0)
    return 0;
  if (v >= 255.0)
    return 255;

  return (uint8_t)v;
  }
//}}}
//{{{
uint8_t* cSensor::getFramePtr (int& frameLen) {

  mSem.wait();

  frameLen = mLastFrameLen;
  mFrameLen = mLastFrameLen;

  return mLastFramePtr;
  }
//}}}
//{{{
uint8_t* cSensor::getRgbFrame (int bayer, bool info) {

  int frameLen;
  auto framePtr = getFramePtr (frameLen);
  if (!framePtr)
    return nullptr;

  // alloc rgba buffer
  auto rgbaBuffer = (uint8_t*)malloc (mWidth * mHeight * 4);

  bool ok = false;
  auto fromTime = system_clock::now();

  switch (getMode()) {
    case ePreview:
    case eCapture:
      //{{{  yuv
      if (frameLen == mWidth*mHeight *2) {
        if (info) {
          mLumaHistogram.startValues();
          mVector.startValues();
          }

        auto bufferPtr = rgbaBuffer;
        for (auto y = 0; y < mHeight; y++) {
          for (auto x = 0; x < mWidth; x += 2) {
            int y1 = *framePtr++;
            int  u = *framePtr++;
            int y2 = *framePtr++;
            int  v = *framePtr++;

            if (info) {
              mLumaHistogram.incValue (y1, y2);
              mVector.addValue (y1, u, v);
              }
            u -= 128;
            v -= 128;

            *bufferPtr++ = limitByte (y1 + (1.8556f * u));
            *bufferPtr++ = limitByte (y1 - (0.1873f * u) - (0.4681f * v));
            *bufferPtr++ = limitByte (y1 + (1.5748f * v));
            *bufferPtr++ = 255;

            *bufferPtr++ = limitByte (y2 + (1.8556f * u));
            *bufferPtr++ = limitByte (y2 - (0.1873f * u) - (0.4681f * v));
            *bufferPtr++ = limitByte (y2 + (1.5748f * v));
            *bufferPtr++ = 255;
            }
          }

        if (info) {
          mLumaHistogram.finishValues();
          mVector.finishValues();
          }

        ok = true;
        }
      break;
      //}}}
    case ePreviewRgb565:
    case eCaptureRgb565:
      //{{{  rgb565
      if (frameLen == mWidth*mHeight*2) {
        uint16_t* src = (uint16_t*)framePtr;
        auto dst = rgbaBuffer;
        for (auto y = 0; y < mHeight; y++) {
          for (auto x = 0; x < mWidth; x++) {
            uint16_t rgb = *src++;
            *dst++ = (rgb & 0x001F) << 3;
            *dst++ = (rgb & 0x07E0) >> 3;
            *dst++ = (rgb & 0xF800) >> 8;
            *dst++ = 255;
            }
          }

        ok = true;
        }
      break;
      //}}}
    case eBayer:
      //{{{  bayer
      if (frameLen == mWidth * mHeight) {
        if (info)
          mRgbHistogram.startValues();

        //   g1 r   =  grbg
        //   b  g2
        auto rgbBuffer = (uint8_t*)malloc (mWidth * mHeight *3);
        bayerDecode8 (framePtr, rgbBuffer, mWidth, mHeight, COLOR_GBRG, tBayer(bayer));

        auto bufferPtr = rgbaBuffer;
        auto rgbPtr = rgbBuffer;
        for (auto i = 0; i < mWidth*mHeight; i++) {
          if (info)
            mRgbHistogram.incValue (*(rgbPtr+2), *(rgbPtr+1), *rgbPtr);

          *bufferPtr++ = *rgbPtr++;
          *bufferPtr++ = *rgbPtr++;
          *bufferPtr++ = *rgbPtr++;
          *bufferPtr++ = 255;
          }
        free (rgbBuffer);

        if (info)
          mRgbHistogram.finishValues();

        ok = true;
        }
      break;
      //}}}
    case eJpeg:
      //{{{  jpeg
      if ((frameLen == 800*600*2) || (frameLen == 1600*800*2) || (frameLen == 1608*1208))
        cLog::log (LOGERROR, "discarding nonJpeg frame %d %dx%d", frameLen, mWidth, mHeight);

      else {
        cLog::log (LOGINFO, "jpeg frame %d %dx%d", frameLen, mWidth, mHeight);
        auto jpegEndPtr = framePtr + frameLen - 4;
        int jpegLen = *jpegEndPtr++;
        jpegLen += (*jpegEndPtr++) << 8;
        jpegLen += (*jpegEndPtr++) << 16;

        auto status = *jpegEndPtr;
        if ((*jpegEndPtr & 0x0f) != 0x01)
          cLog::log (LOGERROR, "err status:%x expectedLen:%d jpegLen:%d %dx%d", *jpegEndPtr, frameLen, jpegLen, mWidth, mHeight);
        else {
          ok = true;

          // form jpeg header, read JPEG_QSCALE_1 = 6?
          writeReg (0xc6, 0xa90a);
          int qscale1 = readReg (0xc8);
          uint8_t jpegHeader[1000];
          int jpegHeaderLen = setJpegHeader (jpegHeader, mWidth, mHeight, 0, qscale1);

          // append EOI marker
          framePtr[jpegLen] = 0xff;
          framePtr[jpegLen+1] = 0xd9;

          cLog::log (LOGINFO, "jpegFrame:%d len:%d:%d %dx%d qscale1:%d", 
                              mFrameCount, frameLen, jpegLen, mWidth, mHeight, qscale1);

          // decompress framePtr to rgbaBuffer
          struct jpeg_error_mgr mJerr;
          struct jpeg_decompress_struct mCinfo;
          mCinfo.err = jpeg_std_error (&mJerr);
          jpeg_create_decompress (&mCinfo);
          jpeg_mem_src (&mCinfo, jpegHeader, jpegHeaderLen);
          jpeg_read_header (&mCinfo, TRUE);
          mCinfo.out_color_space = JCS_EXT_BGRA;
          jpeg_mem_src (&mCinfo, framePtr, jpegLen+2);
          jpeg_start_decompress (&mCinfo);
          while (mCinfo.output_scanline < mCinfo.output_height) {
            unsigned char* bufferArray[1];
            bufferArray[0] = rgbaBuffer + (mCinfo.output_scanline * mCinfo.output_width * mCinfo.output_components);
            jpeg_read_scanlines (&mCinfo, bufferArray, 1);
            }
          jpeg_finish_decompress (&mCinfo);
          jpeg_destroy_decompress (&mCinfo);

          //  write framePtr to file, including appended EOI
          char filename[200];
          sprintf (filename, "C:/Users/colin/Desktop/piccies/cam%d.jpg", mFrameCount++);
          auto file = fopen (filename, "wb");
          fwrite (jpegHeader, 1, jpegHeaderLen, file); // write JPEG header
          fwrite (framePtr, 1, jpegLen+2, file);       // write JPEG data, including appended EOI marker
          fclose (file);
          }
        }
      break;
      //}}}
    }

  mRgbTime = (float)duration_cast<milliseconds>(system_clock::now() - fromTime).count() / 1000.f;
  updateTitle();

  if (ok)
    return rgbaBuffer;
  else {
    free (rgbaBuffer);
    return nullptr;
    }
  }
//}}}

//{{{
void cSensor::updateTitle() {
  mTitle = mSensorTitle +
           " " + dec(getSize().x) + "x" + dec(getSize().y) +
           " " + dec(mFrameLen) +
           "b "   + frac(1.f/getFrameTime(),0,3,' ') + "ms + " + frac(getRgbTime()*1000.f,0,2, ' ') + "ms";
  }
//}}}

//{{{
void cSensor::listenThread() {

  cLog::log (LOGNOTICE, "listenThread start - numBuffers:%d", kLoaderQueueSize);

  uint8_t* bufferPtr[kLoaderQueueSize];
  uint8_t* contexts[kLoaderQueueSize];
  OVERLAPPED overLapped[kLoaderQueueSize];
  auto bufPtr = mBuffer;
  auto pktLen = getBulkEndPoint()->MaxPktSize - 16;
  for (auto i = 0; i < kLoaderQueueSize; i++) {
    //{{{  init, queue kLoaderQueueSize transfers
    bufferPtr[i] = bufPtr;
    overLapped[i].hEvent = CreateEvent (NULL, false, false, NULL);
    contexts[i] = getBulkEndPoint()->BeginDataXfer (bufferPtr[i], pktLen, &overLapped[i]);

    if (getBulkEndPoint()->NtStatus || getBulkEndPoint()->UsbdStatus) {
      // error and return
      cLog::log (LOGNOTICE, "BeginDataXfer init failed " + dec(getBulkEndPoint()->NtStatus) +
                            " " + dec(getBulkEndPoint()->UsbdStatus));
      return;
      }

    bufPtr += pktLen;
    if (bufPtr + pktLen > mBufferEnd)
      cLog::log (LOGERROR, "loaderThread - not enough samples");
    }
    //}}}

  startStreaming();

  auto i = 0;
  auto framePtr = mBuffer;
  while (true) {
    if (!getBulkEndPoint()->WaitForXfer (&overLapped[i], 1000)) {
      //{{{  error - 2sec timeout
      cLog::log (LOGERROR , "listenThread timeOut" + dec(getBulkEndPoint()->LastError));
      getBulkEndPoint()->Abort();
      if (getBulkEndPoint()->LastError == ERROR_IO_PENDING)
        WaitForSingleObject (overLapped[i].hEvent, 2000);
      }
      //}}}

    LONG rxLen = pktLen;
    if (getBulkEndPoint()->FinishDataXfer (bufferPtr[i], rxLen, &overLapped[i], contexts[i])) {
      if (rxLen != pktLen) {
        // incomplete packet at end of frame
        auto fromTime = system_clock::now();
        mFrameTime = (float)duration_cast<milliseconds>(fromTime - mLastTime).count() / 1000.f;
        mLastTime = fromTime;

        mLastFramePtr = framePtr;
        if (framePtr < (bufferPtr[i] + rxLen))
          mLastFrameLen = int((bufferPtr[i] + rxLen) - framePtr);
        else {
          //{{{  wraparound
          mLastFrameLen = int(mBufferEnd - mLastFramePtr + (bufferPtr[i] + rxLen) - mBuffer);
          if (mLastFrameLen > mBufferEnd - mLastFramePtr)
            memcpy (mBufferEnd, mBuffer, mLastFrameLen - (mBufferEnd - mLastFramePtr));
          }
          //}}}
        mSem.notify();

        // framePtr for next frame
        framePtr = (bufferPtr[i] + pktLen + pktLen <= mBufferEnd) ? bufferPtr[i] + pktLen : mBuffer;
        }

      // requeue transfer
      bufferPtr[i] = bufPtr;
      contexts[i] = getBulkEndPoint()->BeginDataXfer (bufferPtr[i], pktLen, &overLapped[i]);
      if (getBulkEndPoint()->NtStatus || getBulkEndPoint()->UsbdStatus) {
        //{{{  error and return
        cLog::log (LOGERROR , "listenThread BeginDataXfer init failed " +
                              dec(getBulkEndPoint()->NtStatus) +
                              " " + dec(getBulkEndPoint()->UsbdStatus));
        return;
        }
        //}}}
      // bufPtr for next requeue transfer
      bufPtr = (bufPtr + pktLen + pktLen <= mBufferEnd) ? bufPtr + pktLen : mBuffer;
      }
    else {
      //{{{  error and return
      cLog::log (LOGERROR , "listenThread FinishDataXfer init failed");
      return;
      }
      //}}}

    i %= kLoaderQueueSize;
    }

  cLog::log (LOGERROR, "listenThread exit on error");
  }
//}}}
