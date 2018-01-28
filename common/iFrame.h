// iFrame.h
#pragma once

class iFrame {
public:
  virtual ~iFrame() {}

  bool isOk() { return mOk; }

  //{{{
  bool hasPts (int64_t pts) {
    return mOk && (pts >= mPts) && (pts < mPtsEnd);
    }
  //}}}
  int64_t getPts() { return mPts; }
  int64_t getPtsEnd() { return mPtsEnd; }

  //{{{
  bool before (int64_t pts) {
  // true if pts is bfore frame

    return mOk && (pts < mPts);
    }
  //}}}
  //{{{
  int64_t beforeDist (int64_t pts) {
    return mPts - pts;
    }
  //}}}

  //{{{
  bool after (int64_t pts) {
  // true if pts is after frame

    return mOk && (pts >= mPtsEnd);
    }
  //}}}
  //{{{

  int64_t afterDist (int64_t pts) {
    return pts - mPtsEnd;
    }
  //}}}

  virtual void invalidate() = 0;
  virtual void freeResources() = 0;

protected:
  // vars
  bool mOk = false;
  int64_t mPts = -1;
  int64_t mPtsEnd = -1;
  };
