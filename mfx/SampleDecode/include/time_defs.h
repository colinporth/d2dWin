#pragma once
#include "mfxdefs.h"
#include <windows.h>

#define MSDK_GET_TIME(T,S,F) ((mfxF64)((T)-(S))/(mfxF64)(F))
typedef mfxI64 msdk_tick;

//{{{
inline msdk_tick msdk_time_get_tick()
{
    LARGE_INTEGER t1;

    QueryPerformanceCounter(&t1);
    return t1.QuadPart;
}
//}}}
//{{{
inline msdk_tick msdk_time_get_frequency()
{
    LARGE_INTEGER t1;

    QueryPerformanceFrequency(&t1);
    return t1.QuadPart;
}
//}}}

//{{{
#define MSDK_SLEEP(msec) Sleep (msec)
#define MSDK_USLEEP(usec) \
{ \
    LARGE_INTEGER due; \
    due.QuadPart = -(10*(int)usec); \
    HANDLE t = CreateWaitableTimer(NULL, TRUE, NULL); \
    SetWaitableTimer(t, &due, 0, NULL, NULL, 0); \
    WaitForSingleObject(t, INFINITE); \
    CloseHandle(t); \
}
//}}}
