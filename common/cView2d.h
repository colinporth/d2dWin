// cView2d.h
#pragma once
#include "cPointRect.h"
#include <d2d1helper.h>

class cView2d {
public:
  cView2d() {}
  cView2d (D2D1::Matrix3x2F matrix) : mTransform (matrix) {}

  // gets
  cPoint getPos() { return cPoint (mView._31, mView._32); }
  float getScale() { return (mView._11 + mView._22) / 2.f; }

  cPoint getSrcPos() { return cPoint (mSrc._31, mSrc._32); }
  float getSrcScale() { return (mSrc._11 + mSrc._22) / 2.f; }

  //{{{
  cPoint getDstToSrc (cPoint dst) {
  // inv Transform

    return cPoint((dst.x - mTransform._31) / mTransform._11, (dst.y - mTransform._32) / mTransform._22);
    }
  //}}}
  //{{{
  cPoint getSrcToDst (cPoint src) {
  // Transform with differnt types
    return cPoint((src.x * mTransform._11) + mTransform._31,  (src.y * mTransform._22) + mTransform._32);
    }
  //}}}
  //{{{
  cRect getSrcToDst (cRect src) {
  // Transform with differnt types
    return cRect((src.left * mTransform._11) + mTransform._31, (src.top * mTransform._11) +  mTransform._32,
                 (src.right * mTransform._11) + mTransform._31,  (src.bottom * mTransform._22) + mTransform._32);
    }
  //}}}

  // sets
  //{{{
  bool setPos (cPoint pos) {

    bool changed = (pos.x != mView._31) || (pos.y != mView._32);
    mView._31 = pos.x;
    mView._32 = pos.y;
    mTransform = mSrc * mView;
    return changed;
    }
  //}}}
  //{{{
  bool setScale (float scale) {

    bool changed = (scale != mView._11) || (scale != mView._22);
    mView._11 = scale;
    mView._22 = scale;
    mTransform = mSrc * mView;
    return changed;
    }
  //}}}
  //{{{
  void setScale (cPoint srcSize, cPoint dstSize) {

    float xScale = dstSize.y / srcSize.y;
    float yScale = dstSize.x / srcSize.x;

    setScale ((xScale < yScale) ? xScale : yScale);
    }
  //}}}

  //{{{
  bool setSrcPos (cPoint pos) {

    bool changed = (pos.x != mSrc._31) || (pos.y != mSrc._32);
    mSrc._31 = pos.x;
    mSrc._32 = pos.y;
    mTransform = mSrc * mView;
    return changed;
    }
  //}}}
  //{{{
  bool setSrcScale (float scale) {

    bool changed = (scale != mSrc._11) || (scale != mSrc._22);
    mSrc._11 = scale;
    mSrc._22 = scale;
    mTransform = mSrc * mView;
    return changed;
    }
  //}}}

  //{{{
  void multiplyBy (D2D1::Matrix3x2F matrix) {

    mView = mView * matrix;
    mTransform = mSrc * mView;
    }
  //}}}

  D2D1::Matrix3x2F mSrc = D2D1::Matrix3x2F::Identity();
  D2D1::Matrix3x2F mView = D2D1::Matrix3x2F::Identity();
  D2D1::Matrix3x2F mTransform = D2D1::Matrix3x2F::Identity();
  };
