// test.cpp - minimal console
//{{{  includes
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <wrl.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <fcntl.h>
#include <io.h>

#include <string>
#include <thread>
#include <mutex>

#include "../../shared/utils/utils.h"
#include "../../shared/utils/cLog.h"
#include "../../shared/utils/cSemaphore.h"

using namespace std;
//}}}

//{{{
void reportOpen (string fileName) {


  while (true) {
    auto file = _open (fileName.c_str(), _O_RDONLY | _O_BINARY);
    struct _stat64 buf;
    _fstat64 (file, &buf);
    auto streamSize = buf.st_size;
    auto atime = buf.st_atime;
    auto ctime = buf.st_ctime;
    auto mtime = buf.st_mtime;
    cLog::log (LOGINFO, "size:" + dec(streamSize) +
                        " mTime:" + dec (mtime));
    _close (file);
    Sleep (1000);
    }

  }
//}}}
//{{{
void reportfOpen (string fileName) {
  while (true) {
    auto file = fopen (fileName.c_str(), "rb");
    _fseeki64 (file, 0, SEEK_END);
    auto pos = _ftelli64 (file);
    cLog::log (LOGINFO, "tell " + dec(pos));
    Sleep (1000);
    fclose (file);
    }
  }
//}}}
//{{{
void reportFileCreate (const string& fileName) {

  while (true) {
    auto file = CreateFile (fileName.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,         // existing file only
                            FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
      cLog::log (LOGERROR, "createFile failed");

    BY_HANDLE_FILE_INFORMATION fileInformation;
    GetFileInformationByHandle (file, &fileInformation);
    cLog::log (LOGINFO, "GetFileInformationByHandle sizel " + dec(fileInformation.nFileSizeLow) +
                         " ser " + dec(fileInformation.dwVolumeSerialNumber) +
                         " att " + dec(fileInformation.dwFileAttributes) +
                         " links " + dec(fileInformation.nNumberOfLinks) +
                         " ind " + dec(fileInformation.nFileIndexLow));
    //DWORD bytesRead;
    //if (ReadFile (file, readBuffer, 100, &bytesRead, NULL)) {
    //  cLog::log (LOGINFO, "ReadFile info " + dec(bytesRead));
    //  }
    //else
    //  cLog::log (LOGERROR, "ReadFile failed");
    CloseHandle (file);

    Sleep (1000);
    }

  }
//}}}

int main (int argc, char* argv[]) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::init (LOGINFO, false);

  cLog::log (LOGINFO, "hello colin");

  string fileName = argv[1];

  reportOpen (fileName);
  //reportfOpen (fileName);
  //reportFileCreate (fileName);

  while (true)
    Sleep (1000);

  CoUninitialize();
  }
