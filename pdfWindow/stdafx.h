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
#include <set>
#include <vector>
#include <deque>
#include <thread>
#include <mutex>

// concurrent
#include "concurrent_vector.h"
#include "concurrent_queue.h"
#include "concurrent_unordered_set.h"
#include "concurrent_unordered_map.h"

// direct2d
#include <d3d11.h>
#include <d3d11_1.h>
#include <d2d1.h>
#include <d2d1_1.h>
#include <d2d1helper.h>
#include <DXGI1_2.h>
#include <dwrite.h>

// utils
#include "../../shared/utils/utils.h"
#include "../../shared/utils/date.h"
#include "../../shared/utils/cLog.h"
#include "../../shared/utils/cSemaphore.h"

using namespace std;

// d2dWindow common
using namespace Microsoft::WRL;
using namespace D2D1;
#include "../common/cD2dWindow.h"
#pragma comment(lib,"common.lib")

// mupdf
#include "mupdf/fitz.h"
#include "mupdf/pdf.h"
#pragma comment(lib,"libmupdf.lib")
