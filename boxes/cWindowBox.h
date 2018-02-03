// cWindowBox.h
//{{{  includes
#pragma once
#include "../common/cD2dWindow.h"
//}}}

class cWindowBox : public cD2dWindow::cBox {
public:
  //{{{
  cWindowBox (cD2dWindow* window, float width, float height)
      : cBox("services", window, width, height) {
    mWindow->getDwriteFactory()->CreateTextFormat (L"Marlett", NULL,
      DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, height, L"en-us",
      &mTextFormat);
    mTextFormat->SetTextAlignment (DWRITE_TEXT_ALIGNMENT_TRAILING);
    }
  //}}}
  virtual ~cWindowBox() {}

  //{{{
  bool onDown (bool right, cPoint pos)  {
    if (pos.x < getWidth()/2)
       mWindow->toggleFullScreen();
    else
      mWindow->setExit();
    return true;
    }
  //}}}

  void onDraw (ID2D1DeviceContext* dc) {
    std::wstring wstr = mWindow->getFullScreen() ? L"\x32\x72" : L"\x31\x72";

    IDWriteTextLayout* textLayout;
    mWindow->getDwriteFactory()->CreateTextLayout (wstr.data(), (uint32_t)wstr.size(), mTextFormat,
                                                   getWidth(), getHeight(), &textLayout);

    dc->DrawTextLayout (getTL(2.f), textLayout, mWindow->getBlackBrush());
    dc->DrawTextLayout (getTL(), textLayout, mWindow->getWhiteBrush());
    textLayout->Release();
    }
  //{{{
  void onResize (ID2D1DeviceContext* dc) {
    layout();
    mEnable = mWindow->getFullScreen();
    }
  //}}}

private:
  IDWriteTextFormat* mTextFormat = nullptr;
  };
