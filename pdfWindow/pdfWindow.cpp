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

//{{{
class cPdfImage {
public:
   cPdfImage() {}
   ~cPdfImage() {}

  bool isOk() { return true; }
  bool isLoaded() { return mBitmap != nullptr; }

  int getImageLen() { return mImageLen; }

  cPoint getSize() { return mSize; }
  int getWidth() { return mSize.width; }
  int getHeight() { return mSize.height; }

  cPoint getImageSize() { return mImageSize; }
  int getImageWidth() { return mImageSize.width; }
  int getImageHeight() { return mImageSize.height; }

  ID2D1Bitmap* getBitmap() { return mBitmap; }

  uint32_t loadImage (ID2D1DeviceContext* dc, int scale) {
    }
  void releaseImage() {
    }

private:
  uint8_t* mBuf = nullptr;
  int mBufLen = 0;
  int mImageLen = 0;

  D2D1_SIZE_U mImageSize = {0,0};
  D2D1_SIZE_U mSize = {0,0};

  int mLoadScale = 0;
  ID2D1Bitmap* mBitmap = nullptr;
  };
//}}}
//{{{
class cPdfImageView : public cD2dWindow::cView {
public:
  cPdfImageView (cD2dWindow* window, float width, float height, cPdfImage* image)
      : cView("image", window, width, height), mImage(image) {

    mPin = true;
    window->getDc()->CreateSolidColorBrush (ColorF (ColorF::Black), &mBrush);
    }

  virtual ~cPdfImageView() {
    mBrush->Release();
    }

  void setImage (cPdfImage* image) {
    mImage = image;
    }

  // overrides
  //{{{
  cPoint getSrcSize() {
    return (mImage && mImage->isLoaded()) ? mImage->getSize() : getSize();
    }
  //}}}
  //{{{
  void layout() {
    cView::layout();
    mView2d.setPos (getCentre());
    }
  //}}}
  //{{{
  bool onProx (bool inClient, cPoint pos) {

    mPos = pos;
    mSamplePos = mView2d.getDstToSrc (pos);
    mWindow->changed();

    return cView::onProx (inClient, pos);
    }
  //}}}
  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    if (mImage && mImage->isOk()) {
      setScale();

      // needs refreshing after load
      auto dstRect = mView2d.getSrcToDst (cRect(getSrcSize()));
      if (mImage->getBitmap()) {
        // draw bitmap
        dc->SetTransform (mView2d.mTransform);
        dc->DrawBitmap (mImage->getBitmap(), cRect (mImage->getSize()));
        dc->DrawRectangle (cRect (mImage->getSize()), mWindow->getWhiteBrush());
        dc->SetTransform (Matrix3x2F::Identity());
        }
      }
    }
  //}}}

private:
  void setScale () {
    auto dstRect = mView2d.getSrcToDst (cRect(getSrcSize()));
    int scale = 1 + int(mImage->getImageSize().x / dstRect.getWidth());
    auto srcScaleX = getSize().x / getSrcSize().x;
    auto srcScaleY = getSize().y / getSrcSize().y;
    auto bestScale = (srcScaleX < srcScaleY) ? srcScaleX : srcScaleY;
    mView2d.setSrcScale (bestScale);
    mView2d.setSrcPos (getSrcSize() * bestScale / -2.f);
    }

  cPdfImage* mImage;

  ID2D1SolidColorBrush* mBrush;
  cPoint mSamplePos;
  cPoint mPos;
  };
//}}}

class cAppWindow : public cD2dWindow {
public:
  cAppWindow() {}
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

    mContext = fz_new_context (NULL, NULL, FZ_STORE_DEFAULT);
    mColorspace = fz_device_bgr (mContext);

    openFile (name.c_str());
    loadPage (0, false);
    showPage();

    messagePump();

    close();
    }
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
      cLog::log (LOGINFO, "pages %d", mPageCount);

      while (true) {
        fz_try (mContext) {
          mOutline = fz_load_outline (mContext, mDocument);
          }
        fz_catch (mContext) {
          mOutline = NULL;
          if (fz_caught (mContext) == FZ_ERROR_TRYLATER)
            cLog::log (LOGINFO, "load outline later");
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
  void loadPage (int number, bool noCache) {

    mPageNumber = number;

    fz_drop_display_list (mContext, mPageList);
    mPageList = NULL;
    fz_drop_display_list (mContext, mAnnotationsList);
    mAnnotationsList = NULL;
    fz_drop_stext_page (mContext, mPageText);
    mPageText = NULL;
    fz_drop_link (mContext, mPageLinks);
    mPageLinks = NULL;
    fz_drop_page (mContext, mPage);
    mPage = NULL;

    mPageBoundingBox.x0 = 0;
    mPageBoundingBox.y0 = 0;
    mPageBoundingBox.x1 = 100;
    mPageBoundingBox.y1 = 100;

    bool incomplete = false;

    fz_try (mContext) {
      mPage = fz_load_page (mContext, mDocument, mPageNumber);
      mPageBoundingBox = fz_bound_page (mContext, mPage);
      }
    fz_catch(mContext) {
      if (fz_caught (mContext) == FZ_ERROR_TRYLATER)
        incomplete = 1;
      else
        cLog::log (LOGERROR, "Cannot load page");
      return;
      }

    bool errored = false;
    fz_cookie cookie = { 0 };

    fz_device* mdev = NULL;
    fz_var (mdev);

    fz_try (mContext) {
      // Create display lists
      mPageList = fz_new_display_list (mContext, fz_infinite_rect);
      mdev = fz_new_list_device (mContext, mPageList);
      if (noCache)
        fz_enable_device_hints (mContext, mdev, FZ_NO_CACHE);
      cookie.incomplete_ok = 1;
      fz_run_page_contents (mContext, mPage, mdev, fz_identity, &cookie);
      fz_close_device (mContext, mdev);
      fz_drop_device (mContext, mdev);
      mdev = NULL;

      mAnnotationsList = fz_new_display_list (mContext, fz_infinite_rect);
      mdev = fz_new_list_device(mContext, mAnnotationsList);
      fz_annot* annot;
      for (annot = fz_first_annot (mContext, mPage); annot; annot = fz_next_annot (mContext, annot))
        fz_run_annot (mContext, annot, mdev, fz_identity, &cookie);
      if (cookie.incomplete)
        incomplete = 1;
        //pdfapp_warn(app, "Incomplete page rendering");
      else if (cookie.errors) {
        cLog::log (LOGERROR, "Errors found on page");
        errored = 1;
        }
      fz_close_device (mContext, mdev);
      }
    fz_always(mContext) {
      fz_drop_device (mContext, mdev);
      }
    fz_catch (mContext) {
      if (fz_caught (mContext) == FZ_ERROR_TRYLATER)
        incomplete = 1;
      else {
        cLog::log (LOGERROR, "Cannot load page");
        errored = 1;
        }
      }

    fz_try (mContext) {
      mPageLinks = fz_load_links (mContext, mPage);
      }
    fz_catch(mContext) {
      if (fz_caught(mContext) == FZ_ERROR_TRYLATER)
        incomplete = 1;
      else if (!errored)
        cLog::log (LOGERROR, "Cannot load page");
      }

    errored = errored;
    }
  //}}}
  //{{{
  void showPage() {

    #define MAX_TITLE 256
    char buf[MAX_TITLE];

    fz_cookie cookie = { 0 };

    char buf2[64];
    size_t len;
    sprintf (buf2, " - %d/%d (%d dpi)", mPageNumber, mPageCount, mResolution);
    len = MAX_TITLE - strlen (buf2);
    if (strlen (mDocumentTitle) > len) {
      fz_strlcpy (buf, mDocumentTitle, len-3);
      fz_strlcat (buf, "...", MAX_TITLE);
      fz_strlcat (buf, buf2, MAX_TITLE);
      }
    else
      sprintf (buf, "%s%s", mDocumentTitle, buf2);
    //wintitle(app, buf);

    fz_matrix ctm = fz_transform_page (mPageBoundingBox, mResolution, mRotate);
    fz_rect bounds = fz_transform_rect (mPageBoundingBox, ctm);
    fz_irect ibounds = fz_round_rect (bounds);
    bounds = fz_rect_from_irect (ibounds);

    fz_drop_pixmap (mContext, mImage);
    mImage = NULL;
    fz_var (mImage);

    fz_device* idev = NULL;
    fz_var (idev);
    fz_try (mContext) {
      mImage = fz_new_pixmap_with_bbox (mContext, mColorspace, ibounds, NULL, 1);
      fz_clear_pixmap_with_value (mContext, mImage, 255);
      if (mPageList || mAnnotationsList) {
        idev = fz_new_draw_device (mContext, fz_identity, mImage);
        if (mPageList)
          fz_run_display_list (mContext, mPageList, idev, ctm, bounds, &cookie);
        if (mAnnotationsList)
          fz_run_display_list (mContext, mAnnotationsList, idev, ctm, bounds, &cookie);
        fz_close_device (mContext, idev);
        }
      }
    fz_always (mContext)
      fz_drop_device (mContext, idev);
    fz_catch (mContext)
      cookie.errors++;

    if (cookie.errors) {
      cLog::log (LOGERROR, "Errors found on page. Page rendering may be incomplete.");
      }

    fz_flush_warnings (mContext);

    int width = fz_pixmap_width (mContext, mImage);
    int height = fz_pixmap_height (mContext, mImage);
    cLog::log (LOGINFO, "page %d  %d x %d", mPageNumber, width, height);
    }
  //}}}

  //{{{  vars
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
  int mResolution = 96;
  int mRotate = 0;

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
  //}}}
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
