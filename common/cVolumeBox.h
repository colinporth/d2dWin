// cVolumeBox.h
#pragma once
#include "cD2dWindow.h"
#include "../../shared/utils/iAudio.h"

class cVolumeBox : public cD2dWindow::cBox {
public:
  cVolumeBox (cD2dWindow* window, float width, float height)
    : cBox("volume", window, width, height) {}
  virtual ~cVolumeBox() {}

  bool onWheel (int delta, cPoint posy)  {
    auto audio = dynamic_cast<iAudio*>(mWindow);
    audio->setVolume (audio->getVolume() - delta/1200.f);
    return true;
    }

  bool onDown (bool right, cPoint pos)  { return setFromPos (pos); }
  bool onMove (bool right, cPoint pos, cPoint inc) { return setFromPos (pos); }

  void onDraw (ID2D1DeviceContext* dc) {
    auto audio = dynamic_cast<iAudio*>(mWindow);
    auto r = mRect;
    r.bottom = r.top + (getHeight() * audio->getVolume());
    dc->FillRectangle (r, mWindow->getYellowBrush());
    }

private:
  bool setFromPos (cPoint pos) {
    auto audio = dynamic_cast<iAudio*>(mWindow);
    audio->setVolume (pos.y / getHeight());
    return true;
    }
  };
