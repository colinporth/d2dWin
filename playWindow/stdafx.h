#pragma once

#define _CRT_SECURE_NO_WARNINGS
#define _SILENCE_CXX17_UNCAUGHT_EXCEPTION_DEPRECATION_WARNING

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
#include <shared_mutex>
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

#include "../../shared/date/date.h"
#include "../../shared/utils/utils.h"
#include "../../shared/utils/cLog.h"
#include "../../shared/utils/cSemaphore.h"

#include "../common/cJpegImage.h"

#include "../common/cD2dWindow.h"
#include "../boxes/cLogBox.h"
#include "../boxes/cBmpBox.h"
#include "../boxes/cWindowBox.h"
#include "../boxes/cCalendarBox.h"
#include "../boxes/cClockBox.h"
#include "../boxes/cIntBox.h"
#include "../boxes/cFloatBox.h"
#include "../boxes/cTitleBox.h"
#include "../boxes/cJpegImageView.h"

extern "C" {
  #include "libavformat/avformat.h"
  #include "libavformat/avio.h"
  #include "libavcodec/avcodec.h"
  #include "libavutil/audio_fifo.h"
  #include "libavutil/avassert.h"
  #include "libavutil/avstring.h"
  #include "libavutil/frame.h"
  #include "libavutil/opt.h"
  #include "libswresample/swresample.h"
  }

#include "../../shared/net/cUrl.h"
#include "../../shared/net/cHttp.h"
#include "../../shared/net/cWinSockHttp.h"
