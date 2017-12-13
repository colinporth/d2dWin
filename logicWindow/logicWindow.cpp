// logicWindow.cpp
//{{{  includes
#include "stdafx.h"

#include "../common/usbUtils.h"

#include "../common/cLogBox.h"
#include "../common/cWindowBox.h"
//}}}
//{{{  defines
#define QUEUESIZE 64
//#define SYNC_SAMPLES
#define SAMPLE_TYPE uint16_t // uint16_t uint32_t
//}}}

class cAppWindow : public cD2dWindow {
public:
  //{{{
  void run (string title, int width, int height) {

    initialise (title, width, height, false);
    addBox (new cBgndBox (this, 0, 0));
    addBox (new cLogBox (this, 200.0f, 0), 0,0);
    addBox (new cWindowBox (this, -60.0f, 24.0f), 0,0);

    getDc()->CreateSolidColorBrush (ColorF(ColorF::CornflowerBlue), &mBrush);

    mSamples = (SAMPLE_TYPE*)malloc (maxSampleBytes);
    setMidSample (0);
    setSamplesPerPixel (maxSamplesPerPixel);

    initUSB();

    mSensorId = readReg (0x3000);
    if (mSensorId == 0x1519) {
      cLog::log (LOGNOTICE, "MT9D111 - sensorId " + hex(mSensorId));
      setPll (16, 1, 1); // 48Mhz
      //setPll(80, 11, 0); // 80 Mhz
      writeReg (0xF0, 0);
      setMode (ePreview);
      }
    else if (mSensorId == 0x1580) {
      cLog::log (LOGNOTICE, "MTD112 - sensorId " + hex(mSensorId));
      setMode (ePreview);
      }
    else {
      cLog::log (LOGNOTICE, "unknown sensorId 0x3000 read " + hex(mSensorId));
      writeReg (0xF0, 0);
      mSensorId = readReg (0x00);
      cLog::log (LOGNOTICE, "unknown sensorId - 0x3F0 write, 0x00 read - " + hex(mSensorId));
      }

    readReg (0x00);

    auto threadHandle = thread([=]() { loaderThread(); });
    SetThreadPriority (threadHandle.native_handle(), THREAD_PRIORITY_ABOVE_NORMAL);
    threadHandle.detach();

    thread ([=]() { analysisThread(); } ).detach();

    messagePump();
    }
  //}}}

protected:
  //{{{
  bool onKey (int key) {

    switch (key) {
      case 0x00 : return false;

      case 0x1B: return true; // escape abort

      case ' ' : moreSamples(); break; // space bar

      case 0x21: setSamplesPerPixel (samplesPerPixel /= 2.0); break; // page up
      case 0x22: setSamplesPerPixel (samplesPerPixel *= 2.0); break; // page down

      case 0x23: setMidSample (mSamplesLoaded); break; // end
      case 0x24: setMidSample (0.0); break;                         // home
      case 0x25: setMidSample (midSample - (getSize().x/4.0) * samplesPerPixel); break; // left arrow
      case 0x26:
        //{{{  up arrow
        if (samplesPerPixel > 1.0)
          setMidSample (midSample - (getControl() ? 4.0 : getShift() ? 2.0 : 1.0) * samplesPerPixel);
        else if (floor (midSample) != midSample)
          setMidSample (floor (midSample));
        else
          setMidSample (midSample - 1.0);
        break;
        //}}}
      case 0x27: setMidSample (midSample + (getSize().x/4.0) * samplesPerPixel); break; // right arrow
      case 0x28:
        //{{{  down arrow
        if (samplesPerPixel > 1.0)
          setMidSample (midSample + (getControl() ? 4.0 : getShift() ? 2.0 : 1.0) * samplesPerPixel);
        else if (ceil (midSample) != midSample)
          setMidSample (midSample);
        else
          setMidSample (midSample + 1.0);
        break;
        //}}}

      case 'P':  setMode (mMode != ePreview ? ePreview : eFull); break;
      case 'B' : setMode (eBayer); break;
      case 'J' : setMode (eJpeg); break;

      case 'A' : mPllm++; setPll (mPllm, mPlln, mPllp); break;
      case 'Z' : mPllm--; setPll(mPllm, mPlln, mPllp); break;
      case 'S' : mPlln++; setPll(mPllm, mPlln, mPllp); break;
      case 'X' : mPlln--; setPll(mPllm, mPlln, mPllp); break;
      case 'D' : mPllp++; setPll(mPllm, mPlln, mPllp); break;;
      case 'C' : mPllp--; setPll(mPllm, mPlln, mPllp); break;
      case 'R' : setPll(80, 11, 0); break;  // 80 Mhz
      case 'T' : setPll(16, 1, 1); break;  // 48 Mhz

      case 'F' : toggleFullScreen(); break;

      default: cLog::log (LOGERROR, "key %x\n", key);
      }

    return false;
    }
  //}}}

private:
  //{{{
  class cBgndBox : public cBox {
  public:
    //{{{
    cBgndBox (cAppWindow* window, float width, float height) : cBox("bgnd", window, width, height) {
      mPin = true;
      }
    //}}}
    virtual ~cBgndBox() {}

    //{{{
    bool onProx (bool inClient, cPoint pos) {

      auto appWindow = dynamic_cast<cAppWindow*>(mWindow);

      uint32_t mask;
      if (appWindow->getMask ((int)pos.x, (int)pos.y, mask))
        appWindow->measure (mask, int(appWindow->midSample + (pos.x - mWindow->getSize().x/2.0f) * appWindow->samplesPerPixel));
      return true;
      }
    //}}}
    //{{{
    bool onWheel (int delta, cPoint pos) {

      auto appWindow = dynamic_cast<cAppWindow*>(mWindow);
      double ratio = mWindow->getControl() ? 1.5 : mWindow->getShift() ? 1.25 : 2.0;
      if (delta > 0)
        ratio = 1.0/ratio;

      appWindow->setSamplesPerPixel (appWindow->samplesPerPixel * ratio);
      return true;
      }
    //}}}
    //{{{
    bool onMove (bool right, cPoint pos, cPoint inc) {

      auto appWindow = dynamic_cast<cAppWindow*>(mWindow);
      if (right) {
        uint32_t mask = 0;
        //if (appWindow->getMask (mDownMousex, mDownMousey, mask))
        //  appWindow->count (mask,
        //         int(appWindow->midSample + (mDownMousex - mWindow->getClientF().x/2.0f) * appWindow->samplesPerPixel),
        //         int(appWindow->midSample + (pos.x - mWindow->getClientF().x/2.0f) * appWindow->samplesPerPixel));
        }
      else
        appWindow->setMidSample (appWindow->midSample - (inc.x * appWindow->samplesPerPixel));

      return true;
      }
    //}}}
    //{{{
    bool onUp (bool right, bool mouseMoved, cPoint pos) {

      auto appWindow = dynamic_cast<cAppWindow*>(mWindow);

      if (!right)
        if (!mouseMoved)
          appWindow->setMidSample (appWindow->midSample + (pos.x - mWindow->getSize().x/2.0) * appWindow->samplesPerPixel);
      return true;
      }
    //}}}
    //{{{
    void onDraw (ID2D1DeviceContext* dc) {

      auto appWindow = dynamic_cast<cAppWindow*>(mWindow);
      appWindow->draw (dc);
      }
    //}}}
    };
  //}}}

  //{{{
  void setMidSample (double newMidSample) {
  // set sample of pixel in middle of display window

    if (newMidSample < 0.0)
      midSample = 0.0;
    else if (newMidSample > (double)mSamplesLoaded)
      midSample = (double)mSamplesLoaded;
    else
      midSample = newMidSample;

    changed();
    }
  //}}}
  //{{{
  void setSamplesPerPixel (double newSamplesPerPixel) {
  // set display window scale, samplesPerPixel

    if (newSamplesPerPixel < minSamplesPerPixel)
      samplesPerPixel = minSamplesPerPixel;
    else if (newSamplesPerPixel > maxSamplesPerPixel)
      samplesPerPixel = maxSamplesPerPixel;
    else
      samplesPerPixel = newSamplesPerPixel;

    samplesPerGraticule = 10.0;
    while (samplesPerGraticule < (100.0 * samplesPerPixel))
      samplesPerGraticule *= 10.0;

    if (samplesPerGraticule <= 10.0)
      graticuleStr = dec(int (samplesPerGraticule / 0.1)) + "ns";
    else if (samplesPerGraticule <= 10000.0)
      graticuleStr = dec(int (samplesPerGraticule / 100)) + "us";
    else if (samplesPerGraticule <= 10000000.0)
      graticuleStr = dec(int (samplesPerGraticule / 100000)) + "ms";
    else if (samplesPerGraticule <= 10000000000.0)
      graticuleStr = dec(int (samplesPerGraticule / 100000000)) + "s";
    else
      graticuleStr = dec(int (samplesPerGraticule / 6000000000)) + "m";
    changed();
    }
  //}}}
  //{{{
  void moreSamples() {

    restart = true;
    setMidSample (0);
    mSamplesLoaded = 0;
    }
  //}}}

  //{{{
  void draw (ID2D1DeviceContext* dc) {

    int channels = sizeof(SAMPLE_TYPE) * 8;
    //{{{  draw title
    std::string str;
    #ifdef SYNC_SAMPLES
      str = "SYNC sample " + dec((int)midSample) + " of " + dec (mSamplesLoaded);
    #else
      str = "sample " +
            dec (midSample / samplesPerSecond) +
            " of " + dec(mSamplesLoaded / samplesPerSecond) +
            " samples:" + dec(mSamplesLoaded);
    #endif
    dc->DrawText (wstring (str.begin(), str.end()).data(), (UINT32)str.size(), getTextFormat(),
                  RectF(leftPixels, 0, getSize().x, getSize().y), getWhiteBrush());
    //}}}
    //{{{  draw graticule
    dc->DrawText (std::wstring(graticuleStr.begin(), graticuleStr.end()).data(), (UINT32)graticuleStr.size(),
                  getTextFormat(), RectF (0, 0, leftPixels, rowPixels), getWhiteBrush());

    auto rg = RectF (0, rowPixels, 0, (channels+1)*rowPixels);

    double leftSample = midSample - ((getSize().x/2.0f - leftPixels) * samplesPerPixel);
    int graticule = int(leftSample + (samplesPerGraticule - 1.0)) / (int)samplesPerGraticule;
    double graticuleSample = graticule * samplesPerGraticule;

    bool more = true;
    while (more) {
      more = graticuleSample <= maxSamples;
      if (!more)
        graticuleSample = (double)maxSamples;

      rg.left = float((graticuleSample - midSample) / samplesPerPixel) + getSize().x/2.0f;
      rg.right = rg.left+1;
      if (graticule > 0)
        dc->FillRectangle (rg, getGreyBrush());

      graticule++;
      graticuleSample += samplesPerGraticule;

      rg.left = rg.right;
      more &= rg.left < getSize().x;
      }

    dc->FillRectangle (
      RectF(getSize().x/2.0f, rowPixels, getSize().x/2.0f + 1.0f, (channels+1)*rowPixels),
      getWhiteBrush());

    auto rl = RectF(0.0f, rowPixels, getSize().x, getSize().y);
    for (auto dq = 0; dq < channels; dq++) {
      std::string str = "dq" + dec(dq);
      dc->DrawText (wstring (str.begin(), str.end()).data(), (UINT32)str.size(), getTextFormat(),
                    rl, getWhiteBrush());
      rl.top += rowPixels;
      }
    //}}}
    //{{{  draw samples
    auto r = RectF (leftPixels, rowPixels, leftPixels+1.0f, 0);
    int lastSampleIndex = 0;
    for (auto pix = leftPixels - int(getSize().x/2.0f); pix < int(getSize().x/2.0f); pix++) {
      int sampleIndex = int(midSample + (pix * samplesPerPixel));
      if (sampleIndex >= mSamplesLoaded)
        break;

      if (sampleIndex >= 0) {
        uint32_t transition = 0;
        // set transition bitMask for samples spanned by this pixel column
        if (lastSampleIndex && (lastSampleIndex != sampleIndex)) {
          // look for transition from lastSampleIndex to sampleIndex+1
          if (sampleIndex - lastSampleIndex <= 32) {
            // use all samples
            for (int index = lastSampleIndex; index < sampleIndex; index++)
              transition |= mSamples[index % maxSamples] ^ mSamples[(index+1) % maxSamples];
            }
          else {
            // look at some samples for transition
            int indexInc = (sampleIndex - lastSampleIndex) / 13;
            for (int index = lastSampleIndex; (index + indexInc <= sampleIndex)  && (index + indexInc < mSamplesLoaded - 1); index += indexInc)
              transition |= mSamples[index % maxSamples] ^ mSamples[(index+indexInc) % maxSamples];
            }
          }

        r.top = rowPixels;
        uint32_t bitMask = 1;
        for (int i = 1; i <= channels; i++) {
          //{{{  draw sample at this pixel column
          float nextTop = r.top + ((bitMask & 0x80808080) ? groupRowPixels : rowPixels);

          if (transition & bitMask)  // bit hi
            r.bottom = r.top + barPixels;
          else {
            if (!(mSamples[sampleIndex % maxSamples] & bitMask)) // bit lo
              r.top += barPixels - 1.0f;
            r.bottom = r.top + 1.0f;
            }

          dc->FillRectangle (r, getBlueBrush());
          //}}}
          bitMask <<= 1;
          r.top = nextTop;
          if (r.top >= getSize().y)
            break;
          }

        if (samplesPerPixel < 0.1) {
          if (sampleIndex != lastSampleIndex) {
            std::string str = hex (mSamples[sampleIndex % maxSamples]);
            dc->DrawText (wstring (str.begin(), str.end()).data(), (UINT32)str.size(), getTextFormat(),
                          RectF (r.left + 2.0f, r.top-12.0f , r.left + 24.0f, r.top + 12.0f), getWhiteBrush());
            }
          r.top += 12.0f;
          }

        uint8_t val = mSamples[sampleIndex % maxSamples] & 0xFF;
        mBrush->SetColor (ColorF ((val << 16) | (val << 8) | (val), 1.0f));
        r.bottom = r.top + 12.0f;
        dc->FillRectangle (r, mBrush);

        r.top += 255 - val;
        r.bottom = r.top + 1.0f;
        dc->FillRectangle (r, getBlueBrush());

        lastSampleIndex = sampleIndex;
        }

      r.left++;
      r.right++;
      }
    //}}}

    dc->DrawText (std::wstring (measureStr.begin(), measureStr.end()).data(), (UINT32)measureStr.size(),
                  getTextFormat(),
                  RectF(getSize().x/2.0f, 0.0f, getSize().x, getSize().y), getWhiteBrush());
    }
  //}}}

  enum eMode { ePreview, eFull, eBayer, eJpeg };
  //{{{
  void setMode (eMode mode) {

    switch (mode) {
      case ePreview:
        //{{{  preview
        if (mSensorId == 0x1519) {
          mMode = mode;
          cLog::log (LOGINFO, "setMode preview");

          mWidth = 800;
          mHeight = 600;

          writeReg (0xF0, 1);
          writeReg (0x09, 0x000A); // factory bypass 10 bit sensor
          writeReg (0xC6, 0xA120); writeReg (0xC8, 0x00); // Sequencer.params.mode - none
          writeReg (0xC6, 0xA103); writeReg (0xC8, 0x01); // Sequencer goto preview A
          }

        else {
          mMode = mode;
          cLog::log (LOGINFO, "setMode preview");

          mWidth = 640;
          mHeight = 480;

          writeReg (0x338C, 0xA120); writeReg (0x3390, 0x0000); // sequencer.params.mode - none
          writeReg (0x338C, 0xA103); writeReg (0x3390, 0x0001); // sequencer.cmd - goto preview mode A
          }

        break;
        //}}}
      case eFull:
        //{{{  capture
        if (mSensorId == 0x1519) {
          mMode = mode;
          cLog::log (LOGINFO, "setMode capture");

          mWidth = 1600;
          mHeight = 1200;

          writeReg (0xF0, 1);
          writeReg (0x09, 0x000A); // factory bypass 10 bit sensor
          writeReg (0xC6, 0xA120); writeReg (0xC8, 0x02); // Sequencer.params.mode - capture video
          writeReg (0xC6, 0xA103); writeReg (0xC8, 0x02); // Sequencer goto capture B
          }
        else {
          mMode = mode;
          cLog::log (LOGINFO, "setMode capture");

          mWidth = 1600;
          mHeight = 1200;

          writeReg (0x338C, 0xA120); writeReg (0x3390, 0x0002); // sequencer.params.mode - capture video
          writeReg (0x338C, 0xA103); writeReg (0x3390, 0x0002); // sequencer.cmd - goto capture mode B
          }

        break;
        //}}}
      case eBayer:
        //{{{  bayer
        if (mSensorId == 0x1519) {
          mMode = mode;
          cLog::log (LOGINFO, "setMode bayer");

          mWidth = 1608;
          mHeight = 1200;

          writeReg (0xF0, 1);
          writeReg (0x09, 0x0008); // factory bypass 10 bit sensor
          writeReg (0xC6, 0xA120); writeReg (0xC8, 0x02); // Sequencer.params.mode - capture video
          writeReg (0xC6, 0xA103); writeReg (0xC8, 0x02); // Sequencer goto capture B
          }

        break;
        //}}}
      case eJpeg:
        //{{{  jpeg
        if (mSensorId == 0x1519) {
          mMode = mode;
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

        break;
        //}}}
      }
    }
  //}}}
  //{{{
  void setPll (int m, int n, int p) {

    mPllm = m;
    mPlln = n;
    mPllp = p;

    if (mSensorId == 0x1519) {
      cLog::log (LOGINFO, "pll m:%d n:%d p:%d Fpfd(2-13):%d Fvco(110-240):%d Fout(6-80):%d\n",
                          mPllm, mPlln, mPllp,
                          24000000 / (mPlln+1),
                          (24000000 / (mPlln+1))*mPllm,
                          ((24000000 / (mPlln+1) * mPllm)/ (2*(mPllp+1))) );

      //  PLL (24mhz/(N+1))*M / 2*(P+1)
      writeReg (0x66, (mPllm << 8) | mPlln);  // PLL Control 1    M:15:8,N:7:0 - M=16, N=1 (24mhz/(n1+1))*m16 / 2*(p1+1) = 48mhz
      writeReg (0x67, 0x0500 | mPllp);  // PLL Control 2 0x05:15:8,P:7:0 - P=3
      writeReg (0x65, 0xA000);  // Clock CNTRL - PLL ON
      writeReg (0x65, 0x2000);  // Clock CNTRL - USE PLL
      }
    }
  //}}}

  //{{{
  bool getMask (int x, int y, uint32_t& mask) {

    float row = rowPixels;

    mask = 0x00000001;
    while ((mask != 0x10000) && (row < getSize().y)) {
     float  nextRow = row + ((mask & 0x80808080) ? groupRowPixels : rowPixels);
      if ((y > row) && (y < nextRow))
        return true;

      row = nextRow;
      mask <<= 1;
      }
    return false;
    }
  //}}}
  //{{{
  void measure (uint32_t mask, int sample) {

    if ((sample > 0) && (sample < mSamplesLoaded)) {
      int back = sample;
      while ((back > 0) && !((mSamples[back] ^ mSamples[back - 1]) & mask) && ((sample - back) < 100000000))
        back--;

      int forward = sample;
      while ((forward < (mSamplesLoaded - 1)) &&
             !((mSamples[forward] ^ mSamples[forward + 1]) & mask) && ((forward - sample) < 100000000))
        forward++;

      if (((sample - back) != 100000000) && ((forward - sample) != 100000000)) {
        double width = (forward - back) / samplesPerSecond;

        if (width > 1.0)
          measureStr = dec(width) + "s";
        else if (width > 0.001)
          measureStr = dec(width*1000.0) + "ms";
        else if (width > 0.000001)
          measureStr = dec(width*1000000.0)+"us";
        else
          measureStr = dec(width*1000000000.0)+"ns";
        changed();
        }
      }

    }
  //}}}
  //{{{
  void count (uint32_t mask, int firstSample, int lastSample) {

    int count = 0;
    int sample = firstSample;
    if ((sample > 0) && (sample < mSamplesLoaded)) {
      while ((sample < lastSample) && (sample < (mSamplesLoaded-2))) {
        if ((mSamples[sample] ^ mSamples[sample + 1]) & mask)
          count++;
        sample++;
        }

      if (count > 0) {
        measureStr = dec(count);
        changed();
        }
      }
    }
  //}}}

  //{{{
  void analysisThread() {

    for (int i = 0; i < 32; i++) {
      hiCount [i] = 0;
      loCount [i] = 0;
      transitionCount [i] = 0;
      }

    int curSamplesAnaled = 0;
    int lastmSamplesLoaded = 0;
    while (mSamplesLoaded < maxSamples-1) {
      curSamplesAnaled = mSamplesLoaded;
      if (curSamplesAnaled > lastmSamplesLoaded) {
        for (int index = lastmSamplesLoaded; index < curSamplesAnaled; index++)
          active |= mSamples[index] ^ mSamples[index+1];

        int dq = 0;
        uint32_t mask = 0x00000001;
        while (mask != 0x10000) {
          for (auto index = lastmSamplesLoaded; index < curSamplesAnaled; index++) {
            uint32_t transition = mSamples[index] ^ mSamples[index+1];
            if (transition & mask) {
              transitionCount[dq]++;
              }
            else if (mSamples[index] & mask) {
              hiCount[dq]++;
              }
            else {
              loCount[dq]++;
              }
            }

          dq++;
          mask <<= 1;
          }

        lastmSamplesLoaded = curSamplesAnaled;
        }
      Sleep (100);
      }
    }
  //}}}
  //{{{
  void loaderThread() {

    if (getBulkEndPoint() == NULL)
      return;
    int packetLen = getBulkEndPoint()->MaxPktSize;
    cLog::log (LOGINFO, "loaderThread bufferLen:%d numBuffers:%d total:%d",
                        packetLen, QUEUESIZE,  packetLen * QUEUESIZE);

    bool first = true;
    uint8_t* buffers[QUEUESIZE];
    uint8_t* contexts[QUEUESIZE];
    OVERLAPPED overLapped[QUEUESIZE];
    for (int i = 0; i < QUEUESIZE; i++)
      overLapped[i].hEvent = CreateEvent (NULL, false, false, NULL);

    while (true) {
      int done = 0;

      auto samplePtr = (uint8_t*)mSamples;
      auto maxSamplePtr = (uint8_t*)mSamples + maxSampleBytes;

      // Allocate buffers and queue them
      for (auto i = 0; i < QUEUESIZE; i++) {
        buffers[i] = samplePtr;
        samplePtr += packetLen;
        contexts[i] = getBulkEndPoint()->BeginDataXfer (buffers[i] , packetLen, &overLapped[i]);
        if (getBulkEndPoint()->NtStatus || getBulkEndPoint()->UsbdStatus) {
          cLog::log (LOGINFO, "BeginDataXfer init failed:%d", getBulkEndPoint()->NtStatus);
          return;
          }
        }

      if (first)
        startStreaming();
      first = false;

      int i = 0;
      int count = 0;
      while (done < QUEUESIZE) {
        if (!getBulkEndPoint()->WaitForXfer (&overLapped[i], timeout)) {
          cLog::log (LOGINFO, "timeOut buffer:%d error:%d", i, getBulkEndPoint()->LastError);
          getBulkEndPoint()->Abort();
          if (getBulkEndPoint()->LastError == ERROR_IO_PENDING)
            WaitForSingleObject (overLapped[i].hEvent, 2000);
          }
        long rxLen = packetLen;
        if (getBulkEndPoint()->FinishDataXfer (buffers[i], rxLen, &overLapped[i], contexts[i])) {
          if (rxLen != packetLen)
            cLog::log (LOGINFO, "FinishDataXfer rxLen %d %d", rxLen, packetLen);
          mSamplesLoaded += packetLen / sizeof(SAMPLE_TYPE);
          changed();
          }
        else {
          cLog::log (LOGINFO, "FinishDataXfer failed");
          return;
          }

        // Re-submit this queue element to keep the queue full
        if (!restart && ((samplePtr + packetLen) < maxSamplePtr)) {
          buffers[i] = samplePtr;
          samplePtr += packetLen;
          contexts[i] = getBulkEndPoint()->BeginDataXfer (buffers[i], packetLen, &overLapped[i]);
          if (getBulkEndPoint()->NtStatus || getBulkEndPoint()->UsbdStatus) {
            cLog::log (LOGINFO, "BeginDataXfer requeue failed:%d", getBulkEndPoint()->NtStatus);
            return;
            }
          }
        else
          done++;

        i = (i + 1) & (QUEUESIZE-1);
        }

      while (!restart)
        Sleep (100);
      restart = false;
      }

    for (int i = 0; i < QUEUESIZE; i++)
      CloseHandle (overLapped[i].hEvent);

    closeUSB();
    }
  //}}}

  //{{{  vars
  ID2D1SolidColorBrush* mBrush = nullptr;

  // display
  float xWindow = 1600.0f;
  float leftPixels = 50.0f;
  float rightPixels = 50.0f;
  float barPixels = 16.0f;
  float rowPixels = 22.0f;
  float groupRowPixels = 28.0f;
  float yWindow = 18.0f * rowPixels;

  std::string graticuleStr;
  std::string measureStr;

  uint32_t active = 0;
  int transitionCount [32];
  int hiCount [32];
  int loCount [32];

  // samples
  int timeout = 2000;
  bool restart = false;

  SAMPLE_TYPE* mSamples = NULL;
  int mSamplesLoaded = 0;

  #ifdef _WIN64
    size_t maxSamples = 0x40000000;
  #else
    size_t maxSamples = 0x10000000;
  #endif
  size_t maxSampleBytes= maxSamples * sizeof(SAMPLE_TYPE);

  double midSample = 0;
  double samplesPerSecond = 100000000;
  double samplesPerGraticule = samplesPerSecond;  // graticule every secoond
  double minSamplesPerPixel = 1.0/16.0;
  double maxSamplesPerPixel = (double)maxSamples / (xWindow - leftPixels - rightPixels);
  double samplesPerPixel = 0;

  // cam
  int mSensorId = 0;
  eMode mMode = ePreview;
  bool mModeChanged = false;

  int mWidth = 800;
  int mHeight = 600;

  // pll
  int mPllm = 16;
  int mPlln = 1;
  int mPllp = 1;
  //}}}
  };

//{{{
int WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

  cLog::init (LOGINFO, true);

  cAppWindow appWindow;
  appWindow.run ("logicWindow", 1600, (((sizeof(SAMPLE_TYPE)*8)+2)*22) + 256);
  }
//}}}
