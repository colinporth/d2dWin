#include "StdAfx.h"
#include "Filter.h"
#include <dvdmedia.h>

tstring MediaTypeToString(const AM_MEDIA_TYPE* pmt);

// filter registration tables
const AMOVIESETUP_MEDIATYPE MonitorFilter::m_sudType = 
{
	&MEDIATYPE_NULL,
	&MEDIASUBTYPE_NULL
};

const AMOVIESETUP_PIN MonitorFilter::m_sudPin[] = 
{
	{
		L"Input",           // pin name
		FALSE,               // is rendered?    
		FALSE,              // is output?
		FALSE,              // zero instances allowed?
		FALSE,              // many instances allowed?
		&CLSID_NULL,        // connects to filter (for bridge pins)
		NULL,               // connects to pin (for bridge pins)
		1,                  // count of registered media types
		&m_sudType          // list of registered media types    
	},
	{
		L"Output",           // pin name
		FALSE,              // is rendered?    
		TRUE,               // is output?
		FALSE,              // zero instances allowed?
		FALSE,              // many instances allowed?
		&CLSID_NULL,        // connects to filter (for bridge pins)
		NULL,               // connects to pin (for bridge pins)
		1,                  // count of registered media types
		&m_sudType          // list of registered media types    
	},
};

const AMOVIESETUP_FILTER MonitorFilter::m_sudFilter = 
{
	&__uuidof(MonitorFilter),
	L"GDCL Monitor",
	MERIT_DO_NOT_USE,
	2,
	m_sudPin
};

CUnknown* WINAPI 
MonitorFilter::CreateInstance(LPUNKNOWN pUnk, HRESULT* phr)
{
	return new MonitorFilter(pUnk, phr);
}

MonitorFilter::MonitorFilter(IUnknown* pUnk, HRESULT* phr)
: CTransInPlaceFilter(NAME("Monitor"), pUnk, *m_sudFilter.clsID, phr),
  m_bDumpGraph(true)
{
}

STDMETHODIMP 
MonitorFilter::NonDelegatingQueryInterface(REFIID iid, void** ppv)
{
	if (iid == IID_IFileSinkFilter)
	{
		return GetInterface((IFileSinkFilter*)this, ppv);
	}
	return __super::NonDelegatingQueryInterface(iid, ppv);
}

STDMETHODIMP
MonitorFilter::SetFileName(LPCOLESTR pszFileName, const AM_MEDIA_TYPE *pmt)
{
#ifdef UNICODE
    m_strLogFile = pszFileName;
#else
	_bstr_t str = pszFileName;
	m_strLogFile = str;
#endif
	CAutoLock lock(&m_csLog);
	log.OpenFile(m_strLogFile.c_str(), true);
	return S_OK;
}

STDMETHODIMP
MonitorFilter::GetCurFile(LPOLESTR *ppszFileName, AM_MEDIA_TYPE *pmt)
{
    int cch = m_strLogFile.length();
    WCHAR* awch = reinterpret_cast<WCHAR*>(CoTaskMemAlloc(sizeof(WCHAR) * (cch + 1)));
#ifdef UNICODE
	CopyMemory(awch, m_strLogFile.c_str(), sizeof(WCHAR) * (cch+1));
#else
    MultiByteToWideChar(CP_ACP, 0, m_strLogFile.c_str(), cch+1, awch, cch+1);
#endif

    *ppszFileName = awch;
    if (pmt) {
	    ZeroMemory(pmt, sizeof(*pmt));
	    pmt->majortype = MEDIATYPE_Stream;
	    pmt->subtype = MEDIASUBTYPE_NULL;
    }

    return S_OK;

}

void MonitorFilter::ListPins(IBaseFilter* f)
{
	IEnumPinsPtr pEnum;
	f->EnumPins(&pEnum);
	IPinPtr pin;
	while (pEnum->Next(1, &pin, NULL) == S_OK)
	{
		PIN_INFO pininfo;
		pin->QueryPinInfo(&pininfo);
		pininfo.pFilter->Release();
		log.strm() << TEXT("Pin ") << pininfo.achName;
		if (pininfo.dir == PINDIR_INPUT)
		{
			log.strm() << TEXT("[In]");
		}
		else
		{
			log.strm() << TEXT("[Out]");
		}

		IPinPtr peer;
		if (pin->ConnectedTo(&peer) != S_OK)
		{
			log.strm() << TEXT("disconnected");
		}
		else
		{
			PIN_INFO peerinfo;
			peer->QueryPinInfo(&peerinfo);
			FILTER_INFO finfo;
			peerinfo.pFilter->QueryFilterInfo(&finfo);
			finfo.pGraph->Release();
			log.strm() << TEXT("=>") << peerinfo.achName << TEXT(" on ") << finfo.achName;
			if (peerinfo.pFilter == static_cast<IBaseFilter*>(this))
			{
				log.strm() << TEXT("(monitor - me)");
			}
			peerinfo.pFilter->Release();
			AM_MEDIA_TYPE mt;
			pin->ConnectionMediaType(&mt);
			log.strm() << TEXT(" <") << MediaTypeToString(&mt) << TEXT(">");
			FreeMediaType(mt);
		}
		log.write();
	}

}

STDMETHODIMP MonitorFilter::GetState(DWORD msecs, FILTER_STATE* pState)
{
	*pState = m_State;
	HRESULT hr = S_OK;

	CAutoLock lock(&m_csLog);
	log.strm() << TEXT("GetState ") << m_State; log.write();

	if (m_bDumpGraph)
	{
		m_bDumpGraph = false;

		IEnumFiltersPtr pEnum;
		if (m_pGraph)
		{
			m_pGraph->EnumFilters(&pEnum);
			IBaseFilterPtr f;
			while (pEnum->Next(1, &f, NULL) == S_OK)
			{
				if (f != static_cast<IBaseFilter*>(this))
				{
					FILTER_INFO info;
					f->QueryFilterInfo(&info);
					info.pGraph->Release();
					log.strm() << TEXT("Filter ") << info.achName;

					FILTER_STATE s;
					HRESULT hrF = f->GetState(0, &s);
					if (hrF == VFW_S_STATE_INTERMEDIATE)
					{
						log.strm() << TEXT(" not ready");
					}
					else if (hrF == VFW_S_CANT_CUE)
					{
						log.strm() << TEXT(" can't cue");
					}
					log.write();
				}
				else
				{
					log.strm() << TEXT("Filter: Monitor - me"); log.write();
				}
				IReferenceClockPtr pClock = f;
				if (pClock != NULL)
				{
					if (pClock == m_pClock)
					{
						log.strm() << TEXT("Filter is selected refclock"); log.write();
					}
					else
					{
						log.strm() << TEXT("Filter is unused refclock"); log.write();
					}
				}
				ListPins(f);
			}
		}
	}
	return hr;
}

STDMETHODIMP MonitorFilter::Run(REFERENCE_TIME tStart)
{
	{
		CAutoLock lock(&m_csLog);
		log.strm() << hex << LONG_PTR(this) << dec << TEXT(" Run"); 
		if (m_pClock != NULL)
		{
			REFERENCE_TIME tNow;
			m_pClock->GetTime(&tNow);
			log.strm() << TEXT(" STO ") << long(tStart/10000);
			log.strm() << TEXT("ms ST ") << long((tNow - tStart)/10000) << TEXT("ms");
		}
		log.write();
	}
	return __super::Run(tStart);
}

STDMETHODIMP MonitorFilter::Stop()
{
	{
		CAutoLock lock(&m_csLog);
		log.strm() << hex << LONG_PTR(this) << dec << TEXT(" Stop"); log.write();
	}
	return __super::Stop();
}

HRESULT MonitorFilter::EndOfStream()
{
	{
		CAutoLock lock(&m_csLog);
		log.strm() << hex << LONG_PTR(this) << dec << TEXT(" EOS"); log.write();
	}
	return __super::EndOfStream();
}

HRESULT MonitorFilter::BeginFlush(void)
{
	{
		CAutoLock lock(&m_csLog);
		log.strm() << hex << LONG_PTR(this) << dec << TEXT(" BeginFlush"); log.write();
	}
	return __super::BeginFlush();
}

HRESULT MonitorFilter::EndFlush(void)
{
	{
		CAutoLock lock(&m_csLog);
		log.strm() << hex << LONG_PTR(this) << dec << TEXT(" EndFlush"); log.write();
	}
	return __super::EndFlush();
}

HRESULT MonitorFilter::Transform(IMediaSample* pSample)
{
	CAutoLock lock(&m_csLog);

	int cBytes = pSample->GetActualDataLength();
	log.strm() << hex << LONG_PTR(this) << dec << TEXT(" Sample ") << cBytes << TEXT(" bytes");
	bool bDiscont = (pSample->IsDiscontinuity() == S_OK);
	if (bDiscont)
	{
		log.strm() << TEXT('D');
	}
	BYTE* pData;
	pSample->GetPointer(&pData);

	REFERENCE_TIME tStart, tStop;
	if (pSample->GetTime(&tStart, &tStop) == S_OK)
	{
		REFERENCE_TIME latency = 0;
		if ((m_State == State_Running) && m_pClock)
		{
			CRefTime now;
			StreamTime(now);
			latency = tStart - now;
		}
		log.strm() << TEXT(" time ") << long(tStart/10000) << "." << abs(long(tStart % 10000));
		log.strm() << TEXT(" to ") << long(tStop/10000) << "." << abs(long(tStop %10000));
		log.strm() << TEXT(" latency ") << long(latency/10000) << "." << abs(long(latency % 10000));
	}
	else
	{
		log.strm() << TEXT(" untimed");
	}
	log.write();
	return S_OK;
}

HRESULT MonitorFilter::AlterQuality(Quality q)
{
	if (q.Late > 0)
	{
		CAutoLock lock(&m_csLog);
		log.strm() << hex << LONG_PTR(this) << dec << TEXT(" Late: ") << long(q.Late/10000) << TEXT(" ms");
		log.write();
	}
	return S_FALSE;
}

HRESULT MonitorFilter::CheckInputType(const CMediaType* mtIn)
{
	CAutoLock lock(&m_csLog);
	log.strm() << hex << LONG_PTR(this) << dec << TEXT(" CheckInputType: ") << MediaTypeToString(mtIn);
	log.write();

	return S_OK;
}
    
HRESULT 
MonitorFilter::SetMediaType(PIN_DIRECTION direction,const CMediaType *pmt)
{
	{
		CAutoLock lock(&m_csLog);
		log.strm() << hex << LONG_PTR(this) << dec << TEXT(" SetType");
		if (direction == PINDIR_INPUT)
		{
			log.strm() << TEXT(" (input):");
		}
		else
		{
			log.strm() << TEXT(" (output):");
		}
		log.strm() << MediaTypeToString(pmt);
		log.write();
	}
	return __super::SetMediaType(direction, pmt);
}

HRESULT 
MonitorFilter::NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
	{
		CAutoLock lock(&m_csLog);
		log.strm() << TEXT("NewSegment: ") << tStart << TEXT("..") << tStop << TEXT(" at ") << dRate << TEXT("x");
		log.write();
	}
	return __super::NewSegment(tStart, tStop, dRate);
}

// mt to string

// map guids to names -- extend this table with a local file?
struct _MapGuidNames
{
	GUID guid;
	const TCHAR* name;
}
GuidMap[] = 
{
	{MEDIATYPE_Video, TEXT("Video")},
	{MEDIATYPE_Audio, TEXT("Audio")},
	{MEDIATYPE_Stream, TEXT("Stream")},

	{MEDIASUBTYPE_RGB24, TEXT("RGB24")},
	{MEDIASUBTYPE_RGB555, TEXT("RGB555")},
	{MEDIASUBTYPE_RGB565, TEXT("RGB565")},
	{MEDIASUBTYPE_RGB32, TEXT("RGB32")},

	{MEDIASUBTYPE_MPEG1Packet, TEXT("Mpeg-1 Packet")},
	{MEDIASUBTYPE_MPEG1Payload, TEXT("Mpeg-1 Payload")},
	{MEDIASUBTYPE_MPEG1System, TEXT("Mpeg-1 System")},
	{MEDIASUBTYPE_MPEG1Video, TEXT("Mpeg-1 Video")},
	{MEDIASUBTYPE_MPEG1Audio, TEXT("Mpeg-1 Audio")},
	{MEDIASUBTYPE_Avi, TEXT("AVI")},
	{MEDIASUBTYPE_Asf, TEXT("ASF")},
	{MEDIASUBTYPE_PCM, TEXT("PCM")},
	{MEDIASUBTYPE_WAVE, TEXT("WAVE")},
};
const int cGuidMaps = SIZEOF_ARRAY(GuidMap);

class DECLSPEC_UUID("00000000-0000-0010-8000-00AA00389B71") FOURCC_MAP;
tstring NameFromGUID(REFGUID guid)
{
	// !! better search
	for (int i = 0; i < cGuidMaps; i++)
	{
		if (GuidMap[i].guid == guid)
		{
			return GuidMap[i].name;
		}
	}

	GUID g = guid;
	g.Data1 = 0;
	if (g == __uuidof(FOURCC_MAP))
	{
		tstring s;
		s.append(1, TCHAR(guid.Data1 & 0xff));
		s.append(1, TCHAR((guid.Data1 >> 8) & 0xff));
		s.append(1, TCHAR((guid.Data1 >> 16) & 0xff));
		s.append(1, TCHAR((guid.Data1 >> 24) & 0xff));
		return s;
	}

	// show first block of GUID for unknowns
	WCHAR wch[40];
	StringFromGUID2(guid, wch, 40);
	_bstr_t str = wch;
	tstring s = str;
	s = s.substr(0, 10);
	s += TEXT("...}");
	return s;
}

tstring FormatToString(const AM_MEDIA_TYPE* pmt)
{
	tstream strm;
	if (pmt->formattype == FORMAT_VideoInfo)
	{
		VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*)pmt->pbFormat;
		strm << pvi->bmiHeader.biWidth << TEXT("x") << pvi->bmiHeader.biHeight;
		double fps = 0;
		if (pvi->AvgTimePerFrame != 0)
		{
			fps = double(UNITS) / pvi->AvgTimePerFrame;
		}
		strm << TEXT(" ") << fps << TEXT("fps");
	}
	else if (pmt->formattype == FORMAT_VideoInfo2)
	{
		VIDEOINFOHEADER2* pvi = (VIDEOINFOHEADER2*)pmt->pbFormat;
		strm << pvi->bmiHeader.biWidth << TEXT("x") << pvi->bmiHeader.biHeight;
		double fps = double(UNITS) / pvi->AvgTimePerFrame;
		strm << TEXT(" ") << fps << TEXT("fps");
	} else if (pmt->formattype == FORMAT_WaveFormatEx)
	{
		WAVEFORMATEX* pwfx = (WAVEFORMATEX*)pmt->pbFormat;
		strm << pwfx->nChannels << TEXT("ch ") << pwfx->wBitsPerSample << TEXT("bits ");
		strm << long(pwfx->nSamplesPerSec / 1000) << ("kHz ");
		if (pwfx->wFormatTag == WAVE_FORMAT_PCM)
		{
			strm << TEXT("PCM");
		}
		else
		{
			strm << TEXT("tag:") << pwfx->wFormatTag;
		}
	}
	return strm.str();
}

tstring MediaTypeToString(const AM_MEDIA_TYPE* pmt)
{
	tstring s;

	s = NameFromGUID(pmt->majortype);
	s += TEXT("/");
	s += NameFromGUID(pmt->subtype);

	tstring sformat = FormatToString(pmt);
	if (sformat.length())
	{
		s += TEXT(" ") + sformat;
	}
	return s;
}

