// iImage.h
#pragma once
#include <string>
#include "cPointRect.h"

class iImage {
public:
  virtual ~iImage() {}

  virtual bool isOk() = 0;
  virtual bool isLoaded() = 0;

  virtual int getImageLen() = 0;

  // return loaded image params, possibly scaled
  virtual cPoint getSize() = 0;
  virtual int getWidth() = 0;
  virtual int getHeight() = 0;
  virtual int getNumComponents() = 0;
  virtual int getScale() = 0;
  virtual std::string getDebugString() = 0;

  // return complete image params
  virtual cPoint getImageSize() = 0;
  virtual int getImageWidth() = 0;
  virtual int getImageHeight() = 0;

  virtual ID2D1Bitmap* getBitmap() = 0;

  virtual uint8_t* getBgraBuf() = 0;
  virtual uint8_t getRed (cPoint pos) = 0;
  virtual uint8_t getGreen (cPoint pos) = 0;
  virtual uint8_t getBlue (cPoint pos) = 0;

  virtual void loadInfo() = 0;
  virtual uint32_t loadImage (ID2D1DeviceContext* dc, int scale) = 0;
  virtual void releaseImage() = 0;
  };
