//{{{
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved
//
//}}}
#pragma once
//{{{  includes
#include <MMDeviceAPI.h>
#include <AudioClient.h>
#include <AudioPolicy.h>
//}}}

//{{{
struct RenderBuffer {
  RenderBuffer*  _Next;
  UINT32 _BufferLength;
  BYTE* _Buffer;

  RenderBuffer() : _Next(NULL), _BufferLength(0), _Buffer(NULL) { }
  ~RenderBuffer() {
    // delete [] _Buffer;
    }

  };
//}}}

class CWASAPIRenderer : IMMNotificationClient, IAudioSessionEvents {
public:
  enum RenderSampleType { SampleTypeFloat, SampleType16BitPCM, };

  CWASAPIRenderer (IMMDevice* Endpoint, bool EnableStreamSwitch, ERole EndpointRole);
  bool Initialize (UINT32 EngineLatency);
  void Shutdown();

  bool Start (RenderBuffer *RenderBufferQueue);
  void Stop();

  WORD getChannelCount() { return _MixFormat->nChannels; }
  UINT32 getSamplesPerSecond() { return _MixFormat->nSamplesPerSec; }
  UINT32 getBytesPerSample() { return _MixFormat->wBitsPerSample / 8; }
  RenderSampleType getSampleType() { return _RenderSampleType; }

  UINT32 getFrameSize() { return _FrameSize; }
  UINT32 getBufferSize() { return _BufferSize; }

  UINT32 BufferSizePerPeriod();

  STDMETHOD_ (ULONG, AddRef)();
  STDMETHOD_ (ULONG, Release)();

private:
  ~CWASAPIRenderer();

  bool InitializeAudioEngine();
  bool CalculateMixFormatType();
  bool LoadFormat();

  bool InitializeStreamSwitch();
  void TerminateStreamSwitch();
  bool HandleStreamSwitchEvent();

  // IAudioSessionEvents
  STDMETHOD (OnDisplayNameChanged) (LPCWSTR /*NewDisplayName*/, LPCGUID /*EventContext*/) { return S_OK; };
  STDMETHOD (OnIconPathChanged) (LPCWSTR /*NewIconPath*/, LPCGUID /*EventContext*/) { return S_OK; };
  STDMETHOD (OnSimpleVolumeChanged) (float /*NewSimpleVolume*/, BOOL /*NewMute*/, LPCGUID /*EventContext*/) { return S_OK; }
  STDMETHOD (OnChannelVolumeChanged) (DWORD /*ChannelCount*/, float /*NewChannelVolumes*/[], DWORD /*ChangedChannel*/, LPCGUID /*EventContext*/) { return S_OK; };
  STDMETHOD (OnGroupingParamChanged) (LPCGUID /*NewGroupingParam*/, LPCGUID /*EventContext*/) {return S_OK; };
  STDMETHOD (OnStateChanged) (AudioSessionState /*NewState*/) { return S_OK; };
  STDMETHOD (OnSessionDisconnected) (AudioSessionDisconnectReason DisconnectReason);

  // IMMNotificationClient
  STDMETHOD (OnDeviceStateChanged) (LPCWSTR /*DeviceId*/, DWORD /*NewState*/) { return S_OK; }
  STDMETHOD (OnDeviceAdded) (LPCWSTR /*DeviceId*/) { return S_OK; };
  STDMETHOD (OnDeviceRemoved) (LPCWSTR /*DeviceId(*/) { return S_OK; };
  STDMETHOD (OnDefaultDeviceChanged) (EDataFlow Flow, ERole Role, LPCWSTR NewDefaultDeviceId);
  STDMETHOD (OnPropertyValueChanged) (LPCWSTR /*DeviceId*/, const PROPERTYKEY /*Key*/){return S_OK; };

  //  IUnknown
  STDMETHOD (QueryInterface)(REFIID iid, void **pvObject);

  // thread
  DWORD CWASAPIRenderer::DoRenderThread();
  static DWORD __stdcall WASAPIRenderThread (LPVOID Context);

  // private vars
  LONG _RefCount;

  //  Core Audio Rendering member variables.
  IMMDevice* _Endpoint;
  IAudioClient*_AudioClient;
  IAudioRenderClient*_RenderClient;

  HANDLE _RenderThread;
  HANDLE _ShutdownEvent;
  HANDLE _AudioSamplesReadyEvent;

  WAVEFORMATEX* _MixFormat;
  UINT32 _FrameSize;
  RenderSampleType _RenderSampleType;
  UINT32 _BufferSize;
  LONG _EngineLatencyInMS;

  //  Render buffer management.
  RenderBuffer* _RenderBufferQueue;

  //  Stream switch related members and methods.
  bool _EnableStreamSwitch;
  ERole _EndpointRole;
  HANDLE _StreamSwitchEvent;          // Set when the current session is disconnected or the default device changes.
  HANDLE _StreamSwitchCompleteEvent;  // Set when the default device changed.
  IAudioSessionControl* _AudioSessionControl;
  IMMDeviceEnumerator* _DeviceEnumerator;
  bool _InStreamSwitch;
  };
