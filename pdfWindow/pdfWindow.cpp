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

  cPoint getSize() { return mSize; }
  int getWidth() { return mSize.width; }
  int getHeight() { return mSize.height; }

  ID2D1Bitmap* getBitmap() { return mBitmap; }

  //{{{
  void loadImage (ID2D1DeviceContext* dc, fz_context* context, fz_pixmap* pixmap) {
    mSize.width = fz_pixmap_width (context, pixmap);
    mSize.height = fz_pixmap_height (context, pixmap);

    if (mBitmap)  {
      mBitmap->Release();
      mBitmap = nullptr;
      }

    if (!mBitmap)
      dc->CreateBitmap (SizeU(mSize.width, mSize.height),
                        { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE, 0,0 },
                        &mBitmap);

    mBitmap->CopyFromMemory (&RectU(0, 0, mSize.width, mSize.height),
                             fz_pixmap_samples (context, pixmap), mSize.width * 4);
    }
  //}}}
  //{{{
  void releaseImage() {
    if (mBitmap) {
      mBitmap->Release();
      mBitmap = nullptr;
      }
    }
  //}}}

private:
  D2D1_SIZE_U mSize = {0,0};
  ID2D1Bitmap* mBitmap = nullptr;
  };
//}}}
//{{{
class cPdfImageView : public cD2dWindow::cView {
public:
  cPdfImageView (cD2dWindow* window, float width, float height, cPdfImage* pdfImage)
      : cView("pdfImageView", window, width, height), mPdfImage(pdfImage) {

    mPin = true;
    window->getDc()->CreateSolidColorBrush (ColorF (ColorF::Black), &mBrush);
    }

  virtual ~cPdfImageView() {
    mBrush->Release();
    }

  void setImage (cPdfImage* pdfImage) {
    mPdfImage = pdfImage;
    }

  // overrides
  //{{{
  cPoint getSrcSize() {
    return mPdfImage ? mPdfImage->getSize() : getSize();
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

    if (mPdfImage) {
      setScale();

      auto dstRect = mView2d.getSrcToDst (cRect(getSrcSize()));
      dc->SetTransform (mView2d.mTransform);

      if (mPdfImage->getBitmap())
        dc->DrawBitmap (mPdfImage->getBitmap(), cRect (mPdfImage->getSize()));

      dc->DrawRectangle (cRect (mPdfImage->getSize()), mWindow->getWhiteBrush());
      dc->SetTransform (Matrix3x2F::Identity());
      }
    }
  //}}}

private:
  void setScale () {
    auto dstRect = mView2d.getSrcToDst (cRect(getSrcSize()));
    auto srcScaleX = getSize().x / getSrcSize().x;
    auto srcScaleY = getSize().y / getSrcSize().y;
    auto bestScale = (srcScaleX < srcScaleY) ? srcScaleX : srcScaleY;
    mView2d.setSrcScale (bestScale);
    mView2d.setSrcPos (getSrcSize() * bestScale / -2.f);
    }

  cPdfImage* mPdfImage;
  ID2D1SolidColorBrush* mBrush;

  cPoint mPos;
  cPoint mSamplePos;
  };
//}}}

class cAppWindow : public cD2dWindow {
public:
  cAppWindow() {}
  //{{{
  void run (const string& title, int width, int height, string name) {

    initialise (title, width, height, kFullScreen);
    add (new cClockBox (this, 50.f, mTimePoint), -110.f,-120.f);
    add (new cCalendarBox (this, 190.f,150.f, mTimePoint), -190.f,0.f);
    add (new cLogBox (this, 200.f,0.f, true), -200.f,0);

    add (new cWindowBox (this, 60.f,24.f), -60.f,0.f);
    add (new cFloatBox (this, 50.f, kLineHeight, mRenderTime), 0.f,-kLineHeight);

    if (name.find (".lnk") <= name.size()) {
      string fullName;
      if (resolveShortcut (name.c_str(), fullName))
        name = fullName;
      }

    mContext = fz_new_context (NULL, NULL, FZ_STORE_DEFAULT);
    mColorspace = fz_device_bgr (mContext);

    mPdfImage = new cPdfImage();
    mPdfImageView = new cPdfImageView (this, 500.f,400.f, mPdfImage);
    add (mPdfImageView);

    openFile (name.c_str());

    changePage (0);

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

      case 0x25: changePage (prevPage()); changed();  break;    // left arrow
      case 0x26: changed();  break; // up arrow
      case 0x27: changePage (nextPage()); changed();  break;    // right arrow
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

    fz_drop_pixmap (mContext, mPixmap);
    mPixmap = NULL;

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
        cLog::log (LOGERROR, "loadPage failed %d of %d", mPageNumber, mPageCount);
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

    fz_matrix matrix = fz_transform_page (mPageBoundingBox, mResolution, mRotate);
    fz_rect bounds = fz_transform_rect (mPageBoundingBox, matrix);
    fz_irect ibounds = fz_round_rect (bounds);
    bounds = fz_rect_from_irect (ibounds);

    fz_drop_pixmap (mContext, mPixmap);
    mPixmap = NULL;
    fz_var (mPixmap);

    fz_device* idev = NULL;
    fz_var (idev);
    fz_try (mContext) {
      mPixmap = fz_new_pixmap_with_bbox (mContext, mColorspace, ibounds, NULL, 1);
      fz_clear_pixmap_with_value (mContext, mPixmap, 255);
      if (mPageList || mAnnotationsList) {
        idev = fz_new_draw_device (mContext, fz_identity, mPixmap);
        if (mPageList)
          fz_run_display_list (mContext, mPageList, idev, matrix, bounds, &cookie);
        if (mAnnotationsList)
          fz_run_display_list (mContext, mAnnotationsList, idev, matrix, bounds, &cookie);
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
    }
  //}}}

  //{{{
  void changePage (int number) {

    loadPage (number, false);
    showPage();
    mPdfImage->loadImage (getDc(), mContext, mPixmap);
    changed();
    }
  //}}}

  //{{{
  int prevPage() {
    if (mPageNumber > 0)
      return mPageNumber - 1;
    else
      return mPageNumber;
    }
  //}}}
  //{{{
  int nextPage() {
    if (mPageNumber > mPageCount - 1)
      return mPageCount - 1;
    else
      return mPageNumber + 1;
    }
  //}}}
  //{{{  vars
  fz_context* mContext = NULL;

  // document
  fz_document* mDocument = NULL;
  char* mDocumentPath = NULL;
  char* mDocumentTitle = NULL;
  fz_outline* mOutline = NULL;
  int mPageCount = 0;

  // page
  int mPageNumber = 0;
  fz_page* mPage = NULL;
  fz_rect mPageBoundingBox;
  fz_display_list* mPageList = NULL;
  fz_display_list* mAnnotationsList = NULL;
  fz_stext_page* mPageText = NULL;
  fz_link* mPageLinks = NULL;
  fz_quad mHitBoundingBox[512];

  // view
  float mLayoutWidth = 450;
  float mLayoutHeight = 600;
  float mLayoutEm = 12;
  char* mLayout_css = NULL;
  int mLayout_use_doc_css = 1;
  fz_colorspace* mColorspace = NULL;
  int mResolution = 96;
  int mRotate = 0;

  fz_pixmap* mPixmap = NULL;

  cPdfImage* mPdfImage = nullptr;
  cPdfImageView* mPdfImageView = nullptr;
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
