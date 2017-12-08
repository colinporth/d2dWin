//*********************************************************************************************
// Copyright 2014 ON Semiconductor.  This software and/or documentation is provided by 
// ON Semiconductor under limited terms and conditions.  
// The terms and conditions pertaining to the software and/or documentation are available at 
// www.onsemi.com (“ON Semiconductor Standard Terms and Conditions of Sale, Section 8 Software”).  
// Do not use this software and/or documentation unless you have carefully read and you agree 
// to the limited terms and conditions.  
// By using this software and/or documentation, you agree to the limited terms and conditions. 
//*********************************************************************************************


#ifndef __MIDLIB2_TRANS_H__   // [
#define __MIDLIB2_TRANS_H__


#ifdef __cplusplus
extern "C" {
#endif

    
typedef enum {MI_CONTEXT_BITS_PER_CLOCK = 1, // deprecated: use helper->setMode(...MI_BITS_PER_CLOCK...)
              MI_CONTEXT_CLOCKS_PER_PIXEL, // deprecated: use helper->setMode(...MI_CLOCKS_PER_PIXEL...)
              MI_CONTEXT_PIXCLK_POLARITY, // deprecated: use helper->setMode(...MI_PIXCLK_POLARITY...)
              MI_CONTEXT_PRIVATE_DATA,
              MI_CONTEXT_APP_HWND,
              MI_CONTEXT_DRIVER_INFO,
} mi_context_fields;

typedef struct
{
    mi_s32      (*getSensor)(mi_camera_t *pCamera, const char *sensor_dir_file);
    void        (*updateImageType)(mi_camera_t *pCamera);
    mi_s32      (*errorLog)(mi_s32 errorCode, mi_u32 errorLevel, const char *logMsg, const char *szSource, const char *szFunc, mi_u32 nLine);
    void        (*log)(mi_u32 logType, const char *logMsg, const char *szSource, const char *szFunc, mi_u32 nLine);
    mi_s32      (*setMode)(mi_camera_t *pCamera, mi_modes mode, mi_u32 val);
    mi_s32      (*getMode)(mi_camera_t *pCamera, mi_modes mode, mi_u32 *val);
    mi_s32      (*setContext)(mi_camera_t *pCamera, mi_context_fields field, mi_intptr val);
    mi_s32      (*getContext)(mi_camera_t *pCamera, mi_context_fields field, mi_intptr *val);
    void        (*updateFrameSize)(mi_camera_t *pCamera, mi_u32 nWidth, mi_u32 nHeight, mi_s32 nBitsPerClock, mi_s32 nClocksPerPixel);
    void        (*unswizzleBuffer)(mi_camera_t* pCamera, mi_u8 *pInBuffer);
    void        (*mergeDeepBayer)(mi_camera_t* pCamera, mi_u8 *pInBuffer);
    mi_u32      (*readReg)(mi_camera_t *pCamera, const char *szRegister, const char *szBitfield, mi_s32 bCached);
    mi_s32      (*getBoardConfig)(mi_camera_t *pCamera, const char *sensor_dir_file);
    void (*_reserved5)(void);
    void (*_reserved6)(void);
    void (*_reserved7)(void);
}  mi_transport_helpers_t;


#ifdef __cplusplus
}
#endif

#endif //__MIDLIB2_TRANS_H__ ]
