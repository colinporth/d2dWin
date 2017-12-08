// bayer.h
#pragma once
#include <stdint.h>

typedef enum { COLOR_RGGB, COLOR_GBRG, COLOR_GRBG, COLOR_BGGR } tBayerOrder;
typedef enum { NEAREST, SIMPLE, BILINEAR, HQLINEAR, VNG, AHD } tBayer;

#define COLOR_MIN  COLOR_RGGB
#define COLOR_MAX  COLOR_BGGR
#define COLOR_NUM (COLOR_FILTER_MAX - COLOR_MIN + 1)

//{{{
typedef enum {
  BSUCCESS                    =  0,
  FAILURE                     = -1,
  NOT_A_CAMERA                = -2,
  FUNCTION_NOT_SUPPORTED      = -3,
  CAMERA_NOT_INITIALIZED      = -4,
  MEMORY_ALLOCATION_FAILURE   = -5,
  TAGGED_REGISTER_NOT_FOUND   = -6,
  NO_ISO_CHANNEL              = -7,
  NO_BANDWIDTH                = -8,
  IOCTL_FAILURE               = -9,
  CAPTURE_IS_NOT_SET          = -10,
  CAPTURE_IS_RUNNING          = -11,
  RAW1394_FAILURE             = -12,
  FORMAT7_ERROR_FLAG_1        = -13,
  FORMAT7_ERROR_FLAG_2        = -14,
  INVALID_ARGUMENT_VALUE      = -15,
  REQ_VALUE_OUTSIDE_RANGE     = -16,
  INVALID_FEATURE             = -17,
  INVALID_VIDEO_FORMAT        = -18,
  INVALID_VIDEO_MODE          = -19,
  INVALID_FRAMERATE           = -20,
  INVALID_TRIGGER_MODE        = -21,
  INVALID_TRIGGER_SOURCE      = -22,
  INVALID_ISO_SPEED           = -23,
  INVALID_IIDC_VERSION        = -24,
  INVALID_COLOR_CODING        = -25,
  INVALID_COLOR_FILTER        = -26,
  INVALID_CAPTURE_POLICY      = -27,
  INVALID_ERROR_CODE          = -28,
  INVALID_bayerMETHOD        = -29,
  INVALID_VIDEO1394_DEVICE    = -30,
  INVALID_OPERATION_MODE      = -31,
  INVALID_TRIGGER_POLARITY    = -32,
  INVALID_FEATURE_MODE        = -33,
  INVALID_LOG_TYPE            = -34,
  INVALID_BYTE_ORDER          = -35,
  INVALID_STEREO_METHOD       = -36,
  BASLER_NO_MORE_SFF_CHUNKS   = -37,
  BASLER_CORRUPTED_SFF_CHUNK  = -38,
  BASLER_UNKNOWN_SFF_CHUNK    = -39
  } eBayerResult;
//}}}
#define ERROR_MIN  BASLER_UNKNOWN_SFF_CHUNK
#define ERROR_MAX  SUCCESS
#define ERROR_NUM (ERROR_MAX-ERROR_MIN+1)

//{{{
#ifdef __cplusplus
  extern "C" {
#endif
//}}}
  eBayerResult bayerDecode8 (const uint8_t* bayer, uint8_t* rgb, uint32_t sx, uint32_t sy,
                             tBayerOrder tile, tBayer method);
  eBayerResult bayerDecode16 (const uint16_t* bayer, uint16_t* rgb, uint32_t sx, uint32_t sy,
                              tBayerOrder tile, tBayer method, uint32_t bits);
//{{{
#ifdef __cplusplus
  }
#endif
//}}}
