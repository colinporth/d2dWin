// sampletransport.cpp
//{{{  includes
#include "pch.h"

#include <stdio.h>
#include <stdlib.h>
#include "midlib2.h"
#include "midlib2_trans.h"

#include <wtypes.h>
#include "../common/usbUtils.h"

#include "sampletransport.h"
//}}}
//{{{  defines
//{{{
typedef struct deviceContext {
  void* handle;
  } DeviceContext;
//}}}
#define ERRORLOG(e,m) HelperFn->errorLog(e,-1,m,__FILE__,__FUNCTION__,__LINE__)
#define MSGLOG(t,m) HelperFn->log(t,m,__FILE__,__FUNCTION__,__LINE__)
//}}}
//{{{  vars
mi_transport_helpers_t *HelperFn;

#define QUEUESIZE 64
int timeout = 2000;

bool restart = false;

BYTE* samples = NULL;
BYTE* maxSamplePtr = NULL;
int samplesLoaded = 0;
size_t maxSamples = (16384-16) * 0x10000;

// frames
int frames = 0;
BYTE* framePtr = NULL;
mi_u32 frameBytes = 0;
mi_u32 grabbedFrameBytes = 0;
BYTE* lastFramePtr = NULL;
//}}}

//{{{
void incSamplesLoaded (int packetLen) {

  samplesLoaded += packetLen;
  }
//}}}
//{{{
BYTE* nextPacket (BYTE* samplePtr, int packetLen) {
// return nextPacket start address, wraparound if no room for another packetLen packet

  if (samplePtr + packetLen + packetLen <= maxSamplePtr)
    return samplePtr + packetLen;
  else
    return samples;
  }
//}}}
//{{{
DWORD WINAPI loaderThread (LPVOID arg) {

  BYTE* samplePtr = samples;
  int packetLen = getBulkEndPoint()->MaxPktSize - 16;

  BYTE* bufferPtr[QUEUESIZE];
  BYTE* contexts[QUEUESIZE];
  OVERLAPPED overLapped[QUEUESIZE];
  for (int xferIndex = 0; xferIndex < QUEUESIZE; xferIndex++) {
    //{{{  setup QUEUESIZE transfers
    bufferPtr[xferIndex] = samplePtr;
    samplePtr = nextPacket (samplePtr, packetLen);
    overLapped[xferIndex].hEvent = CreateEvent (NULL, false, false, NULL);
    contexts[xferIndex] = getBulkEndPoint()->BeginDataXfer (bufferPtr[xferIndex], packetLen, &overLapped[xferIndex]);
    if (getBulkEndPoint()->NtStatus || getBulkEndPoint()->UsbdStatus) {
      //{{{  error
      printf ("BeginDataXfer init failed %d %d\n", getBulkEndPoint()->NtStatus, getBulkEndPoint()->UsbdStatus);
      return 0;
      }
      //}}}
    }
    //}}}

  startStreaming();

  BYTE* frameLoadingPtr = samples;
  int xferIndex = 0;
  while (true) {
    if (!getBulkEndPoint()->WaitForXfer (&overLapped[xferIndex], timeout)) {
      printf (" - timeOut %d\n", getBulkEndPoint()->LastError);
      getBulkEndPoint()->Abort();
      if (getBulkEndPoint()->LastError == ERROR_IO_PENDING)
        WaitForSingleObject (overLapped[xferIndex].hEvent, 2000);
      }

    long rxLen = packetLen;
    if (getBulkEndPoint()->FinishDataXfer (bufferPtr[xferIndex], rxLen, &overLapped[xferIndex], contexts[xferIndex])) {
      incSamplesLoaded (packetLen);
      if (rxLen < packetLen) {
        // short packet =  endOfFrame
        framePtr = frameLoadingPtr;
        frameBytes = (bufferPtr[xferIndex] + rxLen) - frameLoadingPtr;
        frameLoadingPtr = nextPacket (bufferPtr[xferIndex], packetLen);
        frames++;
        }

      bufferPtr[xferIndex] = samplePtr;
      samplePtr = nextPacket (samplePtr, packetLen);
      contexts[xferIndex] = getBulkEndPoint()->BeginDataXfer (bufferPtr[xferIndex], packetLen, &overLapped[xferIndex]);
      if (getBulkEndPoint()->NtStatus || getBulkEndPoint()->UsbdStatus) {
        //{{{  error
        printf ("BeginDataXfer init failed %d %d\n", getBulkEndPoint()->NtStatus, getBulkEndPoint()->UsbdStatus);
        return 0;
        }
        //}}}
      }
    else {
      //{{{  error
      printf ("FinishDataXfer init failed\n");
      return 0;
      }
      //}}}

    xferIndex = xferIndex & (QUEUESIZE-1);
    }

  return 0;
  }
//}}}

//{{{
TRANSPORT_API mi_s32 SetHelpersDLL (mi_transport_helpers_t* pHelpers) {

  HelperFn = pHelpers;
  return MI_CAMERA_SUCCESS;
  }
//}}}

//{{{
TRANSPORT_API mi_s32 OpenCameraDLL (mi_camera_t* pCamera, const char* sensor_dir_file, mi_s32 deviceIndex) {

  MSGLOG(MI_LOG, "OpenCameraDLL");

  mi_s32 retVal = MI_CAMERA_SUCCESS;

  if (deviceIndex > 0)//  support only 1 device in this example
    return MI_CAMERA_ERROR;

  DeviceContext *context = new DeviceContext();
  context->handle = NULL;  //  Fill in with your driver or API data
  HelperFn->setContext(pCamera, MI_CONTEXT_PRIVATE_DATA, (mi_intptr)(context));

  pCamera->productID       = MI_UNKNOWN_PRODUCT;
  pCamera->productVersion  = 0;
  pCamera->firmwareVersion = 0;
  pCamera->num_chips       = 0;
  strcpy(pCamera->productName,   "MyCamera");
  strcpy(pCamera->transportName, "mydriver");

  char filename[256];
  sprintf (filename, "%s\\sensor_data\\MT9D111-REV5.xsdat", getenv("MI_HOME"));
  //sprintf (filename, "%s\\sensor_data\\MT9D112-REV3.xsdat", getenv("MI_HOME"));

  retVal = HelperFn->getSensor(pCamera, filename);
  if (retVal != MI_CAMERA_SUCCESS)
   return ERRORLOG(retVal, "Couldn't open MI-sample.sdat");

  return MI_CAMERA_SUCCESS;
  }
//}}}
//{{{
TRANSPORT_API mi_s32 CloseCameraDLL (mi_camera_t* pCamera) {

  MSGLOG(MI_LOG, "CloseCameraDLL");

  DeviceContext* context = NULL;
  HelperFn->getContext (pCamera, MI_CONTEXT_PRIVATE_DATA, (mi_intptr*)&context);

  delete context;

  return MI_CAMERA_SUCCESS;
  }
//}}}

//{{{
TRANSPORT_API mi_s32 StartTransportDLL (mi_camera_t* pCamera) {

  MSGLOG(MI_LOG, "StartTransportDLL");

  DeviceContext *context = NULL;
  HelperFn->getContext(pCamera, MI_CONTEXT_PRIVATE_DATA, (mi_intptr *)&context);



  initUSB();

  samples = (BYTE*)malloc (maxSamples);
  maxSamplePtr = samples + maxSamples;
  HANDLE hLoaderThread = CreateThread (NULL, 0, loaderThread, NULL, 0, NULL);
  SetThreadPriority (hLoaderThread, THREAD_PRIORITY_ABOVE_NORMAL);



  return MI_CAMERA_SUCCESS;
  }
//}}}
//{{{
TRANSPORT_API mi_s32 StopTransportDLL (mi_camera_t* pCamera) {

  MSGLOG(MI_LOG, "StopTransportDLL");

  DeviceContext *context = NULL;
  HelperFn->getContext(pCamera, MI_CONTEXT_PRIVATE_DATA, (mi_intptr *)&context);

  closeUSB();

  return MI_CAMERA_SUCCESS;
  }
//}}}

//{{{
TRANSPORT_API mi_s32 GetModeDLL (mi_camera_t* pCamera, mi_modes mode, mi_u32* val) {

  DeviceContext *context = NULL;
  HelperFn->getContext(pCamera, MI_CONTEXT_PRIVATE_DATA, (mi_intptr *)&context);

  switch (mode) {
    case MI_SIMUL_REG_FRAMEGRAB:
      *val = 1;
      return MI_CAMERA_SUCCESS;
    }

  return HelperFn->getMode(pCamera, mode, val);
  }
//}}}
//{{{
TRANSPORT_API mi_s32 SetModeDLL (mi_camera_t* pCamera, mi_modes mode, mi_u32 val) {

  DeviceContext *context = NULL;
  HelperFn->getContext(pCamera, MI_CONTEXT_PRIVATE_DATA, (mi_intptr *)&context);

  return HelperFn->setMode(pCamera, mode, val);
  }
//}}}
//{{{
TRANSPORT_API mi_s32 GrabFrameDLL (mi_camera_t* pCamera, mi_u8* pInBuffer, mi_u32 bufferSize) {

  DeviceContext* context = NULL;
  HelperFn->getContext (pCamera, MI_CONTEXT_PRIVATE_DATA, (mi_intptr*)&context);

  if (framePtr) {
    // copy framePtr to pInBuffer bufferSize bytes
    grabbedFrameBytes = frameBytes;
    if ((framePtr + bufferSize) <= maxSamplePtr)
      memcpy (pInBuffer, framePtr, bufferSize);
    else {
      int firstChunk = maxSamplePtr - framePtr;
      memcpy (pInBuffer, framePtr, firstChunk);
      memcpy (pInBuffer + firstChunk, samples, bufferSize - firstChunk);
      }
    }
  else // not ready yet but return anyway
    memcpy (pInBuffer, samples, bufferSize);

  Sleep (30); // about 60hz

  //char msg[255];
  //sprintf (msg, "GrabFrameDLL %d %d %d", (int)bufferSize, framePtr - samples, frame++);
  //MSGLOG(MI_LOG, msg);

  return MI_CAMERA_SUCCESS;
  }
//}}}
//{{{
TRANSPORT_API mi_s32 GetFrameDataDLL (mi_camera_t* pCamera, mi_frame_data_t* pFrameData) {

  DeviceContext* context = NULL;
  HelperFn->getContext (pCamera, MI_CONTEXT_PRIVATE_DATA, (mi_intptr*)&context);

  char msg[255];
  sprintf (msg, "GetFrameDataDLL %d", grabbedFrameBytes);
  MSGLOG(MI_LOG, msg);
  pFrameData->imageBytesReturned = grabbedFrameBytes;

  return MI_CAMERA_SUCCESS;
  }
//}}}

//{{{
TRANSPORT_API mi_s32 ReadRegistersDLL (mi_camera_t* pCamera, mi_u32 shipAddr,
                                       mi_u32 startAddr, mi_u32 numRegs, mi_u32 vals[]) {

  DeviceContext* context;
  HelperFn->getContext (pCamera, MI_CONTEXT_PRIVATE_DATA, (mi_intptr*)&context);

  mi_u32 addr_size;
  mi_u32 data_size;
  HelperFn->getMode (pCamera, MI_REG_ADDR_SIZE, &addr_size);
  HelperFn->getMode (pCamera, MI_REG_DATA_SIZE, &data_size);

  if (numRegs != 1) {
    char msg[255];
    sprintf (msg, "ReadRegistersDLL %d %d - i2c:%x addr:%x numRegs:%x",
             addr_size, data_size, shipAddr, startAddr, numRegs);
    MSGLOG(MI_LOG, msg);
    }
  for (mi_u32 i = 0; i < numRegs; i++)
    vals[i] = readReg (startAddr+i);

  return MI_CAMERA_SUCCESS;
  }
//}}}
//{{{
TRANSPORT_API mi_s32 WriteRegistersDLL (mi_camera_t* pCamera, mi_u32 shipAddr,
                                        mi_u32 startAddr, mi_u32 numRegs, mi_u32 vals[]) {

  DeviceContext* context;
  HelperFn->getContext (pCamera, MI_CONTEXT_PRIVATE_DATA, (mi_intptr*)&context);

  mi_u32 addr_size;
  mi_u32 data_size;
  HelperFn->getMode (pCamera, MI_REG_ADDR_SIZE, &addr_size);
  HelperFn->getMode (pCamera, MI_REG_DATA_SIZE, &data_size);

  //  do I2C writes
  if (numRegs != 1) {
    char msg[255];
    sprintf (msg, "WriteRegistersDLL %d %d - i2c:%x addr:%x num:%x - firstValue:%x",
             addr_size, data_size, shipAddr, startAddr, numRegs, vals[0]);
    MSGLOG(MI_LOG, msg);
    }
  writeReg (startAddr, vals[0]);

  return MI_CAMERA_SUCCESS;
  }
//}}}

//{{{
TRANSPORT_API mi_s32 UpdateFrameSizeDLL (mi_camera_t* pCamera, mi_u32 nWidth, mi_u32 nHeight,
                                         mi_s32 nBitsPerClock, mi_s32 nClocksPerPixel) {

  char msg[255];
  sprintf (msg, "UpdateFrameSizeDLL w:%d h:%d bpc:%d cpp:%d", nWidth, nHeight, nBitsPerClock, nClocksPerPixel);
  MSGLOG(MI_LOG, msg);

  DeviceContext* context;
  HelperFn->getContext (pCamera, MI_CONTEXT_PRIVATE_DATA, (mi_intptr*)&context);

  HelperFn->updateFrameSize (pCamera, nWidth, nHeight, nBitsPerClock, nClocksPerPixel);
  HelperFn->updateImageType (pCamera);

  return MI_CAMERA_SUCCESS;
  }
//}}}

//{{{
BOOL APIENTRY DllMain (HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved ) {

  switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
      break;
    }

  return TRUE;
  }
//}}}
