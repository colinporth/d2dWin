// jpegWindow.cpp
//{{{  includes
#include "stdafx.h"

#include "../common/cJpegImage.h"

#include "../common/box/cFloatBox.h"
#include "../common/box/cLogBox.h"
#include "../common/box/cWindowBox.h"
#include "../common/box/cClockBox.h"
#include "../common/box/cCalendarBox.h"
#include "../common/cJpegImageView.h"

using namespace concurrency;
//}}}
//{{{  const
const int kFullScreen = true;
const int kThumbThreads = 2;
const float kPi = 3.14159265358979323846f;
//}}}

//{{{
bool resolveShortcut (const std::string& shortcut, std::string& fullName) {

  // get IShellLink interface
  IShellLinkA* iShellLink;
  if (CoCreateInstance (CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**)&iShellLink) == S_OK) {
    // get IPersistFile interface
    IPersistFile* iPersistFile;
    iShellLink->QueryInterface (IID_IPersistFile,(void**)&iPersistFile);

    // IPersistFile uses LPCOLESTR, ensure string is Unicode
    WCHAR wideShortcutFileName[MAX_PATH];
    MultiByteToWideChar (CP_ACP, 0, shortcut.c_str(), -1, wideShortcutFileName, MAX_PATH);

    // open shortcut file and init it from its contents
    if (iPersistFile->Load (wideShortcutFileName, STGM_READ) == S_OK) {
      // find target of shortcut, even if it has been moved or renamed
      if (iShellLink->Resolve (NULL, SLR_UPDATE) == S_OK) {
        // get the path to shortcut
        char szPath[MAX_PATH];
        WIN32_FIND_DATAA wfd;
        if (iShellLink->GetPath (szPath, MAX_PATH, &wfd, SLGP_RAWPATH) == S_OK) {
          // Get the description of the target
          char szDesc[MAX_PATH];
          if (iShellLink->GetDescription (szDesc, MAX_PATH) == S_OK) {
            lstrcpynA ((char*)fullName.c_str(), szPath, MAX_PATH);
            return true;
            }
          }
        }
      }
    }

  fullName[0] = '\0';
  return false;
  }
//}}}

class cAppWindow : public cD2dWindow {
public:
  cAppWindow() : mFileScannedSem("fileScanned") {}
  //{{{
  void run (const string& title, int width, int height, string name) {

    initialise (title, width, height, kFullScreen);
    add (new cClockBox (this, 50.f, mTimePoint), -110.f,-120.f);
    add (new cCalendarBox (this, 190.f,150.f, mTimePoint), -190.f-120.f,-150.f);
    add (new cLogBox (this, 200.f,0.f, true), -200.f,0);

    add (new cImageSetView (this, 0.f,0.f, mImageSet));
    mJpegImageView = new cJpegImageView (this, 0.f,0.f, nullptr);
    add (mJpegImageView);

    add (new cWindowBox (this, 60.f,24.f), -60.f,0.f);
    add (new cFloatBox (this, 50.f, kLineHeight, mRenderTime), 0.f,-kLineHeight);

    if (name.find (".lnk") <= name.size()) {
      string fullName;
      if (resolveShortcut (name.c_str(), fullName))
        name = fullName;
      }
    thread ([=]() { filesThread (name); } ).detach();

    for (auto i = 0; i < kThumbThreads; i++)
      thread ([=]() { thumbsThread (i); } ).detach();

    messagePump();
    };
  //}}}

protected:
  //{{{
  bool onKey (int key) {

    switch (key) {
      case 0x00: break;
      case 0x10: changed(); break; // shift
      case 0x11: break; // control

      case 0x1B: return true; // escape abort

      case  ' ': break; // space bar

      case 0x21: break; // page up
      case 0x22: break; // page down

      case 0x23: mJpegImageView->setImage (mImageSet.selectLastIndex()); changed(); break;   // end
      case 0x24: mJpegImageView->setImage (mImageSet.selectFirstIndex()); changed();  break; // home

      case 0x25: mJpegImageView->setImage (mImageSet.selectPrevIndex()); changed();  break;    // left arrow
      case 0x26: mJpegImageView->setImage (mImageSet.selectPrevRowIndex()); changed();  break; // up arrow
      case 0x27: mJpegImageView->setImage (mImageSet.selectNextIndex()); changed();  break;    // right arrow
      case 0x28: mJpegImageView->setImage (mImageSet.selectNextRowIndex()); changed();  break; // down arrow

      case 'F':  toggleFullScreen(); break;

      default: cLog::log (LOGERROR, "unused key %x", key);
      }

    return false;
    }
  //}}}
  //{{{
  bool onKeyUp (int key) {

    switch (key) {
      case 0x10: changed(); break; // shift
      default: break;
      }

    return false;
    }
  //}}}

private:
  //{{{
  class cDirectory {
  public:
    //{{{
    cDirectory (const string& pathName, const string& dirName, int depth)
      :  mPathName(pathName), mDirName(dirName), mDepth(depth) {}
    //}}}
    ~cDirectory() {}

    string getDirName() { return mDirName; }
    string getPathName() { return mPathName; }
    string getPathDirName() { return mPathName + "/" + mDirName; }

    int getDepth() { return mDepth; }
    bool getSelected() { return mSelected; }

    //{{{
    void toggleSelect() {
      mSelected = !mSelected;
      }
    //}}}

  private:
    string mPathName;
    string mDirName;

    int mDepth;
    bool mSelected = false;
    };
  //}}}
  //{{{
  class cImageSet {
  public:
    //{{{  gets
    //{{{
    int getNumImages() {

      return (int)mImages.size();
      }
    //}}}

    //{{{
    int getNumRows() {
      return ((getNumImages() - 1) / mColumns) + 1;
      }
    //}}}
    //{{{
    int getNumColumns() {
      return mColumns;
      }
    //}}}

    //{{{
    cPoint getThumbSize() {
      return kThumbSize;
      }
    //}}}
    //{{{
    cPoint getSrcSize() {
      return cPoint (getNumColumns() * kThumbSize.x, getNumRows() * kThumbSize.y);
      }
    //}}}

    //{{{
    int getBestThumbIndex() {

      auto columns = getNumColumns();

      int bestMetric = INT_MAX;
      int bestThumbImageIndex = -1;

      for (int i = 0; i < getNumImages(); i++) {
        if (!mImages[i]->isThumbAvailable()) {
          auto x = i % columns;
          auto y = i / columns;
          auto metric = x*x + y*y;
          if (metric < bestMetric) {
            bestMetric = metric;
            bestThumbImageIndex = i;
            }
          }
        }

      return bestThumbImageIndex;
      }
    //}}}
    //{{{
    cRect getRectByIndex (int index) {

      auto row = index % mColumns;
      auto column = index / mColumns;
      return cRect (row*kThumbSize.x, column*kThumbSize.y, (row+1)*kThumbSize.x, (column+1)*kThumbSize.y);
      }
    //}}}
    //{{{
    cJpegImage* getImageByIndex (int index) {

      return (index >= 0) ? mImages[index] : nullptr;
      }
    //}}}

    //{{{
    int getPickIndex() {
      return mPickIndex;
      }
    //}}}
    //{{{
    int getSelectIndex() {
      return mSelectIndex;
      }
    //}}}

    //{{{
    int getNumDirs() {

      return (int)mDirs.size();
      }
    //}}}
    //{{{
    int getMaxDirDepth() {

      return mMaxDirDepth;
      }
    //}}}
    //}}}
    //{{{  selects
    cJpegImage* selectImageByIndex (int index) {
      mSelectIndex = max (0, min (getNumImages() - 1, index));
      return mImages[mSelectIndex];
      }

    cJpegImage* selectFirstIndex() { return selectImageByIndex (0); }
    cJpegImage* selectLastIndex() { return selectImageByIndex (getNumImages() - 1); }

    cJpegImage* selectPrevIndex() { return selectImageByIndex (mSelectIndex - 1); }
    cJpegImage* selectNextIndex() { return selectImageByIndex (mSelectIndex + 1); }

    cJpegImage* selectPrevRowIndex() { return selectImageByIndex (mSelectIndex - getNumColumns()); }
    cJpegImage* selectNextRowIndex() { return selectImageByIndex (mSelectIndex + getNumColumns()); }
    //}}}
    //{{{
    void loadInfoByIndex (int index) {
      mImages[index]->loadInfo();
      mExifTimePoint = mImages[index]->getExifTimePoint();
      }
    //}}}

    //{{{
    int pick (cPoint pos) {

      for (auto i = 0; i < getNumImages(); i++) {
        if (getRectByIndex (i).inside (pos)) {
          mPickIndex = i;
          return mPickIndex;
          }
        }

      mPickIndex = -1;
      return mPickIndex;
      }
    //}}}
    //{{{
    void drawBitmaps (ID2D1DeviceContext* dc) {

      for (auto i = 0; i < getNumImages(); i++)
        if (mImages[i]->getThumbBitmap())
          dc->DrawBitmap (mImages[i]->getThumbBitmap(), getRectByIndex (i));
      }
    //}}}
    //{{{
    void drawHighlights (ID2D1DeviceContext* dc, cView2d view2d, ID2D1SolidColorBrush* brush) {

      for (auto i = 0; i < getNumImages(); i++)
        if (mImages[i]->getBitmap())
          dc->DrawRoundedRectangle (
            RoundedRect (getRectByIndex (i), 2.f,2.f), brush, 2.f/view2d.getScale());
      }
    //}}}

    //{{{
    void fileScan (const string& parentDirName, const string& dirName, const string& matchName, int depth) {
    // recursive filescan

      mMaxDirDepth = max (mMaxDirDepth, depth);

      string indexStr;
      for (auto i = 0; i < depth; i++)
        indexStr += "  ";
      cLog::log (LOGINFO, "fileScan - " + indexStr + dirName);

      auto fullDirName = parentDirName.empty() ? dirName : parentDirName + "/" + dirName;

      mDirs.push_back (new cDirectory(parentDirName, dirName, depth));

      auto searchStr = fullDirName +  "/*";
      WIN32_FIND_DATA findFileData;
      auto file = FindFirstFileEx (searchStr.c_str(), FindExInfoBasic, &findFileData,
                                   FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
      if (file != INVALID_HANDLE_VALUE) {
        do {
          if ((findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
              (findFileData.cFileName[0] != '.'))
            // recursive scan of directory
            fileScan (fullDirName, findFileData.cFileName, matchName, depth+1);

          else if (PathMatchSpec (findFileData.cFileName, matchName.c_str())) {
            // add new file
            auto image = new cJpegImage();
            image->setFile (fullDirName, findFileData.cFileName);
            mImages.push_back (image);
            }

          } while (FindNextFile (file, &findFileData));

        FindClose (file);
        }

      mColumns = (int)sqrt (getNumImages())*11/12 + 1;
      }
    //}}}

    concurrent_vector<cDirectory*> mDirs;
    uint32_t mAllocSize = 0;

    chrono::time_point<chrono::system_clock> mExifTimePoint;

  private:
    const cPoint kThumbSize = cPoint(160.f, 120.f);

    // vars
    concurrent_vector<cJpegImage*> mImages;

    int mPickIndex = -1;
    int mSelectIndex = -1;

    int mMaxDirDepth = 0;
    int mColumns = 1;
    };
  //}}}

  //{{{
  class cImageSetView : public cView {
  public:
    //{{{
    cImageSetView (cD2dWindow* window, float width, float height, cImageSet& imageSet)
        : cView("imageSet", window, width, height), mImageSet(imageSet) {
      mPin = true;
      window->getDc()->CreateSolidColorBrush ({0.25f, 0.25f, 0.25f, 1.f }, &mBrush);
      }
    //}}}
    //{{{
    virtual ~cImageSetView() {
      mBrush->Release();
      }
    //}}}

    // overrides
    //{{{
    cPoint getSrcSize() {
      return mImageSet.getSrcSize();
      }
    //}}}
    //{{{
    void layout() {

      // fit half width of window
      cView::layout();
      mView2d.setScale (mImageSet.getSrcSize(), mWindow->getSize()*cPoint (0.5f, 1.f));
      mView2d.setPos (cPoint (0.f, kLineHeight));
      }
    //}}}
    //{{{
    bool pick (bool inClient, cPoint pos, bool& change) {

      auto pick = cView::pick (inClient, pos, change);
      if (!pick) {
        cRect r(0,0, 40.f, mImageSet.mDirs.size() * kLineHeight);
        pick = r.inside (pos);
        }

      return pick;
      }
    //}}}
    //{{{
    bool onProx (bool inClient, cPoint pos) {

      if (inClient) {
        if (mWindow->getControl() || mWindow->getShift()) {
          if (!mLastLocked) {
            mFirstLockedPos = pos;
            mLastLocked = true;
            }
          pos.y = mFirstLockedPos.y;
          }
        else
          mLastLocked = false;

        auto pickIndex = mImageSet.pick (mView2d.getDstToSrc (pos));
        if (pickIndex >= 0) {
          if (mWindow->getShift())
            mImageSet.selectImageByIndex (pickIndex);
          else
            mImageSet.loadInfoByIndex (pickIndex);
          return true;
          }
        }

      return true;
      }
    //}}}
    //{{{
    bool onUp (bool right, bool mouseMoved, cPoint pos) {

      if (!mouseMoved) {
        if (mImageSet.getPickIndex() >= 0) {
          auto image = mImageSet.selectImageByIndex (mImageSet.getPickIndex());
          auto appWindow = dynamic_cast<cAppWindow*>(mWindow);
          appWindow->mJpegImageView->setImage (image);
          return true;
          }
        else {
          cRect r(0,0, 40.f, 0);
          for (auto dir : mImageSet.mDirs) {
            r.bottom = r.top + kLineHeight;
            if (r.inside (pos)) {
              dir->toggleSelect();
              return true;
              }
            r.top = r.bottom;
            }
          }
        }

      return false;
      }
    //}}}
    //{{{
    void onDraw (ID2D1DeviceContext* dc) {

      auto r = mRect;
      //{{{  draw dirs
      for (auto dir : mImageSet.mDirs) {
        string str;
        r.bottom = r.top + kLineHeight;
        for (auto i = 0; i < dir->getDepth(); i++)
          str += " -";
        str += dir->getDirName();
        dc->DrawText (wstring (str.begin(), str.end()).data(), (uint32_t)str.size(), mWindow->getTextFormat(),
                      r, dir->getSelected() ? mWindow->getYellowBrush() : mWindow->getWhiteBrush());
        r.top = r.bottom;
        }
      //}}}

      const auto tl = mView2d.getSrcToDst (cPoint());
      const auto br = mView2d.getSrcToDst (mImageSet.getSrcSize());

      const auto image = mImageSet.getImageByIndex (mImageSet.getPickIndex());
      if (image) {
        //{{{  calc panel height
        auto height = kLineHeight + kLineHeight;
        if (!image->getMakeString().empty() || !image->getModelString().empty())
          height += kLineHeight;
        if (image->getOrientation())
          height += kLineHeight;
        if (image->getFocalLength() > 0)
          height += kLineHeight;
        if (image->getExposure() > 0)
          height += kLineHeight;
        if (image->getAperture() > 0)
          height += kLineHeight;
        if (!image->getGpsString().empty())
          height += kLineHeight;

        height = kBorder + max (mImageSet.getThumbSize().y + kLineHeight, height) + kBorder;
        //}}}

        dc->FillRectangle (cRect(tl.x-1.f, br.y+1.f, br.x+1.f, br.y + height), mBrush);
        //{{{  draw panel text
        cRect r (tl.x + kBorder, br.y + kBorder, br.x, br.y + kBorder + kLineHeight);
        //{{{  draw fullFileName text
        string str = image->getPathFileName();
        dc->DrawText (wstring (str.begin(), str.end()).data(), (uint32_t)str.size(), mWindow->getTextFormat(),
                      r, mWindow->getWhiteBrush());
        r.top = r.bottom;
        r.bottom += kLineHeight;
        //}}}
        //{{{  draw imageSize text
        str = dec(image->getImageSize().x) + "x" + dec(image->getImageSize().y);
        dc->DrawText (wstring (str.begin(), str.end()).data(), (uint32_t)str.size(), mWindow->getTextFormat(),
                      r, mWindow->getWhiteBrush());
        r.top = r.bottom;
        r.bottom += kLineHeight;
        //}}}

        if (!image->getMakeString().empty() || !image->getModelString().empty()) {
          //{{{  draw make model text
          string str = image->getMakeString() + " " + image->getModelString();
          dc->DrawText (wstring (str.begin(), str.end()).data(), (uint32_t)str.size(), mWindow->getTextFormat(),
                        r, mWindow->getWhiteBrush());
          r.top = r.bottom;
          r.bottom += kLineHeight;
          }
          //}}}
        if (image->getOrientation()) {
          //{{{  draw orientation text
          string str = "orientation " + dec(image->getOrientation());
          dc->DrawText (wstring (str.begin(), str.end()).data(), (uint32_t)str.size(), mWindow->getTextFormat(),
                        r, mWindow->getWhiteBrush());
          r.top = r.bottom;
          r.bottom += kLineHeight;
          }
          //}}}
        if (image->getFocalLength() > 0) {
          //{{{  draw focalLength text
          string str = "focal length " + dec(image->getFocalLength());
          dc->DrawText (wstring (str.begin(), str.end()).data(), (uint32_t)str.size(), mWindow->getTextFormat(),
                        r, mWindow->getWhiteBrush());
          r.top = r.bottom;
          r.bottom += kLineHeight;
          }
          //}}}
        if (image->getExposure() > 0) {
          //{{{  draw exposure text
          string str = "exposure " + dec(image->getExposure());
          dc->DrawText (wstring (str.begin(), str.end()).data(), (uint32_t)str.size(), mWindow->getTextFormat(),
                        r, mWindow->getWhiteBrush());
          r.top = r.bottom;
          r.bottom += kLineHeight;
          }
          //}}}
        if (image->getAperture() > 0) {
          //{{{  draw aperture text
          string str = "aperture " + dec(image->getAperture());
          dc->DrawText (wstring (str.begin(), str.end()).data(), (uint32_t)str.size(), mWindow->getTextFormat(),
                        r, mWindow->getWhiteBrush());
          r.top = r.bottom;
          r.bottom += kLineHeight;
          }
          //}}}
        if (!image->getGpsString().empty()) {
          //{{{  draw GPSinfo text
          string str = image->getGpsString();
          dc->DrawText (wstring(str.begin(), str.end()).data(), (uint32_t)str.size(), mWindow->getTextFormat(),
                        r, mWindow->getWhiteBrush());
          }
          //}}}
        //}}}

        r = cRect (br.x - kBorder,  br.y + kBorder, br.x - kBorder, br.y + height);
        //{{{  draw thumbBitmap
        if (image->getThumbBitmap()) {
          r.right = r.left;
          r.left -= mImageSet.getThumbSize().x - kBorder;
          r.bottom = r.top + mImageSet.getThumbSize().y;
          dc->DrawBitmap (image->getThumbBitmap(), r);
          }
        //}}}

        if (!image->getExifTimeString().empty()) {
          auto datePoint = floor<date::days>(image->getExifTimePoint());
          //{{{  draw clock
          auto radius = r.getHeight()/2.f - kLineHeight;
          r.right = r.left;
          r.left -= (2.f * radius) + kLineHeight;

          auto centre = r.getCentre();

          auto timeOfDay = date::make_time (chrono::duration_cast<chrono::seconds>(image->getExifTimePoint() - datePoint));
          auto hourRadius = radius * 0.6f;
          auto h = timeOfDay.hours().count();
          auto hourAngle = (1.f - (h / 6.f)) * kPi;
          dc->DrawLine (centre,
                        centre + cPoint(hourRadius * sin (hourAngle), hourRadius * cos (hourAngle)),
                        mWindow->getWhiteBrush(), 2.f);

          auto minRadius = radius * 0.75f;
          auto m = timeOfDay.minutes().count();
          auto minAngle = (1.f - (m/30.f)) * kPi;
          dc->DrawLine (centre,
                        centre + cPoint (minRadius * sin (minAngle), minRadius * cos (minAngle)),
                        mWindow->getWhiteBrush(), 2.f);

          auto secRadius = radius * 0.85f;
          auto s = timeOfDay.seconds().count();
          auto secAngle = (1.f - (s /30.f)) * kPi;
          dc->DrawLine (centre,
                        centre + cPoint (secRadius * sin (secAngle), secRadius * cos (secAngle)),
                        mWindow->getRedBrush(), 2.f);
          dc->DrawEllipse (Ellipse (centre, radius,radius), mWindow->getWhiteBrush(), 2.f);
          //}}}
          //{{{  draw calendar
          const float kCalendarWidth = 26.f;
          r.right = r.left;
          r.left -= 7.f * kCalendarWidth;

          auto yearMonthDay = date::year_month_day{datePoint};
          auto yearMonth = yearMonthDay.year() / date::month{yearMonthDay.month()};
          auto today = yearMonthDay.day();

          IDWriteTextLayout* textLayout;
          //{{{  print month year
          auto p = r.getTL();

          string str = format ("%B", yearMonth);
          mWindow->getDwriteFactory()->CreateTextLayout (wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
                     mWindow->getTextFormat(), getWidth(), getHeight(), &textLayout);
          dc->DrawTextLayout (p, textLayout, mWindow->getWhiteBrush());
          textLayout->Release();

          // print year
          p.x = r.getTL().x + r.getWidth() - 45.f;

          str = format ("%Y", yearMonth);
          mWindow->getDwriteFactory()->CreateTextLayout (wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
                     mWindow->getTextFormat(), getWidth(), getHeight(), &textLayout);
          dc->DrawTextLayout (p, textLayout, mWindow->getWhiteBrush());
          textLayout->Release();

          p.y += kLineHeight;
          //}}}
          //{{{  print daysOfWeek
          p.x = r.getTL().x;

          auto weekDayToday = date::weekday{ yearMonth / today};

          auto titleWeekDay = date::sun;
          do {
            str = format ("%a", titleWeekDay);
            str.resize (2);

            mWindow->getDwriteFactory()->CreateTextLayout (wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
                       mWindow->getTextFormat(), getWidth(), getHeight(), &textLayout);
            dc->DrawTextLayout (p, textLayout,
              weekDayToday == titleWeekDay ?  mWindow->getWhiteBrush() : mWindow->getGreyBrush());
            textLayout->Release();

            p.x += kCalendarWidth;
            } while (++titleWeekDay != date::sun);

          p.y += kLineHeight;
          //}}}
          //{{{  print lines
          // skip leading space
          auto weekDay = date::weekday{ yearMonth / 1};
          p.x = r.getTL().x + ((weekDay - date::sun).count() * kCalendarWidth);

          using date::operator""_d;
          auto curDay = 1_d;
          auto lastDayOfMonth = (yearMonth / date::last).day();

          int line = 1;
           while (curDay <= lastDayOfMonth) {
            // iterate days of week
            str = format ("%e", curDay);
            mWindow->getDwriteFactory()->CreateTextLayout (
              wstring (str.begin(), str.end()).data(), (uint32_t)str.size(),
              mWindow->getTextFormat(), getWidth(), getHeight(), &textLayout);
            dc->DrawTextLayout (p, textLayout, today == curDay ? mWindow->getWhiteBrush() : mWindow->getGreyBrush());
            textLayout->Release();

            if (++weekDay == date::sun) {
              line++;
              p.y += line <= 5 ? kLineHeight : - 4 * kLineHeight;
              p.x = r.getTL().x;
              }
            else
              p.x += kCalendarWidth;

            ++curDay;
            };

          p.y += kLineHeight;
          //}}}
          //}}}
          //{{{  draw exifTime text
          r = cRect (br.x - kBorder - mImageSet.getThumbSize().x,
                     br.y + kBorder + mImageSet.getThumbSize().y,
                     br.x - kBorder, br.y + height);
          str = image->getExifTimeString();
          dc->DrawText (wstring (str.begin(), str.end()).data(), (uint32_t)str.size(), mWindow->getTextFormat(),
                        r, mWindow->getWhiteBrush());
          //}}}
          }
        }

      auto dst = mView2d.getSrcToDst (cRect (getSrcSize()));
      dc->SetTransform (mView2d.mTransform);
      //{{{  draw thumbs
      mImageSet.drawBitmaps (dc);
      mImageSet.drawHighlights (dc, mView2d, mWindow->getBlueBrush());

      dc->DrawRoundedRectangle (
        RoundedRect(cRect(mImageSet.getSrcSize()), kMainBorder, kMainBorder),
        mWindow->getWhiteBrush(), kMainBorder / mView2d.getScale());

      if (mImageSet.getSelectIndex() >= 0)
        dc->DrawRoundedRectangle (
          RoundedRect (mImageSet.getRectByIndex (mImageSet.getSelectIndex()), kThumbBorder,kThumbBorder),
          mWindow->getWhiteBrush(), kThumbBorder/mView2d.getScale());

      if (mImageSet.getPickIndex() >= 0)
        dc->DrawRoundedRectangle (
          RoundedRect (mImageSet.getRectByIndex (mImageSet.getPickIndex()), kThumbBorder,kThumbBorder),
          mWindow->getYellowBrush(), kThumbBorder/mView2d.getScale());
      //}}}
      dc->SetTransform (Matrix3x2F::Identity());

      string str = mImageSet.mDirs[0]->getDirName() + " - " +
                   dec(mImageSet.mAllocSize/1000000) + "m - " +
                   dec(mImageSet.getNumImages()) + " images";
      drawTab (dc, str, dst, mWindow->getLightGreyBrush());
      }
    //}}}

  private:
    const float kMainBorder = 2.f;
    const float kThumbBorder = 3.f;
    const float kBorder = 4.f;

    cImageSet& mImageSet;

    ID2D1SolidColorBrush* mBrush = nullptr;

    bool mLastLocked = false;
    cPoint mFirstLockedPos;
    };
  //}}}

  //{{{
  void filesThread (const string& rootDir) {

    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::setThreadName ("file");

    setChangeCountDown (20);

    mImageSet.fileScan ("", rootDir, "*.jpg", 0);

    mFileScannedSem.notifyAll();

    cLog::log (LOGNOTICE, "exit " +
                          dec(mImageSet.getNumImages()) + " images in " +
                          dec(mImageSet.getNumDirs()) + " directories " +
                          dec(mImageSet.getMaxDirDepth()) + " deep");
    onResize();

    setChangeCountDown (100);
    CoUninitialize();
    }
  //}}}
  //{{{
  void thumbsThread (int threadNum) {

    CoInitializeEx (NULL, COINIT_MULTITHREADED);
    cLog::setThreadName ("thu" + dec(threadNum));

    mFileScannedSem.wait();
    cLog::log (LOGNOTICE, "signalled");

    int count = 0;
    while (true) {
      auto bestThumbImage = mImageSet.getImageByIndex (mImageSet.getBestThumbIndex());
      if (bestThumbImage) {
        auto alloc = bestThumbImage->loadThumb (getDc(), mImageSet.getThumbSize());
        if (alloc) {
          mImageSet.mAllocSize += alloc;
          changed();
          count++;
          }
        }
      else
        break;
      };

    cLog::log (LOGNOTICE, "exit - loaded " + dec (count));
    changed();

    CoUninitialize();
    }
  //}}}

  // vars
  cImageSet mImageSet;
  cJpegImageView* mJpegImageView = nullptr;
  cSemaphore mFileScannedSem;
  };

//{{{
int __stdcall WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

  CoInitializeEx (NULL, COINIT_MULTITHREADED);
  cLog::init (LOGINFO1, true);

  int numArgs;
  auto args = CommandLineToArgvW (GetCommandLineW(), &numArgs);
  string rootDirName;
  if (numArgs > 1) {
    wstring wstr(args[1]);
    rootDirName = string(wstr.begin(), wstr.end());
    cLog::log (LOGINFO, "JpegWindow resolved " + rootDirName);
    }
  else
    rootDirName = "C:/Users/colin/Pictures";

  cAppWindow window;
  window.run ("jpegWindow", 1920/2, 1080/2, rootDirName);

  CoUninitialize();
  }
//}}}
