// cDateBox.h
//{{{  includes
#pragma once
#include "../common/cD2dWindow.h"
//#include "../date/tz.h"
//}}}

class cDateBox : public cD2dWindow::cBox {
public:
  //{{{
  cDateBox (cD2dWindow* window, float width, float height, std::chrono::system_clock::time_point& timePoint) :
      cBox("date", window, width, height), mTimePoint(timePoint) {}
  //}}}

  void onDraw (ID2D1DeviceContext* dc) {

    std::string str = format (std::cout.getloc(), "%a %e %B %Y",
                         date::year_month_day {date::floor<date::days>(mTimePoint)});

    IDWriteTextLayout* textLayout;
    mWindow->getDwriteFactory()->CreateTextLayout (
      std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
               mWindow->getTextFormat(), getWidth(), getHeight(), &textLayout);
    dc->DrawTextLayout (getTL (2.f), textLayout, mWindow->getBlackBrush());
    dc->DrawTextLayout (getTL(), textLayout, mWindow->getWhiteBrush());
    textLayout->Release();
    }

private:
  std::chrono::system_clock::time_point& mTimePoint;
  };
