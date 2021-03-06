// cTtitleBox.h
//{{{  includes
#pragma once
#include "../common/cD2dWindow.h"
//}}}

class cTitleBox : public cD2dWindow::cBox {
public:
  cTitleBox (cD2dWindow* window, float width, float height, std::string& title,
             std::function<void (cTitleBox* box)> hitCallback = [](cTitleBox*) {})
    : cBox("title", window, width, height), mTitle(title), mHitCallback(hitCallback) {}
  virtual ~cTitleBox() {}

  bool onDown (bool right, cPoint pos)  {
    mHitCallback (this);
    return true;
    }

  void onDraw (ID2D1DeviceContext* dc) {
    IDWriteTextLayout* textLayout;
    mWindow->getDwriteFactory()->CreateTextLayout (
      std::wstring (mTitle.begin(), mTitle.end()).data(), (uint32_t)mTitle.size(), mWindow->getTextFormat(),
      mWindow->getSize().x, mWindow->getSize().y, &textLayout);
    if (textLayout) {
      dc->DrawTextLayout (getTL(2.f), textLayout, mWindow->getBlackBrush(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
      dc->DrawTextLayout (getTL(), textLayout, mWindow->getWhiteBrush(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
      textLayout->Release();
      }
    }

private:
  std::string& mTitle;
  std::function<void (cTitleBox* box)> mHitCallback;
  };
