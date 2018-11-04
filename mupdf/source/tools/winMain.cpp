// winMain.cpp
//{{{  includes
#ifndef UNICODE
  #define UNICODE
#endif

#ifndef _UNICODE
  #define _UNICODE
#endif

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include <time.h>

// Include pdfapp.h *AFTER* the UNICODE defines
#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include "mupdf/helpers/pkcs7-check.h"
//#include "mupdf/helpers/pkcs7-openssl.h"
//}}}
//{{{  defines
#define MINRES 18
#define MAXRES 1152

#define MAX_TITLE 256

#ifndef PATH_MAX
  #define PATH_MAX 4096
#endif

#define MIN(x,y) ((x) < (y) ? (x) : (y))
#ifndef MAX
  #define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#define BEYOND_THRESHHOLD 40

#define ID_ABOUT    0x1000
#define ID_DOCINFO  0x1001

// Create registry keys to associate MuPDF with PDF and XPS files.
#define OPEN_KEY(parent, name, ptr) RegCreateKeyExA (parent, name, 0, 0, 0, KEY_WRITE, 0, &ptr, 0)
#define SET_KEY(parent, name, value) RegSetValueExA (parent, name, 0, REG_SZ, (const BYTE*)(value), (DWORD)strlen(value) + 1)
//}}}
//{{{  enums
enum eCursor { eARROW, eHAND, eWAIT, eCARET };
enum ePanning { eDONT_PAN = 0, ePAN_TO_TOP, ePAN_TO_BOTTOM };
//}}}

//{{{  global vars
HWND gHwndFrame = NULL;
HWND gHwndView = NULL;

HCURSOR gArrowCursor;
HCURSOR gHandCursor;
HCURSOR gWaitCursor;
HCURSOR gCaretCursor;

int td_retry = 0;
char td_textinput[1024] = "";

int cd_nopts;
int* cd_nvals;
const char** cd_opts;
const char** cd_vals;

int gPdOk = 0;
bool gJustCopied = false;

int gOldx = 0;
int gOldy = 0;
bool gIsFullScreen = false;
WINDOWPLACEMENT gSavedPlacement;
//}}}

//{{{
char* usageStr() {
  return
    "L\t\t-- rotate left\n"
    "R\t\t-- rotate right\n"
    "h\t\t-- scroll left\n"
    "j down\t\t-- scroll down\n"
    "k up\t\t-- scroll up\n"
    "l\t\t-- scroll right\n"
    "+\t\t-- zoom in\n"
    "-\t\t-- zoom out\n"
    "W\t\t-- zoom to fit window width\n"
    "H\t\t-- zoom to fit window height\n"
    "Z\t\t-- zoom to fit page\n"
    "f\t\t-- fullscreen\n"
    "r\t\t-- reload file\n"
    ". pgdn right spc\t-- next page\n"
    ", pgup left b bkspc\t-- previous page\n"
    ">\t\t-- next 10 pages\n"
    "<\t\t-- back 10 pages\n"
    "1m\t\t-- mark page in register 1\n"
    "1t\t\t-- go to page in register 1\n"
    "G\t\t-- go to last page\n"
    "123g\t\t-- go to page 123\n"
    "/\t\t-- search forwards for text\n"
    "?\t\t-- search backwards for text\n"
    "n\t\t-- find next search result\n"
    "N\t\t-- find previous search result\n"
    "q\t\t-- quit\n"
    ;
  }
//}}}
//{{{
char* versionStr() {
  return "MuPDF " FZ_VERSION "\n" "Copyright 2006-2017 Artifex Software, Inc.\n";
  }
//}}}
//{{{
INT_PTR CALLBACK dlogAboutProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {

  switch(message) {
    case WM_INITDIALOG:
      SetDlgItemTextA (hwnd, 2, versionStr());
      SetDlgItemTextA (hwnd, 3, usageStr());
      return TRUE;

    case WM_COMMAND:
      EndDialog (hwnd, 1);
      return TRUE;
    }

  return FALSE;
  }
//}}}
//{{{
INT_PTR CALLBACK dlogTextProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {

  switch (message) {
    case WM_INITDIALOG:
      SetDlgItemTextA (hwnd, 3, td_textinput);
      if (!td_retry)
        ShowWindow (GetDlgItem (hwnd, 4), SW_HIDE);
      return TRUE;

    case WM_COMMAND:
      switch (wParam) {
        case 1:
          gPdOk = 1;
          GetDlgItemTextA (hwnd, 3, td_textinput, sizeof td_textinput);
          EndDialog (hwnd, 1);
          return TRUE;
        case 2:
          gPdOk = 0;
          EndDialog (hwnd, 1);
          return TRUE;
      }
      break;

    case WM_CTLCOLORSTATIC:
      if ((HWND)lParam == GetDlgItem (hwnd, 4)) {
        SetTextColor ((HDC)wParam, RGB (255,0,0));
        SetBkMode ((HDC)wParam, TRANSPARENT);
        return (INT_PTR)GetStockObject (NULL_BRUSH);
        }
      break;
    }

  return FALSE;
  }
//}}}
//{{{
INT_PTR CALLBACK dlogChoiceProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {

  HWND listbox;
  int i;
  int item;
  int sel;

  switch (message) {
    case WM_INITDIALOG:
      listbox = GetDlgItem (hwnd, 3);
      for (i = 0; i < cd_nopts; i++)
        SendMessageA (listbox, LB_ADDSTRING, 0, (LPARAM)cd_opts[i]);

      /* FIXME: handle multiple select */
      if (*cd_nvals > 0) {
        item = SendMessageA (listbox, LB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)cd_vals[0]);
        if (item != LB_ERR)
          SendMessageA (listbox, LB_SETCURSEL, item, 0);
        }
      return TRUE;

    case WM_COMMAND:
      switch (wParam) {
        case 1:
          listbox = GetDlgItem (hwnd, 3);
          *cd_nvals = 0;
          for (i = 0; i < cd_nopts; i++) {
            item = SendMessageA (listbox, LB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)cd_opts[i]);
            sel = SendMessageA (listbox, LB_GETSEL, item, 0);
            if (sel && sel != LB_ERR)
              cd_vals[(*cd_nvals)++] = cd_opts[i];
            }
          gPdOk = 1;
          EndDialog (hwnd, 1);
          return TRUE;

        case 2:
          gPdOk = 0;
          EndDialog (hwnd, 1);
          return TRUE;
        }
      break;
    }

  return FALSE;
  }
//}}}

//{{{
void winError (char* msg) {

  MessageBoxA (gHwndFrame, msg, "MuPDF: Error", MB_ICONERROR);
  exit (1);
  }
//}}}
//{{{
void winWarn (char* msg) {
  MessageBoxA (gHwndFrame, msg, "MuPDF: Warning", MB_ICONWARNING);
  }
//}}}
//{{{
void winWarn (const char* fmt, ...) {

  char buf[1024];
  va_list ap;
  va_start (ap, fmt);
  fz_vsnprintf (buf, sizeof(buf), fmt, ap);
  va_end (ap);
  buf[sizeof(buf)-1] = 0;

  winWarn (buf);
  }
//}}}
//{{{
void winAlert (pdf_alert_event* alert) {

  int buttons = MB_OK;
  int icon = MB_ICONWARNING;
  int pressed = PDF_ALERT_BUTTON_NONE;

  switch (alert->icon_type) {
    case PDF_ALERT_ICON_ERROR:
      icon = MB_ICONERROR;
      break;
    case PDF_ALERT_ICON_WARNING:
      icon = MB_ICONWARNING;
      break;
    case PDF_ALERT_ICON_QUESTION:
      icon = MB_ICONQUESTION;
      break;
    case PDF_ALERT_ICON_STATUS:
      icon = MB_ICONINFORMATION;
      break;
    }

  switch (alert->button_group_type) {
    case PDF_ALERT_BUTTON_GROUP_OK:
      buttons = MB_OK;
      break;
    case PDF_ALERT_BUTTON_GROUP_OK_CANCEL:
      buttons = MB_OKCANCEL;
      break;
    case PDF_ALERT_BUTTON_GROUP_YES_NO:
      buttons = MB_YESNO;
      break;
    case PDF_ALERT_BUTTON_GROUP_YES_NO_CANCEL:
      buttons = MB_YESNOCANCEL;
      break;
    }

  pressed = MessageBoxA (gHwndFrame, alert->message, alert->title, icon|buttons);

  switch (pressed) {
    case IDOK:
      alert->button_pressed = PDF_ALERT_BUTTON_OK;
      break;
    case IDCANCEL:
      alert->button_pressed = PDF_ALERT_BUTTON_CANCEL;
      break;
    case IDNO:
      alert->button_pressed = PDF_ALERT_BUTTON_NO;
      break;
    case IDYES:
      alert->button_pressed = PDF_ALERT_BUTTON_YES;
    }
  }
//}}}
//{{{
void winHelp() {
  if (DialogBoxW (NULL, L"IDD_DLOGABOUT", gHwndFrame, dlogAboutProc) <= 0)
    winError ("cannot create help dialog");
  }
//}}}
//{{{
void winCursor (eCursor cursor) {

  if (cursor == eARROW)
    SetCursor (gArrowCursor);
  else if (cursor == eHAND)
    SetCursor (gHandCursor);
  else if (cursor == eWAIT)
    SetCursor (gWaitCursor);
  else if (cursor == eCARET)
    SetCursor (gCaretCursor);
  }
//}}}
//{{{
void winTitle (char* title) {

  wchar_t wide[256];
  wchar_t* dp = wide;
  char* sp = title;
  while (*sp && dp < wide + 255) {
    int rune;
    sp += fz_chartorune (&rune, sp);
    *dp++ = rune;
    }
  *dp = 0;

  SetWindowTextW (gHwndFrame, wide);
  }
//}}}
//{{{
void winResize (int w, int h) {

  ShowWindow (gHwndFrame, SW_SHOWDEFAULT);
  w += GetSystemMetrics (SM_CXFRAME) * 2;
  h += GetSystemMetrics (SM_CYFRAME) * 2;
  h += GetSystemMetrics (SM_CYCAPTION);
  SetWindowPos (gHwndFrame, 0, 0, 0, w, h, SWP_NOZORDER | SWP_NOMOVE);
  }
//}}}
//{{{
void winFullScreen (int state) {

  if (state && !gIsFullScreen) {
    GetWindowPlacement (gHwndFrame, &gSavedPlacement);
    SetWindowLong (gHwndFrame, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    SetWindowPos (gHwndFrame, NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
    ShowWindow (gHwndFrame, SW_SHOWMAXIMIZED);
    gIsFullScreen = true;
    }

  if (!state && gIsFullScreen) {
    SetWindowLong (gHwndFrame, GWL_STYLE, WS_OVERLAPPEDWINDOW);
    SetWindowPos (gHwndFrame, NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
    SetWindowPlacement (gHwndFrame, &gSavedPlacement);
    gIsFullScreen = false;
    }
  }
//}}}
//{{{
char* winTextInput (char* inittext, int retry) {

  td_retry = retry;

  fz_strlcpy (td_textinput, inittext ? inittext : "", sizeof td_textinput);
  if (DialogBoxW (NULL, L"IDD_DLOGTEXT", gHwndFrame, dlogTextProc) <= 0)
    winError ("cannot create text input dialog");

  if (gPdOk)
    return td_textinput;

  return NULL;
  }
//}}}
//{{{
int winChoiceInput (int nopts, const char* opts[], int* nvals, const char* vals[]) {

  cd_nopts = nopts;
  cd_nvals = nvals;
  cd_opts = opts;
  cd_vals = vals;

  if (DialogBoxW (NULL, L"IDD_DLOGLIST", gHwndFrame, dlogChoiceProc) <= 0)
    winError ("cannot create text input dialog");

  return gPdOk;
  }
//}}}
//{{{
void winInstallApp (char* argv0) {

  HKEY software, classes, mupdf, dotpdf, dotxps, dotepub, dotfb2;
  HKEY shell, open, command, supported_types;
  HKEY pdf_progids, xps_progids, epub_progids, fb2_progids;

  OPEN_KEY (HKEY_CURRENT_USER, "Software", software);
  OPEN_KEY (software, "Classes", classes);
  OPEN_KEY (classes, ".pdf", dotpdf);
  OPEN_KEY (dotpdf, "OpenWithProgids", pdf_progids);
  OPEN_KEY (classes, ".xps", dotxps);
  OPEN_KEY (dotxps, "OpenWithProgids", xps_progids);
  OPEN_KEY (classes, ".epub", dotepub);
  OPEN_KEY (dotepub, "OpenWithProgids", epub_progids);
  OPEN_KEY (classes, ".fb2", dotfb2);
  OPEN_KEY (dotfb2, "OpenWithProgids", fb2_progids);
  OPEN_KEY (classes, "MuPDF", mupdf);
  OPEN_KEY (mupdf, "SupportedTypes", supported_types);
  OPEN_KEY (mupdf, "shell", shell);
  OPEN_KEY (shell, "open", open);
  OPEN_KEY (open, "command", command);

  char buf[512];
  sprintf (buf, "\"%s\" \"%%1\"", argv0);

  SET_KEY (open, "FriendlyAppName", "MuPDF");
  SET_KEY (command, "", buf);
  SET_KEY (supported_types, ".pdf", "");
  SET_KEY (supported_types, ".xps", "");
  SET_KEY (supported_types, ".epub", "");
  SET_KEY (pdf_progids, "MuPDF", "");
  SET_KEY (xps_progids, "MuPDF", "");
  SET_KEY (epub_progids, "MuPDF", "");
  SET_KEY (fb2_progids, "MuPDF", "");

  RegCloseKey (dotfb2);
  RegCloseKey (dotepub);
  RegCloseKey (dotxps);
  RegCloseKey (dotpdf);
  RegCloseKey (mupdf);
  RegCloseKey (classes);
  RegCloseKey (software);
  }
//}}}

//{{{
void eventCallback (fz_context* context, pdf_document* document, pdf_doc_event* event, void* data) {

  switch (event->type) {
    case PDF_DOCUMENT_EVENT_ALERT:
      winAlert (pdf_access_alert_event (context, event));
      break;

    case PDF_DOCUMENT_EVENT_PRINT:
    case PDF_DOCUMENT_EVENT_EXEC_MENU_ITEM:
      winWarn ("document attempted to execute menu item");
      break;

    case PDF_DOCUMENT_EVENT_EXEC_DIALOG:
      winWarn ("document attempted to open a dialog box");
      break;

    case PDF_DOCUMENT_EVENT_LAUNCH_URL:
      winWarn ("document attempted to open url: %s", pdf_access_launch_url_event (context, event)->url);
      break;

    case PDF_DOCUMENT_EVENT_MAIL_DOC:
      winWarn ("document attempted to mail the document");
      break;
    }
  }
//}}}

//{{{
class cPdf {
public:
  //{{{
  cPdf() {

    memset (this, 0, sizeof(cPdf));

    mContext = fz_new_context (NULL, NULL, FZ_STORE_DEFAULT);
    mColorspace = fz_device_bgr (mContext);

    mResolution = 96;

    layoutWidth = 450;
    layoutHeight = 600;
    mLayoutEm = 12;
    layout_css = NULL;
    layout_use_doc_css = 1;
    }
  //}}}

  //{{{
  void openFile (char* filename, int reload) {

    fz_try (mContext) {
      fz_register_document_handlers (mContext);
      if (layout_css) {
        fz_buffer* buffer = fz_read_file (mContext, layout_css);
        fz_set_user_css (mContext, fz_string_from_buffer (mContext, buffer));
        fz_drop_buffer (mContext, buffer);
        }
      fz_set_use_document_css (mContext, layout_use_doc_css);
      mDocument = fz_open_document (mContext, filename);
      }
    fz_catch (mContext) {
      if (!reload || makeFakeDoc())
        winError ("cannot open document");
      }

    pdf_document* idoc = pdf_specifics (mContext, mDocument);
    if (idoc) {
      fz_try (mContext) {
        pdf_enable_js (mContext, idoc);
        pdf_set_doc_event_callback (mContext, idoc, eventCallback, this);
        }
      fz_catch (mContext) {
        winError ("cannot load javascript embedded in document");
        }
      }

    fz_try (mContext) {
      if (fz_needs_password (mContext, mDocument))
        winWarn ("document needs password");

      mDocPath = fz_strdup (mContext, filename);
      mDocTitle = filename;
      if (strrchr(mDocTitle, '\\'))
        mDocTitle = strrchr (mDocTitle, '\\') + 1;
      if (strrchr (mDocTitle, '/'))
        mDocTitle = strrchr (mDocTitle, '/') + 1;
      mDocTitle = fz_strdup (mContext, mDocTitle);

      fz_layout_document (mContext, mDocument, layoutWidth, layoutHeight, mLayoutEm);

      while (true) {
        fz_try(mContext) {
          mPageCount = fz_count_pages (mContext, mDocument);
          if (mPageCount <= 0)
            fz_throw (mContext, FZ_ERROR_GENERIC, "No pages in document");
          }
        fz_catch (mContext) {
          if (fz_caught (mContext) == FZ_ERROR_TRYLATER) {
            winWarn ("not enough data to count pages yet");
            continue;
            }
          fz_rethrow (mContext);
          }
        break;
        }

      while (true) {
        fz_try (mContext) {
          mOutline = fz_load_outline (mContext, mDocument);
          }
        fz_catch (mContext) {
          mOutline = NULL;
          winWarn ("failed to load outline");
          }
        break;
        }
      }
    fz_catch (mContext) {
      winError ("cannot open document");
      }

    if (mPageNumber < 1)
      mPageNumber = 1;
    if (mPageNumber > mPageCount)
      mPageNumber = mPageCount;
    if (mResolution < MINRES)
      mResolution = MINRES;
    if (mResolution > MAXRES)
      mResolution = MAXRES;

    if (!reload) {
      rotate = 0;
      panx = 0;
      pany = 0;
      }

    showPage (1, 1, 1, 0);
    }
  //}}}
  //{{{
  void closeFile() {

    fz_drop_display_list (mContext, mPageList);
    mPageList = NULL;

    fz_drop_display_list (mContext, mAnnotationsList);
    mAnnotationsList = NULL;

    fz_drop_stext_page (mContext, mPageText);
    mPageText = NULL;

    fz_drop_link (mContext, mPageLinks);
    mPageLinks = NULL;

    fz_free (mContext, mDocTitle);
    mDocTitle = NULL;

    fz_free (mContext, mDocPath);
    mDocPath = NULL;

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
  void reloadFile() {

    char filename[PATH_MAX];
    fz_strlcpy (filename, mDocPath, PATH_MAX);
    closeFile();
    openFile (filename, 1);
    }
  //}}}

  //{{{
  void loadPage (bool noCache) {

    mErrored = 0;
    mIncomplete = 0;

    fz_drop_display_list (mContext, mPageList);
    fz_drop_display_list (mContext, mAnnotationsList);
    fz_drop_stext_page (mContext, mPageText);
    fz_drop_link (mContext, mPageLinks);
    fz_drop_page (mContext, mPage);

    mPageList = NULL;
    mAnnotationsList = NULL;
    mPageText = NULL;
    mPageLinks = NULL;
    mPage = NULL;
    mPageBoundingBox.x0 = 0;
    mPageBoundingBox.y0 = 0;
    mPageBoundingBox.x1 = 100;
    mPageBoundingBox.y1 = 100;

    fz_try (mContext) {
      mPage = fz_load_page (mContext, mDocument, mPageNumber - 1);
      mPageBoundingBox = fz_bound_page (mContext, mPage);
      }
    fz_catch (mContext) {
      if (fz_caught (mContext) == FZ_ERROR_TRYLATER)
        mIncomplete = 1;
      else
        winWarn ("Cannot load page");
      return;
      }

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
      mdev = fz_new_list_device (mContext, mAnnotationsList);
      for (fz_annot* annot = fz_first_annot (mContext, mPage); annot; annot = fz_next_annot(mContext, annot))
        fz_run_annot (mContext, annot, mdev, fz_identity, &cookie);
      if (cookie.incomplete)
        mIncomplete = 1;
      else if (cookie.errors) {
        winWarn ("Errors found on page");
        mErrored = 1;
        }
      fz_close_device (mContext, mdev);
      }
    fz_always (mContext) {
      fz_drop_device (mContext, mdev);
      }
    fz_catch (mContext) {
      if (fz_caught (mContext) == FZ_ERROR_TRYLATER)
        mIncomplete = 1;
      else {
        winWarn ("Cannot load page");
        mErrored = 1;
        }
      }

    fz_try (mContext) {
      mPageLinks = fz_load_links (mContext, mPage);
      }
    fz_catch (mContext) {
      if (fz_caught (mContext) == FZ_ERROR_TRYLATER)
        mIncomplete = 1;
      else if (!mErrored)
        winWarn ("Cannot load page");
      }
    }
  //}}}
  //{{{
  void gotoPage (int number) {

    isSearching = 0;

    if (number < 1)
      number = 1;
    if (number > mPageCount)
      number = mPageCount;
    if (number == mPageNumber)
      return;

    mPageNumber = number;
    InvalidateRect (gHwndView, NULL, 0);
    showPage (1, 1, 1, 0);
    }
  //}}}
  //{{{
  void updatePage() {

    if (pdf_update_page (mContext, (pdf_page*)mPage)) {
      recreateAnnotations();
      showPage (0, 1, 1, 0);
      }
    else
      showPage (0, 0, 1, 0);
    }
  //}}}
  //{{{
  void showPage (int load, int draw, int repaint, int searching) {

    if (!nowaitcursor)
      winCursor (eWAIT);

    fz_cookie cookie = { 0 };
    if (load) {
      //{{{  load
      loadPage (searching);

      /* Zero search hit position */
      hit_count = 0;

      /* Extract text */
      fz_rect mediabox = fz_bound_page (mContext, mPage);
      mPageText = fz_new_stext_page (mContext, mediabox);

      if (mPageList || mAnnotationsList) {
        fz_device* tdev = fz_new_stext_device (mContext, mPageText, NULL);
        fz_try (mContext) {
          if (mPageList)
            fz_run_display_list (mContext, mPageList, tdev, fz_identity, fz_infinite_rect, &cookie);
          if (mAnnotationsList)
            fz_run_display_list (mContext, mAnnotationsList, tdev, fz_identity, fz_infinite_rect, &cookie);
          fz_close_device (mContext, tdev);
          }
        fz_always (mContext)
          fz_drop_device (mContext, tdev);
        fz_catch (mContext)
          fz_rethrow (mContext);
        }
      }
      //}}}
    if (draw) {
      //{{{  draw
      char buf2[64];
      sprintf (buf2, " - %d/%d (%d dpi)", mPageNumber, mPageCount, mResolution);

      char buf[MAX_TITLE];
      size_t len = MAX_TITLE - strlen (buf2);
      if (strlen (mDocTitle) > len) {
        fz_strlcpy (buf, mDocTitle, len-3);
        fz_strlcat (buf, "...", MAX_TITLE);
        fz_strlcat (buf, buf2, MAX_TITLE);
        }
      else
        sprintf (buf, "%s%s", mDocTitle, buf2);
      winTitle (buf);

      fz_matrix ctm = fz_transform_page (mPageBoundingBox, mResolution, rotate);
      fz_rect bounds = fz_transform_rect (mPageBoundingBox, ctm);
      fz_irect ibounds = fz_round_rect (bounds);
      bounds = fz_rect_from_irect (ibounds);

      // Draw
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
        if (invert)
          fz_invert_pixmap (mContext, mImage);
        }
      fz_always (mContext)
        fz_drop_device (mContext, idev);
      fz_catch (mContext)
        cookie.errors++;
      }
      //}}}
    if (repaint) {
      //{{{  repaint
      panView (panx, pany);

      if (!mImage)
        winResize (layoutWidth, layoutHeight);

      InvalidateRect (gHwndView, NULL, 0);
      winCursor (eARROW);
      }
      //}}}

    if (cookie.errors && mErrored == 0) {
      mErrored = 1;
      winWarn ("Errors found on page, page rendering may be incomplete");
      }

    fz_flush_warnings (mContext);
    }
  //}}}

  //{{{
  void autoZoomVert() {

    mResolution *= (float)mWinHeight / fz_pixmap_height (mContext, mImage);
    if (mResolution > MAXRES)
      mResolution = MAXRES;
    else if (mResolution < MINRES)
      mResolution = MINRES;

    showPage (0, 1, 1, 0);
    }
  //}}}
  //{{{
  void autoZoomHoriz() {

    mResolution *= (float)mWinWidth / fz_pixmap_width (mContext, mImage);
    if (mResolution > MAXRES)
      mResolution = MAXRES;
    else if (mResolution < MINRES)
      mResolution = MINRES;

    showPage (0, 1, 1, 0);
    }
  //}}}
  //{{{
  void autoZoom() {

    float pageAspect = (float) fz_pixmap_width (mContext, mImage) / fz_pixmap_height (mContext, mImage);
    float winAspect = (float) mWinWidth / mWinHeight;

    if (pageAspect > winAspect)
      autoZoomHoriz();
    else
      autoZoomVert();
    }
  //}}}

  //{{{
  void doSearch (enum ePanning* panTo, int dir) {

    if (search[0] == 0) {
      // abort if no search string
      InvalidateRect (gHwndView, NULL, 0);
      return;
      }

    winCursor (eWAIT);

    int firstpage = mPageNumber;
    int page = (searchpage == mPageNumber) ? mPageNumber + dir : mPageNumber;
    if (page < 1)
      page = mPageCount;
    if (page > mPageCount)
      page = 1;

    do {
      if (page != mPageNumber) {
        mPageNumber = page;
        showPage (1, 0, 0, 1);
        }

      hit_count = fz_search_stext_page (mContext, mPageText, search, hit_bbox, nelem (hit_bbox));
      if (hit_count > 0) {
        *panTo = dir == 1 ? ePAN_TO_TOP : ePAN_TO_BOTTOM;
        searchpage = mPageNumber;
        winCursor (eHAND);
        InvalidateRect (gHwndView, NULL, 0);
        return;
        }

      page += dir;
      if (page < 1)
        page = mPageCount;
      if (page > mPageCount)
        page = 1;
      } while (page != firstpage);

      winWarn ("String '%s' not found.", search);

    mPageNumber = firstpage;
    showPage (1, 0, 0, 0);
    winCursor (eHAND);
    InvalidateRect (gHwndView, NULL, 0);
    }
  //}}}

  //{{{
  void onCopy (unsigned short *ucsbuf, int ucslen) {

    fz_matrix ctm = fz_transform_page (mPageBoundingBox, mResolution, rotate);
    ctm = fz_invert_matrix (ctm);

    fz_rect sel = fz_transform_rect (selr, ctm);

    int p = 0;
    int need_newline = 0;

    fz_stext_page* page = mPageText;
    for (fz_stext_block* block = page->first_block; block; block = block->next) {
      if (block->type != FZ_STEXT_BLOCK_TEXT)
        continue;

      for (fz_stext_line* line = block->u.t.first_line; line; line = line->next) {
        int saw_text = 0;
        for (fz_stext_char* ch = line->first_char; ch; ch = ch->next) {
          fz_rect bbox = fz_rect_from_quad(ch->quad);
          int c = ch->c;
          if (c < 32)
            c = 0xFFFD;
          if (bbox.x1 >= sel.x0 && bbox.x0 <= sel.x1 && bbox.y1 >= sel.y0 && bbox.y0 <= sel.y1) {
            saw_text = 1;
            if (need_newline) {
              if (p < ucslen - 1)
                ucsbuf[p++] = '\r';
              if (p < ucslen - 1)
                ucsbuf[p++] = '\n';
              need_newline = 0;
              }
            if (p < ucslen - 1)
              ucsbuf[p++] = c;
            }
          }
        if (saw_text)
          need_newline = 1;
        }
      }

    ucsbuf[p] = 0;
    }
  //}}}
  //{{{
  void doCopy() {

    if (!OpenClipboard (gHwndFrame))
      return;

    EmptyClipboard();

    HGLOBAL handle = GlobalAlloc (GMEM_MOVEABLE, 4096 * sizeof(unsigned short));
    if (!handle) {
      CloseClipboard();
      return;
      }

    unsigned short* ucsbuf = (unsigned short*)GlobalLock (handle);
    onCopy (ucsbuf, 4096);
    GlobalUnlock (handle);

    SetClipboardData (CF_UNICODETEXT, handle);
    CloseClipboard();

    gJustCopied = true;
    }
  //}}}

  //{{{
  void onKey (int c, int modifiers) {

    int oldpage = mPageNumber;
    enum ePanning panTo = ePAN_TO_TOP;
    int loadpage = 1;

    if (isSearching) {
      //{{{  searching
      size_t n = strlen(search);
      if (c < ' ') {
        if (c == '\b' && n > 0) {
          search[n - 1] = 0;
          InvalidateRect (gHwndView, NULL, 0);
          }
        if (c == '\n' || c == '\r') {
          isSearching = 0;
          if (n > 0) {
            InvalidateRect (gHwndView, NULL, 0);
            if (searchdir < 0) {
              if (mPageNumber == 1)
                mPageNumber = mPageCount;
              else
                mPageNumber--;
              showPage(1, 1, 0, 1);
              }

            onKey ('n', 0);
            }
          else
            InvalidateRect (gHwndView, NULL, 0);
          }
        if (c == '\033') {
          isSearching = 0;
          InvalidateRect (gHwndView, NULL, 0);
          }
        }
      else {
        if (n + 2 < sizeof search) {
          search[n] = c;
          search[n + 1] = 0;
          InvalidateRect(gHwndView, NULL, 0);
          }
        }
      return;
      }
      //}}}
    if (c >= '0' && c <= '9') {
      //{{{  number
      number[numberlen++] = c;
      number[numberlen] = '\0';
      }
      //}}}

    switch (c) {
      case 'q':
      //{{{
      case 0x1B: {
        mExit = true;
        break;
        }
      //}}}

      case '+':
      //{{{
      case '=':
        mResolution = zoomIn (mResolution);
        showPage (0, 1, 1, 0);
        break;
      //}}}
      //{{{
      case '-':
        mResolution = zoomOut (mResolution);
        showPage (0, 1, 1, 0);
        break;
      //}}}
      //{{{
      case 'W':
        autoZoomHoriz();
        break;
      //}}}
      //{{{
      case 'H':
        autoZoomVert();
        break;
      //}}}
      //{{{
      case 'Z':
        autoZoom();
        break;
      //}}}
      //{{{
      case 'L':
        rotate -= 90;
        showPage (0, 1, 1, 0);
        break;
      //}}}
      //{{{
      case 'R':
        rotate += 90;
        showPage (0, 1, 1, 0);
        break;
      //}}}
      //{{{
      case 'i':
        invert ^= 1;
        showPage (0, 1, 1, 0);
        break;
      //}}}
      //{{{
      case 'a':
        rotate -= 15;
        showPage (0, 1, 1, 0);
        break;
      //}}}
      //{{{
      case 's':
        rotate += 15;
        showPage (0, 1, 1, 0);
        break;
      //}}}
      //{{{
      case 'f':
        winFullScreen (!fullscreen);
        fullscreen = !fullscreen;
        break;
      //}}}
      //{{{
      case 'h':
        panx += fz_pixmap_width (mContext, mImage) / 10;
        showPage (0, 0, 1, 0);
        break;
      //}}}
      //{{{
      case 'j':
        {
          int h = fz_pixmap_height(mContext, mImage);
          if (h <= mWinHeight || pany <= mWinHeight - h) {
            panTo = ePAN_TO_TOP;
            mPageNumber++;
          }
          else {
            pany -= h / 10;
            showPage (0, 0, 1, 0);
          }
          break;
        }
      //}}}
      //{{{
      case 'k':
        {
          int h = fz_pixmap_height(mContext, mImage);
          if (h <= mWinHeight || pany == 0) {
            panTo = ePAN_TO_BOTTOM;
            mPageNumber--;
          }
          else {
            pany += h / 10;
            showPage ( 0, 0, 1, 0);
          }
          break;
        }
      //}}}
      //{{{
      case 'l':
        panx -= fz_pixmap_width(mContext, mImage) / 10;
        showPage (0, 0, 1, 0);
        break;
      //}}}

      case 'g':
      case '\n':
      //{{{
      case '\r':
        if (numberlen > 0)
          gotoPage (atoi(number));
        else
          gotoPage (1);
        break;
      //}}}
      //{{{
      case 'G':
        gotoPage (mPageCount);
        break;
      //}}}
      //{{{
      case ',':
        panTo = ePAN_TO_BOTTOM;
        if (numberlen > 0)
          mPageNumber -= atoi(number);
        else
          mPageNumber--;
        break;
      //}}}
      //{{{
      case '.':
        panTo = ePAN_TO_TOP;
        if (numberlen > 0)
          mPageNumber += atoi(number);
        else
          mPageNumber++;
        break;
      //}}}

      case '\b':
      //{{{
      case 'b':
        panTo = eDONT_PAN;
        if (numberlen > 0)
          mPageNumber -= atoi(number);
        else
          mPageNumber--;
        break;
      //}}}
      //{{{
      case ' ':
        panTo = eDONT_PAN;
        if (modifiers & 1)
        {
          if (numberlen > 0)
            mPageNumber -= atoi(number);
          else
            mPageNumber--;
        }
        else
        { if (numberlen > 0)
            mPageNumber += atoi(number);
          else
            mPageNumber++;
        }
        break;
      //}}}
      //{{{
      case '<':
        panTo = ePAN_TO_TOP;
        mPageNumber -= 10;
        break;
      //}}}
      //{{{
      case '>':
        panTo = ePAN_TO_TOP;
        mPageNumber += 10;
        break;
      //}}}
      //{{{
      case 'r':
        panTo = eDONT_PAN;
        oldpage = -1;
        reloadFile();
        break;
      //}}}
      //{{{
      case '?':
        isSearching = 1;
        searchdir = -1;
        search[0] = 0;
        hit_count = 0;
        searchpage = -1;
        InvalidateRect (gHwndView, NULL, 0);
        break;
      //}}}
      //{{{
      case '/':
        isSearching = 1;
        searchdir = 1;
        search[0] = 0;
        hit_count = 0;
        searchpage = -1;
        InvalidateRect (gHwndView, NULL, 0);
        break;
      //}}}
      //{{{
      case 'n':
        if (searchdir > 0)
          doSearch (&panTo, 1);
        else
          doSearch (&panTo, -1);
        loadpage = 0;
        break;
      //}}}
      //{{{
      case 'N':
        if (searchdir > 0)
          doSearch(&panTo, -1);
        else
          doSearch(&panTo, 1);
        loadpage = 0;
        break;
      //}}}
      }

    if (c < '0' || c > '9')
      numberlen = 0;

    if (mPageNumber < 1)
      mPageNumber = 1;
    if (mPageNumber > mPageCount)
      mPageNumber = mPageCount;

    if (mPageNumber != oldpage) {
      switch (panTo) {
        //{{{
        case ePAN_TO_TOP:
          pany = 0;
          break;
        //}}}
        //{{{
        case ePAN_TO_BOTTOM:
          pany = -2000;
          break;
        //}}}
        //{{{
        case eDONT_PAN:
          break;
        //}}}
        }
      showPage (loadpage, 1, 1, 0);
      }
    }
  //}}}
  //{{{
  void onMouse (int x, int y, int btn, int modifiers, int state) {

    fz_point p;
    fz_irect irect = { 0, 0, (int)layoutWidth, (int)layoutHeight };
    if (mImage)
      irect = fz_pixmap_bbox (mContext, mImage);
    p.x = x - panx + irect.x0;
    p.y = y - pany + irect.y0;

    fz_matrix ctm = fz_transform_page (mPageBoundingBox, mResolution, rotate);
    ctm = fz_invert_matrix (ctm);

    p = fz_transform_point (p, ctm);

    fz_link* link;
    int processed = 0;
    if (btn == 1 && (state == 1 || state == -1)) {
      pdf_ui_event event;
      pdf_document* idoc = pdf_specifics (mContext, mDocument);

      event.etype = PDF_EVENT_TYPE_POINTER;
      event.event.pointer.pt = p;
      if (state == 1)
        event.event.pointer.ptype = PDF_POINTER_DOWN;
      else /* state == -1 */
        event.event.pointer.ptype = PDF_POINTER_UP;

      if (idoc && pdf_pass_event (mContext, idoc, (pdf_page*)mPage, &event)) {
        pdf_widget* widget = pdf_focused_widget (mContext, idoc);
        nowaitcursor = 1;
        updatePage();
        if (widget) {
          switch (pdf_widget_type (mContext, widget)) {
            //{{{
            case PDF_WIDGET_TYPE_TEXT:
              {
              char* text = pdf_text_widget_text (mContext, idoc, widget);
              char* current_text = text;
              int retry = 0;

              do {
                current_text = winTextInput (current_text, retry);
                retry = 1;
                } while (current_text && !pdf_text_widget_set_text (mContext, idoc, widget, current_text));

              fz_free(mContext, text);
              updatePage();
              }
              break;
            //}}}
            case PDF_WIDGET_TYPE_LISTBOX:
            //{{{
            case PDF_WIDGET_TYPE_COMBOBOX:
              {
              int nopts;
              int nvals;
              const char **opts = NULL;
              const char **vals = NULL;

              fz_var(opts);
              fz_var(vals);
              fz_var(nopts);
              fz_var(nvals);

              fz_try(mContext) {
                nopts = pdf_choice_widget_options(mContext, idoc, widget, 0, NULL);
                opts = (const char**)fz_malloc (mContext, nopts * sizeof(*opts));
                (void)pdf_choice_widget_options (mContext, idoc, widget, 0, opts);

                nvals = pdf_choice_widget_value (mContext, idoc, widget, NULL);
                vals = (const char**)fz_malloc (mContext, MAX(nvals,nopts) * sizeof(*vals));
                (void)pdf_choice_widget_value (mContext, idoc, widget, vals);

                if (winChoiceInput (nopts, opts, &nvals, vals)) {
                  pdf_choice_widget_set_value (mContext, idoc, widget, nvals, vals);
                  updatePage();
                  }
                }
              fz_always(mContext) {
                fz_free (mContext, opts);
                fz_free (mContext, vals);
                }
              fz_catch(mContext) {
                winWarn ("setting of choice failed");
                }
              }
              break;
            //}}}
            //{{{
            case PDF_WIDGET_TYPE_SIGNATURE:
              if (state == -1) {
                char ebuf[256];

                if (pdf_dict_get (mContext, ((pdf_annot *)widget)->obj, PDF_NAME(V))) {
                  /* Signature is signed. Check the signature */
                  ebuf[0] = 0;
                  if (pdf_check_signature (mContext, idoc, widget, ebuf, sizeof(ebuf)))
                    winWarn ("Signature is valid");
                  else {
                    if (ebuf[0] == 0)
                      winWarn ("Signature check failed for unknown reason");
                    else
                      winWarn (ebuf);
                    }
                  }
                }
              break;
            //}}}
            }
          }
        nowaitcursor = 0;
        processed = 1;
        }
      }

    for (link = mPageLinks; link; link = link->next)
      if (p.x >= link->rect.x0 && p.x <= link->rect.x1)
        if (p.y >= link->rect.y0 && p.y <= link->rect.y1)
          break;

    if (link) {
      //{{{  link
      winCursor (eHAND);
      if (btn == 1 && state == 1 && !processed) {
        if (fz_is_external_link (mContext, link->uri)) {
          //appgotouri (app, link->uri);
          }
        else
          gotoPage(fz_resolve_link(mContext, mDocument, link->uri, NULL, NULL) + 1);
        return;
        }
      }
      //}}}
    else {
      fz_annot* annot;
      for (annot = fz_first_annot (mContext, mPage); annot; annot = fz_next_annot (mContext, annot)) {
        fz_rect rect = fz_bound_annot(mContext, annot);
        if (x >= rect.x0 && x < rect.x1)
          if (y >= rect.y0 && y < rect.y1)
            break;
        }
      if (annot)
        winCursor (eCARET);
      else
        winCursor (eARROW);
      }

    if (state == 1 && !processed) {
      if (btn == 1 && !iscopying) {
        isPanning = 1;
        selx = x;
        sely = y;
        beyondy = 0;
        }
      if (btn == 3 && !isPanning) {
        iscopying = 1;
        selx = x;
        sely = y;
        selr.x0 = x;
        selr.x1 = x;
        selr.y0 = y;
        selr.y1 = y;
        }
      if (btn == 4 || btn == 5) /* scroll wheel */
        onScroll (modifiers, btn == 4 ? 1 : -1);
      if (btn == 6 || btn == 7) /* scroll wheel (horizontal) */
        /* scroll left/right or up/down if shift is pressed */
        onScroll (modifiers ^ (1<<0), btn == 6 ? 1 : -1);
      }

    else if (state == -1) {
      if (iscopying) {
        //{{{  copying
        iscopying = 0;
        selr.x0 = fz_mini (selx, x) - panx + irect.x0;
        selr.x1 = fz_maxi (selx, x) - panx + irect.x0;
        selr.y0 = fz_mini (sely, y) - pany + irect.y0;
        selr.y1 = fz_maxi (sely, y) - pany + irect.y0;
        InvalidateRect (gHwndView, NULL, 0);
        if (selr.x0 < selr.x1 && selr.y0 < selr.y1)
          doCopy();
        }
        //}}}
      isPanning = 0;
      }
    else if (isPanning) {
      //{{{  panning
      int newx = panx + x - selx;
      int newy = pany + y - sely;
      int imgh = mWinHeight;
      if (mImage)
        imgh = fz_pixmap_height (mContext, mImage);

      /* Scrolling beyond limits implies flipping pages */
      /* Are we requested to scroll beyond limits? */
      if (newy + imgh < mWinHeight || newy > 0) {
        /* Yes. We can assume that deltay != 0 */
        int deltay = y - sely;
        /* Check whether the panning has occurred in the direction that we are already crossing the
         * limit it. If not, we can conclude that we have switched ends of the page and will thus start over counting */
        if (beyondy == 0 || (beyondy ^ deltay) >= 0) {
          /* Updating how far we are beyond and flipping pages if beyond threshold */
          beyondy += deltay;
          if (beyondy > BEYOND_THRESHHOLD) {
            if( mPageNumber > 1) {
              mPageNumber--;
              showPage (1, 1, 1, 0);
              if (mImage)
                newy = -fz_pixmap_height (mContext, mImage);
              }
            beyondy = 0;
            }
          else if (beyondy < -BEYOND_THRESHHOLD) {
            if (mPageNumber < mPageCount) {
              mPageNumber++;
              showPage (1, 1, 1, 0);
              newy = 0;
              }
            beyondy = 0;
            }
          }
        else
          beyondy = 0;
        }
      /* Although at this point we've already determined that or that no scrolling will be performed in
       * y-direction, the x-direction has not yet been taken care off. Therefore */
      panView (newx, newy);

      selx = x;
      sely = y;
      }
      //}}}
    else if (iscopying) {
      //{{{  copying
      selr.x0 = fz_mini (selx, x) - panx + irect.x0;
      selr.x1 = fz_maxi (selx, x) - panx + irect.x0;
      selr.y0 = fz_mini (sely, y) - pany + irect.y0;
      selr.y1 = fz_maxi (sely, y) - pany + irect.y0;
      InvalidateRect (gHwndView, NULL, 0);
      }
      //}}}
    }
  //}}}
  //{{{
  void onResize (int w, int h) {

    if (mWinWidth != w || mWinHeight != h) {
      mWinWidth = w;
      mWinHeight = h;
      panView (panx, pany);
      InvalidateRect (gHwndView, NULL, 0);
      }
    }
  //}}}

  //  vars
  fz_context* mContext;
  fz_document* mDocument;
  char* mDocPath;
  char* mDocTitle;
  fz_outline* mOutline;
  //{{{  page
  int mPageCount;
  int mPageNumber;

  fz_page* mPage;
  fz_rect mPageBoundingBox;

  fz_display_list* mPageList;
  fz_display_list* mAnnotationsList;
  fz_stext_page* mPageText;
  fz_link* mPageLinks;

  int mErrored;
  int mIncomplete;
  //}}}
  //{{{  layout
  float layoutWidth;
  float layoutHeight;
  float mLayoutEm;
  char* layout_css;
  int layout_use_doc_css;
  //}}}
  //{{{  current view
  int mResolution;
  int rotate;

  fz_pixmap* mImage;
  fz_colorspace* mColorspace;

  int invert;
  //}}}
  //{{{  window system sizes
  int mWinWidth, mWinHeight;
  int scrw, scrh;

  int fullscreen;
  bool mExit = false;
  //}}}
  //{{{  event handling state
  char number[256];
  int numberlen;

  int isPanning;
  int panx, pany;

  int iscopying;
  int selx, sely;
  int beyondy;
  fz_rect selr;

  int nowaitcursor;
  //}}}
  //{{{  search state
  int isSearching;
  int searchdir;
  char search[512];
  int searchpage;

  int hit_count;
  fz_quad hit_bbox[512];
  //}}}

private:
  //{{{
  int makeFakeDoc() {

    fz_buffer* contents = NULL;
    fz_var (contents);

    pdf_obj* page_obj = NULL;
    fz_var (page_obj);

    pdf_document* pdf = NULL;
    fz_try (mContext) {
      pdf = pdf_create_document (mContext);

      fz_rect mediabox = { 0, 0, (float)mWinWidth, (float)mWinHeight };
      contents = fz_new_buffer (mContext, 100);
      fz_append_printf (mContext, contents, "1 0 0 RG %g w 0 0 m %g %g l 0 %g m %g 0 l s\n",
        fz_min (mediabox.x1, mediabox.y1) / 20,
        mediabox.x1, mediabox.y1,
        mediabox.y1, mediabox.x1);

      // Create enough copies of blank page so that page number  preserved if and when subsequent load works
      page_obj = pdf_add_page (mContext, pdf, mediabox, 0, NULL, contents);
      for (int i = 0; i < mPageCount; i++)
        pdf_insert_page (mContext, pdf, -1, page_obj);
      }
    fz_always (mContext) {
      pdf_drop_obj (mContext, page_obj);
      fz_drop_buffer (mContext, contents);
      }
    fz_catch (mContext) {
      fz_drop_document (mContext, (fz_document*)pdf);
      return 1;
      }

    mDocument = (fz_document*)pdf;
    return 0;
    }
  //}}}
  //{{{
  void recreateAnnotations() {

    int mErrored = 0;
    fz_cookie cookie = { 0 };

    fz_drop_display_list (mContext, mAnnotationsList);
    mAnnotationsList = NULL;

    fz_device* mdev = NULL;
    fz_var (mdev);
    fz_try (mContext) {
      /* Create display list */
      mAnnotationsList = fz_new_display_list (mContext, fz_infinite_rect);
      mdev = fz_new_list_device (mContext, mAnnotationsList);

      for (fz_annot* annot = fz_first_annot (mContext, mPage); annot; annot = fz_next_annot(mContext, annot))
        fz_run_annot (mContext, annot, mdev, fz_identity, &cookie);

      if (cookie.incomplete)
        mIncomplete = 1;
      else if (cookie.errors) {
        winWarn ("Errors found on page");
        mErrored = 1;
        }
      fz_close_device (mContext, mdev);
      }
    fz_always (mContext) {
      fz_drop_device (mContext, mdev);
      }
    fz_catch (mContext) {
      winWarn ("Cannot load page");
      mErrored = 1;
      }
    }
  //}}}

  //{{{
  void panView (int newx, int newy) {

    int imageWidth  = 0;
    int imageHeight = 0;
    if (mImage) {
      imageWidth = fz_pixmap_width (mContext, mImage);
      imageHeight = fz_pixmap_height (mContext, mImage);
      }

    if (newx > 0)
      newx = 0;
    if (newy > 0)
      newy = 0;

    if (newx + imageWidth < mWinWidth)
      newx = mWinWidth - imageWidth;
    if (newy + imageHeight < mWinHeight)
      newy = mWinHeight - imageHeight;

    if (mWinWidth >= imageWidth)
      newx = (mWinWidth - imageWidth) / 2;
    if (mWinHeight >= imageHeight)
      newy = (mWinHeight - imageHeight) / 2;

    if (newx != panx || newy != pany)
      InvalidateRect (gHwndView, NULL, 0);

    panx = newx;
    pany = newy;
    }
  //}}}
  //{{{
  int zoomIn (int oldres) {

    int i;
    for (i = 0; i < nelem (zoomList) - 1; ++i)
      if (zoomList[i] <= oldres && zoomList[i+1] > oldres)
        return zoomList[i+1];
    return zoomList[i];
    }
  //}}}
  //{{{
  int zoomOut (int oldres) {

    int i;
    for (i = 0; i < nelem (zoomList) - 1; ++i)
      if (zoomList[i] < oldres && zoomList[i+1] >= oldres)
        return zoomList[i];
    return zoomList[0];
    }
  //}}}
  //{{{
  void onScroll (int modifiers, int dir) {

    isPanning = iscopying = 0;
    if (modifiers & (1<<2)) {
      /* zoom in/out if ctrl is pressed */
      if (dir < 0)
        mResolution = zoomIn (mResolution);
      else
        mResolution = zoomOut (mResolution);
      if (mResolution > MAXRES)
        mResolution = MAXRES;
      if (mResolution < MINRES)
        mResolution = MINRES;
      showPage (0, 1, 1, 0);
    }
    else {
      /* scroll up/down, or left/right if shift is pressed */
      int w = fz_pixmap_width (mContext, mImage);
      int h = fz_pixmap_height (mContext, mImage);
      int xstep = 0;
      int ystep = 0;
      int pagestep = 0;
      if (modifiers & (1<<0)) {
        if (dir > 0 && panx >= 0)
          pagestep = -1;
        else if (dir < 0 && panx <= mWinWidth - w)
          pagestep = 1;
        else
          xstep = 20 * dir;
      }
      else {
        if (dir > 0 && pany >= 0)
          pagestep = -1;
        else if (dir < 0 && pany <= mWinHeight - h)
          pagestep = 1;
        else
          ystep = 20 * dir;
      }
      if (pagestep == 0)
        panView (panx + xstep, pany + ystep);
      else if (pagestep > 0 && mPageNumber < mPageCount) {
        mPageNumber++;
        pany = 0;
        showPage (1, 1, 1, 0);
      }
      else if (pagestep < 0 && mPageNumber > 1) {
        mPageNumber--;
        pany = INT_MIN;
        showPage (1, 1, 1, 0);
      }
    }
  }
  //}}}

  const int zoomList[11] = { 18, 24, 36, 54, 72, 96, 120, 144, 180, 216, 288 };
  };
//}}}
cPdf* gPdf;

//{{{
INT_PTR CALLBACK dlogInfoProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {

  switch (message) {
    case WM_INITDIALOG: {
      wchar_t wbuf[PATH_MAX];
      SetDlgItemTextW (hwnd, 0x10, wbuf);

      fz_context* context = gPdf->mContext;
      fz_document* document = gPdf->mDocument;
      char buf[256];
      if (fz_lookup_metadata (context, document, FZ_META_FORMAT, buf, sizeof buf) >= 0)
        SetDlgItemTextA (hwnd, 0x11, buf);
      else {
        SetDlgItemTextA (hwnd, 0x11, "Unknown");
        SetDlgItemTextA (hwnd, 0x12, "None");
        SetDlgItemTextA (hwnd, 0x13, "n/a");
        return TRUE;
        }

      if (fz_lookup_metadata (context, document, FZ_META_ENCRYPTION, buf, sizeof buf) >= 0)
        SetDlgItemTextA (hwnd, 0x12, buf);
      else
        SetDlgItemTextA (hwnd, 0x12, "None");

      buf[0] = 0;
      if (fz_has_permission (context, document, FZ_PERMISSION_PRINT))
        strcat (buf, "print, ");
      if (fz_has_permission (context, document, FZ_PERMISSION_COPY))
        strcat (buf, "copy, ");
      if (fz_has_permission (context, document, FZ_PERMISSION_EDIT))
        strcat (buf, "edit, ");
      if (fz_has_permission (context, document, FZ_PERMISSION_ANNOTATE))
        strcat (buf, "annotate, ");
      if (strlen (buf) > 2)
        buf[strlen (buf)-2] = 0;
      else
        strcpy (buf, "none");
      SetDlgItemTextA (hwnd, 0x13, buf);

      wchar_t bufx[256];
      #define SETUTF8(ID, STRING) \
        if (fz_lookup_metadata (context, document, "info:" STRING, buf, sizeof buf) >= 0) {  \
          MultiByteToWideChar (CP_UTF8, 0, buf, -1, bufx, nelem (bufx));            \
          SetDlgItemTextW (hwnd, ID, bufx);                                         \
          }

      SETUTF8 (0x20, "Title");
      SETUTF8 (0x21, "Author");
      SETUTF8 (0x22, "Subject");
      SETUTF8 (0x23, "Keywords");
      SETUTF8 (0x24, "Creator");
      SETUTF8 (0x25, "Producer");
      SETUTF8 (0x26, "CreationDate");
      SETUTF8 (0x27, "ModDate");
      return TRUE;
      }
    case WM_COMMAND:
      EndDialog (hwnd, 1);
      return TRUE;
    }

  return FALSE;
  }
//}}}
//{{{
void winInfo() {
  if (DialogBoxW (NULL, L"IDD_DLOGINFO", gHwndFrame, dlogInfoProc) <= 0)
    winError ("cannot create info dialog");
  }
//}}}

//{{{
void handleKey (int c) {

  int modifier = (GetAsyncKeyState (VK_SHIFT) < 0) | ((GetAsyncKeyState (VK_CONTROL) < 0) << 2);

  if (GetCapture() == gHwndView)
    return;

  if (gJustCopied) {
    gJustCopied = false;
    InvalidateRect (gHwndView, NULL, 0);
    }

  // translate VK into ASCII equivalents
  if (c > 256) {
    switch (c - 256) {
      case VK_F1: c = '?'; break;
      case VK_ESCAPE: c = '\033'; break;
      case VK_DOWN: c = 'j'; break;
      case VK_UP: c = 'k'; break;
      case VK_LEFT: c = 'b'; break;
      case VK_RIGHT: c = ' '; break;
      case VK_PRIOR: c = ','; break;
      case VK_NEXT: c = '.'; break;
      }
    }

  gPdf->onKey (c, modifier);
  InvalidateRect (gHwndView, NULL, 0);
  }
//}}}
//{{{
void handleMouse (int x, int y, int btn, int state) {

  int modifier = (GetAsyncKeyState (VK_SHIFT) < 0) | ((GetAsyncKeyState (VK_CONTROL) < 0) << 2);

  if ((state != 0) && gJustCopied) {
    gJustCopied = false;
    InvalidateRect (gHwndView, NULL, 0);
    }

  if (state == 1)
    SetCapture (gHwndView);
  if (state == -1)
    ReleaseCapture();

  gPdf->onMouse (x, y, btn, modifier, state);
  }
//}}}
//{{{
LRESULT CALLBACK viewProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {

  int x = (signed short)LOWORD(lParam);
  int y = (signed short)HIWORD(lParam);

  switch (message) {
    //{{{
    case WM_SIZE:
      if (wParam == SIZE_MINIMIZED)
        return 0;

      gPdf->onResize (LOWORD(lParam), HIWORD(lParam));
      break;
    //}}}
    //{{{
    case WM_PAINT: {
      PAINTSTRUCT ps;
      BeginPaint (hwnd, &ps);
      EndPaint (hwnd, &ps);

      return 0;
      }
    //}}}
    //{{{
    case WM_ERASEBKGND:
      return 1; // well, we don't need to erase to redraw cleanly
    //}}}

    //{{{
    case WM_LBUTTONDOWN:
      SetFocus (gHwndView);
      gOldx = x;
      gOldy = y;
      handleMouse (x, y, 1, 1);

      return 0;
    //}}}
    //{{{
    case WM_LBUTTONUP:
      gOldx = x;
      gOldy = y;
      handleMouse (x, y, 1, -1);

      return 0;
    //}}}

    //{{{
    case WM_MBUTTONDOWN:
      SetFocus (gHwndView);
      gOldx = x;
      gOldy = y;
      handleMouse (x, y, 2, 1);

      return 0;
    //}}}
    //{{{
    case WM_MBUTTONUP:
      gOldx = x;
      gOldy = y;
      handleMouse (x, y, 2, -1);

      return 0;
    //}}}

    //{{{
    case WM_RBUTTONDOWN:
      SetFocus (gHwndView);
      gOldx = x;
      gOldy = y;
      handleMouse (x, y, 3, 1);

      return 0;
    //}}}
    //{{{
    case WM_RBUTTONUP:
      gOldx = x;
      gOldy = y;
      handleMouse(x, y, 3, -1);

      return 0;
    //}}}

    //{{{
    case WM_MOUSEMOVE:
      gOldx = x;
      gOldy = y;
      handleMouse (x, y, 0, 0);
      return 0;
    //}}}
    //{{{
    case WM_MOUSEWHEEL:
      if ((signed short)HIWORD(wParam) <= 0) {
        handleMouse (gOldx, gOldy, 4, 1);
        handleMouse (gOldx, gOldy, 4, -1);
        }
      else {
        handleMouse (gOldx, gOldy, 5, 1);
        handleMouse (gOldx, gOldy, 5, -1);
        }

      return 0;
    //}}}

    //{{{
    case WM_KEYDOWN:
      /* only handle special keys */
      switch (wParam) {
        case VK_F1:
        case VK_LEFT:
        case VK_UP:
        case VK_PRIOR:
        case VK_RIGHT:
        case VK_DOWN:
        case VK_NEXT:
        case VK_ESCAPE:
          handleKey (wParam + 256);
          handleMouse (gOldx, gOldy, 0, 0);  /* update cursor */
          return 0;
        }

      return 1;
    //}}}
    //{{{
    /* unicode encoded chars, including escape, backspace etc... */
    case WM_CHAR:
      if (wParam < 256) {
        handleKey (wParam);
        handleMouse (gOldx, gOldy, 0, 0);  /* update cursor */
        }

      return 0;
    //}}}

    //{{{
    // use WM_APP to trigge a reload and repaint of a page
    case WM_APP:
      break;
    //}}}
    }
  fflush (stdout);

  return DefWindowProc (hwnd, message, wParam, lParam);
  }
//}}}
//{{{
LRESULT CALLBACK frameProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {

  switch (message) {
    case WM_SETFOCUS:
      PostMessage (hwnd, WM_APP+5, 0, 0);
      return 0;

    case WM_APP+5:
      SetFocus (gHwndView);
      return 0;

    case WM_DESTROY:
      gPdf->mExit = true;
      PostQuitMessage (0);
      return 0;

    case WM_SYSCOMMAND:
      if (wParam == ID_ABOUT) {
        winHelp();
        return 0;
        }
      else if (wParam == ID_DOCINFO) {
        winInfo();
        return 0;
        }
      break;

    case WM_SIZE: {
      // More generally, you should use GetEffectiveClientRect if you have a toolbar etc.
      RECT rect;
      GetClientRect (hwnd, &rect);
      MoveWindow (gHwndView, rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top, TRUE);
      return 0;
      }

    case WM_NOTIFY:
    case WM_COMMAND:
      return SendMessage (gHwndView, message, wParam, lParam);

    case WM_CLOSE:
      return 0;

    case WM_SIZING:
      break;
    }

  return DefWindowProc (hwnd, message, wParam, lParam);
  }
//}}}

//{{{
int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {

  gPdf = new cPdf();

  // get command line
  int argc;
  LPWSTR* wargv = CommandLineToArgvW (GetCommandLineW(), &argc);
  char** argv = fz_argv_from_wargv (argc, wargv);
  //{{{  get options
  int c;
  while ((c = fz_getopt (argc, argv, "A:B:C:")) != -1) {
    switch (c) {
      case 'A': break;
      case 'B': break;
      case 'C': break;
      }
    }
  //}}}

  // get app name
  char argv0[256];
  GetModuleFileNameA (NULL, argv0, sizeof argv0);
  winInstallApp (argv0);

  //{{{  create, register windowFrame class
  WNDCLASS wc;
  memset(&wc, 0, sizeof(wc));
  wc.style = 0;
  wc.lpfnWndProc = frameProc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = GetModuleHandle (NULL);
  wc.hIcon = LoadIconA (wc.hInstance, "IDI_ICONAPP");
  wc.hCursor = NULL; //LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = NULL;
  wc.lpszMenuName = NULL;
  wc.lpszClassName = L"FrameWindow";
  ATOM a = RegisterClassW (&wc);
  if (!a)
    winError ("cannot register frame window class");
  //}}}
  //{{{  create, register windowView class
  memset(&wc, 0, sizeof(wc));
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = viewProc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = GetModuleHandle (NULL);
  wc.hIcon = NULL;
  wc.hCursor = NULL;
  wc.hbrBackground = NULL;
  wc.lpszMenuName = NULL;
  wc.lpszClassName = L"ViewWindow";
  a = RegisterClassW (&wc);
  if (!a)
    winError ("cannot register view window class");
  //}}}
  //{{{  get screen size
  RECT r;
  SystemParametersInfo (SPI_GETWORKAREA, 0, &r, 0);
  gPdf->scrw = r.right - r.left;
  gPdf->scrh = r.bottom - r.top;
  //}}}
  //{{{  create cursors
  gArrowCursor = LoadCursor (NULL, IDC_ARROW);
  gHandCursor = LoadCursor (NULL, IDC_HAND);
  gWaitCursor = LoadCursor (NULL, IDC_WAIT);
  gCaretCursor = LoadCursor (NULL, IDC_IBEAM);
  //}}}
  //{{{  create window
  gHwndFrame = CreateWindowW (L"FrameWindow", // window class name
                             NULL, // window caption
                             WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                             CW_USEDEFAULT, CW_USEDEFAULT, // initial position
                             300, // initial x size
                             300, // initial y size
                             0, // parent window handle
                             0, // window menu handle
                             0, // program instance handle
                             0); // creation parameters
  if (!gHwndFrame)
    winError ("cannot create frame");

  gHwndView = CreateWindowW (L"ViewWindow", // window class name
                            NULL,
                            WS_VISIBLE | WS_CHILD,
                            CW_USEDEFAULT, CW_USEDEFAULT,
                            CW_USEDEFAULT, CW_USEDEFAULT,
                            gHwndFrame, 0, 0, 0);
  if (!gHwndView)
    winError ("cannot create view");
  //}}}

  SetWindowTextW (gHwndFrame, L"MuPDF");
  HMENU menu = GetSystemMenu (gHwndFrame, 0);
  AppendMenuW (menu, MF_SEPARATOR, 0, NULL);
  AppendMenuW (menu, MF_STRING, ID_ABOUT, L"About MuPDF");
  AppendMenuW (menu, MF_STRING, ID_DOCINFO, L"Document Properties");
  SetCursor (gArrowCursor);

  if (fz_optind < argc) {
    // has filename
    char filename[PATH_MAX];
    strcpy (filename, argv[fz_optind++]);

    // has pageNumber
    if (fz_optind < argc)
      gPdf->mPageNumber = atoi (argv[fz_optind++]);

    gPdf->openFile (filename, 0);

    MSG msg;
    while (GetMessage (&msg, NULL, 0, 0) && !gPdf->mExit) {
      TranslateMessage (&msg);
      DispatchMessage (&msg);
      }

    gPdf->closeFile();
    }

  fz_drop_context (gPdf->mContext);
  fz_free_argv (argc, argv);

  return 0;
  }
//}}}
