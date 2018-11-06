// pdfWindow.cpp
//{{{  includes
#include "stdafx.h"
#include "../../shared/utils/resolve.h"

#include "../boxes/cFloatBox.h"
#include "../boxes/cLogBox.h"
#include "../boxes/cWindowBox.h"
#include "../boxes/cClockBox.h"
#include "../boxes/cCalendarBox.h"

using namespace concurrency;
//}}}
//{{{  const
const int kFullScreen = false;
const int kThumbThreads = 2;
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

    //add (new cImageSetView (this, 0.f,0.f, mImageSet));
    //mJpegImageView = new cJpegImageView (this, 0.f,0.f, nullptr);
    //add (mJpegImageView);

    add (new cWindowBox (this, 60.f,24.f), -60.f,0.f);
    add (new cFloatBox (this, 50.f, kLineHeight, mRenderTime), 0.f,-kLineHeight);

    if (name.find (".lnk") <= name.size()) {
      string fullName;
      if (resolveShortcut (name.c_str(), fullName))
        name = fullName;
      }
    //thread ([=]() { filesThread (name); } ).detach();

    //for (auto i = 0; i < kThumbThreads; i++)
    //  thread ([=]() { thumbsThread (i); } ).detach();
    mContext = fz_new_context (NULL, NULL, FZ_STORE_DEFAULT);
    mColorspace = fz_device_bgr (mContext);
    openFile (name.c_str());

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

      case 0x23: changed(); break;   // end
      case 0x24: changed();  break; // home

      case 0x25: changed();  break;    // left arrow
      case 0x26: changed();  break; // up arrow
      case 0x27: changed();  break;    // right arrow
      case 0x28: changed();  break; // down arrow

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
  void openFile (const char* filename) {

    pdf_document* idoc;

    fz_try (mContext) {
      fz_register_document_handlers (mContext);
      fz_set_use_document_css (mContext, mLayout_use_doc_css);
      mDocument = fz_open_document (mContext, filename);
      }
    fz_catch (mContext) {
      cLog::log (LOGERROR, "cannot open document");
      }

    idoc = pdf_specifics (mContext, mDocument);
    if (idoc) {
      fz_try (mContext) {
        pdf_enable_js (mContext, idoc);
        //pdf_set_doc_event_callback (mContext, idoc, event_cb, app);
        }
      fz_catch (mContext) {
        cLog::log (LOGERROR, "cannot load document javascript");
        }
      }

    fz_try (mContext) {
      if (fz_needs_password (mContext, mDocument)) {
        cLog::log (LOGERROR, "document needs password");
        }

      mDocumentPath = fz_strdup (mContext, filename);
      mDocumentTitle = (char*)filename;
      if (strrchr (mDocumentTitle, '\\'))
        mDocumentTitle = strrchr (mDocumentTitle, '\\') + 1;
      if (strrchr (mDocumentTitle, '/'))
        mDocumentTitle = strrchr (mDocumentTitle, '/') + 1;
      mDocumentTitle = fz_strdup (mContext, mDocumentTitle);

      fz_layout_document (mContext, mDocument, mLayoutWidth, mLayoutHeight, mLayoutEm);

      while (true) {
        fz_try (mContext) {
          mPageCount = fz_count_pages (mContext, mDocument);
          if (mPageCount <= 0)
            cLog::log (LOGERROR, "no pages");
            }
        fz_catch (mContext) {
          if (fz_caught (mContext) == FZ_ERROR_TRYLATER) {
            cLog::log (LOGERROR,  "not enough data to count pages yet");
            continue;
            }
          fz_rethrow (mContext);
          }
        break;
        }
      cLog::log (LOGERROR, "pages %d", mPageCount);

      while (true) {
        fz_try (mContext) {
          mOutline = fz_load_outline (mContext, mDocument);
          }
        fz_catch (mContext) {
          mOutline = NULL;
          if (fz_caught (mContext) == FZ_ERROR_TRYLATER)
            cLog::log (LOGERROR, "load outline later");
          else
            cLog::log (LOGERROR,  "failed to load outline");
          }
          break;
        }
      }
    fz_catch (mContext) {
      cLog::log (LOGERROR,  "cannot open document");
      }
    }
  //}}}
  //{{{
  void close() {

    fz_drop_display_list (mContext, mPageList);
    mPageList = NULL;

    fz_drop_display_list (mContext, mAnnotationsList);
    mAnnotationsList = NULL;

    fz_drop_stext_page (mContext, mPageText);
    mPageText = NULL;

    fz_drop_link (mContext, mPageLinks);
    mPageLinks = NULL;

    fz_free (mContext, mDocumentTitle);
    mDocumentTitle = NULL;

    fz_free (mContext, mDocumentPath);
    mDocumentPath = NULL;

    fz_drop_pixmap (mContext, mImage);
    mImage = NULL;

    fz_drop_outline (mContext, mOutline);
    mOutline = NULL;

    fz_drop_page (mContext, mPage);
    mPage = NULL;

    fz_drop_document (mContext, mDocument);
    mDocument = NULL;

    fz_flush_warnings (mContext);
    }
  //}}}

  //{{{
  void loadPage (int number) {
    mPageNumber = number;
    }
  //}}}
  //{{{
  void showPage() {
    }
  //}}}

  // vars
  cSemaphore mFileScannedSem;

  fz_context* mContext = NULL;
  fz_document* mDocument = NULL;
  char* mDocumentPath = NULL;
  char* mDocumentTitle = NULL;
  fz_outline* mOutline = NULL;

  float mLayoutWidth = 450;
  float mLayoutHeight = 600;
  float mLayoutEm = 12;
  char* mLayout_css = NULL;
  int mLayout_use_doc_css = 1;

  int mPageCount = 0;
  fz_pixmap* mImage = NULL;
  fz_colorspace* mColorspace = NULL;

  int mPageNumber = 0;
  fz_page* mPage = NULL;
  fz_rect mPageBoundingBox;
  fz_display_list* mPageList = NULL;
  fz_display_list* mAnnotationsList = NULL;
  fz_stext_page* mPageText = NULL;
  fz_link* mPageLinks = NULL;
  fz_quad mHitBoundingBox[512];
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
    cLog::log (LOGINFO, "pdfWindow resolved " + rootDirName);
    }
  else
    rootDirName = "C:/Users/colin/Pictures";

  cAppWindow window;
  window.run ("pdfWindow", 1920/2, 1080/2, rootDirName);

  CoUninitialize();
  }
//}}}
