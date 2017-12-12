// iFrame.h
#pragma once

class iFrame {
public:
  virtual void invalidate() = 0;
  virtual void freeResources() = 0;

  // vars
  bool mLoaded = false;
  int64_t mPts = -1;
  int64_t mPtsEnd = -1;
  };
