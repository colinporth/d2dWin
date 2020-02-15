// cJpegImage.h - windows only turbo jpeg decoder
#pragma once
//{{{  includes
#include "iImage.h"

#include "../../shared/utils/utils.h"
#include "../../shared/utils/date.h"

#include "cView2d.h"

#include "../inc/jpeglib/jpeglib.h"
#pragma comment(lib,"turbojpeg-static.lib")
//}}}

class cJpegImage : public iImage {
public:
  cJpegImage() {}
  cJpegImage (uint8_t* buf, int bufLen) : mBuf(buf), mBufLen (bufLen) {}
  //{{{
  void setBuf (uint8_t* buf, int bufLen) {
    mBuf = buf;
    mBufLen = bufLen;
    }
  //}}}
  //{{{
  void setFile (const std::string& pathName, const std::string& fileName) {
    mPathName = pathName;
    mFileName = fileName;
    }
  //}}}
  virtual ~cJpegImage();

  //{{{  gets
  // iImage
  bool isOk() { return mBuf || !mFileName.empty(); }
  bool isLoaded() { return mBitmap != nullptr; }

  int getImageLen() { return mImageLen; }

  cPoint getSize() { return mSize; }
  int getWidth() { return mSize.width; }
  int getHeight() { return mSize.height; }
  int getNumComponents() { return 4; }
  int getScale() { return mLoadScale; }
  std::string getDebugString();

  cPoint getImageSize() { return mImageSize; }
  int getImageWidth() { return mImageSize.width; }
  int getImageHeight() { return mImageSize.height; }

  ID2D1Bitmap* getBitmap() { return mBitmap; }

  uint8_t* getBgraBuf() { return mBgraBuf; }
  uint8_t getRed (cPoint pos) { return getColour (pos, 2); }
  uint8_t getGreen (cPoint pos) { return getColour (pos, 1); }
  uint8_t getBlue (cPoint pos) { return getColour (pos, 0); }

  // fileName
  std::string getFileName() { return mFileName; }
  std::string getPathName() { return mPathName; }
  std::string getPathFileName() { return mPathName.empty() ? mFileName : (mPathName + "/" + mFileName); }

  // thumb
  bool isThumbLoading() { return mThumbLoading; }
  bool isThumbAvailable() { return isThumbLoaded() || mThumbLoading; }
  bool isThumbLoaded() { return mThumbBitmap != nullptr; }
  std::string getThumbString();
  ID2D1Bitmap* getThumbBitmap() { return mThumbBitmap; }

  // info
  std::string getMakeString() { return mMakeStr; }
  std::string getModelString() { return mModelStr; }

  std::string getExifTimeString();
  std::string getCreationTimeString();
  std::string getLastAccessTimeString();
  std::string getLastWriteTimeString();

  std::string getMarkerString() { return mMarkerStr; }
  std::string getGpsString() { return mGps ? mGps->getString() : ""; }

  int getOrientation() { return mOrientation; }
  float getAperture() { return mAperture; }
  float getFocalLength() { return mFocalLength; }
  float getExposure() { return mExposure; }

  std::chrono::time_point<std::chrono::system_clock> getExifTimePoint() { return mExifTimePoint; }
  std::chrono::time_point<std::chrono::system_clock> getCreationTimePoint() { return mCreationTimePoint; }
  std::chrono::time_point<std::chrono::system_clock> getLastAccessTimePoint()  { return mLastAccessTimePoint; }
  std::chrono::time_point<std::chrono::system_clock> getLastWriteTimePoint()  { return mLastWriteTimePoint; }
  //}}}

  // iImage
  void loadInfo();
  uint32_t loadImage (ID2D1DeviceContext* dc, int scale);
  void releaseImage();

  uint32_t loadThumb (ID2D1DeviceContext* dc, cPoint thumbSize);
  void releaseThumb();

  uint8_t* compressImage (int& bufLen, float quality);

private:
  //{{{
  class cGps {
  public:
    cGps() : mVersion(0),
             mLatitudeRef(0),  mLatitudeDeg(0), mLatitudeMin(0), mLatitudeSec(0),
             mLongitudeRef(0), mLongitudeDeg(0), mLongitudeMin(0), mLongitudeSec(0),
             mAltitudeRef(0),  mAltitude(0), mImageDirectionRef(0), mImageDirection(0),
             mHour(0), mMinute(0), mSecond(0) {}

    std::string getString();

    int mVersion;

    char mLatitudeRef;
    float mLatitudeDeg;
    float mLatitudeMin;
    float mLatitudeSec;

    char mLongitudeRef;
    float mLongitudeDeg;
    float mLongitudeMin;
    float mLongitudeSec;

    char mAltitudeRef;
    float mAltitude;

    char mImageDirectionRef;
    float mImageDirection;

    float mHour;
    float mMinute;
    float mSecond;

    std::string mDate;
    std::string mDatum;
    };
  //}}}

  uint8_t getColour (cPoint pos, int offset);
  std::chrono::system_clock::time_point getFileTimePoint (FILETIME fileTime);
  void getFileTimes (HANDLE fileHandle);

  // exif
  WORD getExifWord (uint8_t* ptr, bool intelEndian);
  DWORD getExifLong (uint8_t* ptr, bool intelEndian);
  float getExifSignedRational (uint8_t* ptr, bool intelEndian, DWORD& numerator, DWORD& denominator);
  std::string getExifString (uint8_t* ptr);
  void getExifTime (const std::string& str);
  cGps* getExifGps (uint8_t* ptr, uint8_t* offsetBasePtr, bool intelEndian);

  std::string getMarkerString (WORD marker, WORD markerLength);

  void parseExifDirectory (uint8_t* offsetBasePtr, uint8_t* ptr, bool intelEndian, int& thumbLen, int& thumbOffset);
  bool parseHeader (uint8_t* ptr, size_t bufLen, int& thumbLen, int& thumbOffset);

  //{{{  vars
  std::string mPathName;
  std::string mFileName;

  uint8_t* mBuf = nullptr;
  int mBufLen = 0;

  int mImageLen = 0;
  D2D1_SIZE_U mImageSize = {0,0};
  D2D1_SIZE_U mThumbSize = {0,0};
  D2D1_SIZE_U mSize = {0,0};
  int mLoadScale = 0;

  bool mThumbLoading = false;
  float mThumbLoadTime = 0;
  float mLoadTime = 0;
  ID2D1Bitmap* mThumbBitmap = nullptr;
  ID2D1Bitmap* mBitmap = nullptr;
  uint8_t* mBgraBuf = nullptr;

  DWORD mThumbLen = 0;

  // info
  std::string mMarkerStr;

  std::string mMakeStr;
  std::string mModelStr;

  std::string mExifTimeString;
  std::chrono::time_point<std::chrono::system_clock> mExifTimePoint;
  std::chrono::time_point<std::chrono::system_clock> mCreationTimePoint;
  std::chrono::time_point<std::chrono::system_clock> mLastAccessTimePoint;
  std::chrono::time_point<std::chrono::system_clock> mLastWriteTimePoint;

  cGps* mGps = nullptr;

  int mOrientation = 0;
  float mExposure = 0;
  float mFocalLength = 0;
  float mAperture = 0;
  //}}}
  };
