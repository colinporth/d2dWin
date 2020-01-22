// cJpegImage.cpp - windows only jpeg file decoder
//{{{  includes
#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS 

#include <vector>

#include "cJpegImage.h"

#include "../../shared/utils/cLog.h"

#include "../inc/jpeglib/jpeglib.h"
#pragma comment(lib,"turbojpeg-static.lib")

using namespace std;
using namespace chrono;
//}}}
//{{{  defines
//{{{  tags x
// tags
#define TAG_INTEROP_INDEX          0x0001
#define TAG_INTEROP_VERSION        0x0002
//}}}
//{{{  tags 1xx
#define TAG_IMAGE_WIDTH            0x0100
#define TAG_IMAGE_LENGTH           0x0101
#define TAG_BITS_PER_SAMPLE        0x0102
#define TAG_COMPRESSION            0x0103
#define TAG_PHOTOMETRIC_INTERP     0x0106
#define TAG_FILL_ORDER             0x010A
#define TAG_DOCUMENT_NAME          0x010D
#define TAG_IMAGE_DESCRIPTION      0x010E
#define TAG_MAKE                   0x010F
#define TAG_MODEL                  0x0110
#define TAG_SRIP_OFFSET            0x0111
#define TAG_ORIENTATION            0x0112
#define TAG_SAMPLES_PER_PIXEL      0x0115
#define TAG_ROWS_PER_STRIP         0x0116
#define TAG_STRIP_BYTE_COUNTS      0x0117
#define TAG_X_RESOLUTION           0x011A
#define TAG_Y_RESOLUTION           0x011B
#define TAG_PLANAR_CONFIGURATION   0x011C
#define TAG_RESOLUTION_UNIT        0x0128
#define TAG_TRANSFER_FUNCTION      0x012D
#define TAG_SOFTWARE               0x0131
#define TAG_DATETIME               0x0132
#define TAG_ARTIST                 0x013B
#define TAG_WHITE_POINT            0x013E
#define TAG_PRIMARY_CHROMATICITIES 0x013F
#define TAG_TRANSFER_RANGE         0x0156
//}}}
//{{{  tags 2xx
#define TAG_JPEG_PROC              0x0200
#define TAG_THUMBNAIL_OFFSET       0x0201
#define TAG_THUMBNAIL_LENGTH       0x0202
#define TAG_Y_CB_CR_COEFFICIENTS   0x0211
#define TAG_Y_CB_CR_SUB_SAMPLING   0x0212
#define TAG_Y_CB_CR_POSITIONING    0x0213
#define TAG_REFERENCE_BLACK_WHITE  0x0214
//}}}
//{{{  tags 1xxx
#define TAG_RELATED_IMAGE_WIDTH    0x1001
#define TAG_RELATED_IMAGE_LENGTH   0x1002
//}}}
//{{{  tags 8xxx
#define TAG_CFA_REPEAT_PATTERN_DIM 0x828D
#define TAG_CFA_PATTERN1           0x828E
#define TAG_BATTERY_LEVEL          0x828F
#define TAG_COPYRIGHT              0x8298
#define TAG_EXPOSURETIME           0x829A
#define TAG_FNUMBER                0x829D
#define TAG_IPTC_NAA               0x83BB
#define TAG_EXIF_OFFSET            0x8769
#define TAG_INTER_COLOR_PROFILE    0x8773
#define TAG_EXPOSURE_PROGRAM       0x8822
#define TAG_SPECTRAL_SENSITIVITY   0x8824
#define TAG_GPSINFO                0x8825
#define TAG_ISO_EQUIVALENT         0x8827
#define TAG_OECF                   0x8828
//}}}
//{{{  tags 9xxx
#define TAG_EXIF_VERSION           0x9000
#define TAG_DATETIME_ORIGINAL      0x9003
#define TAG_DATETIME_DIGITIZED     0x9004
#define TAG_COMPONENTS_CONFIG      0x9101
#define TAG_CPRS_BITS_PER_PIXEL    0x9102
#define TAG_SHUTTERSPEED           0x9201
#define TAG_APERTURE               0x9202
#define TAG_BRIGHTNESS_VALUE       0x9203
#define TAG_EXPOSURE_BIAS          0x9204
#define TAG_MAXAPERTURE            0x9205
#define TAG_SUBJECT_DISTANCE       0x9206
#define TAG_METERING_MODE          0x9207
#define TAG_LIGHT_SOURCE           0x9208
#define TAG_FLASH                  0x9209
#define TAG_FOCALLENGTH            0x920A
#define TAG_SUBJECTAREA            0x9214
#define TAG_MAKER_NOTE             0x927C
#define TAG_USERCOMMENT            0x9286
#define TAG_SUBSEC_TIME            0x9290
#define TAG_SUBSEC_TIME_ORIG       0x9291
#define TAG_SUBSEC_TIME_DIG        0x9292

#define TAG_WINXP_TITLE            0x9c9b // Windows XP - not part of exif standard.
#define TAG_WINXP_COMMENT          0x9c9c // Windows XP - not part of exif standard.
#define TAG_WINXP_AUTHOR           0x9c9d // Windows XP - not part of exif standard.
#define TAG_WINXP_KEYWORDS         0x9c9e // Windows XP - not part of exif standard.
#define TAG_WINXP_SUBJECT          0x9c9f // Windows XP - not part of exif standard.
//}}}
//{{{  tags Axxx
#define TAG_FLASH_PIX_VERSION      0xA000
#define TAG_COLOR_SPACE            0xA001
#define TAG_PIXEL_X_DIMENSION      0xA002
#define TAG_PIXEL_Y_DIMENSION      0xA003
#define TAG_RELATED_AUDIO_FILE     0xA004
#define TAG_INTEROP_OFFSET         0xA005
#define TAG_FLASH_ENERGY           0xA20B
#define TAG_SPATIAL_FREQ_RESP      0xA20C
#define TAG_FOCAL_PLANE_XRES       0xA20E
#define TAG_FOCAL_PLANE_YRES       0xA20F
#define TAG_FOCAL_PLANE_UNITS      0xA210
#define TAG_SUBJECT_LOCATION       0xA214
#define TAG_EXPOSURE_INDEX         0xA215
#define TAG_SENSING_METHOD         0xA217
#define TAG_FILE_SOURCE            0xA300
#define TAG_SCENE_TYPE             0xA301
#define TAG_CFA_PATTERN            0xA302
#define TAG_CUSTOM_RENDERED        0xA401
#define TAG_EXPOSURE_MODE          0xA402
#define TAG_WHITEBALANCE           0xA403
#define TAG_DIGITALZOOMRATIO       0xA404
#define TAG_FOCALLENGTH_35MM       0xA405
#define TAG_SCENE_CAPTURE_TYPE     0xA406
#define TAG_GAIN_CONTROL           0xA407
#define TAG_CONTRAST               0xA408
#define TAG_SATURATION             0xA409
#define TAG_SHARPNESS              0xA40A
#define TAG_DISTANCE_RANGE         0xA40C
#define TAG_IMAGE_UNIQUE_ID        0xA420
//}}}

#define FMT_BYTE       1
#define FMT_STRING     2
#define FMT_USHORT     3
#define FMT_ULONG      4
#define FMT_URATIONAL  5
#define FMT_SBYTE      6
#define FMT_UNDEFINED  7
#define FMT_SSHORT     8
#define FMT_SLONG      9
#define FMT_SRATIONAL 10
#define FMT_SINGLE    11
#define FMT_DOUBLE    12
//}}}
//{{{  const
const int kBytesPerFormat[13] = { 0, 1, 1, 2, 4, 8, 1, 1, 2, 4, 8, 4, 8 };
const string kTimeFormatStr = "%D %T";

//}}}

// public
//{{{
cJpegImage::~cJpegImage() {
  releaseThumb();
  releaseImage();
  }
//}}}

//{{{
string cJpegImage::getThumbString() {

  string str;
  if (mThumbLen)
    str += dec(mThumbLen);
  if (mThumbSize.width)
    str += " " + dec(mThumbSize.width) + "x" + dec(mThumbSize.height);
  if (mThumbLoadTime)
    str += " " + dec(mThumbLoadTime) + "ms";
  return str;
  }
//}}}
//{{{
string cJpegImage::getDebugString() {
  return  dec(mImageLen) + " " + dec(mLoadTime) + "ms";
  }
//}}}

//{{{
string cJpegImage::getExifTimeString() {

  if (mExifTimePoint.time_since_epoch() == seconds::zero())
    return "";
  else
    return date::format (kTimeFormatStr, floor<seconds>(mExifTimePoint));
  }
//}}}
//{{{
string cJpegImage::getCreationTimeString() {

  if (mCreationTimePoint.time_since_epoch() == seconds::zero())
    return "";
  else
    return date::format (kTimeFormatStr, floor<seconds>(mCreationTimePoint));
  }
//}}}
//{{{
string cJpegImage::getLastAccessTimeString() {

  if (mLastAccessTimePoint.time_since_epoch() == seconds::zero())
    return "";
  else
    return date::format (kTimeFormatStr, floor<seconds>(mLastAccessTimePoint));
  }
//}}}
//{{{
string cJpegImage::getLastWriteTimeString() {

  if (mLastWriteTimePoint.time_since_epoch() == seconds::zero())
    return "";
  else
    return date::format (kTimeFormatStr, floor<seconds>(mLastWriteTimePoint));
  }
//}}}

//{{{
void cJpegImage::loadInfo() {

  if (!isThumbAvailable()) {
    auto time = system_clock::now();

    if (mBuf) {
      int thumbLen = 0;
      int thumbOffset = 0;
      parseHeader (mBuf, mBufLen, thumbLen, thumbOffset);
      mThumbLen = thumbLen;
      mImageLen = mBufLen;
      }

    else {
      auto fileHandle = CreateFile (getPathFileName().c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
      auto mapHandle = CreateFileMapping (fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
      auto fileBuf = (uint8_t*)MapViewOfFile (mapHandle, FILE_MAP_READ, 0, 0, 0);
      auto fileBufLen = GetFileSize (fileHandle, NULL);
      getFileTimes (fileHandle);

      int thumbLen = 0;
      int thumbOffset = 0;
      parseHeader (fileBuf, fileBufLen, thumbLen, thumbOffset);
      mThumbLen = thumbLen;
      mImageLen = fileBufLen;

      // close the file mapping object
      UnmapViewOfFile (fileBuf);
      CloseHandle (mapHandle);
      CloseHandle (fileHandle);
      }

    auto took = (float)duration_cast<milliseconds>(system_clock::now() - time).count();
    cLog::log (LOGINFO3, "cJpegImage::loadInfo " + dec (took) + " " + getFileName());
    }
  }
//}}}

//{{{
uint32_t cJpegImage::loadThumb (ID2D1DeviceContext* dc, cPoint thumbSize) {

  int alloc = 0;
  if (!isThumbAvailable()) {
    mThumbLoading = true;
    auto time = system_clock::now();

    //{{{  set buf, buflen
    uint8_t* buf = nullptr;
    int bufLen = 0;

    HANDLE fileHandle = 0;
    HANDLE mapHandle = 0;

    if (mBuf) {
      buf = mBuf;
      bufLen = mBufLen;
      }
    else {
      auto fileHandle = CreateFile (getPathFileName().c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
      auto mapHandle = CreateFileMapping (fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
      buf = (uint8_t*)MapViewOfFile (mapHandle, FILE_MAP_READ, 0, 0, 0);
      bufLen = GetFileSize (fileHandle, NULL);
      getFileTimes (fileHandle);
      }
    //}}}
    int thumbLen = 0;
    int thumbOffset = 0;
    auto hasThumb = parseHeader (buf, bufLen, thumbLen, thumbOffset);
    mThumbLen = thumbLen;

    auto thumbBuf = hasThumb ? buf + thumbOffset : buf;
    thumbLen = hasThumb ? thumbLen : bufLen;
    if ((thumbBuf[0] != 0xFF) || (thumbBuf[1] != 0xD8)) {
      //{{{  no SOI marker, return
      cLog::log (LOGERROR, "loadThumb - no SOI marker " + getPathFileName());

      UnmapViewOfFile (buf);
      CloseHandle (mapHandle);
      CloseHandle (fileHandle);
      return 0;
      }
      //}}}

    struct jpeg_error_mgr jerr;
    struct jpeg_decompress_struct cinfo;
    cinfo.err = jpeg_std_error (&jerr);
    jpeg_create_decompress (&cinfo);

    jpeg_mem_src (&cinfo, thumbBuf, (unsigned long)thumbLen);
    jpeg_read_header (&cinfo, true);

    // adjust scale to match thumbSize
    while ((cinfo.scale_denom < 8) &&
           ((cinfo.image_width/cinfo.scale_denom > ((unsigned int)thumbSize.x*3/2)) ||
             cinfo.image_height/cinfo.scale_denom > ((unsigned int)thumbSize.y*3/2)))
      cinfo.scale_denom *= 2;

    cinfo.out_color_space = JCS_EXT_BGRA;
    jpeg_start_decompress (&cinfo);

    auto pitch = cinfo.output_components * cinfo.output_width;
    auto brgaBuf  = (uint8_t*)malloc (pitch * cinfo.output_height);
    while (cinfo.output_scanline < cinfo.output_height) {
      uint8_t* lineArray[1];
      lineArray[0] = brgaBuf + (cinfo.output_scanline * pitch);
      jpeg_read_scanlines (&cinfo, lineArray, 1);
      }

    alloc = cinfo.output_width * cinfo.output_height * 4;
    dc->CreateBitmap (D2D1::SizeU (cinfo.output_width, cinfo.output_height), brgaBuf, pitch,
                      { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE, 0,0 }, &mThumbBitmap);
    free (brgaBuf);

    mThumbSize.width = cinfo.output_width;
    mThumbSize.height = cinfo.output_height;

    jpeg_finish_decompress (&cinfo);
    jpeg_destroy_decompress (&cinfo);

    if (fileHandle) {
      //{{{  close the file mapping object
      UnmapViewOfFile (buf);
      CloseHandle (mapHandle);
      CloseHandle (fileHandle);
      }
      //}}}

    mThumbLoadTime = (float)duration_cast<milliseconds>(system_clock::now() - time).count();
    mThumbLoading = false;
    cLog::log (LOGINFO3, "cJpegImage::loadThumb - took " + dec (mThumbLoadTime) + "ms " + getFileName());
    }

  return alloc;
  }
//}}}
//{{{
void cJpegImage::releaseThumb() {

  if (mThumbBitmap) {
    mThumbBitmap->Release();
    mThumbBitmap = nullptr;
    }
  }
//}}}

//{{{
uint32_t cJpegImage::loadImage (ID2D1DeviceContext* dc, int scale) {

  auto time = system_clock::now();

  int allocSize = 0;
  mLoadScale = scale;

  //{{{  set buf, buflen
  uint8_t* buf;
  int bufLen;
  HANDLE fileHandle = 0;
  HANDLE mapHandle = 0;
  if (mBuf) {
    buf = mBuf;
    bufLen = mBufLen;
    }
  else {
    fileHandle = CreateFile (getPathFileName().c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    mapHandle = CreateFileMapping (fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
    buf = (uint8_t*)MapViewOfFile (mapHandle, FILE_MAP_READ, 0, 0, 0);
    bufLen = GetFileSize (fileHandle, NULL);
    }
  //}}}
  if ((buf[0] != 0xFF) || (buf[1] != 0xD8)) {
    //{{{  no SOI marker, return false
    cLog::log (LOGERROR, "loadFullBitmap no SOI marker - " + mPathName);

    UnmapViewOfFile (buf);
    CloseHandle (mapHandle);
    CloseHandle (fileHandle);

    return 0;
    }
    //}}}

  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;
  cinfo.err = jpeg_std_error (&jerr);
  jpeg_create_decompress (&cinfo);

  jpeg_mem_src (&cinfo, buf, (unsigned long)bufLen);
  jpeg_read_header (&cinfo, true);

  cinfo.scale_denom = scale;
  cinfo.out_color_space = JCS_EXT_BGRA;
  jpeg_start_decompress (&cinfo);

  auto pitch = cinfo.output_components * cinfo.output_width;
  auto bgraBuf = (uint8_t*)malloc (pitch * cinfo.output_height);
  while (cinfo.output_scanline < cinfo.output_height) {
    uint8_t* lineArray[1];
    lineArray[0] = bgraBuf + (cinfo.output_scanline * pitch);
    jpeg_read_scanlines (&cinfo, lineArray, 1);
    }

  ID2D1Bitmap* bitmap;
  dc->CreateBitmap (D2D1::SizeU (cinfo.output_width, cinfo.output_height), bgraBuf, pitch,
                    { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE, 0,0 }, &bitmap);

  //{{{  switch bitmap, bgraBuf, set sizes
  if (mBgraBuf)
    free (mBgraBuf);
  mBgraBuf = bgraBuf;

  if (mBitmap) {
    auto bitmapSize = mBitmap->GetPixelSize();
    allocSize -= bitmapSize.width * bitmapSize.height * 4;
    mBitmap->Release();
    }
  mBitmap = bitmap;

  mSize.width = cinfo.output_width;
  mSize.height = cinfo.output_height;
  mImageSize.width = cinfo.image_width;
  mImageSize.height = cinfo.image_height;

  allocSize += cinfo.output_width * cinfo.output_height * 4;
  //}}}

  jpeg_finish_decompress (&cinfo);
  jpeg_destroy_decompress (&cinfo);

  if (fileHandle) {
    //{{{  close the file mapping object
    UnmapViewOfFile (buf);
    CloseHandle (mapHandle);
    CloseHandle (fileHandle);
    }
    //}}}

  mLoadTime = (float)duration_cast<milliseconds>(system_clock::now() - time).count();
  cLog::log (LOGINFO3, "cJpegImage::loadImage - scale " + dec(scale) + " took " + dec (mLoadTime) + "ms " + getFileName());

  return allocSize;
  }
//}}}
//{{{
void cJpegImage::releaseImage() {

  if (mBitmap) {
    mBitmap->Release();
    mBitmap = nullptr;
    }

  free (mBgraBuf);
  mBgraBuf = nullptr;
  }
//}}}

//{{{
uint8_t* cJpegImage::compressImage (int& bufLen, float quality) {

  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;

  unsigned char* outbuffer = NULL;
  unsigned long outsize = 0;
  JDIMENSION num_scanlines;

  auto time = system_clock::now();
  cinfo.err = jpeg_std_error (&jerr);
  jpeg_create_compress (&cinfo);

  cinfo.in_color_space = JCS_EXT_BGRX;
  jpeg_set_defaults (&cinfo);

  /* Now that we know input colorspace, fix colorspace-dependent defaults */
  jpeg_default_colorspace (&cinfo);

  //cinfo.optimize_coding = true;
  jpeg_set_quality (&cinfo, (int)quality, false);

  /* Specify data destination for compression */
  outsize = 1000000;
  outbuffer = (unsigned char*)malloc (outsize);
  jpeg_mem_dest (&cinfo, &outbuffer, &outsize);

  /* Start compressor */
  cinfo.image_width = getWidth();
  cinfo.image_height = getHeight();
  cinfo.input_components = 4;
  jpeg_start_compress (&cinfo, TRUE);

  auto bufPtr = mBgraBuf;
  /* Process data */
  //while (cinfo.next_scanline < cinfo.image_height) {
  uint8_t* lineArray[1];
  while ((int)cinfo.next_scanline < cinfo.image_height) {
    num_scanlines = 1;
    lineArray[0] = bufPtr;
    jpeg_write_scanlines (&cinfo, lineArray, num_scanlines);
    bufPtr += cinfo.input_components * cinfo.image_width;
    }

  bufLen = int(cinfo.dest->next_output_byte - outbuffer);
  jpeg_finish_compress (&cinfo);
  jpeg_destroy_compress (&cinfo);

  auto compressTime = (float)duration_cast<milliseconds>(system_clock::now() - time).count();
  cLog::log (LOGNOTICE, "compress " + dec(outsize) + " " + dec(bufLen) + " took " + dec(compressTime) + "ms");

  return outbuffer;
  }
//}}}

// private
//{{{
uint8_t cJpegImage::getColour (cPoint pos, int offset) {
// get colour from pos in fullImage

  if (mBgraBuf && (pos.x < mSize.width) && (pos.y < mSize.height)) {
    offset += ((int(pos.y) * mSize.width) + int(pos.x)) * 4;
    return *(mBgraBuf + offset);
    }
  else
    return 0;
  }
//}}}

//{{{
system_clock::time_point cJpegImage::getFileTimePoint (FILETIME fileTime) {

  // filetime_duration has the same layout as FILETIME; 100ns intervals
  using filetime_duration = duration<int64_t, ratio<1, 10'000'000>>;

  // January 1, 1601 (NT epoch) - January 1, 1970 (Unix epoch):
  constexpr duration<int64_t> nt_to_unix_epoch{INT64_C(-11644473600)};

  const filetime_duration asDuration{static_cast<int64_t>(
      (static_cast<uint64_t>((fileTime).dwHighDateTime) << 32)
          | (fileTime).dwLowDateTime)};
  const auto withUnixEpoch = asDuration + nt_to_unix_epoch;
  return system_clock::time_point{ duration_cast<system_clock::duration>(withUnixEpoch)};
  }
//}}}
//{{{
void cJpegImage::getFileTimes (HANDLE fileHandle) {

  FILETIME creationTime;
  FILETIME lastAccessTime;
  FILETIME lastWriteTime;
  GetFileTime (fileHandle, &creationTime, &lastAccessTime, &lastWriteTime);

  mCreationTimePoint = getFileTimePoint (creationTime);
  mLastAccessTimePoint = getFileTimePoint (lastAccessTime);
  mLastWriteTimePoint = getFileTimePoint (lastWriteTime);
  }
//}}}

// exif
//{{{
WORD cJpegImage::getExifWord (uint8_t* ptr, bool intelEndian) {

  return *(ptr + (intelEndian ? 1 : 0)) << 8 |  *(ptr + (intelEndian ? 0 : 1));
  }
//}}}
//{{{
DWORD cJpegImage::getExifLong (uint8_t* ptr, bool intelEndian) {

  return getExifWord (ptr + (intelEndian ? 2 : 0), intelEndian) << 16 |
         getExifWord (ptr + (intelEndian ? 0 : 2), intelEndian);
  }
//}}}
//{{{
float cJpegImage::getExifSignedRational (uint8_t* ptr, bool intelEndian, DWORD& numerator, DWORD& denominator) {

  numerator = getExifLong (ptr, intelEndian);
  denominator = getExifLong (ptr+4, intelEndian);

  if (denominator == 0)
    return 0;
  else
    return (float)numerator / denominator;
  }
//}}}
//{{{
string cJpegImage::getExifString (uint8_t* ptr) {

  return string ((char*)ptr);
  }
//}}}
//{{{
void cJpegImage::getExifTime (const string& str) {

  // parse ISO time format from string
  istringstream inputStream (str);
  inputStream >> date::parse ("%Y:%m:%d %T", mExifTimePoint);
  }
//}}}
//{{{
cJpegImage::cGps* cJpegImage::getExifGps (uint8_t* ptr, uint8_t* offsetBasePtr, bool intelEndian) {

  auto gps = new cGps();

  auto numDirectoryEntries = getExifWord (ptr, intelEndian);
  ptr += 2;

  for (auto entry = 0; entry < numDirectoryEntries; entry++) {
    auto tag = getExifWord (ptr, intelEndian);
    auto format = getExifWord (ptr+2, intelEndian);
    auto components = getExifLong (ptr+4, intelEndian);
    auto offset = getExifLong (ptr+8, intelEndian);
    auto bytes = components * kBytesPerFormat[format];
    auto valuePtr = (bytes <= 4) ? ptr+8 : offsetBasePtr + offset;
    ptr += 12;

    DWORD numerator;
    DWORD denominator;
    switch (tag) {
      //{{{
      case 0x00:  // version
        gps->mVersion = getExifLong (valuePtr, intelEndian);
        break;
      //}}}
      //{{{
      case 0x01:  // latitude ref
        gps->mLatitudeRef = valuePtr[0];
        break;
      //}}}
      //{{{
      case 0x02:  // latitude
        gps->mLatitudeDeg = getExifSignedRational (valuePtr, intelEndian, numerator, denominator);
        gps->mLatitudeMin = getExifSignedRational (valuePtr + 8, intelEndian, numerator, denominator);
        gps->mLatitudeSec = getExifSignedRational (valuePtr + 16, intelEndian, numerator, denominator);
        break;
      //}}}
      //{{{
      case 0x03:  // longitude ref
        gps->mLongitudeRef = valuePtr[0];
        break;
      //}}}
      //{{{
      case 0x04:  // longitude
        gps->mLongitudeDeg = getExifSignedRational (valuePtr, intelEndian, numerator, denominator);
        gps->mLongitudeMin = getExifSignedRational (valuePtr + 8, intelEndian, numerator, denominator);
        gps->mLongitudeSec = getExifSignedRational (valuePtr + 16, intelEndian, numerator, denominator);
        break;
      //}}}
      //{{{
      case 0x05:  // altitude ref
        gps->mAltitudeRef = valuePtr[0];
        break;
      //}}}
      //{{{
      case 0x06:  // altitude
        gps->mAltitude = getExifSignedRational (valuePtr, intelEndian, numerator, denominator);
        break;
      //}}}
      //{{{
      case 0x07:  // timeStamp
        gps->mHour = getExifSignedRational (valuePtr, intelEndian, numerator, denominator),
        gps->mMinute = getExifSignedRational (valuePtr + 8, intelEndian, numerator, denominator),
        gps->mSecond = getExifSignedRational (valuePtr + 16, intelEndian, numerator, denominator);
        break;
      //}}}
      //{{{
      case 0x08:  // satellites
        printf ("TAG_gps_Satellites %s \n", getExifString (valuePtr).c_str());
        break;
      //}}}
      //{{{
      case 0x0B:  // dop
        printf ("TAG_gps_DOP %f\n", getExifSignedRational (valuePtr, intelEndian, numerator, denominator));
        break;
      //}}}
      //{{{
      case 0x10:  // direction ref
        gps->mImageDirectionRef = valuePtr[0];
        break;
      //}}}
      //{{{
      case 0x11:  // direction
        gps->mImageDirection = getExifSignedRational (valuePtr, intelEndian, numerator, denominator);
        break;
      //}}}
      //{{{
      case 0x12:  // datum
        gps->mDatum = getExifString (valuePtr);
        break;
      //}}}
      //{{{
      case 0x1D:  // date
        gps->mDate = getExifString (valuePtr);
        break;
      //}}}
      //{{{
      case 0x1B:
        //printf ("TAG_gps_ProcessingMethod\n");
        break;
      //}}}
      //{{{
      case 0x1C:
        //printf ("TAG_gps_AreaInformation\n");
        break;
      //}}}
      case 0x09: printf ("TAG_gps_status\n"); break;
      case 0x0A: printf ("TAG_gps_MeasureMode\n"); break;
      case 0x0C: printf ("TAG_gps_SpeedRef\n"); break;
      case 0x0D: printf ("TAG_gps_Speed\n"); break;
      case 0x0E: printf ("TAG_gps_TrackRef\n"); break;
      case 0x0F: printf ("TAG_gps_Track\n"); break;
      case 0x13: printf ("TAG_gps_DestLatitudeRef\n"); break;
      case 0x14: printf ("TAG_gps_DestLatitude\n"); break;
      case 0x15: printf ("TAG_gps_DestLongitudeRef\n"); break;
      case 0x16: printf ("TAG_gps_DestLongitude\n"); break;
      case 0x17: printf ("TAG_gps_DestBearingRef\n"); break;
      case 0x18: printf ("TAG_gps_DestBearing\n"); break;
      case 0x19: printf ("TAG_gps_DestDistanceRef\n"); break;
      case 0x1A: printf ("TAG_gps_DestDistance\n"); break;
      case 0x1E: printf ("TAG_gps_Differential\n"); break;
      default: printf ("unknown gps tag %x\n", tag); break;
      }
    }

  return gps;
  }
//}}}

//{{{
string cJpegImage::getMarkerString (WORD marker, WORD markerLength) {

  string str;
  switch (marker) {
    case 0xFFC0: str = "sof0"; break;
    case 0xFFC2: str = "sof2"; break;
    case 0xFFC4: str = "dht"; break;
    case 0xFFD8: str = "soi"; break;
    case 0xFFDA: str = "sos"; break;
    case 0xFFDB: str = "dqt"; break;
    case 0xFFDD: str = "dri"; break;
    case 0xFFE0: str = "app0"; break;
    case 0xFFE1: str = "app1"; break;
    case 0xFFE2: str = "app2"; break;
    case 0xFFE3: str = "app3"; break;
    case 0xFFE4: str = "app4"; break;
    case 0xFFE5: str = "app5"; break;
    case 0xFFE6: str = "app6"; break;
    case 0xFFE7: str = "app7"; break;
    case 0xFFE8: str = "app8"; break;
    case 0xFFE9: str = "app9"; break;
    case 0xFFEA: str = "app10"; break;
    case 0xFFEB: str = "app11"; break;
    case 0xFFEC: str = "app12"; break;
    case 0xFFED: str = "app13"; break;
    case 0xFFEE: str = "app14"; break;
    case 0xFFFE: str = "com"; break;
    default: str = hex(marker);
    }

  return str + (markerLength ? (":" + dec(markerLength) + " ") : " ");
  }
//}}}

//{{{
void cJpegImage::parseExifDirectory (uint8_t* offsetBasePtr, uint8_t* ptr, bool intelEndian, int& thumbLen, int& thumbOffset) {

  auto numDirectoryEntries = getExifWord (ptr, intelEndian);
  ptr += 2;

  for (auto entry = 0; entry < numDirectoryEntries; entry++) {
    auto tag = getExifWord (ptr, intelEndian);
    auto format = getExifWord (ptr+2, intelEndian);
    auto components = getExifLong (ptr+4, intelEndian);
    auto offset = getExifLong (ptr+8, intelEndian);
    auto bytes = components * kBytesPerFormat[format];
    auto valuePtr = (bytes <= 4) ? ptr+8 : offsetBasePtr + offset;
    ptr += 12;

    DWORD numerator;
    DWORD denominator;
    switch (tag) {
      case TAG_EXIF_OFFSET:
        parseExifDirectory (offsetBasePtr, offsetBasePtr + offset, intelEndian, thumbLen, thumbOffset); break;
      case TAG_ORIENTATION:
        mOrientation = offset; break;
      case TAG_APERTURE:
        mAperture = (float)exp(getExifSignedRational (valuePtr, intelEndian, numerator, denominator)*log(2)*0.5); break;
      case TAG_FOCALLENGTH:
        mFocalLength = getExifSignedRational (valuePtr, intelEndian, numerator, denominator); break;
      case TAG_EXPOSURETIME:
        mExposure = getExifSignedRational (valuePtr, intelEndian, numerator, denominator); break;
      case TAG_MAKE:
        if (mMakeStr.empty()) mMakeStr = getExifString (valuePtr); break;
      case TAG_MODEL:
        if (mModelStr.empty()) mModelStr = getExifString (valuePtr); break;
      case TAG_DATETIME:
      case TAG_DATETIME_ORIGINAL:
      case TAG_DATETIME_DIGITIZED:
        mExifTimeString = getExifString (valuePtr);
        getExifTime (mExifTimeString);
        break;
      case TAG_THUMBNAIL_OFFSET:
        thumbOffset = offset; break;
      case TAG_THUMBNAIL_LENGTH:
        thumbLen = offset; break;
      case TAG_GPSINFO:
        mGps = getExifGps (offsetBasePtr + offset, offsetBasePtr, intelEndian); break;
      //case TAG_MAXAPERTURE:
      //  printf ("TAG_MAXAPERTURE\n"); break;
      //case TAG_SHUTTERSPEED:
      //  printf ("TAG_SHUTTERSPEED\n"); break;
      default:;
      //  printf ("TAG %x\n", tag);
      }
    }

  auto extraDirectoryOffset = getExifLong (ptr, intelEndian);
  if (extraDirectoryOffset > 4)
    parseExifDirectory (offsetBasePtr, offsetBasePtr + extraDirectoryOffset, intelEndian, thumbLen, thumbOffset);
  }
//}}}
//{{{
bool cJpegImage::parseHeader (uint8_t* ptr, size_t bufLen, int& thumbLen, int& thumbOffset) {
// find and read APP1 EXIF marker, return true if thumb
// - make more resilient to bytes running out etc

  thumbOffset = 0;
  thumbLen = 0;

  if (bufLen < 4) {
    //{{{  return false if not enough bytes for first marker
    cLog::log (LOGERROR, "parseHeader not enough bytes - " + mPathName);
    return false;
    }
    //}}}

  auto soiPtr = ptr;
  if (getExifWord (ptr, false) != 0xFFD8) {
    //{{{  return false if no SOI marker
    cLog::log (LOGERROR, "parseHeader no soi marker - " + mPathName);
    return false;
    }
    //}}}
  ptr += 2;

  auto marker = getExifWord (ptr, false);
  ptr += 2;
  auto markerLength = getExifWord (ptr, false);
  ptr += 2;
  auto exifIdPtr = ((marker == 0xFFE1) && (markerLength < bufLen-6)) ? ptr : nullptr;
  //{{{  parse markers, SOF marker for imageSize, until SOS, should abort if no more bytes
  mMarkerStr = ""; // abit pointless to print soi - getMarkerString (0xFFD8, 0);

  while ((marker >= 0xFF00) && (marker != 0xFFDA)) {
    // until invalid marker or SOS
    mMarkerStr += getMarkerString (marker, markerLength);
    if ((marker == 0xFFC0) || (marker == 0xFFC2)) {
      mImageSize.height = getExifWord (ptr+1, false);
      mImageSize.width = getExifWord (ptr+3, false);
      }
    ptr += markerLength-2;

    marker = getExifWord (ptr, false);
    ptr += 2;
    markerLength = getExifWord (ptr, false);
    ptr += 2;
    };

  mMarkerStr += getMarkerString (marker, markerLength);
  //}}}

  if (exifIdPtr) {
    //   check exifId, ignore trailing two nulls
    if (getExifLong (exifIdPtr, false) != 0x45786966) {
      //{{{  return false if not EXIF
      cLog::log (LOGERROR, "parseHeader EXIF00 ident error - " + mPathName);
      return false;
      }
      //}}}
    exifIdPtr += 6;
    auto offsetBasePtr = exifIdPtr;

    bool intelEndian = getExifWord (exifIdPtr, false) == 0x4949;
    exifIdPtr += 2;

    if (getExifWord (exifIdPtr, intelEndian) != 0x002a) {
      //{{{  return false if no 0x002a word
      cLog::log (LOGERROR, "parseHeader EXIF 2a error - " + mPathName);
      return false;
      }
      //}}}
    exifIdPtr += 2;

    auto firstOffset = getExifLong (exifIdPtr, intelEndian);
    if (firstOffset != 8) {
      //{{{  return false if unusual firstOffset,
      cLog::log (LOGERROR, "parseHeader firstOffset warning - " + mPathName + dec(firstOffset));
      return false;
      }
      //}}}
    exifIdPtr += 4;

    parseExifDirectory (offsetBasePtr, exifIdPtr, intelEndian, thumbLen, thumbOffset);

    // adjust mThumbOffset to be from soiMarker
    thumbOffset += DWORD(offsetBasePtr - soiPtr);
    return thumbLen > 0;
    }
  else // no thumb
    return false;
  }
//}}}

//{{{
string cJpegImage::cGps::getString() {

  std::string str;

  if (!mDatum.empty())
    str = mDatum + " ";
  if (mLatitudeDeg || mLatitudeMin || mLatitudeSec)
    str += dec(mLatitudeDeg) + std::string(&mLatitudeRef,1) + dec(mLatitudeMin) + "'" + dec(mLatitudeSec) + " ";
  if (mLongitudeDeg || mLongitudeMin || mLongitudeSec)
    str += dec(mLongitudeDeg) + std::string(&mLongitudeRef,1) + dec(mLongitudeMin) + "'" + dec(mLongitudeSec) + " ";

  if (mAltitude)
    str += "alt:" + hex(mAltitudeRef,2) + ":" + dec(mAltitude) + ' ';
  if (mImageDirection)
    str += "dir:" + std::string(&mImageDirectionRef,1) + ":" + dec(mImageDirection) + ' ';

  if (!mDatum.empty())
    str += mDate + ' ';

  if (mHour || mMinute || mSecond)
    str += dec(mHour) + ':' + dec(mMinute) + ':' + dec(mSecond);

  return str;
  }
//}}}
