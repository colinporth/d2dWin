#pragma once
//{{{  includes
#include "mfxstructures.h"
#include "time_defs.h"
#include "strings_defs.h"
#include "math.h"
//}}}

#pragma warning(disable:4100)

//{{{
class CTimeStatisticsReal
{
public:
    //{{{
    CTimeStatisticsReal()
    {
        ResetStatistics();
        start=0;
    }
    //}}}

    //{{{
    static msdk_tick GetFrequency()
    {
        if (!frequency)
        {
            frequency = msdk_time_get_frequency();
        }
        return frequency;
    }
    //}}}
    //{{{
    static mfxF64 ConvertToSeconds(msdk_tick elapsed)
    {
        return MSDK_GET_TIME(elapsed, 0, GetFrequency());
    }
    //}}}
    //{{{
    inline void StartTimeMeasurement()
    {
        start = msdk_time_get_tick();
    }
    //}}}
    //{{{
    inline void StopTimeMeasurement()
    {
        mfxF64 delta=GetDeltaTime();
        totalTime+=delta;
        totalTimeSquares+=delta*delta;

        if(delta<minTime)
        {
            minTime=delta;
        }

        if(delta>maxTime)
        {
            maxTime=delta;
        }
        numMeasurements++;
    }
    //}}}
    //{{{
    inline void StopTimeMeasurementWithCheck()
    {
        if(start)
        {
            StopTimeMeasurement();
        }
    }
    //}}}
    //{{{
    inline mfxF64 GetDeltaTime()
    {
        return MSDK_GET_TIME(msdk_time_get_tick(), start, GetFrequency());
    }
    //}}}
    //{{{
    inline void PrintStatistics(const msdk_char* prefix)
    {
        msdk_printf(MSDK_STRING("%s Total:%.3lf(%lld smpls),Avg %.3lf,StdDev:%.3lf,Min:%.3lf,Max:%.3lf\n"),prefix,totalTime*1000,numMeasurements,GetAvgTime()*1000,GetTimeStdDev()*1000,minTime*1000,maxTime*1000);
    }
    //}}}
    //{{{
    inline mfxU64 GetNumMeasurements()
    {
        return numMeasurements;
    }
    //}}}
    //{{{
    inline mfxF64 GetAvgTime()
    {
        return numMeasurements ? totalTime/numMeasurements : 0;
    }
    //}}}
    //{{{
    inline mfxF64 GetTimeStdDev()
    {
        mfxF64 avg = GetAvgTime();
        return numMeasurements ? sqrt(totalTimeSquares/numMeasurements-avg*avg) : 0;
    }
    //}}}
    //{{{
    inline mfxF64 GetMinTime()
    {
        return minTime;
    }
    //}}}
    //{{{
    inline mfxF64 GetMaxTime()
    {
        return maxTime;
    }
    //}}}
    //{{{
    inline mfxF64 GetTotalTime()
    {
        return  totalTime;
    }
    //}}}
    //{{{
    inline void ResetStatistics()
    {
        totalTime=0;
        totalTimeSquares=0;
        minTime=1E100;
        maxTime=-1;
        numMeasurements=0;
    }
    //}}}

protected:
    static msdk_tick frequency;

    msdk_tick start;
    mfxF64 totalTime;
    mfxF64 totalTimeSquares;
    mfxF64 minTime;
    mfxF64 maxTime;
    mfxU64 numMeasurements;

};
//}}}

//{{{
class CTimeStatisticsDummy {
public:
    //{{{
    static msdk_tick GetFrequency()
    {
        if (!frequency)
        {
            frequency = msdk_time_get_frequency();
        }
        return frequency;
    }
    //}}}
    //{{{
    static mfxF64 ConvertToSeconds(msdk_tick elapsed)
    {
        return 0;
    }
    //}}}

    //{{{
    inline void StartTimeMeasurement()
    {
    }
    //}}}
    //{{{
    inline void StopTimeMeasurement()
    {
    }
    //}}}
    //{{{
    inline void StopTimeMeasurementWithCheck()
    {
    }
    //}}}
    //{{{
    inline mfxF64 GetDeltaTime()
    {
        return 0;
    }
    //}}}
    //{{{
    inline void PrintStatistics(const msdk_char* prefix)
    {
    }
    //}}}
    //{{{
    inline mfxU64 GetNumMeasurements()
    {
        return 0;
    }
    //}}}
    //{{{
    inline mfxF64 GetAvgTime()
    {
        return 0;
    }
    //}}}
    //{{{
    inline mfxF64 GetTimeStdDev()
    {
        return 0;
    }
    //}}}
    //{{{
    inline mfxF64 GetMinTime()
    {
        return 0;
    }
    //}}}
    //{{{
    inline mfxF64 GetMaxTime()
    {
        return 0;
    }
    //}}}
    //{{{
    inline mfxF64 GetTotalTime()
    {
        return  0;
    }
    //}}}
    //{{{
    inline void ResetStatistics()
    {
    }
    //}}}

protected:
    static msdk_tick frequency;
};
//}}}

#ifdef TIME_STATS
    typedef CTimeStatisticsReal CTimeStatistics;
#else
    typedef CTimeStatisticsDummy CTimeStatistics;
#endif
