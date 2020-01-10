// cCalendarBox.h
//{{{  includes
#pragma once
#include "../common/cD2dWindow.h"
//#include "../../date/tz.h"
//}}}

class cCalendarBox : public cD2dWindow::cBox {
public:
  //{{{
  cCalendarBox (cD2dWindow* window, float width, float height, std::chrono::system_clock::time_point& timePoint) :
      cBox("date", window, width, height), mTimePoint(timePoint) {}
  //}}}

  void onDraw (ID2D1DeviceContext* dc) {

    const float kRoundWidth = 5.f;
    auto datePoint = date::floor<date::days>(mTimePoint);
    dc->FillRoundedRectangle (D2D1::RoundedRect (mRect, kRoundWidth,kRoundWidth), mWindow->getBlackBrush());
    auto r = cRect (mRect.left+kRoundWidth, mRect.top+kRoundWidth,
                    mRect.right-2.f*kRoundWidth, mRect.bottom-2.f*kRoundWidth);

    const float kCalendarWidth = 26.f;
    auto yearMonthDay = date::year_month_day{datePoint};
    auto yearMonth = yearMonthDay.year() / date::month{yearMonthDay.month()};
    auto today = yearMonthDay.day();

    IDWriteTextLayout* textLayout;
    //{{{  print month year
    auto p = r.getTL();

    std::string str = format ("%B", yearMonth);
    mWindow->getDwriteFactory()->CreateTextLayout (std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
               mWindow->getTextFormat(), getWidth(), getHeight(), &textLayout);
    dc->DrawTextLayout (p, textLayout, mWindow->getWhiteBrush());
    textLayout->Release();

    // print year
    p.x = r.getTL().x + r.getWidth() - 45.f;

    str = format ("%Y", yearMonth);
    mWindow->getDwriteFactory()->CreateTextLayout (std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
               mWindow->getTextFormat(), getWidth(), getHeight(), &textLayout);
    dc->DrawTextLayout (p, textLayout, mWindow->getWhiteBrush());
    textLayout->Release();

    p.y += kLineHeight;
    //}}}
    //{{{  print daysOfWeek
    p.x = r.getTL().x;

    auto weekDayToday = date::weekday{yearMonth / today};

    auto titleWeekDay = date::sun;
    do {
      str = format ("%a", titleWeekDay);
      str.resize (2);

      mWindow->getDwriteFactory()->CreateTextLayout (std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
                 mWindow->getTextFormat(), getWidth(), getHeight(), &textLayout);
      dc->DrawTextLayout (p, textLayout,
        weekDayToday == titleWeekDay ?  mWindow->getWhiteBrush() : mWindow->getGreyBrush());
      textLayout->Release();

      p.x += kCalendarWidth;
      } while (++titleWeekDay != date::sun);

    p.y += kLineHeight;
    //}}}
    //{{{  print lines
    // skip leading space
    auto weekDay = date::weekday{ yearMonth / 1};
    p.x = r.getTL().x + (weekDay - date::sun).count()*kCalendarWidth;

    using date::operator""_d;
    auto curDay = 1_d;
    auto lastDayOfMonth = (yearMonth / date::last).day();

    int line = 1;
     while (curDay <= lastDayOfMonth) {
      // iterate days of week
      str = format ("%e", curDay);
      mWindow->getDwriteFactory()->CreateTextLayout (
        std::wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
        mWindow->getTextFormat(), getWidth(), getHeight(), &textLayout);
      dc->DrawTextLayout (p, textLayout, today == curDay ? mWindow->getWhiteBrush() : mWindow->getGreyBrush());
      textLayout->Release();

      if (++weekDay == date::sun) {
        // line 6 folds back to first
        line++;
        p.y += line <= 5 ? kLineHeight : - 4* kLineHeight;
        p.x = r.getTL().x;
        }
      else
        p.x += kCalendarWidth;

      ++curDay;
      };

    p.y += kLineHeight;
    //}}}
    }

private:
  std::chrono::system_clock::time_point& mTimePoint;
  };
