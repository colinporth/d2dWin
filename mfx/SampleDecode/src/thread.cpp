#include <new>
#include "mfx_samples_config.h"
#include "thread_defs.h"
#include <intrin.h>

#pragma intrinsic (_InterlockedIncrement16)
#pragma intrinsic (_InterlockedDecrement16)

//{{{
mfxU16 msdk_atomic_inc16(volatile mfxU16 *pVariable) {
  return _InterlockedIncrement16 ((volatile short*)pVariable);
  }
//}}}
//{{{
/* Thread-safe 16-bit variable decrementing */
mfxU16 msdk_atomic_dec16(volatile mfxU16 *pVariable) {
  return _InterlockedDecrement16 ((volatile short*)pVariable);
  }
//}}}

//{{{
AutomaticMutex::AutomaticMutex(MSDKMutex& mutex):
    m_rMutex(mutex),
    m_bLocked(false)
{
    if (MFX_ERR_NONE != Lock()) throw std::bad_alloc();
};
//}}}
//{{{
AutomaticMutex::~AutomaticMutex(void)
{
    Unlock();
}
//}}}

//{{{
mfxStatus AutomaticMutex::Lock(void)
{
    mfxStatus sts = MFX_ERR_NONE;
    if (!m_bLocked)
    {
        if (!m_rMutex.Try())
        {
            // add time measurement here to estimate how long you sleep on mutex...
            sts = m_rMutex.Lock();
        }
        m_bLocked = true;
    }
    return sts;
}
//}}}
//{{{
mfxStatus AutomaticMutex::Unlock(void)
{
    mfxStatus sts = MFX_ERR_NONE;
    if (m_bLocked)
    {
        sts = m_rMutex.Unlock();
        m_bLocked = false;
    }
    return sts;
}
//}}}
//{{{
MSDKMutex::MSDKMutex(void)
{
    InitializeCriticalSection(&m_CritSec);
}
//}}}
//{{{
MSDKMutex::~MSDKMutex(void)
{
    DeleteCriticalSection(&m_CritSec);
}
//}}}
//{{{
mfxStatus MSDKMutex::Lock(void)
{
    EnterCriticalSection(&m_CritSec);
    return MFX_ERR_NONE;
}
//}}}
//{{{
mfxStatus MSDKMutex::Unlock(void)
{
    LeaveCriticalSection(&m_CritSec);
    return MFX_ERR_NONE;
}
//}}}
//{{{
int MSDKMutex::Try(void)
{
    return TryEnterCriticalSection(&m_CritSec);
}
//}}}

//{{{
MSDKSemaphore::MSDKSemaphore(mfxStatus &sts, mfxU32 count)
{
    sts = MFX_ERR_NONE;
    m_semaphore = CreateSemaphore(NULL, count, LONG_MAX, 0);
    if (!m_semaphore) throw std::bad_alloc();
}
//}}}
//{{{
MSDKSemaphore::~MSDKSemaphore(void)
{
    CloseHandle(m_semaphore);
}
//}}}
//{{{
mfxStatus MSDKSemaphore::Post(void)
{
    return (ReleaseSemaphore(m_semaphore, 1, NULL) == false) ? MFX_ERR_UNKNOWN : MFX_ERR_NONE;
}
//}}}
//{{{
mfxStatus MSDKSemaphore::Wait(void)
{
    return (WaitForSingleObject(m_semaphore, INFINITE) != WAIT_OBJECT_0) ? MFX_ERR_UNKNOWN : MFX_ERR_NONE;
}
//}}}

//{{{
MSDKEvent::MSDKEvent(mfxStatus &sts, bool manual, bool state)
{
    sts = MFX_ERR_NONE;
    m_event = CreateEvent(NULL, manual, state, NULL);
    if (!m_event) throw std::bad_alloc();
}
//}}}
//{{{
MSDKEvent::~MSDKEvent(void)
{
    CloseHandle(m_event);
}
//}}}
//{{{
mfxStatus MSDKEvent::Signal(void)
{
    return (SetEvent(m_event) == false) ? MFX_ERR_UNKNOWN : MFX_ERR_NONE;
}
//}}}
//{{{
mfxStatus MSDKEvent::Reset(void)
{
    return (ResetEvent(m_event) == false) ? MFX_ERR_UNKNOWN : MFX_ERR_NONE;
}
//}}}
//{{{
mfxStatus MSDKEvent::Wait(void)
{
    return (WaitForSingleObject(m_event, INFINITE) != WAIT_OBJECT_0) ? MFX_ERR_UNKNOWN : MFX_ERR_NONE;
}
//}}}
//{{{
mfxStatus MSDKEvent::TimedWait(mfxU32 msec)
{
    if(MFX_INFINITE == msec) return MFX_ERR_UNSUPPORTED;
    mfxStatus mfx_res = MFX_ERR_NOT_INITIALIZED;
    DWORD res = WaitForSingleObject(m_event, msec);

    if(WAIT_OBJECT_0 == res) mfx_res = MFX_ERR_NONE;
    else if (WAIT_TIMEOUT == res) mfx_res = MFX_TASK_WORKING;
    else mfx_res = MFX_ERR_UNKNOWN;

    return mfx_res;
}
//}}}

//{{{
MSDKThread::MSDKThread(mfxStatus &sts, msdk_thread_callback func, void* arg)
{
    sts = MFX_ERR_NONE;
    m_thread = (void*)_beginthreadex(NULL, 0, func, arg, 0, NULL);
    if (!m_thread) throw std::bad_alloc();
}
//}}}
//{{{
MSDKThread::~MSDKThread(void)
{
    CloseHandle(m_thread);
}
//}}}
//{{{
mfxStatus MSDKThread::Wait(void)
{
    return (WaitForSingleObject(m_thread, INFINITE) != WAIT_OBJECT_0) ? MFX_ERR_UNKNOWN : MFX_ERR_NONE;
}
//}}}
//{{{
mfxStatus MSDKThread::TimedWait(mfxU32 msec)
{
    if(MFX_INFINITE == msec) return MFX_ERR_UNSUPPORTED;

    mfxStatus mfx_res = MFX_ERR_NONE;
    DWORD res = WaitForSingleObject(m_thread, msec);

    if(WAIT_OBJECT_0 == res) mfx_res = MFX_ERR_NONE;
    else if (WAIT_TIMEOUT == res) mfx_res = MFX_TASK_WORKING;
    else mfx_res = MFX_ERR_UNKNOWN;

    return mfx_res;
}
//}}}
//{{{
mfxStatus MSDKThread::GetExitCode()
{
    mfxStatus mfx_res = MFX_ERR_NOT_INITIALIZED;

    DWORD code = 0;
    int sts = 0;
    sts = GetExitCodeThread(m_thread, &code);

    if (sts == 0) mfx_res = MFX_ERR_UNKNOWN;
    else if (STILL_ACTIVE == code) mfx_res = MFX_TASK_WORKING;
    else mfx_res = MFX_ERR_NONE;

    return mfx_res;
}
//}}}
//{{{
mfxU32 msdk_get_current_pid()
{
    return GetCurrentProcessId();
}
//}}}
