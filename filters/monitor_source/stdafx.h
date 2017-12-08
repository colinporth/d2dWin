// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#pragma comment(linker, "/nodefaultlib:libc.lib")
#pragma comment(linker, "/nodefaultlib:libcd.lib")

#include <streams.h>
#include <comdef.h>

#define SIZEOF_ARRAY(a) (sizeof(a) / sizeof(a[0]))
#include <tchar.h>