#pragma once

#include "mfxdefs.h"
#include <stdio.h>

#define MSDK_FOPEN(file, name, mode) _tfopen_s(&file, name, mode)
#define msdk_fgets  _fgetts
