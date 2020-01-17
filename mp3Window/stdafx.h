#pragma once

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <wrl.h>

//{{{  shell
#include <shellapi.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <shlguid.h>
//}}}
//{{{  std
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <fcntl.h>
#include <io.h>
//}}}
//{{{  stl
#include <algorithm>
#include <string>
#include <map>
#include <vector>
#include <deque>
#include <thread>
#include <mutex>
//}}}
//{{{  direct2d
#include <d3d11.h>
#include <d3d11_1.h>
#include <d2d1.h>
#include <d2d1_1.h>
#include <d2d1helper.h>
#include <DXGI1_2.h>
#include <dwrite.h>
//}}}

#include "../../shared/utils/utils.h"
#include "../../shared/utils/date.h"
#include "../../shared/utils/cLog.h"
#include "../../shared/utils/cSemaphore.h"

#include "../common/cD2dWindow.h"

#include "../../shared/utils/cWinAudio32.h"

#include "../common/cJpegImage.h"
#include "../common/cJpegImageView.h"

#include "../boxes/cLogBox.h"
#include "../boxes/cWindowBox.h"
#include "../boxes/cVolumeBox.h"
#include "../boxes/cCalendarBox.h"
#include "../boxes/cClockBox.h"
#include "../boxes/cIntBox.h"
#include "../boxes/cFloatBox.h"
#include "../boxes/cTitleBox.h"

extern "C" {
  #include <libavcodec/avcodec.h>
  #include <libavformat/avformat.h>
  }

#define CURL_STATICLIB
#include "../../curl/include/curl/curl.h"
