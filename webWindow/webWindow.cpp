// webWindow.cpp
//{{{  includes
#include "stdafx.h"

#include <winsock2.h>
#include <WS2tcpip.h>
#pragma comment(lib,"ws2_32.lib")

#include "../../shared/net/cWinSockHttp.h"

#include "../../shared/utils/date.h"
#include "../../shared/utils/format.h"

#include "../inc/jpeglib/jpeglib.h"
#pragma comment(lib,"turbojpeg-static.lib")

#include "../common/cBitmapBox.h"
#include "../common/cLogBox.h"
#include "../common/cClockBox.h"
#include "../common/cCalendarBox.h"
#include "../common/cIndexBox.h"
#include "../common/cFloatBox.h"
#include "../common/cWindowBox.h"

#include "apikey.h"

using namespace chrono;
using namespace concurrency;
//}}}
//{{{  const
const int kMaxZoom = 21;
const int kLoadThreads = 4;
//}}}

//{{{
class cMapSpec {
public:
  cMapSpec (double minLat, double maxLat, double minLon, double maxLon,
            int minZoom, int maxZoom, int tileSize,
            const string& fileRoot, const string& host, const string& path) :
    mMinLat(minLat), mMaxLat(maxLat), mMinLon(minLon), mMaxLon(maxLon),
    mMinZoom(minZoom), mMaxZoom(maxZoom), mTileSize(tileSize),
    mFileRoot(fileRoot), mHost(host), mPath(path) {}

  double mMinLat;
  double mMaxLat;
  double mMinLon;
  double mMaxLon;

  int mMinZoom;
  int mMaxZoom;
  int mTileSize;

  string mFileRoot;
  string mHost;
  string mPath;
  };
//}}}
//{{{
class cMapPlace {
public:
  //{{{
  cMapPlace (double lat, double lon, int zoom) :
    mLat(lat), mLon(lon), mZoom(zoom) {}
  //}}}

  double mLat;
  double mLon;
  int mZoom;
  };
//}}}

//{{{
class cZoomTile {
public:
  cZoomTile (ID2D1Bitmap* bitmap, const ColorF color, float scaledX, float scaledY) :
    mBitmap(bitmap), mColor(color), mScaledX(scaledX), mScaledY(scaledY) {}

  //{{{
  void set (ID2D1Bitmap* bitmap, const ColorF& color, float scaledX, float scaledY) {
    mBitmap = bitmap;
    mColor = color;
    mScaledX = scaledX;
    mScaledY = scaledY;
    }
  //}}}

  ID2D1Bitmap* mBitmap;
  ColorF mColor;
  float mScaledX;
  float mScaledY;
  };
//}}}
//{{{
class cZoomTileSet {
public:
  cZoomTileSet (int zoom) : mScale (float(1 << zoom)) {}
  ~cZoomTileSet() {}

  //{{{
  void insert (const string& quadKey, ID2D1Bitmap* bitmap, const ColorF& color, float scaledX, float scaledY) {
    mTileMap.insert (
      concurrent_unordered_map <string, cZoomTile>::value_type (quadKey, cZoomTile(bitmap, color, scaledX, scaledY)));
    }
  //}}}
  //{{{
  void insertEmpty (const string& quadKey) {
    mEmptyTileSet.insert (quadKey);
    }
  //}}}
  //{{{
  void clear() {

    for (auto tile : mTileMap)
      if (tile.second.mBitmap) {
        auto temp = tile.second.mBitmap;
        tile.second.mBitmap = nullptr;
        //temp->Release();
        }
    cLog::log (LOGERROR, "******* remember not releasing bitmaps ********");
    }
  //}}}

  concurrent_unordered_map <string, cZoomTile> mTileMap;
  concurrent_unordered_set <string> mEmptyTileSet;

  float mScale;
  };
//}}}
//{{{
class cMap {
public:
  //{{{
  cMap (cMapSpec mapSpec, cMapPlace* mapPlace, cD2dWindow* window, const string& arg) :
      mMapSpec(mapSpec), mMapPlace(mapPlace), mWindow(window), mArg(arg) {

    // just make zoom index the zoomTileSet, 0 never used, many others may not be used, less maths
    for (auto i = 0; i <= kMaxZoom; i++)
      mTileSet.push_back (new cZoomTileSet (i));

    setZoom (mMapPlace->mZoom);
    }
  //}}}
  //{{{
  ~cMap() {
    for (auto zoomTileSet : mTileSet) {
      //delete zoomTileSet;
      }
    }
  //}}}

  //{{{  gets
  double getCentreLat() { return mMapPlace->mLat; }
  double getCentreLon() { return mMapPlace->mLon; }
  //{{{
  void getCentrePix (int& xCentrePix, int& yCentrePix) {

    double x = (mMapPlace->mLon + 180.) / 360.;
    double sinLatitude = sin(mMapPlace->mLat * kPi / 180.);
    double y = 0.5 - log((1. + sinLatitude) / (1. - sinLatitude)) / (4. * kPi);

    auto mapSize = (int)getTileSize() << mMapPlace->mZoom;
    xCentrePix = (int) clip (x * mapSize + 0.5, 0, mapSize - 1);
    yCentrePix = (int) clip (y * mapSize + 0.5, 0, mapSize - 1);
    }
  //}}}

  int getTileSize() { return mMapSpec.mTileSize; }

  int getZoom() { return mMapPlace->mZoom; }
  cZoomTileSet* getZoomTileSet() { return mTileSet[getZoom()]; }

  //{{{
  cPoint getView() {
    return cPoint (mViewWidth, mViewHeight);
    }
  //}}}
  //{{{
  int getViewWidth() {
    return mViewWidth;
    }
  //}}}
  //{{{
  int getViewHeight() {
    return mViewHeight;
    }
  //}}}
  //{{{
  float getViewScale() {
    return mViewScale;
    }
  //}}}

  double getMinTileRangeX() { return mMinTileRangeX; }
  double getMinTileRangeY() { return mMinTileRangeY; }
  double getRangeX() { return mMaxTileRangeX - mMinTileRangeX; }
  double getRangeY() { return mMaxTileRangeY - mMinTileRangeY; }


  int getNumDownloads() { return mNumDownloads; }
  int getNumEmptyDownloads() { return mNumEmptyDownloads; }

  //{{{
  int getTileXYFromQuadKey (const string& quadKey, int& tileX, int& tileY) {

    tileX = 0;
    tileY = 0;
    auto zoom = (int)quadKey.size();

    for (int i = zoom; i > 0; i--) {
      int mask = 1 << (i - 1);
      switch (quadKey[zoom - i]) {
        case '0':
          break;

        case '1':
          tileX |= mask;
          break;

        case '2':
          tileY |= mask;
          break;

        case '3':
          tileX |= mask;
          tileY |= mask;
          break;
        }
      }

    return zoom;
    }
  //}}}
  //{{{
  string getQuadKeyFromTileXY (int tileX, int tileY) {
  // Converts tile XY coordinates into a QuadKey at a specified mZoom.

    string quadKey;

    for (int i = mMapPlace->mZoom; i > 0; i--) {
      char digit = '0';
      int mask = 1 << (i - 1);
      if ((tileX & mask) != 0)
        digit++;
      if ((tileY & mask) != 0)
         digit += 2;
      quadKey += digit;
      }

    return quadKey;
    }
  //}}}
  //{{{
  ID2D1Bitmap* getBitmapFromTileXY (int tileX, int tileY) {

    auto quadKey = getQuadKeyFromTileXY (tileX, tileY);

    auto zoomTileSet = mTileSet[quadKey.size()];
    auto it = zoomTileSet->mTileMap.find (quadKey);
    if (it != zoomTileSet->mTileMap.end())
      return it->second.mBitmap;

    return nullptr;
    }
  //}}}
  //}}}
  //{{{
  void incPix (int xIncPix, int yIncPix) {
  // must be simpler way to do this, pix inc as LatLon inc

    // latLon to pix
    int xCentrePix;
    int yCentrePix;
    getCentrePix (xCentrePix, yCentrePix);

    // inc in pix
    xCentrePix += xIncPix;
    yCentrePix += yIncPix;

    // pix back to latLon
    auto mapSize = (int)getTileSize() << mMapPlace->mZoom;
    auto x = (clip (xCentrePix, 0, mapSize - 1) / mapSize) - 0.5;
    auto y = 0.5 - (clip (yCentrePix, 0, mapSize - 1) / mapSize);
    auto lat = 90. - 360. * atan (exp (-y * 2. * kPi)) / kPi;
    auto lon = 360. * x;

    setCentreLatLon (lat, lon);
    }
  //}}}
  //{{{
  void incZoom (int inc) {

    setZoom (mMapPlace->mZoom + inc);
    changed();
    }
  //}}}
  //{{{
  void setCentreLatLonFromNormalisedXY (double x, double y) {

    setCentreLatLon (90. - (360. * atan (exp ((y - 0.5) * 2*kPi)) / kPi), 360. * (x - 0.5));
    }
  //}}}
  //{{{
  void setView (int width, int height) {
    mViewWidth = width;
    mViewHeight = height;
    }
  //}}}
  //{{{
  void setViewScale (float scale) {
    mViewScale = scale;
    }
  //}}}

  //{{{
  void clear() {

    getZoomTileSet()->clear();

    mMinTileRangeX = 1.;
    mMaxTileRangeX = 0;
    mMinTileRangeY = 1.;
    mMaxTileRangeY = 0;

    changed();
    }
  //}}}
  //{{{
  void dumpEmpty() {

    for (int zoom = mMapSpec.mMinZoom; zoom <= mMapSpec.mMaxZoom; zoom++) {
      auto fileName = mArg + mMapSpec.mFileRoot + "/" + dec(zoom) + "/empty.txt";
      auto writeFile = fopen (fileName.c_str(), "w");

      for (auto tile : mTileSet[zoom]->mEmptyTileSet) {
        auto str = tile + "\n";
        fwrite (str.c_str(), 1, str.size(), writeFile);
        }
      fclose (writeFile);
      }
    }
  //}}}

  //{{{
  void changed() {

    auto flushCount = mWindow->getShift() ? 0 : queueFlush();

    int xCentrePix;
    int yCentrePix;
    getCentrePix (xCentrePix, yCentrePix);

    auto xTile = xCentrePix / getTileSize();
    auto yTile = yCentrePix / getTileSize();

    // load window, cheap and cheerful load order, seems a bit crude
    auto queueCount = 0;
    int xWindow = int(ceil(getViewWidth() / (float)getTileSize() / getViewScale() / 2.f)) + 1;
    int yWindow = int(ceil(getViewHeight() / (float)getTileSize() / getViewScale() / 2.f)) + 1;
    auto window = xWindow > yWindow ? xWindow : yWindow;
    for (auto w = 0; w <= window; w++)
      for (auto y = -w; y <= w; y++)
          for (auto x = -w; x <= w; x++)
            if (((abs(x) == w) || (abs(y) == w)) &&              // outside edge
                ((abs(x) <= xWindow) && (abs(y) <= yWindow)) &&  // limited to x and y window
                ((abs(x) != xWindow || (abs(y) != yWindow))))    // miss visible +1 corners
              if (queueLoad (xTile+x, yTile+y))
                queueCount++;

    cLog::log (LOGINFO, "cMap::changed - window:" + dec(xWindow)+ ":" + dec(yWindow) +
                        (flushCount ? (" flush:" + dec(flushCount)) : "") +
                        (queueCount ? (" queue:" + dec(queueCount)) : " none"));
    if (queueCount)
      mLoadSem.notifyAll();

    mWindow->changed();
    }
  //}}}

  //{{{
  void fileThread() {

    cLog::log (LOGNOTICE, "fileScanThread start");

    auto totalFileCount = 0;
    for (auto zoom = mMapSpec.mMinZoom; zoom <= mMapSpec.mMaxZoom; zoom++) {
      string searchStr = mArg + mMapSpec.mFileRoot + "/" + dec(zoom) + "/*";

      auto fileCount = 0;
      WIN32_FIND_DATA findFileData;
      auto file = FindFirstFileEx (searchStr.c_str(), FindExInfoBasic, &findFileData,
                                   FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
      if (file != INVALID_HANDLE_VALUE) {
        do {
          if (PathMatchSpec (findFileData.cFileName, "*.jpg")) {
            // add tile
            string trimStr = findFileData.cFileName;
            trimStr.resize (trimStr.size()-4);
            addTile (trimStr, nullptr, 0);
            fileCount++;
            }
          } while (FindNextFile (file, &findFileData));
        FindClose (file);

        changed();
        cLog::log (LOGNOTICE, "loadFile zoom:" + dec(zoom) + " found:" + dec(fileCount) + " at:" + searchStr);
        }
      totalFileCount += fileCount;
      }

    cLog::log (LOGNOTICE, "fileScanThread - loadEmptyTiles");

    int emptyTileCount = 0;
    for (auto zoom = mMapSpec.mMinZoom; zoom <= mMapSpec.mMaxZoom; zoom++) {
      auto fileName = mArg + mMapSpec.mFileRoot  + "/" + dec(zoom)+ "/empty.txt";
      auto readFile = fopen (fileName.c_str(), "r");
      if (readFile) {
        string quadKey;
        while (true) {
          char ch;
          auto numRead = fread (&ch, 1, 1, readFile);
          if (!numRead)
            break;
          if (ch == '\n') {
            emptyTileCount++;
            addEmptyTile (quadKey);
            quadKey = "";
            }
          else
            quadKey += ch;
          }
        fclose (readFile);
        }
      }

    cLog::log (LOGNOTICE, "fileScanThread exit - total:" + dec(totalFileCount) +
                          " emptyTiles:" + dec(emptyTileCount));
    }
  //}}}
  //{{{
  void loadThread (int threadNum) {

    cLog::log (LOGNOTICE, "loadThread t%d start", threadNum);

    cWinSockHttp http;
    http.initialise();

    while (true) {
      string quadKey;
      if (!mLoadQueue.try_pop (quadKey))
        mLoadSem.wait();
      else {
        auto time = system_clock::now();
        auto fileName = mArg + mMapSpec.mFileRoot + "/" + dec(quadKey.size()) + "/" + quadKey + ".jpg";
        auto fileHandle = CreateFile (fileName.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (fileHandle == 0  || (fileHandle > (HANDLE)0xFFFFFFF)) {
          // no file, download
          auto response = http.get (fmt::format (mMapSpec.mHost, threadNum),
                                    fmt::format (mMapSpec.mPath, quadKey, getApiKey()));
          if (response == 200) {
            //{{{  download ok, load bitmap from jpeg
            ColorF color (0.f, 0.f, 0.f,0.f);
            auto bitmap = loadJpeg (http.getContent(), http.getContentSize(), 1, color);
            if (bitmap) {
              // piccy ok, save download file
              auto writeFile = fopen (fileName.c_str(), "wb");
              if (writeFile) {
                fwrite (http.getContent(), 1, http.getContentSize(), writeFile);
                fclose (writeFile);

                addTile (quadKey, bitmap, color);
                mWindow->changed();

                mNumDownloads++;
                auto loadTime = duration_cast<milliseconds>(system_clock::now() - time).count();
                cLog::log (LOGINFO, "download t" + dec(threadNum) + " " + quadKey + " " +
                                    " " + dec(response) + " " + dec(http.getContentSize()) + dec(loadTime,4) + "ms");
                }
              else {
                // error, usually being written bu other thread in a race
                cLog::log (LOGERROR, "download - t" + dec(threadNum) + " failed to save " + quadKey);
                }
              }

            else {
              // no piccy, save in empty tile set
              addEmptyTile (quadKey);
              mWindow->changed();

              mNumEmptyDownloads++;
              auto loadTime = duration_cast<milliseconds>(system_clock::now() - time).count();
              cLog::log (LOGINFO2, "loadEmpty t" + dec(threadNum) + " " + quadKey + dec(loadTime,4) + "ms");
              }
            }
            //}}}
          }
        else {
          // load file, make bitmap, cache bitmap
          auto buf = (uint8_t*)MapViewOfFile (CreateFileMapping (fileHandle, NULL, PAGE_READONLY, 0, 0, NULL), FILE_MAP_READ, 0, 0, 0);
          auto fileSize = (int)GetFileSize (fileHandle, NULL);
          ColorF color (0.f, 0.f, 0.f, 0.f);
          auto bitmap = loadJpeg (buf, fileSize, 1, color);
          addTile (quadKey, bitmap, color);
          mWindow->changed();

          UnmapViewOfFile (buf);
          CloseHandle (fileHandle);
          auto loadTime = duration_cast<milliseconds>(system_clock::now() - time).count();
          cLog::log (LOGINFO1, "loadFile t" + dec(threadNum) + " " + quadKey + dec(loadTime,3) + "ms");
          }
        }
      }

    cLog::log (LOGERROR, "loadThread t%d exit", threadNum);
    }
  //}}}

private:
  //{{{  const
  const double kEarthRadius = 6378137;
  const double kPi = 3.14159265358979323846;
  //}}}

  //{{{
  void addTile (const string& quadKey, ID2D1Bitmap* bitmap, const ColorF& color) {

    int xTile;
    int yTile;
    int zoom = getTileXYFromQuadKey (quadKey, xTile, yTile);

    auto zoomTileSet = mTileSet[zoom];

    // update tile range
    auto xScaled = xTile / zoomTileSet->mScale;
    auto yScaled = yTile / zoomTileSet->mScale;
    mMinTileRangeX = min (mMinTileRangeX, xScaled);
    mMaxTileRangeX = max (mMaxTileRangeX, xScaled);
    mMinTileRangeY = min (mMinTileRangeY, yScaled);
    mMaxTileRangeY = max (mMaxTileRangeY, yScaled);

    auto it = zoomTileSet->mTileMap.find (quadKey);
    if (it == zoomTileSet->mTileMap.end())
      zoomTileSet->insert (quadKey, bitmap, color, xScaled, yScaled);
    else if (bitmap && !it->second.mBitmap)
      it->second.set (bitmap, color, xScaled, yScaled);
    }
  //}}}
  //{{{
  void addEmptyTile (const string& quadKey) {

    int tileX;
    int tileY;
    int zoom = getTileXYFromQuadKey (quadKey, tileX, tileY);

    auto zoomTileSet = mTileSet[zoom];

    // update tile range
    mMinTileRangeX = min (mMinTileRangeX, tileX / zoomTileSet->mScale);
    mMaxTileRangeX = max (mMaxTileRangeX, tileX / zoomTileSet->mScale);
    mMinTileRangeY = min (mMinTileRangeY, tileY / zoomTileSet->mScale);
    mMaxTileRangeY = max (mMaxTileRangeY, tileY / zoomTileSet->mScale);

    zoomTileSet->insertEmpty (quadKey);
    }
  //}}}

  //{{{
  double clip (double n, double minValue, double maxValue) {
    return min (max (n, minValue), maxValue);
    }
  //}}}
  //{{{
  void setCentreLatLon (double lat, double lon) {
    mMapPlace->mLat = max (min (lat, mMapSpec.mMaxLat), mMapSpec.mMinLat);
    mMapPlace->mLon = max (min (lon, mMapSpec.mMaxLon), mMapSpec.mMinLon);
    mMapPlace->mLat = max (min (lat, mMapSpec.mMaxLat), mMapSpec.mMinLat);
    mMapPlace->mLon = max (min (lon, mMapSpec.mMaxLon), mMapSpec.mMinLon);
    changed();
    }
  //}}}
  //{{{
  void setZoom (int zoom) {
    mMapPlace->mZoom = max (min (zoom, mMapSpec.mMaxZoom), mMapSpec.mMinZoom);

    // extra min zoom uses a load scale trick
    mScale = zoom < mMapSpec.mMinZoom ? (1 << (mMapSpec.mMinZoom - zoom)) : 1;
    }
  //}}}

  //{{{
  bool queueLoad (int xTile, int yTile) {

    auto quadKey = getQuadKeyFromTileXY (xTile, yTile);

    auto zoomTileSet = getZoomTileSet();
    auto it = zoomTileSet->mTileMap.find (quadKey);
    if (it == zoomTileSet->mTileMap.end() || !(it->second.mBitmap)) {
      if (zoomTileSet->mEmptyTileSet.find (quadKey) == zoomTileSet->mEmptyTileSet.end()) {
        // not in emptyTileMap, load it
        mLoadQueue.push (quadKey);
        return true;
        }
      }

    return false;
    }
  //}}}
  //{{{
  int queueFlush() {

    int flushCount = 0;

    string quadKey;
    while (mLoadQueue.try_pop (quadKey))
      flushCount++;

    return flushCount;
    }
  //}}}

  //{{{
  ID2D1Bitmap* loadJpeg (uint8_t* buf, int bufLen, int scale, ColorF& color) {

    if ((buf[0] != 0xFF) || (buf[1] != 0xD8)) {
      // no SOI marker, return nullptr
      cLog::log (LOGERROR, "loadJpeg - no SOI marker ");
      return nullptr;
      }

    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error (&jerr);
    jpeg_create_decompress (&cinfo);

    jpeg_mem_src (&cinfo, buf, (unsigned long)bufLen);
    jpeg_read_header (&cinfo, true);

    cinfo.scale_denom = scale;
    cinfo.out_color_space = JCS_EXT_BGRA;

    ID2D1Bitmap* bitmap = nullptr;
    jpeg_start_decompress (&cinfo);

    mWindow->getDc()->CreateBitmap (SizeU (cinfo.image_width, cinfo.image_height),
                                    { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE, 0,0 }, &bitmap);
    float red = 0.f;
    float green = 0.f;
    float blue = 0.f;
    BYTE* lineArray[1];
    auto pitch = cinfo.output_components * cinfo.image_width;
    lineArray[0] = (BYTE*)malloc (pitch);
    D2D1_RECT_U r(RectU (0, 0, cinfo.image_width/scale, 0));
    while (cinfo.output_scanline < cinfo.image_height/scale) {
      r.top = cinfo.output_scanline;
      r.bottom = r.top + 1;
      jpeg_read_scanlines (&cinfo, lineArray, 1);
      auto ptr = lineArray[0] + (128 * 4);
      blue += *ptr++;
      green += *ptr++;
      red += *ptr++;
      bitmap->CopyFromMemory (&r, lineArray[0], pitch);
      }

    color.a = 1.f;
    color.r = red / 256.f / 256.f;
    color.g = green / 256.f / 256.f;
    color.b = blue / 256.f / 256.f;
    free (lineArray[0]);

    jpeg_finish_decompress (&cinfo);
    jpeg_destroy_decompress (&cinfo);

    return bitmap;
    }
  //}}}


  // private vars
  const cMapSpec mMapSpec;
  cMapPlace* mMapPlace;
  cD2dWindow* mWindow;
  string mArg;

  int mScale = 1;

  vector <cZoomTileSet*> mTileSet;

  int mViewWidth = 1920;
  int mViewHeight = 1080;
  float mViewScale = 1.f;

  float mMinTileRangeX = 1.;
  float mMaxTileRangeX = 0;
  float mMinTileRangeY = 1.;
  float mMaxTileRangeY = 0;

  // could be shared amongst all cMaps
  concurrent_queue <string> mLoadQueue;
  cSemaphore mLoadSem;

  int mNumDownloads = 0;
  int mNumEmptyDownloads = 0;
  };
//}}}

//{{{
class cMapView : public cD2dWindow::cView {
public:
  //{{{
  cMapView (cD2dWindow* window, float width, float height, cMap*& map)
      : cView("mapView", window, width, height), mMap(map) {

    mPin = true;
    }
  //}}}
  virtual ~cMapView() {}

  //{{{
  bool onWheel (int delta, cPoint pos)  {

    if (mWindow->getControl()) {
      bool result = cView::onWheel (delta, pos);
      mMap->setViewScale (mView2d.getScale());
      return result;
      }
    else {
      mMap->incZoom (delta/120);
      return true;
      }
    }
  //}}}
  //{{{
  bool onMove (bool right, cPoint pos, cPoint inc) {

    if (mWindow->getControl()) {
      cView::onMove (right, pos, inc);
      mMap->changed();
      }
    else
      mMap->incPix (int(-inc.x), int(-inc.y));

    return true;
    }
  //}}}
  //{{{
  void onDraw (ID2D1DeviceContext* dc) {
  // draw map centred on mPixX,mPixY

    int xCentrePix;
    int yCentrePix;
    mMap->getCentrePix (xCentrePix, yCentrePix);

    auto xTopLeftPix = xCentrePix - int(getWidth() / 2.f / mMap->getViewScale());
    auto yTopLeftPix = yCentrePix - int(getHeight() / 2.f / mMap->getViewScale());
    auto xFirstTile = xTopLeftPix / mMap->getTileSize();
    auto yFirstTile = yTopLeftPix / mMap->getTileSize();
    auto xSubTile = float(-(xTopLeftPix % mMap->getTileSize()));
    auto ySubTile = float(-(yTopLeftPix % mMap->getTileSize()));
    cRect dstRect (xSubTile, ySubTile, xSubTile + mMap->getTileSize(), ySubTile + mMap->getTileSize());

    dc->SetTransform (mView2d.mTransform);

    auto yTile = yFirstTile;
    while (dstRect.top < (getHeight() / mMap->getViewScale())) {
      dstRect.left = xSubTile;
      dstRect.right = dstRect.left + mMap->getTileSize();
      auto xTile = xFirstTile;
      while (dstRect.left < (getWidth() /mMap->getViewScale())) {
        //{{{  find and draw tileX,tileY
        auto dst = cRect (getTL() + dstRect.getTL(), getTL() + dstRect.getBR());

        ID2D1Bitmap* bitmap = mMap->getBitmapFromTileXY (xTile, yTile);
        if (bitmap) {
          auto src = cRect (cPoint(mMap->getTileSize(), mMap->getTileSize()));

          if (mView2d.getScale() == 1.f) {
            // clip bitmap draws to mRect, simpler for viewScale == 1.f
            if (dst.left < mRect.left) {
              src.left += mRect.left - dst.left;
              dst.left = mRect.left;
              }
            if (dst.top < mRect.top) {
              src.top += mRect.top - dst.top;
              dst.top = mRect.top;
              }
            if (dst.right > mRect.right) {
              src.right -= dst.right - mRect.right;
              dst.right = mRect.right;
              }
            if (dst.bottom > mRect.bottom) {
              src.bottom -= dst.bottom - mRect.bottom;
              dst.bottom = mRect.bottom;
              }
            }
          dc->DrawBitmap (bitmap, dst, 1.f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, src);
          }
        else
          dc->DrawRectangle (dst, mWindow->getGreenBrush());
        //}}}
        //{{{  draw debug
        //dc->DrawRectangle (dst, mWindow->getOrangeBrush());

        //auto str = mMap->getQuadKeyFromTileXY (xTile, yTile);

        //IDWriteTextLayout* textLayout;
        //mWindow->getDwriteFactory()->CreateTextLayout (
          //wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
          //mWindow->getTextFormat(), getWidth(), getHeight(), &textLayout);
        //dc->DrawTextLayout (dst.getCentre() + cPoint(2.f,2.f), textLayout, mWindow->getBlackBrush());
        //dc->DrawTextLayout (dst.getCentre(), textLayout, mWindow->getWhiteBrush());

        //textLayout->Release();
        //}}}
        xTile++;
        dstRect.left = dstRect.right;
        dstRect.right += mMap->getTileSize();
        }
      yTile++;
      dstRect.top = dstRect.bottom;
      dstRect.bottom += mMap->getTileSize();
      }

    dc->SetTransform (Matrix3x2F::Identity());

    auto r = cRect (getTL(), getTL() + cPoint(250.f, kTextHeight));
    drawDebug (dc, "Lat:" + dec(mMap->getCentreLat()) +
                   " Lon:" + dec(mMap->getCentreLon()) +
                   //" " + dec(xCentrePix / mMap->getTileSize()) + "." + dec(xCentrePix % mMap->getTileSize()) +
                   //"," + dec(yCentrePix / mMap->getTileSize()) + "." + dec(yCentrePix % mMap->getTileSize()) +
                   " zoom:" + dec(mMap->getZoom()) +
                   " " + dec(mMap->getViewScale()), r);
    drawDebug (dc, "down:" + dec(mMap->getNumDownloads()) + " empty:" + dec(mMap->getNumEmptyDownloads()), r);
    }
  //}}}
  //{{{
  void onResize (ID2D1DeviceContext* dc) {
    mMap->setView (getWidthInt(), getHeightInt());
    cView::onResize (dc);
    }
  //}}}

private:
  cMap*& mMap;
  };
//}}}
//{{{
class cMapOverView : public cD2dWindow::cBox {
public:
  //{{{
  cMapOverView (cD2dWindow* window, float width, float height, cMap*& map)
      : cBox("mapOverView", window, width, height), mMap(map) {

    //mPin = true;
    window->getDc()->CreateSolidColorBrush (ColorF (ColorF::MediumSeaGreen), &mLightGreenBrush);
    }
  //}}}
  //{{{
  virtual ~cMapOverView() {
    mLightGreenBrush->Release();
    }
  //}}}

  //{{{
  bool onWheel (int delta, cPoint pos)  {

    mMap->incZoom (delta/120);
    return true;
    }
  //}}}
  //{{{
  bool onDown (bool right, cPoint pos)  {

    setLatLon (pos);

    //togglePin();

    return true;
    }
  //}}}
  //{{{
  bool onMove (bool right, cPoint pos, cPoint inc) {

    setLatLon (pos);
    return true;
    }
  //}}}
  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    // cMapView dimensions in TileXY cordinates, how do we get them here
    auto viewSize = mMap->getView();

    dc->FillRectangle (mRect, mWindow->getTransparentBlackBrush());

    auto colour = ColorF(0.1f,0,0.1f, 1.f);
    auto zoomTileSet = mMap->getZoomTileSet();
    if (!zoomTileSet->mTileMap.empty()) {
      auto xScale = getWidth() / mMap->getRangeX();
      auto yScale = getHeight() / mMap->getRangeY();
      auto scale = xScale > yScale ? yScale : xScale;
      auto dotWidth = float(scale / zoomTileSet->mScale);
      auto dotSize = cPoint (dotWidth < 1.f ? 1.f : dotWidth + 1.f);

      int xTile;
      int yTile;
      for (auto tile : zoomTileSet->mTileMap) {
        //mMap->getTileXYFromQuadKey (tile.first, xTile, yTile);
        auto xScaledTile = tile.second.mScaledX;
        auto yScaledTile = tile.second.mScaledY;
        auto dot = cPoint ((xScaledTile - mMap->getMinTileRangeX()) * scale,
                           (yScaledTile - mMap->getMinTileRangeY()) * scale);

        mLightGreenBrush->SetColor (tile.second.mColor);
        dc->FillRectangle (cRect (getTL() + dot, getTL() + dot + dotSize),
                           tile.second.mBitmap ? mLightGreenBrush : mWindow->getGreenBrush());
        }

      if (false)
        for (auto tile : zoomTileSet->mEmptyTileSet) {
          mMap->getTileXYFromQuadKey (tile, xTile, yTile);
          auto xScaledTile = xTile / zoomTileSet->mScale;
          auto yScaledTile = yTile / zoomTileSet->mScale;
          auto dot = cPoint ((xScaledTile - mMap->getMinTileRangeX()) * scale,
                             (yScaledTile - mMap->getMinTileRangeY()) * scale);
          dc->FillRectangle (cRect (getTL() + dot, getTL() + dot + dotSize), mWindow->getBlueBrush());
          }


      // draw viewSize at centre graphic
      int xCentrePix;
      int yCentrePix;
      mMap->getCentrePix (xCentrePix, yCentrePix);

      auto xScaledTile = xCentrePix / (float)mMap->getTileSize() / zoomTileSet->mScale;
      auto yScaledTile = yCentrePix / (float)mMap->getTileSize() / zoomTileSet->mScale;
      auto dot = cPoint ((xScaledTile - mMap->getMinTileRangeX()) * scale,
                         (yScaledTile - mMap->getMinTileRangeY()) * scale);
      auto xySize = (viewSize / (float)mMap->getTileSize() / zoomTileSet->mScale) * scale;
      dc->DrawRectangle (cRect (getTL() + dot - xySize/2.f, getTL() + dot + xySize/2.f), mWindow->getWhiteBrush());

      // draw debug
      auto xTiles = int(mMap->getRangeX() * zoomTileSet->mScale);
      auto yTiles = int(mMap->getRangeY() * zoomTileSet->mScale);
      auto r = cRect (getTL(), getTL() + cPoint(100.f, kTextHeight));
      drawDebug (dc, "tiles ", (int)zoomTileSet->mTileMap.size(), r);
      drawDebug (dc, "empty ", (int)zoomTileSet->mEmptyTileSet.size(), r);
      drawDebug (dc, "xtiles ", xTiles, r);
      drawDebug (dc, "ytiles ", yTiles, r);
      }

    dc->DrawRectangle (mRect, mWindow->getWhiteBrush());
    }
  //}}}

private:
  //{{{
  void setLatLon (cPoint pos) {

    auto xScale = getWidth() / mMap->getRangeX();
    auto yScale = getHeight() / mMap->getRangeY();
    auto scale = xScale > yScale ? yScale : xScale;

    auto xCentrePix = (pos.x / scale) + mMap->getMinTileRangeX();
    auto yCentrePix = (pos.y / scale) + mMap->getMinTileRangeY();

    mMap->setCentreLatLonFromNormalisedXY (xCentrePix, yCentrePix);
    }
  //}}}

  cMap*& mMap;
  ID2D1SolidColorBrush* mLightGreenBrush;
  };
//}}}

class cAppWindow : public cD2dWindow {
public:
  //{{{
  const cMapSpec kOrdSurveyUk = {
    50.10319,60.15456, -7.64133,1.75159, // lat,lon limits
    10,15, 256, "/Maps/os",
    "ecn.t{}.tiles.virtualearth.net", "tiles/r{}?g=5938&lbl=l1&productSet=mmOS&key={}" };
  //}}}
  //{{{
  const cMapSpec kAerial = {
    -85.05112878, 85.05112878, -180., 180., // lat,lon limits
    3, 18, 256, "/Maps/aerial",
    "ecn.t{}.tiles.virtualearth.net", "tiles/a{}.jpeg?g=5939&key={}"};
  //}}}
  //{{{
  const cMapSpec kRoadUk = {
    50.10319,60.15456, -7.64133,1.75159, // lat,lon limits
    10, 13, 256, "/Maps/road",
    "ecn.t{}.tiles.virtualearth.net", "tiles/r{}.jpeg?g=5939&mkt=en-GB&shading=hill&key={}"};
  //}}}
  const cMapPlace kPerranporth = { 50.3444, -5.1544, 14 };

  //{{{
  void run (const string& title, int width, int height, const string& arg) {

    initialise (title, width, height, true);

    auto mapPlace = new cMapPlace (kPerranporth);

    mMaps.push_back (new cMap (kOrdSurveyUk, mapPlace, this, arg));
    mMaps.push_back (new cMap (kAerial, mapPlace, this, arg));
    //mMaps.push_back (new cMap (kRoadUk, mapPlace, this));
    mMap = mMaps[0];

    addBox (new cMapView (this, 0,0, mMap));
    addBox (new cLogBox (this, 200.f,0, true), -200.f,0);
    addBox (new cMapOverView (this, 1920/8, 1080/3, mMap), -1920/8, -1080/3);
    addBox (new cCalendarBox (this, 190.f,150.f, mTimePoint), -190.f,0.f);
    addBox (new cClockBox (this, 40.f, mTimePoint), -82.f,150.f);
    addBox (new cWindowBox (this, 60.f,24.f), -60.f,0);
    addBox (new cFloatBox (this, 50.f,kTextHeight, mRenderTime), -50.f,-kTextHeight);

    addBox (new cIndexBox (this, 100.f,3*20.f, {"os", "aerial"}, mMapIndex, &mMapIndexSem), 0.f, 80.f);

    // launch threads
    for (auto map : mMaps) {
      thread([=]() { map->fileThread(); }).detach();
      for (auto threadNum = 0; threadNum < kLoadThreads; threadNum++)
        thread([=]() { map->loadThread (threadNum); }).detach();
      }

    thread([=]() { changeThread(); }).detach();

    // loop till quit
    messagePump();

    //for (auto map : mMaps)
    delete mMap;
    delete mapPlace;
    }
  //}}}

protected:
  //{{{
  bool onKey (int key) {

    switch (key) {
      case 0x00: break;
      case 0x1B: return true;

      case  ' ':
        mMapIndex = (mMapIndex + 1) % mMaps.size();
        mMapIndexSem.notify();
        break;

      case  'C':  mMap->clear(); changed(); break;

      case 0x21:  mMap->incZoom (-1); break; // page up
      case 0x22:  mMap->incZoom (+1); break; // page down
      case 0x25:  mMap->incPix (- mMap->getTileSize(), 0); break; // left arrow
      case 0x27:  mMap->incPix (+ mMap->getTileSize(), 0); break; // right arrow
      case 0x26:  mMap->incPix (0, - mMap->getTileSize()); break; // up arrow
      case 0x28:  mMap->incPix (0, + mMap->getTileSize()); break; // down arrow

      case  'F': toggleFullScreen(); break;
      case  'D': {
        //{{{  dump empty
        mMap->dumpEmpty();
        break;
        }
        //}}}
      default  : printf ("key %x\n", key);
      }

    return false;
    }
  //}}}

private:
  //{{{
  void changeThread() {

    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::log (LOGNOTICE, "changeThread - start");

    while (true) {
      mMapIndexSem.wait();
      mMap = mMaps[mMapIndex];
      mMap->changed();
      }

    cLog::log (LOGNOTICE, "changeThread - exit");
    CoUninitialize();
    }
  //}}}

  vector <cMap*> mMaps;
  cMap* mMap;

  int mMapIndex = 0;
  cSemaphore mMapIndexSem;
  };

//{{{
int __stdcall WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::init (LOGINFO3, true);

  int numArgs;
  auto args = CommandLineToArgvW (GetCommandLineW(), &numArgs);

  string arg;
  if (numArgs > 1) {
    // get fileName from commandLine
    wstring wstr (args[1]);
    arg = string (wstr.begin(), wstr.end());
    }
  else
    arg = "D:";
  cLog::log (LOGNOTICE, "arg - " + arg);

  WSADATA wsaData;
  if (WSAStartup (MAKEWORD(2,2), &wsaData)) {
    //{{{  error exit
    cLog::log (LOGERROR, "WSAStartup failed");
    exit (0);
    }
    //}}}

  cAppWindow appWindow;
  appWindow.run ("webWindow", 1920/2 , 1080/2, arg);

  CoUninitialize();
  }
//}}}
