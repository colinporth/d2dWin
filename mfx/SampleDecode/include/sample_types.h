#pragma once

#ifdef UNICODE
    #define msdk_cout std::wcout
    #define msdk_err std::wcerr
#else
    #define msdk_cout std::cout
    #define msdk_err std::cerr
#endif

typedef std::basic_string<msdk_char> msdk_string;
typedef std::basic_stringstream<msdk_char> msdk_stringstream;
typedef std::basic_ostream<msdk_char, std::char_traits<msdk_char> > msdk_ostream;
typedef std::basic_istream<msdk_char, std::char_traits<msdk_char> > msdk_istream;
typedef std::basic_fstream<msdk_char, std::char_traits<msdk_char> > msdk_fstream;
