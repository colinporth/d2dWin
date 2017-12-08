// usbUtils.h
#pragma once
//{{{  includes
#include "../inc/CyAPI/CyAPI.h"

#pragma comment (lib,"setupapi.lib")
#pragma comment (lib,"legacy_stdio_definitions.lib")
#pragma comment (lib,"CyAPI.lib")
//}}}

// vars
static CCyUSBDevice* USBDevice = NULL;
static HANDLE Handle = 0;

static BYTE outEpAddress = 0x0;
static BYTE inEpAddress = 0x0;

static CCyControlEndPoint* controlEndPt = NULL;
static CCyBulkEndPoint* epBulkOut = NULL;
static CCyBulkEndPoint* epBulkIn = NULL;

//{{{
void initUSB() {

  USBDevice = new CCyUSBDevice (Handle, CYUSBDRV_GUID, true);
  if (USBDevice) {
    int deviceCount = USBDevice->DeviceCount();
    printf ("USBDevices:%d\n", deviceCount);

    for (auto device = 0; device < deviceCount; device++) {
      USBDevice->Open (device);
      printf ("vid:%04x pid:%04x  AltIntfcCount:%d EndPointCount:%d - %s\n",
              USBDevice->VendorID, USBDevice->ProductID,
              USBDevice->AltIntfcCount(), USBDevice->EndPointCount(), USBDevice->FriendlyName);

      controlEndPt = USBDevice->ControlEndPt;
      printf ("Add:%02x - controlEndpoint - att:%x bIn:%x\n",
              controlEndPt->Address, controlEndPt->Attributes, controlEndPt->bIn);

      for (auto altInterface = 0; altInterface < USBDevice->AltIntfcCount()+1; altInterface++) {
        USBDevice->SetAltIntfc (altInterface);
        int endPointCount = USBDevice->EndPointCount();
        for (int endPoint = 1; endPoint < endPointCount; endPoint++) {
          CCyUSBEndPoint* cyUSBendPoint = USBDevice->EndPoints[endPoint];

          printf ("Add:%02x - altInterface:%d - endpoint:%d - att:%x bIn:%x\n",
                  cyUSBendPoint->Address, altInterface, endPoint, cyUSBendPoint->Attributes, cyUSBendPoint->bIn);
          if (cyUSBendPoint->bIn) {
            inEpAddress = cyUSBendPoint->Address;
            epBulkIn = (CCyBulkEndPoint*)USBDevice->EndPointOf (inEpAddress);
            epBulkIn->TimeOut = 500;
            printf ("- in - MaxPktSize:%d\n", epBulkIn->MaxPktSize );
            }
          else
            outEpAddress = cyUSBendPoint->Address;
          }
        }
      }
    }
  else
    printf ("no USBDevice\n");
  }
//}}}
//{{{
void closeUSB() {

  USBDevice->Close();
  delete USBDevice;
  }
//}}}
//{{{
CCyBulkEndPoint* getBulkEndPoint() {

  return epBulkIn;
  }
//}}}

//{{{
void writeReg (WORD addr, WORD data) {

  //printf ("writeReg (%02x, %04x);\n", addr, data);

  controlEndPt->Target = TGT_DEVICE;
  controlEndPt->ReqType = REQ_VENDOR;
  controlEndPt->Direction = DIR_TO_DEVICE;
  controlEndPt->ReqCode = 0xAE;
  controlEndPt->Value = addr;
  controlEndPt->Index = 0;

  BYTE buf[2];
  buf[0] = data >> 8;
  buf[1] = data & 0xFF;
  LONG buflen = 2;
  controlEndPt->XferData (buf, buflen);
  }
//}}}
//{{{
WORD readReg (WORD addr) {

  controlEndPt->Target = TGT_DEVICE;
  controlEndPt->ReqType = REQ_VENDOR;
  controlEndPt->Direction = DIR_FROM_DEVICE;
  controlEndPt->ReqCode = 0xAD;
  controlEndPt->Value = addr;
  controlEndPt->Index = 0;

  BYTE buf[2];
  LONG buflen = 2;
  controlEndPt->XferData (buf, buflen);
  WORD value = (buf[0] << 8) | buf[1];

  //printf ("%04x = readReg (%02x);\n", value, addr);

  return value;
  }
//}}}

//{{{
void startStreaming() {

  controlEndPt->Target = TGT_DEVICE;
  controlEndPt->ReqType = REQ_VENDOR;
  controlEndPt->Direction = DIR_TO_DEVICE;
  controlEndPt->ReqCode = 0xAF;
  controlEndPt->Value = 0x123;
  controlEndPt->Index = 0;

  BYTE buf[2];
  buf[0] = 0x12;
  buf[1] = 0x34;
  LONG buflen = 2;
  controlEndPt->XferData(buf, buflen);
  }
//}}}
//{{{
void sendFocus (int value) {

  controlEndPt->Target = TGT_DEVICE;
  controlEndPt->ReqType = REQ_VENDOR;
  controlEndPt->Direction = DIR_TO_DEVICE;
  controlEndPt->ReqCode = 0xAC;
  controlEndPt->Value = value;
  controlEndPt->Index = 0;

  BYTE buf[2];
  buf[0] = 0x12;
  buf[1] = 0x34;
  LONG buflen = 2;
  controlEndPt->XferData (buf, buflen);
  }
//}}}
