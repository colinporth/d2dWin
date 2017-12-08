#pragma once
#include "debugout.h"

#include <comdef.h>
#define SMARTPTR(x)	_COM_SMARTPTR_TYPEDEF(x, __uuidof(x))
SMARTPTR(IBaseFilter);
SMARTPTR(IEnumFilters);
SMARTPTR(IEnumPins);
SMARTPTR(IPin);
SMARTPTR(IReferenceClock);

class DECLSPEC_UUID("{000A91F7-CEB1-478b-AAC2-0AAA73828F02}") MonitorFilter 
: public CTransInPlaceFilter,
  public IFileSinkFilter
{
public:
    // constructor method used by class factory
    static CUnknown* WINAPI CreateInstance(LPUNKNOWN pUnk, HRESULT* phr);
	LPAMOVIESETUP_FILTER GetSetupData() 
	{
		return const_cast<LPAMOVIESETUP_FILTER>(&m_sudFilter);
	}

    // expose interface IFileSinkFilter
    DECLARE_IUNKNOWN
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

    // filter registration tables
    static const AMOVIESETUP_MEDIATYPE m_sudType;
    static const AMOVIESETUP_PIN m_sudPin[];
    static const AMOVIESETUP_FILTER m_sudFilter;

    // IFileSinkFilter methods (for log file)
    STDMETHOD(SetFileName)(LPCOLESTR pszFileName, const AM_MEDIA_TYPE *pmt);
    STDMETHOD(GetCurFile)(LPOLESTR *ppszFileName, AM_MEDIA_TYPE *pmt);


	STDMETHODIMP GetState(DWORD msecs, FILTER_STATE* pState);
	STDMETHODIMP Run(REFERENCE_TIME tStart);
	STDMETHODIMP Stop();
	HRESULT Transform(IMediaSample* pSample);
	HRESULT AlterQuality(Quality q);
	HRESULT CheckInputType(const CMediaType* mtIn);
    HRESULT SetMediaType(PIN_DIRECTION direction,const CMediaType *pmt);
	HRESULT EndOfStream();
    HRESULT BeginFlush(void);
    HRESULT EndFlush(void);

	HRESULT NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate);

private:
	MonitorFilter(IUnknown* pUnk, HRESULT* phr);
	void ListPins(IBaseFilter* f);

private:
	CCritSec m_csLog;
	tstring m_strLogFile;
	CDebugLog log;
	bool m_bDumpGraph;
};
