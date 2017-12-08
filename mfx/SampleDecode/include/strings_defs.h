#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string>
#include <tchar.h>

#define MSDK_STRING(x) _T(x)
#define MSDK_CHAR(x) _T(x)

typedef std::basic_string<TCHAR> msdk_tstring;
typedef TCHAR msdk_char;

#define msdk_printf   _tprintf
#define msdk_fprintf  _ftprintf
#define msdk_sprintf  _stprintf_s // to be removed
#define msdk_vprintf  _vtprintf
#define msdk_strlen   _tcslen
#define msdk_strcmp   _tcscmp
#define msdk_stricmp  _tcsicmp
#define msdk_strncmp  _tcsnicmp
#define msdk_strstr   _tcsstr
#define msdk_atoi     _ttoi
#define msdk_strtol   _tcstol
#define msdk_strtod   _tcstod
#define msdk_strchr   _tcschr
#define msdk_itoa_decimal(value, str)   _itow_s(value, str, 4, 10)
#define msdk_strnlen(str,lenmax) strnlen_s(str,lenmax)
#define msdk_sscanf _stscanf_s

// msdk_strcopy is intended to be used with 2 parmeters, i.e. msdk_strcopy(dst, src)
// for _tcscpy_s that's possible if DST is declared as: TCHAR DST[n];
#define msdk_strcopy _tcscpy_s
#define msdk_strncopy_s _tcsncpy_s

#define MSDK_MEMCPY_BITSTREAM(bitstream, offset, src, count) memcpy_s((bitstream).Data + (offset), (bitstream).MaxLength - (offset), (src), (count))
#define MSDK_MEMCPY_BUF(bufptr, offset, maxsize, src, count) memcpy_s((bufptr)+ (offset), (maxsize) - (offset), (src), (count))
#define MSDK_MEMCPY_VAR(dstVarName, src, count) memcpy_s(&(dstVarName), sizeof(dstVarName), (src), (count))
#define MSDK_MEMCPY(dst, src, count) memcpy_s(dst, (count), (src), (count))
