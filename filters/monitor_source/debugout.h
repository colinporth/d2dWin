//
// debug output using C++ streams
//
// Supports output to debugger (if _DEBUG) and
// output to named file if it already exists.
//
// Copyright 2005-2006 GDCL.
#pragma once

#include <sstream>
#if defined(_UNICODE)
typedef std::wstring            tstring;
typedef std::wostringstream     tstream;
#else
typedef std::string             tstring;
typedef std::ostringstream      tstream;
#endif
using namespace std;
#include "strsafe.h"

// debug output for a std::string 
// create using
//      theLog.strm() << "debug output:" << value;
//      theLog.write();


// singleton class manages timed output
// by recording time at constructor
class CDebugLog
{
public:
    CDebugLog(const TCHAR* pszFile = NULL, bool bForce = false)
	: m_hFile(NULL),
	  m_nch(0),
	  m_ach(NULL)
    {
        m_tBase = getNow();
        if (pszFile != NULL)
        {
			OpenFile(pszFile, bForce);
        }
    }

	void OpenFile(const TCHAR* pszFile, bool bForce)
	{
		Close();
		m_hFile = CreateFile(pszFile, GENERIC_WRITE, FILE_SHARE_READ, NULL, bForce ? CREATE_ALWAYS:OPEN_EXISTING, 0, NULL);
        if (m_hFile == INVALID_HANDLE_VALUE)
        {
            m_hFile = NULL;
        }
		SetFilePointer(m_hFile, 0, NULL, FILE_END);
	}

	void Close()
	{
		if (m_hFile != NULL)
		{
			CloseHandle(m_hFile);
			m_hFile = NULL;
		}
	}

    ~CDebugLog()
    {
		Close();
#ifdef _UNICODE
		delete[] m_ach;
#endif
    }

    // msec time function
    unsigned long long getNow()
    {
        LARGE_INTEGER liNow;
        QueryPerformanceCounter(&liNow);
        LARGE_INTEGER liFreq;
        QueryPerformanceFrequency(&liFreq);
        return liNow.QuadPart * 1000 / liFreq.QuadPart;
    }

	void writestring(tstring s)
	{
#ifdef _UNICODE
		if (s.length() > m_nch)
		{
			m_nch = s.length()+32;
			delete[] m_ach;
			m_ach = new char[m_nch];
		}
		unsigned long cbytes = WideCharToMultiByte(CP_ACP, 0, s.c_str(), -1, m_ach, m_nch, NULL, NULL);

		unsigned long cActual;
		size_t cb = cbytes;
		HRESULT hr = StringCbLengthA(m_ach, cbytes, &cb);
		if (SUCCEEDED(hr))
		{
			WriteFile(m_hFile, m_ach, cb, &cActual, NULL);
		}
#else
		unsigned long cbytes = s.length() * sizeof(TCHAR);
		unsigned long cActual;
		WriteFile(m_hFile, s.c_str(), cbytes, &cActual, NULL);
#endif
	}

    // timed debug output for string
    void
    dbgout(tstring s)
    {
        tstream strm;
        strm << long(getNow() - m_tBase);
        strm << " : " << s;
#ifdef _DEBUG
        OutputDebugString(strm.str().c_str());
        OutputDebugString(TEXT("\n"));
#endif
        if (m_hFile != NULL)
        {
			writestring(strm.str());
			writestring(tstring(TEXT("\r\n")));
        }
    }

    tstream& strm() 
    {
        return m_strm;
    }
    void reset()
    {
        m_strm.str(tstring());
    }
    void write()
    {
        dbgout(m_strm.str());
        reset();
    }
    void loghr(const TCHAR* psz, DWORD hr)
    {
        strm() << psz << TEXT(" 0x") << hex << hr << dec;
        write();
    }
private:
    HANDLE m_hFile;
    unsigned long long m_tBase;
    tstream m_strm;
#ifdef UNICODE
	char* m_ach;
	size_t m_nch;
#endif
};

