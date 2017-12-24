// cIntBox.h
//{{{  includes
#pragma once

#include "cD2dWindow.h"
#include "../../shared/utils/utils.h"
#include "../../shared/utils/cLog.h"
//}}}

class cIntBox : public cD2dWindow::cBox {
public:
  //{{{
  cIntBox (cD2dWindow* window, float width, float height, const string& title, int& value) :
      cBox("info", window, width, height), mTitle(title), mValue(value) {
    mPin = true;
    }
  //}}}
  virtual ~cIntBox() {}

  //{{{
  void onDraw (ID2D1DeviceContext* dc) {
    string str = mTitle + dec(mValue);

    IDWriteTextLayout* textLayout;
    mWindow->getDwriteFactory()->CreateTextLayout (
      wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
      mWindow->getTextFormat(), getWidth(), getHeight(), &textLayout);
    dc->DrawTextLayout (getTL(2.f), textLayout, mWindow->getBlackBrush());
    dc->DrawTextLayout (getTL(), textLayout, mWindow->getWhiteBrush());

    textLayout->Release();
    }
  //}}}

private:
  string mTitle;
  int& mValue;
  };

class cIntBgndBox : public cIntBox {
public:
  //{{{
  cIntBgndBox (cD2dWindow* window, float width, float height, string title, int& value) :
      cIntBox(window, width, height, title, value) {
    }
  //}}}
  virtual ~cIntBgndBox() {}

  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    dc->FillRectangle (mRect, mWindow->getGreyBrush());
    cIntBox::onDraw (dc);
    }
  //}}}
  };


class cUInt64Box : public cD2dWindow::cBox {
public:
  //{{{
  cUInt64Box (cD2dWindow* window, float width, float height, const string& title, uint64_t& value) :
      cBox("info", window, width, height), mTitle(title), mValue(value) {
    mPin = true;
    }
  //}}}
  virtual ~cUInt64Box() {}

  //{{{
  void onDraw (ID2D1DeviceContext* dc) {
    string str = mTitle + dec(mValue);

    IDWriteTextLayout* textLayout;
    mWindow->getDwriteFactory()->CreateTextLayout (
      wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
      mWindow->getTextFormat(), getWidth(), getHeight(), &textLayout);
    dc->DrawTextLayout (getTL(2.f), textLayout, mWindow->getBlackBrush());
    dc->DrawTextLayout (getTL(), textLayout, mWindow->getWhiteBrush());

    textLayout->Release();
    }
  //}}}

private:
  string mTitle;
  uint64_t& mValue;
  };

class cUInt64BgndBox : public cUInt64Box {
public:
  //{{{
  cUInt64BgndBox (cD2dWindow* window, float width, float height, string title, uint64_t& value) :
      cUInt64Box(window, width, height, title, value) {
    }
  //}}}
  virtual ~cUInt64BgndBox() {}

  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    dc->FillRectangle (mRect, mWindow->getGreyBrush());
    cUInt64Box::onDraw (dc);
    }
  //}}}
  };
