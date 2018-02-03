// cLogBox.h
//{{{  includes
#pragma once

#include "../common/cD2dWindow.h"
#include "../../shared/utils/cLog.h"
#include "../../shared/utils/date.h"
//}}}

class cLogBox : public cD2dWindow::cBox {
public:
  //{{{
  cLogBox(cD2dWindow* window, float width, float height, bool pin = false)
      : cBox("log", window, width, height) {

    mWindow->getDwriteFactory()->CreateTextFormat (L"Consolas", NULL,
      DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
      20.f, L"en-us",
      &mTextFormat);

    window->getDc()->CreateSolidColorBrush (ColorF (ColorF::CornflowerBlue), &mBrush);
    layout();
    }
  //}}}
  //{{{
  virtual ~cLogBox() {
    mTextFormat->Release();
    mBrush->Release();
    }
  //}}}

  //{{{
  void layout() {
    mRect = cRect (0, mLayoutHeight, mLayoutWidth, mWindow->getSize().y-12.f);
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

    auto logWidth = 0.f;
    auto logHeight = mWindow->getSize().y - 12.f;

    auto textHeight = kSizes[cLog::getLogLevel()];
    auto y = logHeight + (mLogScroll % int(textHeight));
    int logLineNum = int(mLogScroll / int(textHeight));

    chrono::system_clock::time_point lastTimePoint;
    cLog::cLine logLine;
    while ((y > 20.f) && cLog::getLine (logLine, logLineNum++)) {

      textHeight = kSizes[logLine.mLogLevel];
      mBrush->SetColor (kColours[logLine.mLogLevel]);

      //{{{  draw timeElapsed interLine bar
      auto timeDiff = chrono::duration_cast<chrono::milliseconds>(lastTimePoint - logLine.mTimePoint).count();
      if (timeDiff < 20)
        dc->FillRectangle (RectF(0, y+1.f, timeDiff*10.f, y+2.f), mBrush);
      else {
        timeDiff = min (timeDiff, 4000ll);
        dc->FillRectangle (RectF(0, y+3.f, timeDiff/10.f, y+4.f), mWindow->getWhiteBrush());
        y -= 1.f;
        }

      lastTimePoint = logLine.mTimePoint;
      //}}}

      auto datePoint = floor<date::days>(lastTimePoint);
      auto timeOfDay = date::make_time (chrono::duration_cast<chrono::microseconds>(lastTimePoint - datePoint));
      auto str = wdec(timeOfDay.hours().count()) +
                 L":" + wdec(timeOfDay.hours().count(),2,L'0') +
                 L":" + wdec(timeOfDay.seconds().count(),2,'0') +
                 L"." + wdec(timeOfDay.subseconds().count(),6,'0') +
                 L" " + cLog::getThreadNameWstring (logLine.mThreadId) +
                 L" " + strToWstr(logLine.mStr);

      // draw text with possible trasnparent background
      IDWriteTextLayout* textLayout;
      mWindow->getDwriteFactory()->CreateTextLayout (str.data(), (uint32_t)str.size(), mTextFormat,
        mWindow->getSize().x, textHeight, &textLayout);
      textLayout->SetFontSize (textHeight, {0, (uint32_t)str.size()});
      struct DWRITE_TEXT_METRICS textMetrics;
      textLayout->GetMetrics (&textMetrics);
      y -= textHeight;
      if (mPin)
        dc->FillRectangle (RectF(0, y+4.f, textMetrics.width+2.f, y+4.f + textHeight),
                           mWindow->getTransparentBgndBrush());
      dc->DrawTextLayout (Point2F(0,y), textLayout, mBrush);
      textLayout->Release();

      logWidth = max (logWidth, textMetrics.width);
      logHeight = min (logHeight, float(y));
      mLayoutWidth = logWidth;
      mLayoutHeight = logHeight;

      layout();
      }

    }
  //}}}

private:
  //{{{
  const float kSizes[LOGMAX] = {
    20.f, // LOGTITLE
    16.f, // LOGNOTICE
    16.f, // LOGERROR
    16.f, // LOGINFO
    15.f, // LOGINFO1
    14.f, // LOGINFO2
    13.f, // LOGINFO3
    };
  //}}}
  //{{{
  const ColorF kColours[LOGMAX] = {
    {1.f, 1.f, 0.f, 1.f }, // LOGTITLE
    {1.f, 1.f, 1.f, 1.f }, // LOGNOTICE
    {1.f, 0.5f, 0.5f, 1.f }, // LOGERROR
    {0.5f, 0.5f, 0.8f, 1.f }, // LOGINFO
    {0.5f, 0.8f, 0.5f, 1.f }, // LOGINFO1
    {1.f, 1.f, 0.2f, 1.f }, // LOGINFO2
    {0.8f, 0.1f, 0.8f, 1.f }, // LOGINFO3
    };
  //}}}

  int mLogScroll = 0;

  IDWriteTextFormat* mTextFormat = nullptr;
  ID2D1SolidColorBrush* mBrush = nullptr;
  };
