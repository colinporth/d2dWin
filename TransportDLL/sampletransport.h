//*********************************************************************************************
// Copyright 2014 ON Semiconductor.  This software and/or documentation is provided by
// ON Semiconductor under limited terms and conditions.
// The terms and conditions pertaining to the software and/or documentation are available at
// www.onsemi.com (“ON Semiconductor Standard Terms and Conditions of Sale, Section 8 Software”).
// Do not use this software and/or documentation unless you have carefully read and you agree
// to the limited terms and conditions.
// By using this software and/or documentation, you agree to the limited terms and conditions.
//*********************************************************************************************
// The following ifdef block is the standard way of creating macros which make exporting
// from a DLL simpler. All files within this DLL are compiled with the SAMPLETRANSPORT_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see
// SAMPLETRANSPORT_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef WIN32
    #ifdef TRANSPORT_EXPORTS
        #define TRANSPORT_API __declspec(dllexport)
    #else
        #define TRANSPORT_API __declspec(dllimport)
    #endif
#else
    #define TRANSPORT_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

TRANSPORT_API mi_s32 SetHelpersDLL(mi_transport_helpers_t *pHelpers);
TRANSPORT_API mi_s32 OpenCameraDLL(mi_camera_t *pCamera, const char *sensor_dir_file, mi_s32 deviceIndex);
TRANSPORT_API mi_s32 CloseCameraDLL(mi_camera_t *pCamera);
TRANSPORT_API mi_s32 StartTransportDLL(mi_camera_t *pCamera);
TRANSPORT_API mi_s32 StopTransportDLL(mi_camera_t *pCamera);
TRANSPORT_API mi_s32 ReadSensorRegistersDLL(mi_camera_t *pCamera, mi_u32 addrSpace,
                                mi_u32 startAddr, mi_u32 numRegs, mi_u32 vals[]);
TRANSPORT_API mi_s32 WriteSensorRegistersDLL(mi_camera_t *pCamera, mi_u32 addrSpace,
                                mi_u32 startAddr, mi_u32 numRegs, mi_u32 vals[]);
TRANSPORT_API mi_s32 ReadRegisterDLL(mi_camera_t *pCamera, mi_u32 shipAddr, mi_u32 regAddr, mi_u32 *val);
TRANSPORT_API mi_s32 WriteRegisterDLL(mi_camera_t *pCamera, mi_u32 shipAddr, mi_u32 regAddr, mi_u32 val);
TRANSPORT_API mi_s32 ReadRegistersDLL(struct _mi_camera_t *pCamera, mi_u32 shipAddr,
                                mi_u32 startAddr, mi_u32 numRegs, mi_u32 vals[]);
TRANSPORT_API mi_s32 WriteRegistersDLL(struct _mi_camera_t *pCamera, mi_u32 shipAddr,
                                mi_u32 startAddr, mi_u32 numRegs, mi_u32 vals[]);
TRANSPORT_API mi_s32 GrabFrameDLL(mi_camera_t *pCamera, mi_u8 *pInBuffer, mi_u32 bufferSize);
TRANSPORT_API mi_s32 UpdateFrameSizeDLL(mi_camera_t *pCamera, mi_u32 nWidth, mi_u32 nHeight,
                                mi_s32 nBitsPerClock, mi_s32 nClocksPerPixel);
TRANSPORT_API mi_s32 UpdateBufferSizeDLL(mi_camera_t *pCamera, mi_u32 rawBufferSize);
TRANSPORT_API mi_s32 SetModeDLL(mi_camera_t *pCamera, mi_modes mode, mi_u32 val);
TRANSPORT_API mi_s32 GetModeDLL(mi_camera_t *pCamera, mi_modes mode, mi_u32* val);
TRANSPORT_API const char * GetDeviceNameDLL(mi_camera_t *pCamera);
TRANSPORT_API mi_u32 SetNotifyCallbackDLL(mi_camera_t *pCamera, MI_NOTIFYCALLBACK pNotifyCallback);
TRANSPORT_API mi_s32 EnableDeviceNotificationDLL(mi_camera_t* pCamera, mi_intptr hWnd);
TRANSPORT_API mi_s32 MatchDevChangeMsgDLL(mi_camera_t *pCamera, DWORD evtype, void *pDBH);
TRANSPORT_API mi_s32 DisableDeviceNotificationDLL(mi_camera_t *pCamera);
TRANSPORT_API mi_s32 ReadFarRegistersDLL(mi_camera_t *pCamera, mi_u32 shipAddr, mi_u32 startAddr, mi_u32 addrSize, mi_u32 dataSize, mi_u32 numRegs, mi_u32 vals[]);
TRANSPORT_API mi_s32 WriteFarRegistersDLL(mi_camera_t *pCamera, mi_u32 shipAddr, mi_u32 startAddr, mi_u32 addrSize, mi_u32 dataSize, mi_u32 numRegs, mi_u32 vals[]);

TRANSPORT_API mi_s32 GetFrameDataDLL (mi_camera_t* pCamera, mi_frame_data_t* pFrameData);

#ifdef __cplusplus
};
#endif
