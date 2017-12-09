//{{{  Dump.cpp
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// We are a generic renderer that can be attached to any data stream that
// uses IMemInputPin data transport. For each sample we receive we write
// its contents including its properties into a dump file. The file we
// will write into is specified when the dump filter is created. GraphEdit
// creates a file open dialog automatically when it sees a filter being
// created that supports the IFileSinkFilter interface.
//
// Pretty straightforward.  We have our own input pin class so that
// we can override Receive, and all that does is to write the properties and
// data into a raw data file (using the Write function). We don't keep
// the file open when we are stopped, so the flags to the open function
// ensure that we open a file if already there; otherwise, we create it.
//
// dump.cpp             Main implementation of the dump renderer
// dump.def             What APIs the DLL will import and export
// dump.h               Class definition of the derived renderer
// dump.rc              Version information for the sample DLL
// dumpuids.h           CLSID for the dump filter
// makefile             How to build it...
//
// CBaseFilter          Base filter class supporting IMediaFilter
// CRenderedInputPin    An input pin attached to a renderer
// CUnknown             Handle IUnknown for our IFileSinkFilter
// CPosPassThru         Passes seeking interfaces upstream
// CCritSec             Helper class that wraps a critical section
//}}}
//{{{  includes
#include <windows.h>
#include <commdlg.h>
#include <streams.h>
#include <initguid.h>
#include <strsafe.h>

#include "dumpuids.h"
//}}}

//{{{  DLL stuff
STDAPI DllRegisterServer() { return AMovieDllRegisterServer2 (TRUE); }
STDAPI DllUnregisterServer() { return AMovieDllRegisterServer2 (FALSE); }

extern "C" BOOL WINAPI DllEntryPoint (HINSTANCE, ULONG, LPVOID);
BOOL APIENTRY DllMain (HANDLE hModule, DWORD  dwReason, LPVOID lpReserved) {
  return DllEntryPoint ((HINSTANCE)(hModule), dwReason, lpReserved);
  }
//}}}

//{{{
const AMOVIESETUP_MEDIATYPE sudPinTypes = {
  &MEDIATYPE_NULL,            // Major type
  &MEDIASUBTYPE_NULL          // Minor type
  };
//}}}
//{{{
const AMOVIESETUP_PIN sudPins = {
  L"Input",      // Pin string name
  FALSE,         // Is it rendered
  FALSE,         // Is it an output
  FALSE,         // Allowed none
  FALSE,         // Likewise many
  &CLSID_NULL,   // Connects to filter
  L"Output",     // Connects to pin
  1,             // Number of types
  &sudPinTypes   // Pin information
  };
//}}}
//{{{
const AMOVIESETUP_FILTER sudDump = {
  &CLSID_Dump,        // Filter CLSID
  L"Dump",            // String name
  MERIT_DO_NOT_USE,   // Filter merit
  1,                  // Number pins
  &sudPins            // Pin details
  };
//}}}

//{{{
class CDump : public CUnknown, public IFileSinkFilter {
public:
  DECLARE_IUNKNOWN CDump (LPUNKNOWN unknown, HRESULT* phr);
  ~CDump();

  static CUnknown* WINAPI CreateInstance (LPUNKNOWN unknown, HRESULT* hr);

  // Write raw data stream to a file
  HRESULT Write (PBYTE data, LONG lDataLength);

  // Implements the IFileSinkFilter interface
  STDMETHODIMP SetFileName (LPCOLESTR pszFileName,const AM_MEDIA_TYPE* mediaType);
  STDMETHODIMP GetCurFile (LPOLESTR* ppszFileName,AM_MEDIA_TYPE* mediaType);

private:
  // Overriden to say what interfaces we support where
  STDMETHODIMP NonDelegatingQueryInterface (REFIID riid, void** ppv);

  // Open and write to the file
  HRESULT OpenFile();
  HRESULT CloseFile();
  HRESULT HandleWriteFailure();

  friend class CDumpFilter;
  friend class CDumpInputPin;

  CDumpFilter* mFilter;       // Methods for filter interfaces
  CDumpInputPin* mPin;          // A simple rendered input pin

  CCritSec mLock;                // Main renderer critical section
  CCritSec mReceiveLock;         // Sublock for received samples

  CPosPassThru* mPosition;      // Renderer position controls

  HANDLE   mFile;               // Handle to file for dumping
  LPOLESTR mFileName;           // The filename where we dump
  BOOL     mWriteError;
  };
//}}}

CFactoryTemplate g_Templates[]= { L"Dump", &CLSID_Dump, CDump::CreateInstance, NULL, &sudDump };
int g_cTemplates = 1;

//{{{
class CDumpInputPin : public CRenderedInputPin {
public:
  //{{{
  CDumpInputPin (CDump* dump, LPUNKNOWN unknown, CBaseFilter* filter,
                                CCritSec* lock, CCritSec* receiveLock, HRESULT* hr) :
      CRenderedInputPin (NAME("CDumpInputPin"), filter, lock, hr, L"Input"),
      mReceiveLock(receiveLock), mDump(dump), mLast(0) {}
  //}}}

  //{{{
  STDMETHODIMP Receive (IMediaSample* sample) {

    CheckPointer(sample,E_POINTER);

    CAutoLock lock(mReceiveLock);

    // Has the filter been stopped yet?
    if (mDump->mFile == INVALID_HANDLE_VALUE)
      return NOERROR;

    REFERENCE_TIME tStart, tStop;
    sample->GetTime (&tStart, &tStop);

    DbgLog((LOG_TRACE, 1, TEXT("tStart(%s), tStop(%s), Diff(%d ms), Bytes(%d)"),
           (LPCTSTR) CDisp(tStart), (LPCTSTR) CDisp(tStop), (LONG)((tStart - mLast) / 10000),
           sample->GetActualDataLength()));

    mLast = tStart;

    // Copy the data to the file
    PBYTE data;
    auto hr = sample->GetPointer (&data);
    if (FAILED(hr))
      return hr;

    return mDump->Write (data, sample->GetActualDataLength());
    }
  //}}}
  //{{{
  STDMETHODIMP EndOfStream() {

    CAutoLock lock(mReceiveLock);
    return CRenderedInputPin::EndOfStream();
    }
  //}}}
  STDMETHODIMP ReceiveCanBlock() { return S_FALSE; }

  // Check if the pin can support this specific proposed type and format
  HRESULT CheckMediaType (const CMediaType*) { return S_OK; }

  //{{{
  HRESULT BreakConnect() {

    if (mDump->mPosition != NULL)
      mDump->mPosition->ForceRefresh();

    return CRenderedInputPin::BreakConnect();
    }
  //}}}

  //{{{
  STDMETHODIMP NewSegment (REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate) {

    mLast = 0;
    return S_OK;
    }
  //}}}

private:
  CDump* const mDump;           // Main renderer object
  CCritSec* const mReceiveLock; // Sample critical section
  REFERENCE_TIME mLast;         // Last sample receive time
  };
//}}}
//{{{
class CDumpFilter : public CBaseFilter {
public:
  CDumpFilter (CDump* dump, LPUNKNOWN unknown, CCritSec* lock, HRESULT* hr) :
    CBaseFilter (NAME("CDumpFilter"), unknown, lock, CLSID_Dump), mDump(dump) {}

  CBasePin* GetPin (int n) { return (n == 0) ? mDump->mPin : NULL; }
  int GetPinCount() { return 1; }

  //{{{
  STDMETHODIMP Stop() {

    CAutoLock cObjectLock(m_pLock);

    if (mDump)
      mDump->CloseFile();

    return CBaseFilter::Stop();
    }
  //}}}
  //{{{
  STDMETHODIMP Pause() {
  // GraphEdit calls Pause() before calling Stop() for this filter.
  // If we have encountered a write error (such as disk full),
  // then stopping the graph could cause our log to be deleted
  // (because the current log file handle would be invalid).
  // To preserve the log, don't open/create the log file on pause
  // if we have previously encountered an error.  The write error
  // flag gets cleared when setting a new log file name or
  // when restarting the graph with Run().

    CAutoLock cObjectLock(m_pLock);

    if (mDump)
      if (!mDump->mWriteError)
        mDump->OpenFile();

    return CBaseFilter::Pause();
    }
  //}}}
  //{{{
  STDMETHODIMP Run (REFERENCE_TIME tStart) {
  // Clear the global 'write error' flag that would be set
  // if we had encountered a problem writing the previous dump file.
  // (eg. running out of disk space).
  // Since we are restarting the graph, a new file will be created.

    CAutoLock cObjectLock(m_pLock);

    mDump->mWriteError = FALSE;
    if (mDump)
      mDump->OpenFile();

    return CBaseFilter::Run(tStart);
    }
  //}}}

private:
  CDump* const mDump;
  };
//}}}

// CDump
//{{{
CDump::CDump (LPUNKNOWN unknown, HRESULT* hr) : CUnknown(NAME("CDump"), unknown),
    mFilter(NULL), mPin(NULL), mPosition(NULL),
    mFile(INVALID_HANDLE_VALUE), mFileName(0), mWriteError(0) {

  ASSERT(hr);

  mFilter = new CDumpFilter (this, GetOwner(), &mLock, hr);
  if (mFilter == NULL) {
    if (hr)
      *hr = E_OUTOFMEMORY;
    return;
    }

  mPin = new CDumpInputPin (this, GetOwner(), mFilter, &mLock, &mReceiveLock, hr);
  if (mPin == NULL) {
    if (hr)
      *hr = E_OUTOFMEMORY;
    return;
    }
  }
//}}}
//{{{
CDump::~CDump() {

  CloseFile();

  delete mPin;
  delete mFilter;
  delete mPosition;
  delete mFileName;
  }
//}}}

//{{{
CUnknown* WINAPI CDump::CreateInstance (LPUNKNOWN unknown, HRESULT* hr) {

  ASSERT(hr);

  auto dump = new CDump (unknown, hr);
  if (dump == NULL)
    if (hr)
      *hr = E_OUTOFMEMORY;

  return dump;
  }
//}}}
//{{{
HRESULT CDump::Write (PBYTE data, LONG dataLength) {

  // If the file has already been closed, don't continue
  if (mFile == INVALID_HANDLE_VALUE)
    return S_FALSE;

  DWORD dwWritten;
  if (!WriteFile (mFile, (PVOID)data, (DWORD)dataLength, &dwWritten, NULL))
   return (HandleWriteFailure());

  return S_OK;
  }
//}}}

//{{{
STDMETHODIMP CDump::SetFileName (LPCOLESTR szFileName, const AM_MEDIA_TYPE* mediaType) {

  // Is this a valid filename supplied
  CheckPointer (szFileName,E_POINTER);
  if (wcslen (szFileName) > MAX_PATH)
    return ERROR_FILENAME_EXCED_RANGE;

  // Take a copy of the filename
  size_t len = 1 + lstrlenW (szFileName);
  mFileName = new WCHAR[len];
  if (mFileName == 0)
    return E_OUTOFMEMORY;

  auto hr = StringCchCopyW (mFileName, len, szFileName);

  // Clear the global 'write error' flag that would be set
  // if we had encountered a problem writing the previous dump file.
  // (eg. running out of disk space).
  mWriteError = FALSE;

  // Create the file then close it
  hr = OpenFile();
  CloseFile();

  return hr;
  }
//}}}
//{{{
STDMETHODIMP CDump::GetCurFile (LPOLESTR* szFileName, AM_MEDIA_TYPE* mediaType) {

  CheckPointer (szFileName, E_POINTER);

  *szFileName = NULL;
  if (mFileName != NULL) {
    size_t len = 1 + lstrlenW (mFileName);
    *szFileName = (LPOLESTR)
    QzTaskMemAlloc (sizeof(WCHAR) * (len));

    if (*szFileName != NULL)
      HRESULT hr = StringCchCopyW (*szFileName, len, mFileName);
    }

  if (mediaType) {
    ZeroMemory (mediaType, sizeof(*mediaType));
    mediaType->majortype = MEDIATYPE_NULL;
    mediaType->subtype = MEDIASUBTYPE_NULL;
    }

  return S_OK;
  }
//}}}

// CDump private
//{{{
STDMETHODIMP CDump::NonDelegatingQueryInterface (REFIID riid, void** ppv) {

  CheckPointer (ppv,E_POINTER);
  CAutoLock lock (&mLock);

  // Do we have this interface
  if (riid == IID_IFileSinkFilter)
    return GetInterface ((IFileSinkFilter *) this, ppv);

  else if (riid == IID_IBaseFilter || riid == IID_IMediaFilter || riid == IID_IPersist)
    return mFilter->NonDelegatingQueryInterface (riid, ppv);

  else if (riid == IID_IMediaPosition || riid == IID_IMediaSeeking) {
    if (mPosition == NULL) {
      HRESULT hr = S_OK;
      mPosition = new CPosPassThru (NAME("Dump Pass Through"), (IUnknown*)GetOwner(), (HRESULT*)&hr, mPin);
      if (mPosition == NULL)
        return E_OUTOFMEMORY;

      if (FAILED(hr)) {
        delete mPosition;
        mPosition = NULL;
        return hr;
        }
      }

    return mPosition->NonDelegatingQueryInterface (riid, ppv);
    }

  return CUnknown::NonDelegatingQueryInterface (riid, ppv);
  }
//}}}
//{{{
HRESULT CDump::OpenFile() {

  // Is the file already opened
  if (mFile != INVALID_HANDLE_VALUE)
    return NOERROR;

  // Has a filename been set yet
  if (mFileName == NULL)
    return ERROR_INVALID_NAME;

  // Convert the UNICODE filename if necessary
  TCHAR* fileName = NULL;
  #if defined(WIN32) && !defined(UNICODE)
    char convert[MAX_PATH];
    if (!WideCharToMultiByte (CP_ACP, 0, mFileName, -1, convert, MAX_PATH, 0, 0))
        return ERROR_INVALID_NAME;
    fileName = convert;
  #else
    fileName = mFileName;
  #endif

  // Try to open the file
  mFile = CreateFile ((LPCTSTR)fileName, GENERIC_WRITE, FILE_SHARE_READ,
                      NULL, CREATE_ALWAYS, (DWORD)0, NULL);

  if (mFile == INVALID_HANDLE_VALUE) {
    DWORD dwErr = GetLastError();
    return HRESULT_FROM_WIN32(dwErr);
    }

  return S_OK;
  }
//}}}
//{{{
HRESULT CDump::CloseFile() {
// Must lock this section to prevent problems related to
// closing the file while still receiving data in Receive()

  CAutoLock lock(&mLock);

  if (mFile == INVALID_HANDLE_VALUE)
    return NOERROR;

  CloseHandle (mFile);

  mFile = INVALID_HANDLE_VALUE; // Invalidate the file

  return NOERROR;
  }
//}}}
//{{{
HRESULT CDump::HandleWriteFailure() {

  DWORD dwErr = GetLastError();
  if (dwErr == ERROR_DISK_FULL) {
    // Close the dump file and stop the filter,
    // which will prevent further write attempts
    mFilter->Stop();

    // Set a global flag to prevent accidental deletion of the dump file
    mWriteError = TRUE;

    // Display a message box to inform the developer of the write failure
    TCHAR szMsg[MAX_PATH + 80];
    auto hr = StringCchPrintf (szMsg, MAX_PATH + 80,
                               TEXT("The disk containing dump file has run out of space, ")
                               TEXT("so the dump filter has been stopped.\r\n\r\n")
                               TEXT("You must set a new dump file name or restart the graph ")
                               TEXT("to clear this filter error."));
    MessageBox (NULL, szMsg, TEXT("Dump Filter failure"), MB_ICONEXCLAMATION);
    }

  return HRESULT_FROM_WIN32(dwErr);
  }
//}}}
