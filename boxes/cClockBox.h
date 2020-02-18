// cIndexBox.h
//{{{  includes
#pragma once
#include "../common/cD2dWindow.h"
#include "../../shared/utils/date.h"
//#include "../../date/tz.h"
//}}}

class cClockBox : public cD2dWindow::cBox {
public:
  //{{{
  cClockBox (cD2dWindow* window, float radius, bool showSubSec = false)
      : cBox("clock", window, radius*2, radius*2), mShowSubSec(showSubSec) {}
  //}}}

  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    float radius = getWidth() / 2.f;

    dc->DrawEllipse (D2D1::Ellipse (getCentre(), radius,radius), mWindow->getWhiteBrush(), 2.f);

    //auto timePointTz = date::make_zoned (date::current_zone(), mTimePoint);
    auto timePoint = mWindow->getNowDayLight();
    auto datePoint = floor<date::days>(timePoint);
    auto timeOfDay = date::make_time (std::chrono::duration_cast<std::chrono::milliseconds>(timePoint - datePoint));

    auto hourRadius = radius * 0.6f;
    auto h = timeOfDay.hours().count() + (timeOfDay.minutes().count() / 60.f);
    auto hourAngle = (1.f - (h / 6.f)) * kPi;
    dc->DrawLine (getCentre(),
                  getCentre() + cPoint(hourRadius * sin (hourAngle), hourRadius * cos (hourAngle)),
                  mWindow->getWhiteBrush(), 2.f);

    auto minRadius = radius * 0.75f;
    auto m = timeOfDay.minutes().count() + (timeOfDay.seconds().count() / 60.f);
    auto minAngle = (1.f - (m/30.f)) * kPi;
    dc->DrawLine (getCentre(),
                  getCentre() + cPoint (minRadius * sin (minAngle), minRadius * cos (minAngle)),
                  mWindow->getWhiteBrush(), 2.f);

    auto secRadius = radius * 0.85f;
    float s = (float)timeOfDay.seconds().count();
    if (mShowSubSec)
      s += (timeOfDay.subseconds().count() / 1000.f);
    auto secAngle = (1.f - (s /30.f)) * kPi;
    dc->DrawLine (getCentre(),
                  getCentre() + cPoint (secRadius * sin (secAngle), secRadius * cos (secAngle)),
                  mWindow->getRedBrush(), 2.f);

    if (mShowSubSec) {
      auto subSecRadius = radius * 0.8f;
      auto subSec = timeOfDay.subseconds().count();
      auto subSecAngle = (1.f - (subSec / 500.f)) * kPi;
      dc->DrawLine (getCentre(),
                    getCentre() + cPoint (subSecRadius * sin (subSecAngle), secRadius * cos (subSecAngle)),
                    mWindow->getBlueBrush(), 2.f);
      }

    }
  //}}}

private:
  const float kPi = 3.14159265358979323846f;

  bool mShowSubSec;
  };
