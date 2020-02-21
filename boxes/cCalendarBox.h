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
    auto datePoint = date::floor<date::days>(mWindow->getNowDayLight());
    dc->FillRoundedRectangle (D2D1::RoundedRect (mRect, kRoundWidth,kRoundWidth), mWindow->getBlackBrush());
    auto r = cRect (mRect.left+kRoundWidth, mRect.top+kRoundWidth,
                    mRect.right-2.f*kRoundWidth, mRect.bottom-2.f*kRoundWidth);

    const float kCalendarWidth = 26.f;
    auto yearMonthDay = date::year_month_day{datePoint};
    auto yearMonth = yearMonthDay.year() / date::month{yearMonthDay.month()};
    auto today = yearMonthDay.day();

    //{{{  print month year
    auto p = r.getTL();
    std::wstring str = format (L"%B", yearMonth);
    dc->DrawText (str.data(), (uint32_t)str.size(), mWindow->getTextFormat(),
                  { p.x, p.y, mRect.right, mRect.bottom }, mWindow->getWhiteBrush());

    // print year
    p.x = r.getTL().x + r.getWidth() - 45.f;
    str = format (L"%Y", yearMonth);
    dc->DrawText (str.data(), (uint32_t)str.size(), mWindow->getTextFormat(),
                  { p.x, p.y, mRect.right, mRect.bottom }, mWindow->getWhiteBrush());

    p.y += kLineHeight;
    //}}}
    //{{{  print daysOfWeek
    auto weekDayToday = date::weekday{yearMonth / today};
    auto titleWeekDay = date::sun;
    p.x = r.getTL().x;
    do {
      str = format (L"%a", titleWeekDay);
      str.resize (2);
      dc->DrawText (str.data(), (uint32_t)str.size(), mWindow->getTextFormat(),
                    { p.x, p.y, mRect.right, mRect.bottom },
                    weekDayToday == titleWeekDay ?  mWindow->getWhiteBrush() : mWindow->getGreyBrush());

      p.x += kCalendarWidth;
      } while (++titleWeekDay != date::sun);

    p.y += kLineHeight;
    //}}}
    //{{{  print lines
    // skip leading space
    auto weekDay = date::weekday{ yearMonth / 1};

    using date::operator""_d;
    auto curDay = 1_d;
    auto lastDayOfMonth = (yearMonth / date::last).day();

    int line = 1;
    p.x = r.getTL().x + (weekDay - date::sun).count()*kCalendarWidth;
    while (curDay <= lastDayOfMonth) {
      // iterate days of week
      str = format (L"%e", curDay);
      dc->DrawText (str.data(), (uint32_t)str.size(), mWindow->getTextFormat(),
                    { p.x, p.y, mRect.right, mRect.bottom },
                    today == curDay ? mWindow->getWhiteBrush() : mWindow->getGreyBrush());

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
  };
