// iFrame.h
#pragma once

class iFrame {
public:
  //{{{
  bool hasPts (int64_t pts) {
    return mLoaded && (pts >= mPts) && (pts < mPtsEnd);
    }
  //}}}
  //{{{
  bool before (int64_t pts) {
  // true if pts is bfore frame

    return mLoaded && (pts < mPts);
    }
  //}}}
  //{{{
  bool after (int64_t pts) {
  // true if pts is after frame

    return mLoaded && (pts > mPtsEnd);
    }
  //}}}

  virtual void invalidate() = 0;
  virtual void freeResources() = 0;

  // vars
  bool mLoaded = false;
  int64_t mPts = -1;
  int64_t mPtsEnd = -1;
  };
