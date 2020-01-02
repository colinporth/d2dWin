// cVolumeBox.h
//{{{  includes
#pragma once
#include "../common/cD2dWindow.h"
#include "../../shared/utils/iAudio.h"
//}}}

class cVolumeBox : public cD2dWindow::cBox {
public:
  cVolumeBox (cD2dWindow* window, float width, float height, iAudio* audio)
    : cBox("volume", window, width, height) { mAudio = audio; }
  virtual ~cVolumeBox() {}

  void setAudio (iAudio* audio) { mAudio = audio; }

  bool onWheel (int delta, cPoint posy)  {
    if (mAudio)
      mAudio->setVolume (mAudio->getVolume() - delta/1200.f);
    return true;
    }

  bool onDown (bool right, cPoint pos)  { return setFromPos (pos); }
  bool onMove (bool right, cPoint pos, cPoint inc) { return setFromPos (pos); }

  void onDraw (ID2D1DeviceContext* dc) {
    if (mAudio) {
      auto r = mRect;
      r.bottom = r.top + (getHeight() * mAudio->getVolume());
      dc->FillRectangle (r, mWindow->getYellowBrush());
      }
    }

private:
  bool setFromPos (cPoint pos) {
    if (mAudio)
      mAudio->setVolume (pos.y / getHeight());
    return true;
    }

  iAudio* mAudio = nullptr;
  };
