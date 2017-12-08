#pragma once

#include "sample_defs.h"

#ifndef MSDK_MAJOR
#define MSDK_MAJOR 8
#endif

#ifndef MSDK_MINOR
#define MSDK_MINOR 0
#endif

#ifndef MSDK_RELEASE
#define MSDK_RELEASE 24
#endif

#ifndef MSDK_BUILD
#define MSDK_BUILD 0
#endif

static msdk_string GetMSDKSampleVersion()
{
    msdk_stringstream ss;
    ss << MSDK_MAJOR << "." << MSDK_MINOR << "." << MSDK_RELEASE << "." << MSDK_BUILD;
    return ss.str();
}
