// cCalendarBox.h
#pragma once
#include "../common/cD2dWindow.h"
#include "../../shared/date/date.h"

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

    auto datePoint = date::floor<date::days>(mWindow->getNow());
    auto yearMonthDay = date::year_month_day { datePoint };
    auto yearMonth = yearMonthDay.year() / date::month { yearMonthDay.month() };
    auto today = yearMonthDay.day();

    // draw month
    rect = mRect;
    auto monthStr = format (L"%B", yearMonth);
    dc->DrawText (monthStr.data(), (uint32_t)monthStr.size(), mWindow->getTextFormat(), rect, mWindow->getWhiteBrush());

    // draw year
    rect.left = mRect.right - 45.f;
    auto yearStr = format (L"%Y", yearMonth);
    dc->DrawText (yearStr.data(), (uint32_t)yearStr.size(), mWindow->getTextFormat(), rect, mWindow->getWhiteBrush());
    rect.top += kLineHeight;

    // draw daysOfWeek labels
    auto weekDayToday = date::weekday { yearMonth / today };
    auto titleWeekDay = date::sun;
    rect.left = mRect.left;
    do {
      auto dayStr = format (L"%a", titleWeekDay);
      dayStr.resize (2);
      dc->DrawText (dayStr.data(), (uint32_t)dayStr.size(), mWindow->getTextFormat(), rect,
                    weekDayToday == titleWeekDay ?  mWindow->getWhiteBrush() : mWindow->getGrayBrush());
      rect.left += kCalendarWidth;
      } while (++titleWeekDay != date::sun);
    rect.top += kLineHeight;

    //{{{  draw lines of days
    // skip leading space
    auto weekDay = date::weekday{ yearMonth / 1};

    using date::operator""_d;
    auto curDay = 1_d;
    auto lastDayOfMonth = (yearMonth / date::last).day();

    int line = 1;
    rect.left = mRect.left + (weekDay - date::sun).count()*kCalendarWidth;
    while (curDay <= lastDayOfMonth) {
      // iterate days of week
      auto numStr = format (L"%e", curDay);
      dc->DrawText (numStr.data(), (uint32_t)numStr.size(), mWindow->getTextFormat(), rect,
                    today == curDay ? mWindow->getWhiteBrush() : mWindow->getGrayBrush());

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
