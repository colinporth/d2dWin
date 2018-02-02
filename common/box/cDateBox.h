// cDateBox.h
#pragma once
#include "../cD2dWindow.h"
//#include "../date/tz.h"

class cDateBox : public cD2dWindow::cBox {
public:
  //{{{
  cDateBox (cD2dWindow* window, float width, float height, chrono::system_clock::time_point& timePoint) :
      cBox("date", window, width, height), mTimePoint(timePoint) {
    mPin = true;
    }
  //}}}

  void onDraw (ID2D1DeviceContext* dc) {

    string str = format (cout.getloc(), "%a %e %B %Y",
                         date::year_month_day {date::floor<date::days>(mTimePoint)});

    IDWriteTextLayout* textLayout;
    mWindow->getDwriteFactory()->CreateTextLayout (
      wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
               mWindow->getTextFormat(), getWidth(), getHeight(), &textLayout);
    dc->DrawTextLayout (getTL (2.f), textLayout, mWindow->getBlackBrush());
    dc->DrawTextLayout (getTL(), textLayout, mWindow->getWhiteBrush());
    textLayout->Release();
    }

private:
  chrono::system_clock::time_point& mTimePoint;
  };
