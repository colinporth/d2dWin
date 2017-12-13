#pragma once
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <wrl.h>

#include <algorithm>
#include <string>
#include <map>
#include <deque>
#include <vector>
#include "concurrent_vector.h"
#include <thread>
#include <mutex>

#include "../../shared/utils/utils.h"
#include "../../shared/utils/cLog.h"
#include "../../shared/utils/cSemaphore.h"

#pragma comment(lib,"common.lib")
