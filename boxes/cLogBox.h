// cLogBox.h
#pragma once
#include "../common/cD2dWindow.h"
#include "../../shared/utils/date.h"
#include "../../shared/utils/cLog.h"

class cLogBox : public cD2dWindow::cBox {
public:
  //{{{
  cLogBox(cD2dWindow* window, float width)
      : cBox("log", window, width, 0.f) {

    setPin (false);
    window->getDc()->CreateSolidColorBrush (D2D1::ColorF (D2D1::ColorF::CornflowerBlue), &mBrush);
    }
  //}}}
  //{{{
  virtual ~cLogBox() {
    mBrush->Release();
    }
  //}}}

  //{{{
  bool onWheel (int delta, cPoint pos)  {

    if (getShow()) {
      cLog::setLogLevel (eLogLevel(cLog::getLogLevel() + (delta/120)));
      return true;
      }

    return false;
    }
  //}}}
  //{{{
  bool onMove (bool right, cPoint pos, cPoint inc) {

    mLogScroll += (int)inc.y * (mWindow->getControl() ? 100 : 1);
    if (mLogScroll < 0)
      mLogScroll = 0;
    return true;
    }
  //}}}
  //{{{
  bool onUp (bool right, bool mouseMoved, cPoint pos) {
    if (!mouseMoved)
      togglePin();
    return true;
    }
  //}}}

  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    if (mPin)
      dc->FillRectangle ({ 0.f,0.f, mWindow->getWidth(),mWindow->getHeight() }, mWindow->getDimBgndBrush());
    else
      dc->FillRectangle ({ 0.f,0.f, mWindow->getWidth(),mWindow->getHeight() }, mWindow->getTransparentBgndBrush());

    cLog::cLine logLine;
    unsigned lastLineIndex = 0;
    int logLineNum = int(mLogScroll / int(kConsoleHeight));
    std::chrono::system_clock::time_point lastTimePoint;

    auto y = mWindow->getHeight() + (mLogScroll % int(kConsoleHeight)) - 2.f;
    while ((y > 20.f) && cLog::getLine (logLine, logLineNum++, lastLineIndex)) {
      mBrush->SetColor (kColours[logLine.mLogLevel]);

      // draw timeElapsed bar beneath log text
      auto timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(lastTimePoint - logLine.mTimePoint).count();
      if (timeDiff < 20) {
        dc->FillRectangle ({ 0.f, y-1.f, timeDiff*10.f, y+1.f }, mBrush);
        y -= kConsoleHeight + 2.f;
        }
      else {
        timeDiff = std::min (timeDiff, 4000ll);
        dc->FillRectangle ( { 0.f, y-2.f, timeDiff/10.f, y+1.f }, mWindow->getWhiteBrush());
        y -= kConsoleHeight + 3.f;
        }

      auto datePoint = floor<date::days>(lastTimePoint);
      auto timeOfDay = date::make_time (std::chrono::duration_cast<std::chrono::microseconds>(lastTimePoint - datePoint));
      auto str = wdec (timeOfDay.hours().count()) +
                 L":" + wdec (timeOfDay.hours().count(), 2, L'0') +
                 L":" + wdec (timeOfDay.seconds().count(), 2 ,L'0') +
                 L"." + wdec (timeOfDay.subseconds().count(), 6 ,L'0') +
                 L" " + cLog::getThreadNameWstring (logLine.mThreadId) +
                 L" " + strToWstr (logLine.mStr);
      dc->DrawText (str.data(), (uint32_t)str.size(), mWindow->getConsoleTextFormat(),
                   { 0.f, y, mWindow->getWidth(), y + kConsoleHeight },
                   mBrush, D2D1_DRAW_TEXT_OPTIONS_CLIP);

      lastTimePoint = logLine.mTimePoint;
      }
    }
  //}}}

private:
  //{{{
  const D2D1::ColorF kColours[LOGMAX] = {
    {  1.f,  1.f,  0.f, 1.f }, // LOGTITLE
    {  1.f,  1.f,  1.f, 1.f }, // LOGNOTICE
    {  1.f, 0.5f, 0.5f, 1.f }, // LOGERROR
    { 0.5f, 0.5f, 0.8f, 1.f }, // LOGINFO
    { 0.5f, 0.8f, 0.5f, 1.f }, // LOGINFO1
    {  1.f,  1.f, 0.2f, 1.f }, // LOGINFO2
    { 0.8f, 0.1f, 0.8f, 1.f }, // LOGINFO3
    };
  //}}}

  int mLogScroll = 0;

  ID2D1SolidColorBrush* mBrush = nullptr;
  };
