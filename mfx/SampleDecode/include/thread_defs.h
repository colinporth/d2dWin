#pragma once
#include "mfxdefs.h"
#include "strings_defs.h"
#include <windows.h>
#include <process.h>

typedef unsigned int (MFX_STDCALL * msdk_thread_callback)(void*);

/* Thread-safe 16-bit variable incrementing */
mfxU16 msdk_atomic_inc16(volatile mfxU16 *pVariable);

/* Thread-safe 16-bit variable decrementing */
mfxU16 msdk_atomic_dec16(volatile mfxU16 *pVariable);

//{{{
struct msdkMutexHandle
{
    CRITICAL_SECTION m_CritSec;
};
//}}}
//{{{
struct msdkSemaphoreHandle
{
    void* m_semaphore;
};
//}}}
//{{{
struct msdkEventHandle
{
    void* m_event;
};
//}}}
//{{{
struct msdkThreadHandle
{
    void* m_thread;
};
//}}}

//{{{
class MSDKMutex: public msdkMutexHandle
{
public:
    MSDKMutex(void);
    ~MSDKMutex(void);

    mfxStatus Lock(void);
    mfxStatus Unlock(void);
    int Try(void);

private:
    MSDKMutex(const MSDKMutex&);
    void operator=(const MSDKMutex&);
};
//}}}
//{{{
class AutomaticMutex
{
public:
    AutomaticMutex(MSDKMutex& mutex);
    ~AutomaticMutex(void);

private:
    mfxStatus Lock(void);
    mfxStatus Unlock(void);

    MSDKMutex& m_rMutex;
    bool m_bLocked;

private:
    AutomaticMutex(const AutomaticMutex&);
    void operator=(const AutomaticMutex&);
};
//}}}
//{{{
class MSDKSemaphore: public msdkSemaphoreHandle
{
public:
    MSDKSemaphore(mfxStatus &sts, mfxU32 count = 0);
    ~MSDKSemaphore(void);

    mfxStatus Post(void);
    mfxStatus Wait(void);

private:
    MSDKSemaphore(const MSDKSemaphore&);
    void operator=(const MSDKSemaphore&);
};

class MSDKEvent: public msdkEventHandle
{
public:
    MSDKEvent(mfxStatus &sts, bool manual, bool state);
    ~MSDKEvent(void);

    mfxStatus Signal(void);
    mfxStatus Reset(void);
    mfxStatus Wait(void);
    mfxStatus TimedWait(mfxU32 msec);

private:
    MSDKEvent(const MSDKEvent&);
    void operator=(const MSDKEvent&);
};
//}}}
//{{{
class MSDKThread: public msdkThreadHandle
{
public:
    MSDKThread(mfxStatus &sts, msdk_thread_callback func, void* arg);
    ~MSDKThread(void);

    mfxStatus Wait(void);
    mfxStatus TimedWait(mfxU32 msec);
    mfxStatus GetExitCode();

#if !defined(_WIN32) && !defined(_WIN64)
    friend void* msdk_thread_start(void* arg);
#endif

private:
    MSDKThread(const MSDKThread&);
    void operator=(const MSDKThread&);
};
//}}}

mfxU32 msdk_get_current_pid();
mfxStatus msdk_setrlimit_vmem(mfxU64 size);
mfxStatus msdk_thread_get_schedtype(const msdk_char*, mfxI32 &type);
void msdk_thread_printf_scheduling_help();
