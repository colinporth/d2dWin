#pragma once
#include <cstring>

#ifndef strcasecmp
  #define strcasecmp   _stricmp
#endif

#ifndef strncasecmp
  #define strncasecmp  _strnicmp
#endif

#ifndef snprintf
  #define snprintf     _snprintf
#endif
