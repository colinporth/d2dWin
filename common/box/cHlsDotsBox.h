// hlsDotsBox.h
//{{{  includes
#pragma once
#include "../cD2dWindow.h"
#include "../../../shared/hls/hls.h"
//}}}

class cHlsDotsBox : public cD2dWindow::cBox {
public:
  //{{{
  cHlsDotsBox (cD2dWindow* window, float width, float height, cHls* hls) :
      cBox("hlsDots", window, width, height), mHls(hls) {

    mPin = true;

    mWindow->getDwriteFactory()->CreateTextFormat (L"FreeSans", NULL,
      DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.f, L"en-us",
      &mTextFormat);
    mTextFormat->SetTextAlignment (DWRITE_TEXT_ALIGNMENT_CENTER);
    }
  //}}}
  //{{{
  virtual ~cHlsDotsBox() {
    mTextFormat->Release();
    }
  //}}}

  // overrides
  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    for (auto chunk = 0; chunk < 3; chunk++) {
      bool loaded;
      bool loading;
      int offset;
      mHls->getChunkLoad (chunk, loaded, loading, offset);
      auto brush = loading ? mWindow->getOrangeBrush() :
                     loaded ? mWindow->getGreenBrush() : mWindow->getRedBrush();

      auto centre = cPoint(getCentre().x, mRect.top + (chunk + 0.5f) * (getHeight() / 3.f));
      auto radius = getWidth() / 2.f;
      dc->FillEllipse (Ellipse (centre, radius, radius), brush);

      if (loaded || loading) {
        auto str = dec(offset);
        dc->DrawText (wstring (str.begin(), str.end()).data(), (uint32_t)str.size(), mTextFormat,
                      cRect (mRect.left, centre.y-8.f, mRect.right, centre.y +4.f),
                      mWindow->getLightGreyBrush());
        }
      }
    }
  //}}}

private:
  // vars
  cHls* mHls;
  IDWriteTextFormat* mTextFormat = nullptr;
  };
