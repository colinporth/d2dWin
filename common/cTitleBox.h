// cLogBox.h
#pragma once
#include "cD2dWindow.h"

class cTitleBox : public cD2dWindow::cBox {
public:
  //{{{
  cTitleBox (cD2dWindow* window, float width, float height, const std::string& title)
      : cBox("title", window, width, height), mTitle(title) {
    mPin = true;
    }
  //}}}
  virtual ~cTitleBox() {}

  //{{{
  void onDraw (ID2D1DeviceContext* dc) {
    IDWriteTextLayout* textLayout;
    mWindow->getDwriteFactory()->CreateTextLayout (
      wstring (mTitle.begin(), mTitle.end()).data(), (uint32_t)mTitle.size(), mWindow->getTextFormat(),
               mWindow->getSize().x, mWindow->getSize().y, &textLayout);
    dc->DrawTextLayout (getTL(2.f), textLayout, mWindow->getBlackBrush());
    dc->DrawTextLayout (getTL(), textLayout, mWindow->getWhiteBrush());
    textLayout->Release();
    }
  //}}}

private:
  string& mTitle;
  };
