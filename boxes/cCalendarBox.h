// cCalendarBox.h
//{{{  includes
#pragma once
#include "../common/cD2dWindow.h"
//}}}

class cCalendarBox : public cD2dWindow::cBox {
public:
  //{{{
  cCalendarBox (cD2dWindow* window, float width, float height) :
      cBox("date", window, width, height) {}
  //}}}

  void onDraw (ID2D1DeviceContext* dc) {

    const float kRoundWidth = 5.f;
    const float kCalendarWidth = 26.f;

    dc->FillRoundedRectangle (D2D1::RoundedRect (mRect, kRoundWidth,kRoundWidth), mWindow->getBlackBrush());
    cRect rect = { mRect.left + kRoundWidth, mRect.top + kRoundWidth,
                   mRect.right - 2.f*kRoundWidth, mRect.bottom - 2.f*kRoundWidth };

    auto datePoint = date::floor<date::days>(mWindow->getNowDayLight());
    auto yearMonthDay = date::year_month_day { datePoint };
    auto yearMonth = yearMonthDay.year() / date::month { yearMonthDay.month() };
    auto today = yearMonthDay.day();

    //{{{  print month year
    rect = mRect;
    auto str = format (L"%B", yearMonth);
    dc->DrawText (str.data(), (uint32_t)str.size(), mWindow->getTextFormat(), rect, mWindow->getWhiteBrush());

    // print year
    rect.left = mRect.right - 45.f;
    str = format (L"%Y", yearMonth);
    dc->DrawText (str.data(), (uint32_t)str.size(), mWindow->getTextFormat(), rect, mWindow->getWhiteBrush());

    rect.top += kLineHeight;
    //}}}
    //{{{  print daysOfWeek
    auto weekDayToday = date::weekday{yearMonth / today};
    auto titleWeekDay = date::sun;

    rect.left = mRect.left;
    do {
      str = format (L"%a", titleWeekDay);
      str.resize (2);
      dc->DrawText (str.data(), (uint32_t)str.size(), mWindow->getTextFormat(), rect,
                    weekDayToday == titleWeekDay ?  mWindow->getWhiteBrush() : mWindow->getGreyBrush());

      rect.left += kCalendarWidth;
      } while (++titleWeekDay != date::sun);

    rect.top += kLineHeight;
    //}}}
    //{{{  print lines
    // skip leading space
    auto weekDay = date::weekday{ yearMonth / 1};

    using date::operator""_d;
    auto curDay = 1_d;
    auto lastDayOfMonth = (yearMonth / date::last).day();

    int line = 1;
    rect.left = mRect.left + (weekDay - date::sun).count()*kCalendarWidth;
    while (curDay <= lastDayOfMonth) {
      // iterate days of week
      str = format (L"%e", curDay);
      dc->DrawText (str.data(), (uint32_t)str.size(), mWindow->getTextFormat(), rect,
                    today == curDay ? mWindow->getWhiteBrush() : mWindow->getGreyBrush());

      if (++weekDay == date::sun) {
        // line 6 folds back to first
        line++;
        rect.top += line <= 5 ? kLineHeight : - 4* kLineHeight;
        rect.left = mRect.left;
        }
      else
        rect.left += kCalendarWidth;

      ++curDay;
      };
    //}}}
    }
  };
