//*********************************************************************************************
// Copyright 2014 ON Semiconductor.  This software and/or documentation is provided by 
// ON Semiconductor under limited terms and conditions.  
// The terms and conditions pertaining to the software and/or documentation are available at 
// www.onsemi.com (“ON Semiconductor Standard Terms and Conditions of Sale, Section 8 Software”).  
// Do not use this software and/or documentation unless you have carefully read and you agree 
// to the limited terms and conditions.  
// By using this software and/or documentation, you agree to the limited terms and conditions. 
//*********************************************************************************************

// MIDLIB (Micron Imaging Device LIBrary) 

// NOTE: to use Link-time dynamic loading (default) do the following:
//       1) Include the library module midlib.lib to your project.
//       2) Call the functions as defined below

#ifndef __MIDLIB2_H__   // [
#define __MIDLIB2_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WINDOWS
    #ifdef MIDLIB_EXPORTS
        #define MIDLIB_API __declspec(dllexport)
    #else
        #define MIDLIB_API __declspec(dllimport)
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#else
    #define MIDLIB_API
#endif

//Make sure that the data is packed w/ 8-byte alignment
//Also make sure that enums are treated as integers
#ifndef _WIN64
#pragma pack(8)
#endif

/***************************************************************************
  Error and log types

  MI_NO_ERROR_LOG    Error logging is turned off
  MI_ERROR_SEVERE    Log Severe errors
  MI ERROR_MINOR     Log Minor errors
  MI_ALL_ERRORS      Logs all error messages (Severe and Minor)
  MI_LOG             Logs General logging message
  MI_LOG_SHIP        Log Serial I/O messages (SHIP) 
  MI_LOG_USB         Log USB transactions 
  MI_LOG_DEBUG       Log Debug messages
*****************************************************************************/
#define MI_NO_ERROR_LOG             0
#define MI_ERROR_SEVERE             1
#define MI_ERROR_MINOR              2
#define MI_ALL_ERRORS               (MI_ERROR_SEVERE | MI_ERROR_MINOR)
#define MI_LOG                      4
#define MI_LOG_SHIP                 8
#define MI_LOG_USB                  16
#define MI_LOG_DEBUG                32
#define MI_ALL_LOGS                 (MI_LOG | MI_LOG_SHIP | MI_LOG_USB | MI_LOG_DEBUG)
#define MI_ERROR_LOG_ALL            (MI_ALL_ERRORS | MI_ALL_LOGS)

/***************************************************************************
  Constants

  MI_MAX_CAMERAS       maximum number of cameras supported
  MI_MAX_REGS          maximum number of register values in a framedata structure
  MI_APTINA_VID        Aptina Imaging Vendor ID for USB
  MI_MICRON_VID        Micron Imaging Vendor ID for USB
  MI_MICRON_PCI_VID    Micron Imaging Vendor ID for PCI
  MI_ONSEMI_VID        ON Semiconductor Vendor ID for USB
  MI_MAX_STRING        maximum length of mi_string type
*****************************************************************************/
#define MI_MAX_CAMERAS              50
#define MI_MAX_REGS                 400
#define MI_APTINA_VID               0x20FB
#define MI_MICRON_VID               0x0634
#define MI_MICRON_PCI_VID           0x1344
#define MI_ONSEMI_VID               0x1057
#define MI_TRUESENSE_VID            0x26D4
#define MI_MAX_STRING               256

/**************************************************************************
  Available Camera Transports
***************************************************************************/
#define MI_USB_BULK_TRANSPORT              0x0001
#define MI_CARDBUS_TRANSPORT               0x0002
#define MI_DLL_TRANSPORT                   0x0004
#define MI_CL_TRANSPORT                    0x0008
#define MI_BMP_TRANSPORT                   0x0010
#define MI_AVI_TRANSPORT                   0x0020
#define MI_RAW_TRANSPORT                   0x0040
#define MI_WMV_TRANSPORT                   0x0080
#define MI_ALL_TRANSPORTS                  0xFFFF
#define MI_NUL_TRANSPORT                   0x10000 // not included in MI_ALL_TRANSPORTS

/**************************************************************************
  Device Removal/Arrival Notification Flags

MI_DEVEVENT_REMOVAL     A device is being removed
MI_DEVEVENT_PRIMARY     A primary device (i.e. opened device) is being removed
MI_DEVEVENT_OTHER       A non-opened device is being removed
MI_DEVEVENT_ARRIVAL     currently not supported
***************************************************************************/
#define  MI_DEVEVENT_REMOVAL 0x00000001
#define  MI_DEVEVENT_ARRIVAL 0x00000002
#define  MI_DEVEVENT_PRIMARY 0x00000004
#define  MI_DEVEVENT_OTHER   0x00000008

/***************************************************************************
 
  Type Definitions

  The following types are used to maintain portability.
*****************************************************************************/
typedef unsigned char     mi_u8;
typedef unsigned short    mi_u16;
typedef unsigned int      mi_u32;
typedef signed char       mi_s8;
typedef signed short      mi_s16;
typedef signed int        mi_s32;
typedef char              mi_string[MI_MAX_STRING];
#if defined(_WIN64)
typedef __int64           mi_intptr;
#elif defined(__GNUC__)
typedef __attribute__((mode (__pointer__))) int mi_intptr;
#elif defined(_MSC_VER) && _MSC_VER >= 0x1300
typedef __w64 int         mi_intptr;
#else
typedef int               mi_intptr;
#endif
typedef unsigned long long mi_u64;
typedef long long         mi_s64;

/****************************************************************************
  Enums

  mi_error_code   - Function return codes
  mi_image_types  - Supported image types
  mi_product_ids  - Current camera boards supported
  mi_sensor_types - Supported sensors
  mi_modes        - Modes which can be used with setMode/getMode routines
  mi_unswizzle_modes - Modes for unswizzling, used with setMode/getMode
  mi_sync_types   - Possible read/write register synchronization modes
*****************************************************************************/
/***************************************************************************

                    Midlib Error codes
                    
*****************************************************************************/
typedef enum  
{
     //Generic return codes
     MI_CAMERA_SUCCESS          = 0x00,         //Success value for midlib routines
     MI_CAMERA_ERROR            = 0x01,         //General failure for midlib routines
     //Grabframe return codes
     MI_GRAB_FRAME_ERROR        = 0x03,         //General failure for grab frame routine
     MI_NOT_ENOUGH_DATA_ERROR   = 0x04,         //Grab frame failed to return enough data
     MI_EOF_MARKER_ERROR        = 0x05,         //EOF packet not found in grab frame data
     MI_BUFFER_SIZE_ERROR       = 0x06,         //GrabFrame buffer is too small
     //mi_OpenCameras return codes
     MI_SENSOR_FILE_PARSE_ERROR = 0x07,         //There was an error parsing the sdat file
     MI_SENSOR_DOES_NOT_MATCH   = 0x08,         //Cannot find sdat file which matches sensor
     MI_SENSOR_NOT_INITIALIZED  = 0x09,         //The sensor structure has not been initialized (call updateFrame)
     MI_SENSOR_NOT_SUPPORTED    = 0x0A,         //The sensor is no longer supported
     //I2C return codes
     MI_I2C_BIT_ERROR           = 0x0B,         //I2C bit error  
     MI_I2C_NACK_ERROR          = 0x0C,         //I2C NAC error
     MI_I2C_TIMEOUT             = 0x0D,         //I2C time out error
     MI_CAMERA_TIMEOUT          = 0x0E,      
     MI_TOO_MUCH_DATA_ERROR     = 0x0F,         //Grab frame returned more data than expected

     MI_CAMERA_NOT_SUPPORTED    = 0x10,         //The function call is not supported

     //return codes for parsing sdat file
     MI_PARSE_SUCCESS           = 0x20,         //Parsing was successful
     MI_DUPLICATE_DESC_ERROR    = 0x21,         //Duplicate unique descriptor was found
     MI_PARSE_FILE_ERROR        = 0x22,         //Unable to open sensor data file
     MI_PARSE_REG_ERROR         = 0x23,         //Error parsing the register descriptors
     MI_UKNOWN_SECTION_ERROR    = 0x24,         //Unknown Section found in sensor data file
     MI_CHIP_DESC_ERROR         = 0x25,         //Error parsing the chip descriptor section
     MI_PARSE_ADDR_SPACE_ERROR  = 0x26,         //Error parsing the address space section
     //Error codes for loading INI presets 
     MI_INI_SUCCESS             = 0x100,        //INI Preset is loaded successfully
     MI_INI_KEY_NOT_SUPPORTED   = 0x101,        //Key is not supported - will be ignored
     MI_INI_LOAD_ERROR          = 0x102,        //Error loading INI preset
     MI_INI_POLLREG_TIMEOUT     = 0x103,        //time out in POLLREG/POLL_VAR/POLL_FIELD command
     MI_INI_HANDLED_SUCCESS     = 0x104,        //transport handled the command, success
     MI_INI_HANDLED_ERROR       = 0x105,        //transport handled the command, with error
     MI_INI_NOT_HANDLED         = 0x106,        //transport did not handle the command
} mi_error_code;

typedef enum { MI_UNKNOWN_IMAGE_TYPE, 
               MI_BAYER_8,
               MI_BAYER_10,
               MI_BAYER_8_ZOOM,
               MI_BAYER_10_ZOOM,
               MI_YCBCR,
               MI_RGB565,
               MI_RGB555,
               MI_RGB444X,
               MI_RGBX444,
               MI_RGB24,
               MI_RGB32,
               MI_BAYER_12,

               MI_BAYER_S12,  // Signed 16-bit, 0-4095 nominal value range,
                              // intermediate image format used in software colorpipe
               MI_RGB48,      // R, G, B, 0-65535
               MI_JPEG,
               MI_BAYER_STEREO,  //  each 16-bit pixel is 8-bit left pixel and 8-bit right pixel
               MI_PNG,
               MI_BGRG,      // Like YCbCr but with Y->G Cb->B and Cr->R to make an RGB format
               MI_YUV420,    // Like YCbCr, but even (numbering from 0) rows are Y-only
               MI_BAYER_14,
               MI_BAYER_12_HDR,  // Compressed HiDy on A-1000ERS (deprecated, equivalent to MI_BAYER_12)
               MI_BAYER_14_HDR,  // Compressed HiDy on A-1000ERS (deprecated, equivalent to MI_BAYER_14)
               MI_BAYER_20,
               MI_RGB332,
               MI_M420,     // Like YCbCr, but even rows are Y-only and odd rows are YYYY...CbCrCbCr...
               MI_BAYER_10_IHDR,  //  Interlaced HiDy (A-8150, etc.)
               MI_JPEG_SPEEDTAGS,  // JPEG with Scalado SpeedTags
               MI_BAYER_16,
               MI_YCBCR_10,
               MI_BAYER_6,
               MI_JPEG_ROT,   // JPEG with 90-deg rotation within the blocks
               MI_Y400,     //  Like YCbCr, but with Y only
               MI_RGB555L,
               MI_RGB555M,
               MI_BAYER_S24,  // Signed 24-bit, 0-16777215 nominal value range,
                              // intermediate image format used in software colorpipe
               MI_H264,
               MI_BAYER_18,
               //  Only add symbols to the end

  MI_NUM_IMAGE_TYPES
} mi_image_types;

typedef enum { MI_UNKNOWN_PRODUCT,
               MI_BIGDOG    = 0x1002,
               MI_DEMO_1    = 0x1003,
               MI_DEMO_1A   = 0x1004,
               MI_WEBCAM    = 0x1006,
               MI_DEMO_2    = 0x1007,
               MI_DEV_2     = 0x1008,
               MI_MIGMATE   = 0x1009,
               MI_PCCAM     = 0x100A,
               MI_MIDES     = 0x100B,
               MI_MIDES_XL  = 0x100C,
               MI_DEMO_2X   = 0x100D,
               MI_DEMO_3    = 0x100E,
               MI_MIGMATE_3 = 0x100F,
               MI_EMULATION_3 = 0x1010,
               MI_DIAGS_RECEIVER= 0x1011,
               MI_CLINK_1   = 0x5555,            // Camera Link product ID
               MI_CARDCAM_1 = 0xD100
} mi_product_ids;

typedef enum { MI_UNKNOWN_SENSOR,
               MI_0133,             //No longer supported
               MI_0343,             //No longer supported
               MI_0360,
               MI_0366,             //No longer supported
               MI_0133_SOC,         //No longer supported
               MI_0343_SOC,         //No longer supported
               MI_1300,
               MI_1310_SOC,
               MI_2000,
               MI_0360_SOC,
               MI_1310,             //No longer supported
               MI_3100,
               MI_0350,
               MI_0366_SOC,
               MI_2010,
               MI_2010_SOCJ,
               MI_0370,
               MI_RESERVED_SENSOR0,             
               MI_SENSOR_X,         //Used for testing
               MI_1320_SOC,
               MI_1320,             //No longer supported
               MI_5100,
               MI_RESERVED_SENSOR1,
               MI_0354_SOC,
               MI_3120,
               MI_RESERVED_SENSOR2,         
               MI_RESERVED_SENSOR3,
               MI_RESERVED_SENSOR4, 
               MI_RESERVED_SENSOR5,
               MI_RESERVED_SENSOR6,
               MI_RESERVED_SENSOR7,
               MI_RESERVED_SENSOR8,
               MI_RESERVED_SENSOR9,
               MI_RESERVED_SENSOR10,
               MI_RESERVED_SENSOR11,
               MI_BITMAP_SENSOR,
               MI_RESERVED_SENSOR12,
               MI_RESERVED_SENSOR13,
               MI_RESERVED_SENSOR14,
               MI_RESERVED_SENSOR15,
               MI_RESERVED_SENSOR16,
               MI_RESERVED_SENSOR17,
               MI_RESERVED_SENSOR18,
               MI_RESERVED_SENSOR19,
               MI_RESERVED_SENSOR20,
               MI_RESERVED_SENSOR21,
               MI_RESERVED_SENSOR22,
               MI_RESERVED_SENSOR23,
               MI_RESERVED_SENSOR24,
               MI_RESERVED_SENSOR25,
               MI_RESERVED_SENSOR26,
               MI_RESERVED_SENSOR27,
               MI_RESERVED_SENSOR28,
               MI_RESERVED_SENSOR29,
               MI_RESERVED_SENSOR30,
               MI_RESERVED_SENSOR31,
               MI_RESERVED_SENSOR32,
               MI_RESERVED_SENSOR33,
               MI_RESERVED_SENSOR34,
               MI_RESERVED_SENSOR35,
               MI_RESERVED_SENSOR36,
               MI_RESERVED_SENSOR37,
               MI_8131,
               MI_1000GS,
               MI_1000ERS,
               MI_RESERVED_SENSOR38,
               MI_RESERVED_SENSOR39,
               MI_RESERVED_SENSOR40,
               MI_RESERVED_SENSOR41,
               MI_RESERVED_SENSOR42,
               MI_RESERVED_SENSOR43,
               MI_RESERVED_SENSOR44,
               MI_RESERVED_SENSOR45,
               MI_2031_SOC,
               MI_RESERVED_SENSOR46,
               MI_RESERVED_SENSOR47,
               MI_5131E,
               MI_5131E_PLCC,
               MI_RESERVED_SENSOR48,
               MI_RESERVED_SENSOR49,
               MI_RESERVED_SENSOR50,
               MI_RESERVED_SENSOR51,
               MI_RESERVED_SENSOR52,
               MI_RESERVED_SENSOR53,
               MI_51HD_PLUS,
               MI_RESERVED_SENSOR54,
               MI_RESERVED_SENSOR55,
               MI_RESERVED_SENSOR56,
               MI_RESERVED_SENSOR57,
               MI_RESERVED_SENSOR58,
               MI_RESERVED_SENSOR59,
               MI_RESERVED_SENSOR60,
               MI_RESERVED_SENSOR61,
               MI_RESERVED_SENSOR62,
               MI_RESERVED_SENSOR63,
               MI_RESERVED_SENSOR64,
               MI_RESERVED_SENSOR65,
               MI_RESERVED_SENSOR66,
               MI_RESERVED_SENSOR67,
               MI_RESERVED_SENSOR68,
               MI_RESERVED_SENSOR69,
               MI_RESERVED_SENSOR70,
               MI_RESERVED_SENSOR71,
               MI_RESERVED_SENSOR72,
               MI_RESERVED_SENSOR73,
               MI_RESERVED_SENSOR74,
               MI_RESERVED_SENSOR75,
               MI_RESERVED_SENSOR76,
               MI_RESERVED_SENSOR77,
               MI_RESERVED_SENSOR78,
               MI_RESERVED_SENSOR79,
               MI_RESERVED_SENSOR80,
               MI_RESERVED_SENSOR81,
               MI_RESERVED_SENSOR82,
               MI_RESERVED_SENSOR83,
               MI_RESERVED_SENSOR84,
               MI_RESERVED_SENSOR85,
               MI_RESERVED_SENSOR86,
               MI_GRANITE,
               MI_RESERVED_SENSOR87,
               MI_RESERVED_SENSOR88,
               MI_RESERVED_SENSOR89,
               MI_RESERVED_SENSOR90,
               MI_RESERVED_SENSOR91,
               MI_RESERVED_SENSOR92,
               MI_IMX188,
               MI_RESERVED_SENSOR93,
               MI_RESERVED_SENSOR94,
               MI_RESERVED_SENSOR95,
               MI_RESERVED_SENSOR96,
               MI_RESERVED_SENSOR97,
               MI_RESERVED_SENSOR98,
               MI_RESERVED_SENSOR99,
               MI_RESERVED_SENSOR100,
               MI_RESERVED_SENSOR101,
               MI_PYTHON_0300,
               MI_PYTHON_0500,
               MI_PYTHON_1300,
               MI_PYTHON_2000,
               MI_PYTHON_5000,
               MI_RESERVED_SENSOR102,
               MI_RESERVED_SENSOR103,
               MI_RESERVED_SENSOR104,
               //  Only add symbols to the end
} mi_sensor_types;

//  Public and Announced Products
#define MI_AP0100    MI_RESERVED_SENSOR67  /* AP0100AT and AP0100CS */
#define MI_AP0101    MI_RESERVED_SENSOR63  /* AP0101AT and AP0101CS */
#define MI_AP1302    MI_RESERVED_SENSOR79
#define MI_AR0130    MI_RESERVED_SENSOR60
#define MI_AR0132    MI_1000ERS
#define MI_AR0134    MI_RESERVED_SENSOR76
#define MI_AR0140    MI_RESERVED_SENSOR66  /* AR0140AT and AR0140CS */
#define MI_AR0141    MI_RESERVED_SENSOR91  /* AR0141CS */
#define MI_AR0230    MI_RESERVED_SENSOR90  /* AR0230CS */
#define MI_AR0261    MI_RESERVED_SENSOR84
#define MI_AR0330    MI_RESERVED_SENSOR53  /* 3135 */
#define MI_AR0331    MI_RESERVED_SENSOR57  /* 3110 */
#define MI_AR0542    MI_RESERVED_SENSOR61  /* 5141 */
#define MI_AR0543    MI_RESERVED_SENSOR61  /* 5142 */
#define MI_AR0833    MI_RESERVED_SENSOR65
#define MI_AR0835    MI_RESERVED_SENSOR65  /* AR0835HS */
#define MI_AR1011    MI_RESERVED_SENSOR33  /* AR1011HS */
#define MI_AR1335    MI_RESERVED_SENSOR88
#define MI_AR1411    MI_RESERVED_SENSOR81 
#define MI_AR1820HS  MI_RESERVED_SENSOR70
#define MI_ARX550    MI_RESERVED_SENSOR59
#define MI_AS0260    MI_RESERVED_SENSOR47  /* 2340_SOC */
#define MI_ASX340    MI_RESERVED_SENSOR46  /* ASX340AT and ASX340CS - 361_SOC */
#define MI_ASX344    MI_RESERVED_SENSOR83  /* ASX344AT */
#define MI_ASX350    MI_RESERVED_SENSOR72  /* ASX350AT and ASX350CS */
#define MI_MT9D015   MI_RESERVED_SENSOR51  /* A-2030 */
#define MI_MT9D115   MI_2031_SOC
#define MI_MT9D131   MI_2010_SOCJ
#define MI_MT9F002   MI_RESERVED_SENSOR55  /* 14041 */
#define MI_10030     MI_RESERVED_SENSOR32
#define MI_MT9J003   MI_RESERVED_SENSOR32
#define MI_MT9M001   MI_1300
#define MI_MT9M021   MI_1000GS
#define MI_MT9M024   MI_1000ERS
#define MI_MT9M025   MI_1000ERS
#define MI_MT9M114   MI_RESERVED_SENSOR44  /* 1040_SOC */
#define MI_MT9P001   MI_5100
#define MI_MT9P006   MI_51HD_PLUS
#define MI_MT9P031   MI_5100
#define MI_5140_SOC  MI_RESERVED_SENSOR30
#define MI_MT9P111   MI_RESERVED_SENSOR30
#define MI_MT9P401   MI_5100
#define MI_MT9V022   MI_0350
#define MI_MT9V023   MI_RESERVED_SENSOR24  /* 0351 */
#define MI_MT9V024   MI_RESERVED_SENSOR24  /* 0351 */
#define MI_MT9V032   MI_0350
#define MI_MT9V034   MI_RESERVED_SENSOR24  /* 0351 */
#define MI_0351      MI_RESERVED_SENSOR24
#define MI_MT9V111   MI_0360_SOC
#define MI_MT9V114   MI_RESERVED_SENSOR38  /* 390_SOC  */
#define MI_MT9V115   MI_RESERVED_SENSOR48  /* 0391_SOC */
#define MI_MT9V124   MI_RESERVED_SENSOR38  /* SOC-0390-serial */
#define MI_MT9V125   MI_0354_SOC  
#define MI_0356_SOC  MI_RESERVED_SENSOR29
#define MI_MT9V126   MI_RESERVED_SENSOR29  /* 356-SOC */
#define MI_MT9V127   MI_RESERVED_SENSOR29  /* 356-SOC */
#define MI_MT9V128   MI_RESERVED_SENSOR29  /* 356-SOC */

// Products no longer recommended
#define MI_2020_SOC  MI_RESERVED_SENSOR1
#define MI_2020      MI_RESERVED_SENSOR3
#define MI_0350_ST   MI_RESERVED_SENSOR4   //Stereo Test board
#define MI_8130      MI_RESERVED_SENSOR9
#define MI_1325      MI_RESERVED_SENSOR10
#define MI_0380_SOC  MI_RESERVED_SENSOR13
#define MI_1330_SOC  MI_RESERVED_SENSOR14
#define MI_1600      MI_RESERVED_SENSOR15
#define MI_0380      MI_RESERVED_SENSOR16
#define MI_3130      MI_RESERVED_SENSOR17
#define MI_5130      MI_RESERVED_SENSOR18
#define MI_RAINBOW2  MI_RESERVED_SENSOR19
#define MI_3130_SOC  MI_RESERVED_SENSOR21
#define MI_2030_SOC  MI_RESERVED_SENSOR22
#define MI_3125      MI_RESERVED_SENSOR23
#define MI_9130      MI_RESERVED_SENSOR25
#define MI_5135      MI_RESERVED_SENSOR26
#define MI_2025      MI_RESERVED_SENSOR27
#define MI_0351_ST   MI_RESERVED_SENSOR28   //Stereo Test board
#define MI_3132_SOC  MI_RESERVED_SENSOR31
#define MI_5131      MI_RESERVED_SENSOR34
#define MI_AR0830    MI_RESERVED_SENSOR35  /* 8140 */
#define MI_MT9V117   MI_RESERVED_SENSOR36  /* 367_SOC  */
#define MI_AS0350    MI_RESERVED_SENSOR37  /* 3140_SOC */
#define MI_AR0540    MI_RESERVED_SENSOR39  /* 5140 */
#define MI_MT9F001   MI_RESERVED_SENSOR42  /* 14040 */
#define MI_AP1400    MI_RESERVED_SENSOR43  /* ICPHD */
#define MI_MT9M003   MI_RESERVED_SENSOR45  /* 1601 */
#define MI_AR0150    MI_RESERVED_SENSOR49  /* 1326 */
#define MI_AR0250    MI_RESERVED_SENSOR51  /* 2030 */
#define MI_MT9H003   MI_RESERVED_SENSOR52  /* 16002 */
#define MI_AR0832    MI_RESERVED_SENSOR58  /* 8141 */

typedef enum { MI_ERROR_CHECK_MODE,
               MI_REG_ADDR_SIZE,
               MI_REG_DATA_SIZE,
               MI_USE_REG_CACHE,

               MI_SW_UNSWIZZLE_MODE,
               MI_UNSWIZZLE_MODE,
               MI_SW_UNSWIZZLE_DEFAULT,
               MI_DATA_IS_SWIZZLED,

               _MI_READ_SYNC, //  deprecated
               _MI_WRITE_SYNC, //  deprecated
               _MI_CONTINUOUS_READ, //  deprecated

               MI_ERROR_LOG_TYPE,

               MI_SPOOF_SIZE,
               MI_SPOOF_SUPPORTED,
               MI_HW_BUFFERING,
               MI_OUTPUT_CLOCK_FREQ,
               MI_ALLOW_FAR_ACCESS,
               MI_PIXCLK_POLARITY,
               MI_SENSOR_POWER,
               MI_SENSOR_RESET,
               MI_SHIP_SPEED,

               //  XDMA state
               MI_DIRECT_VAR_ACCESS,
               MI_XDMA_LOGICAL,
               MI_XDMA_PHY_A15,
               MI_XDMA_PHY_REGION,

               MI_HW_FRAME_COUNT,

               MI_HIDY,
               MI_COMPRESSED_LENGTH,
               MI_SENSOR_SHUTDOWN,

               //  ICPHD 'XDMA' state
               MI_XDMA_ADV_BASE,

               MI_PIXCLK_FREQ,  //  sensor PIXCLK measurement

               MI_SIMUL_REG_FRAMEGRAB,
               MI_DETECT_FRAME_SIZE,

               //  Current bitsperclock and clocksperpixel settings
               //  Read-only, use mi_updateFrameSize() to set
               MI_BITS_PER_CLOCK,
               MI_CLOCKS_PER_PIXEL,

               //  Image data receiver parameters (CCP, MIPI, HiSPi, etc.)
               MI_RX_TYPE,              //  mi_rx_types
               MI_RX_LANES,
               MI_RX_BIT_DEPTH,
               MI_RX_MODE,              //  mi_rx_modes
               MI_RX_CLASS,
               MI_RX_SYNC_CODE,
               MI_RX_EMBEDDED_DATA,
               MI_RX_VIRTUAL_CHANNEL,
               MI_RX_MSB_FIRST,

               //  HDMI output
               MI_HDMI_MODE,
               MI_EIS,  // electronic image stabilization (ICP-HD + HDMI Demo)

               //  Capture to demo board RAM
               MI_MEM_CAPTURE,           // set number of frames to capture
               MI_MEM_CAPTURE_PROGRESS,  // frames stored so far, read-only
               MI_MEM_CAPTURE_MB,        // available RAM in MB, read-only

               //  Stereo
               MI_STEREO_MODE,          //  mi_stereo_modes

               MI_HW_FRAME_COUNT_MASK,
               MI_GRABFRAME_TIMEOUT,    //  timeout value per frame in grabFrame call
               MI_HW_FRAME_TIME,        //  in microseconds
               MI_LSB_ALIGNED,          //  parallel port data is LSB-aligned
               MI_NACK_RETRIES,         //  number of times to retry after I2C NACK

               //  Image data receiver parameters (CCP, MIPI, HiSPi, etc.)
               MI_RX_CCIR656,           //  use CCIR-656 embedded sync codes
               MI_RX_INTERLACED,        //  incoming stream is interlaced

               MI_MEM_CAPTURE_CYCLE,    // linear, circular

               MI_TRIGGER_HIGH_WIDTH,   // trigger high width in clock.
               MI_TRIGGER_LOW_WIDTH,   // trigger low width in clock.
               MI_TRIGGER, // trigger modes, 0 : No trigger, 1: single shot, 2: continuous.
               MI_OUTPUT_PORT, // Port for image data out, 0: demo3, 1: CameraLink

               MI_PIXEL_PACK,   //  Pack 10- or 12-bit pixel data

               MI_PARITY,   //  parity mode for UART transports (same definition as WinBase.h)
               MI_GPIO,   //  general-purpose I/Os on the demo board
               MI_FAR_BROADCAST,  //  broadcast I2C on multiple Far buses

               MI_EXTCLK_FREQ,  //  sensor EXTCLK measurement

               MI_RX_PHY_SELECT,    //  bitwise select MIPI PHYs or HiSPi PHY groups, 0 for default PHY selection
               MI_MEM_CAPTURE_TRIGGER,  //  set a GPIO trigger for cyclic mem capture
               MI_RESET_IMAGE_FIFO,  //  on setMode(), clear image data buffers and FIFOs
               MI_MIDDLE_12,    //  parallel port data is on middle 12 bits (AR0231)

               MI_BITSLIP_PROCEDURE,  //  perform Python kit 'bitslip' procedure

               MI_DESCRAM,      //  FrameBufferDemo Sunrise descrambler
               MI_DESCRAM_RNO,  //  Row Nesting Offset
               MI_DESCRAM_REF_ROWS, //  Reference Rows
               //  Only add symbols to the end
} mi_modes;

//#define MI_SWIZZLE_MODE MI_SW_UNSWIZZLE_MODE //for backwards compatibility
//#define MI_SWIZZLE_DEFAULT MI_SW_UNSWIZZLE_DEFAULT //for backwards compatibility

typedef enum { MI_NO_UNSWIZZLE,
               MI_HW_UNSWIZZLE,
               MI_SW_UNSWIZZLE,
               MI_ANY_UNSWIZZLE,
               //  Only add symbols to the end
} mi_unswizzle_modes;

typedef enum { MI_RX_UNKNOWN = -1,
               MI_RX_DISABLED = 0,
               MI_RX_PARALLEL,
               MI_RX_CCP,
               MI_RX_MIPI,
               MI_RX_HISPI,
               MI_RX_ETHERNET,
               MI_RX_LVDS,
               //  Only add symbols to the end
} mi_rx_types;

typedef enum { MI_RX_HISPI_S,
               MI_RX_HISPI_SPS,
               MI_RX_HISPI_SPP,
               MI_RX_HISPI_DSLR,
               //  Only add symbols to the end
} mi_rx_modes;

typedef enum { MI_PARITY_NONE,
               MI_PARITY_ODD,
               MI_PARITY_EVEN,
               MI_PARITY_MARK,
               MI_PARITY_SPACE,
               //  Only add symbols to the end
} mi_parity_modes;

typedef enum { MI_MEM_CAPTURE_LINEAR,
               MI_MEM_CAPTURE_CIRCULAR,
               //  Only add symbols to the end
} mi_mem_capture_cycles;

typedef enum { MI_MEM_CAPTURE_TRIG_NONE,
               MI_MEM_CAPTURE_TRIG_SIZE,
               MI_MEM_CAPTURE_TRIG_GPIO0_RISE = 16,
               MI_MEM_CAPTURE_TRIG_GPIO1_RISE,
               MI_MEM_CAPTURE_TRIG_GPIO2_RISE,
               MI_MEM_CAPTURE_TRIG_GPIO3_RISE,
               MI_MEM_CAPTURE_TRIG_GPIO4_RISE,
               MI_MEM_CAPTURE_TRIG_GPIO5_RISE,
               MI_MEM_CAPTURE_TRIG_SPARE7_RISE,
               MI_MEM_CAPTURE_TRIG_GPIO7_RISE,
               MI_MEM_CAPTURE_TRIG_GPIO0_FALL,
               MI_MEM_CAPTURE_TRIG_GPIO1_FALL,
               MI_MEM_CAPTURE_TRIG_GPIO2_FALL,
               MI_MEM_CAPTURE_TRIG_GPIO3_FALL,
               MI_MEM_CAPTURE_TRIG_GPIO4_FALL,
               MI_MEM_CAPTURE_TRIG_GPIO5_FALL,
               MI_MEM_CAPTURE_TRIG_SPARE7_FALL,
               MI_MEM_CAPTURE_TRIG_GPIO7_FALL,
} mi_mem_capture_triggers;

//  This is a special value for mode MI_RX_LANES that indicates the
//  MT9H004 'HD' mode that uses 4 lanes out of the 8-lane interface.
#define MI_RX_HISPI_HD_4_LANES      0x10000004

//  HDMI mode numbers from CEA-861
typedef enum { MI_HDMI_OFF      = 0,
               MI_HDMI_480p60   = 3,  //  720x480
               MI_HDMI_720p60   = 4,
               MI_HDMI_1080p60  = 16,
               MI_HDMI_1080p30  = 34,
               MI_HDMI_800x480p60 = 128, // not standardized, may change later
} mi_hdmi_modes;

typedef enum { MI_STEREO_LEFT, // or monocular
               MI_STEREO_RIGHT,
               MI_STEREO_ROWWISE,
               MI_STEREO_BLEND,
               //  Only add symbols to the end
} mi_stereo_modes;

typedef enum { MI_RW_READONLY,
               MI_RW_READWRITE,
               MI_RW_WRITEONLY
               //  Only add symbols to the end
} mi_reg_accessibility;

typedef enum { MI_RC_EXTERNAL,
               MI_RC_CONFIDENTIAL,
               MI_RC_PUBLIC
               //  Only add symbols to the end
} mi_reg_confidentiality;

typedef enum { MI_REG_ADDR,
               MI_MCU_ADDR, //  MCU driver variable (MCU logical addressing)
               MI_SFR_ADDR, //  MCU special function reg (MCU absolute addressing)
               MI_IND_ADDR, //  Indirect access to sensor regs on stereo board
               MI_FAR1_REG_ADDR, //  Registers on 1st sensor on far bus
               MI_FAR1_MCU_ADDR, //  MCU driver variable on 1st sensor on far bus
               MI_FAR1_SFR_ADDR, //  SFR on 1st sensor on far bus
               MI_FAR2_REG_ADDR, //  Registers on 2nd sensor on far bus
               MI_FAR2_MCU_ADDR, //  MCU driver variable on 2nd sensor on far bus
               MI_FAR2_SFR_ADDR, //  SFR on 2nd sensor on far bus
               //  Only add symbols to the end
} mi_addr_type;

typedef enum
{
    MI_UNKNOWN_DATA_TYPE = -1,
    MI_TYPE_HEX = 0,     //  Same as unsigned but conversion to string is hex
    MI_TYPE_UNSIGNED,
    MI_TYPE_UFIXED0   = MI_TYPE_UNSIGNED,
    MI_TYPE_UFIXED1,
    MI_TYPE_UFIXED2,
    MI_TYPE_UFIXED3,
    MI_TYPE_UFIXED4,
    MI_TYPE_UFIXED5,
    MI_TYPE_UFIXED6,
    MI_TYPE_UFIXED7,
    MI_TYPE_UFIXED8,
    MI_TYPE_UFIXED9,
    MI_TYPE_UFIXED10,
    MI_TYPE_UFIXED11,
    MI_TYPE_UFIXED12,
    MI_TYPE_UFIXED13,
    MI_TYPE_UFIXED14,
    MI_TYPE_UFIXED15,
    MI_TYPE_UFIXED16,
    MI_TYPE_UFIXED17,
    MI_TYPE_UFIXED18,
    MI_TYPE_UFIXED19,
    MI_TYPE_UFIXED20,
    MI_TYPE_UFIXED21,
    MI_TYPE_UFIXED22,
    MI_TYPE_UFIXED23,
    MI_TYPE_UFIXED24,
    MI_TYPE_UFIXED25,
    MI_TYPE_UFIXED26,
    MI_TYPE_UFIXED27,
    MI_TYPE_UFIXED28,
    MI_TYPE_UFIXED29,
    MI_TYPE_UFIXED30,
    MI_TYPE_UFIXED31,
    MI_TYPE_UFIXED32,
    MI_TYPE_SIGNED,
    MI_TYPE_FIXED0 = MI_TYPE_SIGNED,
    MI_TYPE_FIXED1,
    MI_TYPE_FIXED2,
    MI_TYPE_FIXED3,
    MI_TYPE_FIXED4,
    MI_TYPE_FIXED5,
    MI_TYPE_FIXED6,
    MI_TYPE_FIXED7,
    MI_TYPE_FIXED8,
    MI_TYPE_FIXED9,
    MI_TYPE_FIXED10,
    MI_TYPE_FIXED11,
    MI_TYPE_FIXED12,
    MI_TYPE_FIXED13,
    MI_TYPE_FIXED14,
    MI_TYPE_FIXED15,
    MI_TYPE_FIXED16,
    MI_TYPE_FIXED17,
    MI_TYPE_FIXED18,
    MI_TYPE_FIXED19,
    MI_TYPE_FIXED20,
    MI_TYPE_FIXED21,
    MI_TYPE_FIXED22,
    MI_TYPE_FIXED23,
    MI_TYPE_FIXED24,
    MI_TYPE_FIXED25,
    MI_TYPE_FIXED26,
    MI_TYPE_FIXED27,
    MI_TYPE_FIXED28,
    MI_TYPE_FIXED29,
    MI_TYPE_FIXED30,
    MI_TYPE_FIXED31,
    MI_TYPE_FIXED32,
    MI_TYPE_SIGNMAG,
    MI_TYPE_FLOAT,
    MI_TYPE_FLOAT16,
    //  Only add symbols to the end
} mi_data_types;


/****************************************************************************
  Type: mi_bitfield_t
 
  This structure is used to hold register-bitfield description information
  parsed from the sensor data files.  

  id                bitfield descriptor name (register unique)
  bitmask           bitmask describing valid bits
  rw                1 if read/write; 0 if readonly
  desc              description 
  detail            additional details describing field
  datatype          Data type of the bitfield
  minimum           Minimum recommended value
  maximum           Maximum recommended value
  confidential      description is confidential
*****************************************************************************/
typedef struct
{
    mi_string       id;
    mi_u32          bitmask;
    mi_s32          rw;
    mi_string       desc;
    mi_string       detail;
    mi_data_types   datatype;
    mi_u32          minimum;
    mi_u32          maximum;
    mi_s32          confidential;
} mi_bitfield_t;

/****************************************************************************
  Type: mi_addr_space_val_t
 
  This structure is used to describe a value for the address space selector.  
  This information is filled in when a sensor data file is parsed

  ID             unique ID used in .sdat file - example "CORE"
  name           name of the address space
  val            value to set the address space selector to, or firmware driver
  type           type of address (simple register, firmware variable, etc.)
  far_base       base address of sensor on far bus
  far_addr_size  register address size of sensor on far bus
  far_data_size  register width of sensor on far bus
*****************************************************************************/
typedef struct
{
    mi_string           ID;
    mi_string           name;
    mi_u32              val;
    mi_addr_type        type;
    mi_u32              far_base;
    mi_s32              far_addr_size;
    mi_s32              far_data_size;
} mi_addr_space_val_t;

/****************************************************************************
  Type: mi_reg_data_t
 
  This structure is used to hold all of the register bitmap information
  stored in the sensor data files.  This structure can hold information
  about an entire register or a particular bitfield of the register.

  unique_desc       unique register descriptor name
  reg_addr          register address
  reg_space         register space of register (core/ip/camctrl/colpipe)
  bitmask           bitmask describing valid bits
  default_val       default value for bitfield
  rw                1 if read/write; 0 if readonly
  reg_desc          description of register
  detail            additional details describing field
  num_bitfields     the number of bitfields known for this register
  bitfield          pointer to array of bitfields
  addr_span         Number of addresses register covers
  addr_space        Pointer to address space structure
  datatype          Data type of the register
  minimum           Minimum recommended value
  maximum           Maximum recommended value
  volat             is volatile (do not cache)
  confidential      description is confidential
*****************************************************************************/
typedef struct _mi_reg_data_t
{
    mi_string            unique_desc;
    mi_u32               reg_addr;
    mi_u32               reg_space; //superseded by addr_space
    mi_u32               bitmask;
    mi_u32               default_val;
    mi_s32               rw;
    mi_string            reg_desc;
    mi_string            detail;
    mi_s32               num_bitfields;
    mi_bitfield_t*       bitfield;
    mi_s32               addr_span;
    mi_addr_space_val_t* addr_space;
    mi_data_types        datatype;
    mi_u32               minimum;
    mi_u32               maximum;
    mi_s32               volat;
    mi_s32               confidential;
} mi_reg_data_t;

/****************************************************************************
  Type: mi_addr_space_t
 
  This structure is used to describe the address space register for this sensor.
  It is filled in when a sensor data file is parsed only for a sensor which has
  an address space selector register.

  regAddr           register address for the address space selector
  num_vals          number of possible address spaces available
  addr_space_val    array of possible address space values
*****************************************************************************/
typedef struct
{
    mi_u32                  reg_addr_read;  // = 0 if there is no selector register
    mi_u32                  reg_addr_write;  // = 0 if there is no selector register
    mi_s32                  num_vals; 
    mi_addr_space_val_t*    addr_space_val;
    mi_u32                  far1_reg_addr;
    mi_u32                  far2_reg_addr;
} mi_addr_space_t;

/****************************************************************************
  Type: mi_long_desc_t
 
  This structure is used to hold the "long description" of a register or
  bitfield.

  regName           register name
  bitfieldName      bitfield name if this is a bitfield
  longDesc          long description, UTF-8
*****************************************************************************/
typedef struct
{
    char *          regName;
    char *          bitfieldName;
    char *          longDesc;
} mi_long_desc_t;

/****************************************************************************
  Type: mi_sensor_t
 
  This structure is used to hold all of the pertinent information about
  a supported sensor.  

  sensorName           Name of the sensor
  sensorType           the sensor type (from mi_sensor_types)
  fullWidth            full sensor image width
  fullHeight           full sensor image height
  width                current sensor image width
  height               current sensor image height
  zoomFactor           the current zoom factor (default 1, may be 2 or 4)
  pixelBytes           number of bytes per pixel
  pixelBits            number of bits per pixel (may not be on a byte boundary)
  bufferSize           minimum size of the raw buffer used to get frame
  imageType            the raw image type (from mi_image_types)
  shipAddr             base SHIP address for sensor
  reg_addr_size        Register address size 8/16
  reg_data_size        Register data size 8/16
  num_regs             number of registers
  regs                 array of registers
  addr_space           address spaces info (or NULL for Bayer Sensor)
  sensorFileName       filename of .sdat file being used
  sensorVersion        version number of sensor (default 1)
  partNumber           Micron MT9 part number
  versionName          name of version of part, for example Rev0
  far1/2_sensor        far sensor info
  long_desc            info from ldat file
  productVariant       string for optional variant name

*****************************************************************************/
typedef struct _mi_sensor_t
{
    mi_string               sensorName; 
    mi_sensor_types         sensorType;
    mi_u32                  fullWidth;
    mi_u32                  fullHeight;
    mi_u32                  width;                
    mi_u32                  height;  
    mi_u32                  zoomFactor;
    mi_u32                  pixelBytes;
    mi_u32                  pixelBits;
    mi_u32                  bufferSize;
    mi_image_types          imageType;
    mi_u32                  shipAddr;
    mi_s32                  reg_addr_size;
    mi_s32                  reg_data_size;
    mi_s32                  num_regs;
    mi_reg_data_t*          regs;
    mi_addr_space_t*        addr_space;
    mi_string               sensorFileName;
    mi_u32                  sensorVersion;
    mi_string               partNumber;
    mi_string               versionName;
    struct _mi_sensor_t *   far1_sensor;
    struct _mi_sensor_t *   far2_sensor;
    mi_s32                  num_long_desc;
    mi_long_desc_t *        long_desc;
    mi_string               productVariant;
} mi_sensor_t;

/****************************************************************************
  Type: mi_chip_t
 
  This structure is used to hold all of the pertinent information about
  a chip.  

  chipName            Name of the companion chip
  baseAddr            base serial address for chip
  serial_addr_size    Register address size 8/16bit
  serial_data_size    Register data size 8/16bit 
  num_regs            number of registers on companion chip
  regs                array of num_regs registers on chip

*****************************************************************************/
typedef struct
{
    mi_string       chipName;
    mi_u32          baseAddr;
    mi_s32          serial_addr_size;
    mi_s32          serial_data_size;
    mi_s32          num_regs;
    mi_reg_data_t*  regs;
    mi_u32          baseAddrs[8];
    mi_s32          num_baseAddrs;
} mi_chip_t;

/****************************************************************************
  Type: mi_frame_data_t
 
  This structure is used to hold all of the information about received for
  a grabFrame.

  frameNumber     -  The frame number of the last frame grabbed 
  bytesRequested  -  The number of bytes requested for the last frame grabbed
  bytesReturned   -  The number of bytes returned for the last frame grabbed      
  numRegsReturned -  The number of register returned with the last frame grabbed      
  regValsReturned -  The registers returned with the last frame grabbed      
  imageBytesReturned - The number of bytes of image data for the last frame grabbed

*****************************************************************************/
typedef struct 
{
    mi_u32      frameNumber;    //  from end packet
    mi_u32      bytesRequested;
    mi_u32      bytesReturned;
    mi_u32      numRegsReturned;
    mi_u32      frameTime;      //  microseconds
    mi_u32      frameLines;
    mi_u32      lineSize;       //  in samples, as seen by the FPGA
    mi_u32      gpio;           //  from DEMO3 end packet
    mi_u32      frameStatus;    //  from DEMO3 end packet
    mi_u32      eolPadBytes;    //  end-of-line pad bytes
    mi_u32      regValsReturned[400 - 6]; // adjust to keep structure size 405 DWORDs
    mi_u32      imageBytesReturned;
} mi_frame_data_t;

/****************************************************************************
  Type: _mi_camera_t
 
  This structure is used to hold all of the pertinent information about
  a camera.  

  productID            - Product ID for the Camera
  productVersion       - Version # of the product
  productName          - Name of product
  firmwareVersion      - Version of the firmware on board
  transportName        - Name of the transport
  transportType        - Type of the transport
  context              - This is the camera specific context 
  sensor               - Sensor that is attached to the device
  num_chips            - Number of companion chips on board
  chip                 - The companion chip data

  The following are function pointers for a specific transport
  startTransport        - Starts a transport
  stopTransport         - Stops a transport
  readSensorRegisters   - read a sequence of sensor registers
  writeSensorRegisters  - write a sequence of sensor register values
  readSensorRegList     - read a non sequential list of sensor registers 
  readRegister          - Reads a register
  writeRegister         - Writes to a register
  readRegisters         - read a sequence of registers
  writeRegisters        - write a sequence of register values
  grabFrame             - Returns an image frame
  getFrameData          - Returns additional information about last grabbed frame
  updateFrameSize       - Set bufferSize given new width, height and bits per clock
  updateBufferSize      - Set bufferSize given new rawBufferSize
  setMode               - Sets one of the mi_modes 
  getMode               - Gets the value of one of the mi_modes
  initTransport         - Used to initialize the transport if .sdat file not used in openCameras
*****************************************************************************/
typedef struct _mi_camera_t
{
    mi_product_ids  productID;
    mi_u32          productVersion;
    mi_string       productName;
    mi_u32          firmwareVersion;
    mi_string       transportName;
    mi_u32          transportType;
    void*           context;
    mi_sensor_t*    sensor;
    mi_s32          num_chips;
    mi_chip_t*      chip;
    void*           int_dev_functions;
    mi_s32      (* startTransport)(struct _mi_camera_t *pCamera);
    mi_s32      (* stopTransport)(struct _mi_camera_t *pCamera);
    mi_s32      (* readSensorRegisters)(struct _mi_camera_t *pCamera, mi_u32 addrSpace, mi_u32 startAddr, mi_u32 numRegs, mi_u32 vals[]);
    mi_s32      (* writeSensorRegisters)(struct _mi_camera_t *pCamera, mi_u32 addrSpace, mi_u32 startAddr, mi_u32 numRegs, mi_u32 vals[]);
    void *          _reserved0;// was mi_s32 (* readSensorRegList)(struct _mi_camera_t *pCamera, mi_u32 numRegs, mi_u32 addrSpaces[], mi_u32 regAddrs[], mi_u32 vals[]);
    mi_s32      (* readRegister)(struct _mi_camera_t *pCamera, mi_u32 shipAddr, mi_u32 regAddr, mi_u32 *val);
    mi_s32      (* writeRegister)(struct _mi_camera_t *pCamera, mi_u32 shipAddr, mi_u32 regAddr, mi_u32 val);
    mi_s32      (* readRegisters)(struct _mi_camera_t *pCamera, mi_u32 shipAddr, mi_u32 startAddr, mi_u32 numRegs, mi_u32 vals[]);
    mi_s32      (* writeRegisters)(struct _mi_camera_t *pCamera, mi_u32 shipAddr, mi_u32 startAddr, mi_u32 numRegs, mi_u32 vals[]);
    mi_s32      (* grabFrame)(struct _mi_camera_t *pCamera, mi_u8 *pInBuffer, mi_u32 bufferSize);
    mi_s32      (* getFrameData)(struct _mi_camera_t *pCamera, mi_frame_data_t *frameData);
    mi_s32      (* updateFrameSize)(struct _mi_camera_t *pCamera, mi_u32 width, mi_u32 height, mi_s32 nBitsPerClock, mi_s32 nClocksPerPixel);
    mi_s32      (* updateBufferSize)(struct _mi_camera_t *pCamera, mi_u32 rawBufferSize);
    mi_s32      (* setMode)(struct _mi_camera_t *pCamera, mi_modes mode, mi_u32 val);
    mi_s32      (* getMode)(struct _mi_camera_t *pCamera, mi_modes mode, mi_u32* val);
    mi_s32      (* initTransport)(struct _mi_camera_t *pCamera, mi_s32 bitsPerClock, mi_s32 clocksPerPixel, mi_s32 polarity, mi_s32 pixelOffset, mi_s32 noFWCalls);
} mi_camera_t;


typedef enum { MI_NOTIFY_PAUSE = 1,
               MI_NOTIFY_RESUME,
               MI_NOTIFY_UPDATEFRAMESIZE,
               MI_NOTIFY_REFRESH,
} mi_notify_codes;

typedef struct
{
    mi_u32          width;
    mi_u32          height;
    mi_s32          bitsPerClock;
    mi_s32          clocksPerPixel;
    mi_image_types  imageType;
} mi_notify_updateframesize_t;

typedef mi_u32 (*MI_NOTIFYCALLBACK)(mi_camera_t *pCamera, mi_notify_codes notifyCode, mi_intptr lParam);
typedef mi_s32 (*MI_INICOMMANDCALLBACK)(mi_camera_t *pCamera, const char *szKey, const char *szValue);

/****************************************************************************
  Type: mi_ccr_header_t
 
  This is the header for CCR files, which are used by the ICP-HD
  calibration tools.
  The values are stored in the file little-endian.
*/
#define MI_CCR_ID               0x0a524343
#define MI_CCR_IT_RGB           0   //  planar; R, G, B
#define MI_CCR_IT_BAYER         1
#define MI_CCR_IT_BAYER_ODDH    2
#define MI_CCR_IT_BAYER_ODDV    4
typedef struct
{
    mi_u32      ccrID;          //  MI_CCR_ID = 0x0a524343 = 'CCR\n'
    mi_u32      ccrVersion;     //  MI_CCR_VERSION = 0x102 or 0x103
    mi_u32      width;
    mi_u32      height;
    mi_u32      imageType;      //  MI_CCR_IT_BAYER for Bayer, plus MI_CCR_IT_BAYER_ODDH/V
    mi_u32      bitsPerPixel;
    double      gamma;          //  1.0
    mi_u32      x;              //  unknown use, set to 0
    mi_u32      y;              //  unknown use, set to 0
    mi_u32      fill[2];        //  unknown use, set to 0
    mi_u32      black[4];       //  per color channel, order unknown
    mi_u32      border[4];      //
    char        comment[256];   //  NUL-terminated string
} mi_ccr_header_t;

/****************************************************************************
  Type: mi_dxr_header_t
 
  This is the header for DXR files, which are used by the DxO Labs tools.
  The values are stored in the file little-endian.
  Bayer data is planar.
*/
#define MI_DXR_ID               0x20525844
#define MI_DXR_IT_RGB           "RGB"   //  planar; R, G, B
#define MI_DXR_IT_BAYER0        "Bayer0"//  planar; Gb B R Gr
#define MI_DXR_IT_BAYER1        "Bayer1"//  planar; B Gb Gr R
#define MI_DXR_IT_BAYER2        "Bayer2"//  planar; R Gr Gb B
#define MI_DXR_IT_BAYER3        "Bayer3"//  planar; Gr R B Gb
#define MI_DXR_IT_MONO          ""      //  one plane
typedef struct DXRImageHeader
{
    mi_u32      ucFourCC;       // 0x20525844 = 'DXR '
    char        ucType[16];     // hint for interpreting the raw data
    mi_u32      uiWidth;        // image width
    mi_u32      uiHeight;       // image height
    mi_u32      uiPrecision;    // precision, i.e. number of bits per sample
    mi_u32      uiIsSigned;     // 0 for unsigned, 1 for signed samples
    mi_u32      uiChannels;     // number of channels
    char        ucPadding[24];  // reserved for future use
} mi_dxr_header_t;

/****************************************************************************
  Exported Functions

  mi_OpenCameras   - used to open all currently available cameras
  mi_OpenCameras2  - Extended version of mi_OpenCameras()
  mi_CloseCameras  - closes all open camera transports
  mi_CloseCameras2 - closes a list of camera transports

  mi_ExistsBitfield     - determine if the registerName and bitfieldName exist in the sensor register list
  mi_ExistsRegister     - determine if the registerName exists in the sensor register list
  mi_FindBitfield       - return a pointer to the bitfield with "bitfieldName" within "registerName" in the sensor register list
  mi_FindRegister       - return a pointer to the register with "registerName" in the sensor register list
  mi_FindRegisterAddr   - return a pointer to the register with regAddr/addrSpace/AddrType in the sensor register list

  mi_GetImageTypeStr    - Given a image type return a string with that name
  mi_InvalidateRegCache - Invalidates the entire register cache
  mi_IsBayer            - returns true if the current image type is a Bayer format
  mi_IsSOC              - returns true if the sensor is considered an SOC sensor
  mi_IsArrayImager      - returns true if the sensor is array imager
  mi_IsFarSpace         - returns true if the address space belongs to attatched sensor

  mi_ParseSensorFile    - parse a given .sdat file into a mi_sensor_t structure
  mi_ParseChipFile      - parse a given .cdat file into a mi_chip_t structure
  mi_LoadINIPreset      - used to load  a preset section from ini file. return MI_INI_SUCCESS: succeed; else error.

  mi_ReadSensorReg      - read the register or variable using pointer to a register
  mi_WriteSensorReg     - writes register or variable using pointer to a register
  mi_ReadSensorRegAddr  - read the register or variable given a regAddr, addrSpace and addrType
  mi_WriteSensorRegAddr - writes register or variable given a regAddr, addrSpace and addrType
  mi_ReadSensorRegStr   - read the register or variable using "register name" and "bitfield name" or NULL
  mi_WriteSensorRegStr  - writes register or variable using "register name" and "bitfield name" or NULL

  *****************************************************************************/
MIDLIB_API const char *     mi_Home();
MIDLIB_API const char *     mi_SensorData();
MIDLIB_API mi_s32           mi_OpenCameras(mi_camera_t* pCameras[MI_MAX_CAMERAS],  mi_s32* nNumCameras, const char *sensor_dir);
MIDLIB_API mi_s32           mi_OpenCameras2(mi_camera_t* pCameras[MI_MAX_CAMERAS],  mi_s32* nNumCameras, const char *sensor_dir, mi_u32 transportType, const char *dllName);
MIDLIB_API mi_s32           mi_GetCameras(mi_camera_t *pCamera[MI_MAX_CAMERAS], mi_s32 *nNumCameras, mi_u32 transportType, const char *dllName);
MIDLIB_API void             mi_CloseCameras();
MIDLIB_API void             mi_CloseCameras2(mi_camera_t* pCameras[], mi_s32 nNumCameras);
MIDLIB_API void             mi_CancelProbe();

MIDLIB_API mi_s32           mi_ExistsBitfield(mi_camera_t* pCamera, const char* pszRegisterName, const char* pszBitfieldName);
MIDLIB_API mi_s32           mi_ExistsRegister(mi_camera_t* pCamera, const char* pszRegisterName);
MIDLIB_API mi_bitfield_t*   mi_FindBitfield(mi_camera_t* pCamera, const char* pszRegisterName, const char* pszBitfieldrName);
MIDLIB_API mi_reg_data_t*   mi_FindRegister(mi_camera_t* pCamera, const char* pszRegisterName);
MIDLIB_API mi_s32           mi_FindChip(mi_camera_t * pCamera, const char *szChipName);
MIDLIB_API mi_s32           mi_ExistsBitfield2(mi_camera_t* pCamera, mi_s32 nChip, const char* pszRegisterName, const char* pszBitfieldName);
MIDLIB_API mi_s32           mi_ExistsRegister2(mi_camera_t* pCamera, mi_s32 nChip, const char* pszRegisterName);
MIDLIB_API mi_bitfield_t*   mi_FindBitfield2(mi_camera_t* pCamera, mi_s32 nChip, const char* pszRegisterName, const char* pszBitfieldrName);
MIDLIB_API mi_reg_data_t*   mi_FindRegister2(mi_camera_t* pCamera, mi_s32 nChip, const char* pszRegisterName);
MIDLIB_API mi_reg_data_t*   mi_FindRegisterAddr(mi_camera_t* pCamera, mi_u32 regAddr, mi_u32 addrSpace, mi_addr_type addrType);
MIDLIB_API mi_reg_data_t*   mi_FindRegisterWildcard(mi_camera_t *pCamera, const char *pszWildcard, int *pnPos);

MIDLIB_API void             mi_InvalidateRegCache(mi_camera_t *pCamera);
MIDLIB_API void             mi_InvalidateRegCacheReg(mi_camera_t *pCamera, mi_reg_data_t *pReg);
MIDLIB_API void             mi_InvalidateRegCacheRegName(mi_camera_t *pCamera, const char *pszRegName);
MIDLIB_API mi_s32           mi_GetRegCacheValueReg(mi_camera_t *pCamera, mi_reg_data_t *pReg, mi_u32 *val);
MIDLIB_API void             mi_SetVolatileRegStr(mi_camera_t *pCamera, const char *pszRegisterName);

MIDLIB_API mi_s32           mi_GetImageTypeStr(mi_image_types image_type, mi_string pImageStr);
MIDLIB_API mi_s32           mi_IsBayerImageType(mi_image_types imageType);
MIDLIB_API mi_s32           mi_IsBayer(mi_camera_t* pCamera);
MIDLIB_API mi_s32           mi_IsYcbcrImageType(mi_image_types imageType);
MIDLIB_API mi_s32           mi_IsJpegImageType(mi_image_types imageType);
MIDLIB_API mi_s32           mi_IsSOC(mi_camera_t* pCamera);
MIDLIB_API mi_s32           mi_IsISPSOC(mi_camera_t* pCamera);
MIDLIB_API mi_s32           mi_IsISP(mi_camera_t* pCamera);
MIDLIB_API mi_s32           mi_IsArrayImager(mi_camera_t* pCamera);
MIDLIB_API mi_s32           mi_IsFarSpace(mi_addr_space_val_t *pSpace);

MIDLIB_API mi_s32           mi_ParseSensorFile(mi_camera_t *pCamera, const char *fileName, mi_sensor_t *sensor_data);
MIDLIB_API mi_s32           mi_RefreshSensorFile(mi_camera_t *pCamera, const char *fileName);
MIDLIB_API mi_s32           mi_ParseFarSensorFile(mi_camera_t *pCamera, const char *far_id, mi_addr_type far_type, mi_u32 far_base, const char *far_sdat, mi_sensor_t *sensor_data);
MIDLIB_API mi_s32           mi_ParseLongDescFile(mi_camera_t *pCamera, const char *fileName);
MIDLIB_API void             mi_DestructSensor(mi_sensor_t *pSensor);
MIDLIB_API mi_s32           mi_ParseChipFile(mi_camera_t *pCamera, const char *fileName, mi_chip_t *chip_data);
MIDLIB_API mi_s32           mi_DetectSensorBaseAddr(mi_camera_t *pCamera);
MIDLIB_API mi_s32           mi_DetectPartNumber(mi_camera_t *pCamera);
MIDLIB_API void             mi_InitCameraContext(mi_camera_t *pCamera);
MIDLIB_API void             mi_FreeCameraContext(mi_camera_t *pCamera);
MIDLIB_API const char *     mi_GetDriverInfoString(mi_camera_t *pCamera);
MIDLIB_API mi_s32			mi_GetShipAddrList(mi_camera_t *pCamera, mi_u32* buffer, mi_u32* count);
MIDLIB_API mi_s32           mi_ProbeFarBus(mi_camera_t *pCamera);

MIDLIB_API mi_s32           mi_LoadINIPreset(mi_camera_t* m_pCamera, const char *pszIniFileName, const char* szPresetName);
MIDLIB_API mi_s32           mi_DefaultIniFile(mi_camera_t *pCamera, char *pszIniFileName, mi_u32 bufferLen);
MIDLIB_API mi_s32           mi_SetIniCommandHandler(mi_camera_t *pCamera, MI_INICOMMANDCALLBACK fnHandler);
MIDLIB_API mi_s32           mi_LoadPromLensCalibration(mi_camera_t* pCamera, mi_u32 nBase, const char * szLabel);
MIDLIB_API mi_s32           mi_LoadPromColorCorrection(mi_camera_t* pCamera, mi_u32 nBase, const char * szLabel);
MIDLIB_API mi_s32           mi_LoadPromScratchpad(mi_camera_t* pCamera, mi_u32 nBase, const char * szLabel, mi_u32 nDest);

MIDLIB_API mi_s32           mi_ReadSensorReg(mi_camera_t* pCamera, mi_reg_data_t* pReg, mi_u32* val);
MIDLIB_API mi_s32           mi_WriteSensorReg(mi_camera_t *pCamera, mi_reg_data_t* pReg, mi_u32 val);
MIDLIB_API mi_s32           mi_ReadSensorRegAddr(mi_camera_t* pCamera, mi_addr_type addrType, mi_u32 addrSpace, mi_u32 addr, mi_s32 is8, mi_u32 *value);
MIDLIB_API mi_s32           mi_WriteSensorRegAddr(mi_camera_t* pCamera, mi_addr_type addrType, mi_u32 addrSpace, mi_u32 addr, mi_s32 is8, mi_u32 value);
MIDLIB_API mi_s32           mi_ReadSensorRegStr(mi_camera_t *pCamera, const char *pszRegisterName, const char *pszBitfieldName, mi_u32 *val);
MIDLIB_API mi_s32           mi_WriteSensorRegStr(mi_camera_t *pCamera, const char *pszRegisterName, const char *pszBitfieldName, mi_u32 val);
MIDLIB_API mi_s32           mi_ReadSensorRegs(mi_camera_t* pCamera, mi_u32 numRegs, mi_reg_data_t* pReg[], mi_u32 vals[], mi_s32 retVals[]);
MIDLIB_API mi_s32           mi_WriteSensorRegs(mi_camera_t *pCamera, mi_u32 numRegs, mi_reg_data_t* pReg[], mi_u32 vals[], mi_s32 retVals[]);
MIDLIB_API mi_s32           mi_ReadSensorRegsAddr(mi_camera_t* pCamera, mi_u32 numRegs, mi_addr_type addrType[], mi_u32 addrSpace[], mi_u32 addr[], mi_s32 is8[], mi_u32 vals[], mi_s32 retVals[]);
MIDLIB_API mi_s32           mi_WriteSensorRegsAddr(mi_camera_t* pCamera, mi_u32 numRegs, mi_addr_type addrType[], mi_u32 addrSpace[], mi_u32 addr[], mi_s32 is8[], mi_u32 vals[], mi_s32 retVals[]);
MIDLIB_API mi_s32           mi_ReadSensorRegsStr(mi_camera_t *pCamera, mi_u32 numRegs, const char *pszRegisterName[], const char *pszBitfieldName[], mi_u32 vals[], mi_s32 retVals[]);
MIDLIB_API mi_s32           mi_WriteSensorRegsStr(mi_camera_t *pCamera, mi_u32 numRegs, const char *pszRegisterName[], const char *pszBitfieldName[], mi_u32 vals[], mi_s32 retVals[]);
MIDLIB_API mi_u32           mi_GetAddrIncrement(mi_camera_t *pCamera, mi_u32 shipAddr, mi_addr_type addrType);
MIDLIB_API mi_u32           mi_GetAddrIncrementDataSize(mi_camera_t *pCamera, mi_u32 shipAddr, mi_addr_type addrType);
MIDLIB_API mi_u32           mi_GetSensorAddrIncrement(mi_camera_t *pCamera, mi_addr_type addrType);
MIDLIB_API int              mi_McuAddrStyle(mi_camera_t *pCamera);
MIDLIB_API int              mi_McuDirectStyle(mi_camera_t *pCamera);
MIDLIB_API mi_s32           mi_GetLongDescription(mi_camera_t *pCamera, const char *pszRegisterName, const char *pszBitfieldName, char *szLongDesc, mi_s32 bufferLen);

MIDLIB_API mi_s32           mi_startTransport(mi_camera_t *pCamera);
MIDLIB_API mi_s32           mi_stopTransport(mi_camera_t *pCamera);
MIDLIB_API mi_s32           mi_readSystemRegisters(mi_camera_t *pCamera, mi_addr_type addrType, mi_u32 addrSpace, mi_u32 startAddr, mi_u32 dataSize, mi_u32 numRegs, mi_u32 vals[]);
MIDLIB_API mi_s32           mi_writeSystemRegisters(mi_camera_t *pCamera, mi_addr_type addrType, mi_u32 addrSpace, mi_u32 startAddr, mi_u32 dataSize, mi_u32 numRegs, mi_u32 vals[]);
MIDLIB_API mi_s32           mi_sendCommand(mi_camera_t *pCamera, mi_u32 command, mi_u32 paramSize, mi_u8 *paramBuffer, mi_u32 *statusCode, mi_u32 resultSize, mi_u8 *resultBuffer, mi_u32 *bytesReturned);
MIDLIB_API mi_s32           mi_readSensorRegisters(mi_camera_t *pCamera, mi_u32 addrSpace, mi_u32 startAddr, mi_u32 numRegs, mi_u32 vals[]);
MIDLIB_API mi_s32           mi_writeSensorRegisters(mi_camera_t *pCamera, mi_u32 addrSpace, mi_u32 startAddr, mi_u32 numRegs, mi_u32 vals[]);
MIDLIB_API mi_s32           mi_readRegister(mi_camera_t *pCamera, mi_u32 shipAddr, mi_u32 regAddr, mi_u32 *val);
MIDLIB_API mi_s32           mi_writeRegister(mi_camera_t *pCamera, mi_u32 shipAddr, mi_u32 regAddr, mi_u32 val);
MIDLIB_API mi_s32           mi_readRegisters(mi_camera_t *pCamera, mi_u32 shipAddr, mi_u32 startAddr, mi_u32 numRegs, mi_u32 vals[]);
MIDLIB_API mi_s32           mi_writeRegisters(mi_camera_t *pCamera, mi_u32 shipAddr, mi_u32 startAddr, mi_u32 numRegs, mi_u32 vals[]);
MIDLIB_API mi_s32           mi_grabFrame(mi_camera_t *pCamera, mi_u8 *pInBuffer, mi_u32 bufferSize);
MIDLIB_API mi_s32           mi_getFrameData(mi_camera_t *pCamera, mi_frame_data_t *frameData);
MIDLIB_API mi_s32           mi_updateFrameSize(mi_camera_t *pCamera, mi_u32 width, mi_u32 height, mi_s32 bitsPerClock, mi_s32 clocksPerPixel);
MIDLIB_API mi_s32           mi_updateBufferSize(mi_camera_t *pCamera, mi_u32 rawBufferSize);
MIDLIB_API mi_s32           mi_setMode(mi_camera_t *pCamera, mi_modes mode, mi_u32 val);
MIDLIB_API mi_s32           mi_getMode(mi_camera_t *pCamera, mi_modes mode, mi_u32* val);
MIDLIB_API mi_s32           mi_initTransport(mi_camera_t *pCamera, mi_s32 bitsPerClock, mi_s32 clocksPerPixel, mi_s32 polarity, mi_s32 pixelOffset, mi_s32 noFWCalls);

MIDLIB_API const char *     mi_GetDataTypeString(mi_data_types dataType);
MIDLIB_API mi_data_types    mi_DataTypeFromString(const char *pszStr);
MIDLIB_API mi_s32           mi_Int2Str(mi_data_types dataType, mi_u32 val, mi_u32 mask, char *pszStr, mi_s32 bufferLen);
MIDLIB_API double           mi_Int2Double(mi_data_types dataType, mi_u32 val, mi_u32 mask);
MIDLIB_API mi_u32           mi_Str2Int(mi_data_types dataType, const char *pszStr, mi_u32 mask);
MIDLIB_API double           mi_Str2Double(mi_data_types dataType, const char *pszStr, mi_u32 mask);
MIDLIB_API mi_u32           mi_Double2Int(mi_data_types dataType, double d, mi_u32 mask);
MIDLIB_API mi_s32           mi_Double2Str(mi_data_types dataType, double d, mi_u32 mask, char *pszStr, mi_s32 bufferLen);
MIDLIB_API double           mi_DataTypeMinimum(mi_data_types dataType, mi_u32 mask);
MIDLIB_API double           mi_DataTypeMaximum(mi_data_types dataType, mi_u32 mask);
MIDLIB_API double           mi_DataTypeStep(mi_data_types dataType, mi_u32 mask);

MIDLIB_API void             mi_OpenErrorLog(mi_u32 error_log_level, const char * baseFileName);
MIDLIB_API mi_s32           mi_Errorlog(mi_s32 errorcode, mi_u32 errorlevel, const char *logMsg, const char *szSource, const char *szFunc, mi_u32 nLine);
MIDLIB_API void             mi_Msglog(mi_u32 logType, const char *logMsg, const char *szSource, const char *szFunc, mi_u32 nLine);
MIDLIB_API void             mi_CloseErrorLog();
MIDLIB_API mi_s32           mi_GetErrorLogFileName(mi_string fileName);
MIDLIB_API mi_u32           mi_GetErrorLogLevel();
MIDLIB_API mi_s32           mi_SetErrorLogLevel(mi_u32 logLevel);


// Removal notification is available for Windowing applications only since
//  we need to hook the Windows Message queue.
#ifdef _WINDOWS
/****************************************************************************
   type: MIDEVCALLBACK

 Used for device removal/arrival notification
 NOTE: If the following definitions cause compiler errors in your application,
   do one of the following:
   1) Remove the _WINDOWS from your project properties (C/C++ section,
      Preprocessor Definitions, remove the '_WINDOWS' directive).
   2) -OR- Comment out this section (change the '#ifdef _WINDOWS' above to
   '#if 0' or comment out each of the following lines that contain HWND).
*****************************************************************************/
typedef mi_u32 (*MIDEVCALLBACK)(HWND hWnd, mi_camera_t *pCamera, mi_u32 Flags);
/****************************************************************************
  Exported Functions
  mi_SetDeviceChangeCallback - enable/disable device removal/arrival notification
*****************************************************************************/
MIDLIB_API mi_s32 mi_SetDeviceChangeCallback(HWND hWnd, MIDEVCALLBACK lpDCB);
#endif //_WINDOWS

#ifdef __cplusplus
}
#endif

#endif //__MIDLIB2_H__ ]
