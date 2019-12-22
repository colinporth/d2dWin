#pragma once
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <wrl.h>

#include <shellapi.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <shlguid.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <fcntl.h>
#include <io.h>

#include <algorithm>
#include <string>
#include <map>
#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include "concurrent_vector.h"

// direct2d
#include <d3d11.h>
#include <d3d11_1.h>
#include <d2d1.h>
#include <d2d1_1.h>
#include <d2d1helper.h>
#include <DXGI1_2.h>
#include <dwrite.h>

#include "../../shared/utils/utils.h"
#include "../../shared/utils/date.h"
#include "../../shared/utils/cLog.h"
#include "../../shared/utils/cSemaphore.h"

using namespace Microsoft::WRL;
using namespace D2D1;

#include "../common/cD2dWindow.h"
#pragma comment(lib,"common.lib")
