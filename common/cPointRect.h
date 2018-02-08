// cPointRectView2d.h
#pragma once
#include <d2d1.h>
#include <d2d1_1.h>

class cPoint : public D2D1_POINT_2F {
public:
  //{{{
  cPoint()  {
    x = 0;
    y = 0;
    }
  //}}}
  //{{{
  cPoint (const float value) {
    this->x = value;
    this->y = value;
    }
  //}}}
  //{{{
  cPoint (const double value) {
    this->x = (float)value;
    this->y = (float)value;
    }
  //}}}
  //{{{
  cPoint (const float x, const float y) {
    this->x = x;
    this->y = y;
    }
  //}}}
  //{{{
  cPoint (const double x, const double y) {
    this->x = (float)x;
    this->y = (float)y;
    }
  //}}}
  //{{{
  cPoint (const int x, const int y) {
    this->x = (float)x;
    this->y = (float)y;
    }
  //}}}
  //{{{
  cPoint (const D2D1_SIZE_U& size)  {
    x = (float)size.width;
    y = (float)size.height;
    }
  //}}}

  //{{{
  cPoint operator - (const cPoint& point) const {
    return cPoint (x - point.x, y - point.y);
    }
  //}}}
  //{{{
  cPoint operator + (const cPoint& point) const {
    return cPoint (x + point.x, y + point.y);
    }
  //}}}
  //{{{
  cPoint operator * (const float f) const {
    return cPoint (x * f, y * f);
    }
  //}}}
  //{{{
  cPoint operator * (const double f) const {
    return cPoint (float(x * f), float(y * f));
    }
  //}}}
  //{{{
  cPoint operator * (const cPoint& point) const {
    return cPoint (x * point.x, y * point.y);
    }
  //}}}
  //{{{
  cPoint operator / (const float f) const {
    return cPoint (x / f, y / f);
    }
  //}}}
  //{{{
  cPoint operator / (const double f) const {
    return cPoint (float(x / f), float(y / f));
    }
  //}}}

  //{{{
  const cPoint& operator += (const cPoint& point)  {
    x += point.x;
    y += point.y;
    return *this;
    }
  //}}}
  //{{{
  const cPoint& operator -= (const cPoint& point)  {
    x -= point.x;
    y -= point.y;
    return *this;
    }
  //}}}

  //{{{
  bool inside (const cPoint& pos) const {
  // return pos inside rect formed by us as size
    return pos.x >= 0 && pos.x < x && pos.y >= 0 && pos.y < y;
    }
  //}}}
  //{{{
  float magnitude() const {
  // return magnitude of point as vector
    return sqrt ((x*x) + (y*y));
    }
  //}}}
  };

class cRect : public D2D1_RECT_F {
public:
  //{{{
  cRect()  {
    left = 0;
    bottom = 0;
    right = 0;
    bottom = 0;
    }
  //}}}
  //{{{
  cRect (const float sizeX, const float sizeY)  {
    left = 0;
    top = 0;
    right = sizeX;
    bottom = sizeY;
    }
  //}}}
  //{{{
  cRect (const float l, const float t, const float r, const float b)  {
    left = l;
    top = t;
    right = r;
    bottom = b;
    }
  //}}}
  //{{{
  cRect (const D2D1_SIZE_F& size)  {
    left = 0;
    top = 0;
    right = size.width;
    bottom = size.height;
    }
  //}}}
  //{{{
  cRect (const cPoint& size)  {
    left = 0;
    top = 0;
    right = size.x;
    bottom = size.y;
    }
  //}}}
  //{{{
  cRect (const cPoint& topLeft, const D2D1_SIZE_F& size)  {
    left = topLeft.x;
    top = topLeft.y;
    right = left + size.width;
    bottom = top + size.height;
    }
  //}}}
  //{{{
  cRect (const cPoint& topLeft, const cPoint& bottomRight)  {
    left = topLeft.x;
    top = topLeft.y;
    right = bottomRight.x;
    bottom = bottomRight.y;
    }
  //}}}

  //{{{
  cRect operator + (const cPoint& point) const {
    return cRect (left + point.x, top + point.y, right + point.x, bottom + point.y);
    }
  //}}}

  float getWidth() const { return right - left; }
  float getHeight() const { return bottom - top; }
  int getWidthInt() const { return int(right - left); }
  int getHeightInt() const { return int(bottom - top); }

  cPoint getTL() const { return cPoint(left, top); }
  cPoint getTL (float offset) const { return cPoint(left+offset, top+offset); }
  cPoint getTR() const { return cPoint(right, top); }
  cPoint getBL() const { return cPoint(left, bottom); }
  cPoint getBR() const { return cPoint(right, bottom); }

  cPoint getSize() const { return cPoint(right-left, bottom-top); }
  cPoint getCentre() const { return cPoint(getCentreX(), getCentreY()); }
  float getCentreX() const { return (left + right)/2.f; }
  float getCentreY() const { return (top + bottom)/2.f; }
  //{{{
  bool inside (const cPoint& pos) const {
  // return pos inside rect
    return (pos.x >= left) && (pos.x < right) && (pos.y >= top) && (pos.y < bottom);
    }
  //}}}
  };
