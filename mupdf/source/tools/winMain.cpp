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

// Include pdfapp.h *AFTER* the UNICODE defines
#include "mupdf/fitz.h"
#include "mupdf/pdf.h"
#include <time.h>

#include "mupdf/helpers/pkcs7-check.h"
#include "mupdf/helpers/pkcs7-openssl.h"
//}}}
//{{{  defines
// 25% .. 1600%
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
enum { ARROW, HAND, WAIT, CARET };
enum { DISCARD, SAVE, CANCEL };
enum { QUERY_NO, QUERY_YES };

enum panning { DONT_PAN = 0, PAN_TO_TOP, PAN_TO_BOTTOM };

enum { appOUTLINE_DEFERRED = 1, appOUTLINE_LOAD_NOW = 2 };
//}}}

class cApp;
//{{{  global vars
HWND hwndframe = NULL;
HWND hwndview = NULL;
HDC hdc;
HBRUSH bgbrush;
HBRUSH shbrush;
BITMAPINFO* dibinf = NULL;
HCURSOR arrowcurs, handcurs, waitcurs, caretcurs;

int justcopied = 0;

cApp* gApp;

wchar_t wbuf[PATH_MAX];
char filename[PATH_MAX];

char td_textinput[1024] = "";
int td_retry = 0;
int cd_nopts;
int* cd_nvals;
const char** cd_opts;
const char** cd_vals;
int pd_okay = 0;
//}}}

//{{{  windows
//{{{
char* version() {
  return "MuPDF " FZ_VERSION "\n" "Copyright 2006-2017 Artifex Software, Inc.\n";
  }
//}}}
//{{{
char* usage() {
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
    "[\t\t-- decrease font size (EPUB only)\n"
    "]\t\t-- increase font size (EPUB only)\n"
    "w\t\t-- shrinkwrap\n"
    "f\t\t-- fullscreen\n"
    "r\t\t-- reload file\n"
    ". pgdn right spc\t-- next page\n"
    ", pgup left b bkspc\t-- previous page\n"
    ">\t\t-- next 10 pages\n"
    "<\t\t-- back 10 pages\n"
    "m\t\t-- mark page for snap back\n"
    "t\t\t-- pop back to latest mark\n"
    "1m\t\t-- mark page in register 1\n"
    "1t\t\t-- go to page in register 1\n"
    "G\t\t-- go to last page\n"
    "123g\t\t-- go to page 123\n"
    "/\t\t-- search forwards for text\n"
    "?\t\t-- search backwards for text\n"
    "n\t\t-- find next search result\n"
    "N\t\t-- find previous search result\n"
    "c\t\t-- toggle between color and grayscale\n"
    "i\t\t-- toggle inverted color mode\n"
    "q\t\t-- quit\n"
    ;
  }
//}}}
//{{{
INT_PTR CALLBACK dlogaboutproc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {

  switch(message) {
    case WM_INITDIALOG:
      SetDlgItemTextA (hwnd, 2, version());
      SetDlgItemTextA (hwnd, 3, usage());
      return TRUE;

    case WM_COMMAND:
      EndDialog (hwnd, 1);
      return TRUE;
    }

  return FALSE;
  }
//}}}

//{{{
INT_PTR CALLBACK dlogtextproc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {

  switch (message) {
    case WM_INITDIALOG:
      SetDlgItemTextA (hwnd, 3, td_textinput);
      if (!td_retry)
        ShowWindow (GetDlgItem (hwnd, 4), SW_HIDE);
      return TRUE;

    case WM_COMMAND:
      switch (wParam) {
        case 1:
          pd_okay = 1;
          GetDlgItemTextA (hwnd, 3, td_textinput, sizeof td_textinput);
          EndDialog (hwnd, 1);
          return TRUE;
        case 2:
          pd_okay = 0;
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
INT_PTR CALLBACK dlogchoiceproc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {

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
          pd_okay = 1;
          EndDialog (hwnd, 1);
          return TRUE;

        case 2:
          pd_okay = 0;
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

  MessageBoxA (hwndframe, msg, "MuPDF: Error", MB_ICONERROR);
  exit (1);
  }
//}}}
//{{{
void winWarn (char* msg) {
  MessageBoxA (hwndframe, msg, "MuPDF: Warning", MB_ICONWARNING);
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

  pressed = MessageBoxA (hwndframe, alert->message, alert->title, icon|buttons);

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
void winHelp (cApp* app) {
  if (DialogBoxW (NULL, L"IDD_DLOGABOUT", hwndframe, dlogaboutproc) <= 0)
    winError ("cannot create help dialog");
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
void winCursor (int curs) {

  if (curs == ARROW)
    SetCursor (arrowcurs);
  if (curs == HAND)
    SetCursor (handcurs);
  if (curs == WAIT)
    SetCursor (waitcurs);
  if (curs == CARET)
    SetCursor (caretcurs);
  }
//}}}
//{{{
void winTitle (char *title) {

  wchar_t wide[256];
  wchar_t* dp = wide;
  char* sp = title;

  while (*sp && dp < wide + 255) {
    int rune;
    sp += fz_chartorune (&rune, sp);
    *dp++ = rune;
    }
  *dp = 0;

  SetWindowTextW (hwndframe, wide);
  }
//}}}

//{{{
void winResize (int w, int h) {

  ShowWindow (hwndframe, SW_SHOWDEFAULT);
  w += GetSystemMetrics (SM_CXFRAME) * 2;
  h += GetSystemMetrics (SM_CYFRAME) * 2;
  h += GetSystemMetrics (SM_CYCAPTION);
  SetWindowPos (hwndframe, 0, 0, 0, w, h, SWP_NOZORDER | SWP_NOMOVE);
  }
//}}}
//{{{
void winFullScreen (int state) {

  static WINDOWPLACEMENT savedplace;
  static int isfullscreen = 0;

  if (state && !isfullscreen) {
    GetWindowPlacement (hwndframe, &savedplace);
    SetWindowLong (hwndframe, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    SetWindowPos (hwndframe, NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
    ShowWindow (hwndframe, SW_SHOWMAXIMIZED);
    isfullscreen = 1;
    }

  if (!state && isfullscreen) {
    SetWindowLong (hwndframe, GWL_STYLE, WS_OVERLAPPEDWINDOW);
    SetWindowPos (hwndframe, NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
    SetWindowPlacement (hwndframe, &savedplace);
    isfullscreen = 0;
    }
  }
//}}}

//{{{
char* winTextInput (char* inittext, int retry) {

  td_retry = retry;

  fz_strlcpy (td_textinput, inittext ? inittext : "", sizeof td_textinput);
  if (DialogBoxW (NULL, L"IDD_DLOGTEXT", hwndframe, dlogtextproc) <= 0)
    winError ("cannot create text input dialog");

  if (pd_okay)
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

  if (DialogBoxW (NULL, L"IDD_DLOGLIST", hwndframe, dlogchoiceproc) <= 0)
    winError ("cannot create text input dialog");

  return pd_okay;
  }
//}}}

//{{{
void winDrawRect (int x0, int y0, int x1, int y1) {

  RECT r;
  r.left = x0;
  r.top = y0;
  r.right = x1;
  r.bottom = y1;
  FillRect (hdc, &r, (HBRUSH)GetStockObject(WHITE_BRUSH));
  }
//}}}
//{{{
void winDrawString (int x, int y, char *s) {

  HFONT font = (HFONT)GetStockObject (ANSI_FIXED_FONT);
  SelectObject (hdc, font);
  TextOutA (hdc, x, y - 12, s, (int)strlen(s));
  }
//}}}

//{{{
void installApp (char* argv0) {

  char buf[512];
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
//}}}

//{{{
void event_cb (fz_context* ctx, pdf_document* doc, pdf_doc_event* event, void* data) {

  cApp* app = (cApp*)data;

  switch (event->type) {
    case PDF_DOCUMENT_EVENT_ALERT:
      winAlert (pdf_access_alert_event (ctx, event));
      break;

    case PDF_DOCUMENT_EVENT_PRINT:
    case PDF_DOCUMENT_EVENT_EXEC_MENU_ITEM:
      winWarn ("The document attempted to execute menu item");
      break;

    case PDF_DOCUMENT_EVENT_EXEC_DIALOG:
      winWarn ("The document attempted to open a dialog box. (Not supported)");
      break;

    case PDF_DOCUMENT_EVENT_LAUNCH_URL:
      winWarn ("The document attempted to open url: %s. (Not supported by app)",
                   pdf_access_launch_url_event(ctx, event)->url);
      break;

    case PDF_DOCUMENT_EVENT_MAIL_DOC:
      winWarn ("The document attempted to mail the document");
      break;
    }
  }
//}}}
//{{{
class cApp {
public:
  //{{{
  cApp() {

    memset (this, 0, sizeof(cApp));

    ctx = fz_new_context (NULL, NULL, FZ_STORE_DEFAULT);
    colorspace = fz_device_bgr (ctx);

    scrw = 640;
    scrh = 480;
    resolution = 72;

    layout_w = 450;
    layout_h = 600;
    layout_em = 12;
    layout_css = NULL;
    layout_use_doc_css = 1;

    tint_r = 255;
    tint_g = 250;
    tint_b = 240;
    }
  //}}}
  //{{{
  void setResolution (int res) {
    resolution = res;
    }
  //}}}

  //{{{
  void viewCtm (fz_matrix* mat) {
    *mat = fz_transform_page (page_bbox, resolution, rotate);
    }
  //}}}
  //{{{
  void invertHit() {

    fz_matrix ctm;
    viewCtm (&ctm);

    int i;
    for (i = 0; i < hit_count; i++) {
      fz_rect bbox;
      bbox = fz_rect_from_quad (hit_bbox[i]);
      bbox = fz_transform_rect (bbox, ctm);
      fz_invert_pixmap_rect (ctx, image, fz_round_rect (bbox));
      }
    }
  //}}}
  //{{{
  void blitSearch() {

    if (issearching) {
      char buf[sizeof (search) + 50];
      sprintf (buf, "Search: %s", search);
      winDrawRect (0, 0, winw, 30);
      winDrawString (10, 20, buf);
      }
    }
  //}}}
  //{{{
  void blit() {

    int image_w = fz_pixmap_width (ctx, image);
    int image_h = fz_pixmap_height (ctx, image);
    int image_n = fz_pixmap_components (ctx, image);
    unsigned char* samples = fz_pixmap_samples (ctx, image);

    int x0 = panx;
    int y0 = pany;
    int x1 = panx + image_w;
    int y1 = pany + image_h;

    if (image) {
      if (iscopying || justcopied) {
        fz_invert_pixmap_rect (ctx, image, fz_round_rect (selr));
        justcopied = 1;
        }

      invertHit();

      dibinf->bmiHeader.biWidth = image_w;
      dibinf->bmiHeader.biHeight = -image_h;
      dibinf->bmiHeader.biSizeImage = image_h * 4;

      if (image_n == 2) {
        int i = image_w * image_h;
        unsigned char* color = (unsigned char*)malloc (i*4);
        unsigned char* s = samples;
        unsigned char* d = color;
        for (; i > 0 ; i--) {
          d[2] = d[1] = d[0] = *s++;
          d[3] = *s++;
          d += 4;
          }
        SetDIBitsToDevice (hdc, panx, pany, image_w, image_h,
                           0, 0, 0, image_h, color, dibinf, DIB_RGB_COLORS);
        free (color);
        }
      else if (image_n == 4)
        SetDIBitsToDevice (hdc, panx, pany, image_w, image_h,
                           0, 0, 0, image_h, samples, dibinf, DIB_RGB_COLORS);

      invertHit();

      if (iscopying || justcopied) {
        fz_invert_pixmap_rect (ctx, image, fz_round_rect (selr));
        justcopied = 1;
        }
      }

    // Grey background
    RECT r;
    r.top = 0;
    r.bottom = winh;
    r.left = 0;
    r.right = x0;
    FillRect (hdc, &r, bgbrush);

    r.left = x1;
    r.right = winw;
    FillRect (hdc, &r, bgbrush);

    r.left = 0;
    r.right = winw;
    r.top = 0;
    r.bottom = y0;
    FillRect (hdc, &r, bgbrush);

    r.top = y1;
    r.bottom = winh;
    FillRect (hdc, &r, bgbrush);

    /* Drop shadow */
    r.left = x0 + 2;
    r.right = x1 + 2;
    r.top = y1;
    r.bottom = y1 + 2;
    FillRect (hdc, &r, shbrush);

    r.left = x1;
    r.right = x1 + 2;
    r.top = y0 + 2;
    r.bottom = y1;
    FillRect (hdc, &r, shbrush);

   blitSearch();
    }
  //}}}

  //{{{
  int makeFakeDoc() {

    fz_buffer* contents = NULL;
    fz_var (contents);

    pdf_obj* page_obj = NULL;
    fz_var (page_obj);

    pdf_document* pdf = NULL;
    fz_try(ctx) {
      fz_rect mediabox = { 0, 0, (float)winw, (float)winh };
      int i;

      pdf = pdf_create_document (ctx);

      contents = fz_new_buffer (ctx, 100);
      fz_append_printf (ctx, contents, "1 0 0 RG %g w 0 0 m %g %g l 0 %g m %g 0 l s\n",
        fz_min (mediabox.x1, mediabox.y1) / 20,
        mediabox.x1, mediabox.y1,
        mediabox.y1, mediabox.x1);

      /* Create enough copies of our blank(ish) page so that the
       * page number is preserved if and when a subsequent load works. */
      page_obj = pdf_add_page (ctx, pdf, mediabox, 0, NULL, contents);
      for (i = 0; i < pagecount; i++)
        pdf_insert_page (ctx, pdf, -1, page_obj);
      }

    fz_always(ctx) {
      pdf_drop_obj (ctx, page_obj);
      fz_drop_buffer (ctx, contents);
      }

    fz_catch(ctx) {
      fz_drop_document (ctx, (fz_document*)pdf);
      return 1;
      }

    doc = (fz_document*)pdf;
    return 0;
    }
  //}}}
  //{{{
  void open_progressive (char* filename, int reload, int bps) {

    pdf_document* idoc;

    fz_try (ctx) {
      fz_register_document_handlers (ctx);

      if (layout_css) {
        fz_buffer *buf = fz_read_file (ctx, layout_css);
        fz_set_user_css (ctx, fz_string_from_buffer (ctx, buf));
        fz_drop_buffer (ctx, buf);
        }

      fz_set_use_document_css (ctx, layout_use_doc_css);

      if (bps == 0)
        doc = fz_open_document (ctx, filename);
      else {
        fz_stream* stream = fz_open_file_progressive (ctx, filename, bps);
        while (1) {
          fz_try (ctx) {
            fz_seek (ctx, stream, 0, SEEK_SET);
            doc = fz_open_document_with_stream (ctx, filename, stream);
            }
          fz_catch (ctx) {
            if (fz_caught (ctx) == FZ_ERROR_TRYLATER) {
              winWarn ("not enough data to open yet");
              continue;
              }
            fz_rethrow (ctx);
            }
          break;
          }
        }
      }
    fz_catch (ctx) {
      if (!reload || makeFakeDoc())
        winError ("cannot open document");
      }

    idoc = pdf_specifics (ctx, doc);
    if (idoc) {
      fz_try(ctx) {
        pdf_enable_js (ctx, idoc);
        pdf_set_doc_event_callback (ctx, idoc, event_cb, this);
        }
      fz_catch (ctx) {
        winError ("cannot load javascript embedded in document");
        }
      }

    fz_try(ctx) {
      if (fz_needs_password (ctx, doc))
        winWarn ("needs password.");

      docpath = fz_strdup (ctx, filename);
      doctitle = filename;
      if (strrchr(doctitle, '\\'))
        doctitle = strrchr (doctitle, '\\') + 1;
      if (strrchr (doctitle, '/'))
        doctitle = strrchr (doctitle, '/') + 1;
      doctitle = fz_strdup (ctx, doctitle);

      fz_layout_document (ctx, doc, layout_w, layout_h, layout_em);

      while (1) {
        fz_try(ctx) {
          pagecount = fz_count_pages (ctx, doc);
          if (pagecount <= 0)
            fz_throw(ctx, FZ_ERROR_GENERIC, "No pages in document");
          }
        fz_catch (ctx) {
          if (fz_caught (ctx) == FZ_ERROR_TRYLATER) {
            winWarn ("not enough data to count pages yet");
            continue;
            }
          fz_rethrow (ctx);
          }
        break;
        }

      while (1) {
        fz_try (ctx) {
          outline = fz_load_outline (ctx, doc);
          }

        fz_catch(ctx) {
          outline = NULL;
          if (fz_caught (ctx) == FZ_ERROR_TRYLATER)
            outline_deferred = appOUTLINE_DEFERRED;
          else
            winWarn ("failed to load outline");
          }
        break;
        }
      }
    fz_catch (ctx) {
      winError ("cannot open document");
      }

    if (pageno < 1)
      pageno = 1;
    if (pageno > pagecount)
      pageno = pagecount;
    if (resolution < MINRES)
      resolution = MINRES;
    if (resolution > MAXRES)
      resolution = MAXRES;

    if (!reload) {
      shrinkwrap = 1;
      rotate = 0;
      panx = 0;
      pany = 0;
      }

    showPage (1, 1, 1, 0);
    }
  //}}}
  //{{{
  void open (char* filename, int reload) {
    open_progressive (filename, reload, 0);
    }
  //}}}
  //{{{
  void close() {

    fz_drop_display_list (ctx, page_list);
    page_list = NULL;

    fz_drop_display_list (ctx, annotations_list);
    annotations_list = NULL;

    fz_drop_stext_page (ctx, page_text);
    page_text = NULL;

    fz_drop_link (ctx, page_links);
    page_links = NULL;

    fz_free (ctx, doctitle);
    doctitle = NULL;

    fz_free (ctx, docpath);
    docpath = NULL;

    fz_drop_pixmap (ctx, image);
    image = NULL;

    fz_drop_outline (ctx, outline);
    outline = NULL;

    fz_drop_page (ctx, page);
    page = NULL;

    fz_drop_document (ctx, doc);
    doc = NULL;

    fz_flush_warnings (ctx);
    }
  //}}}
  //{{{
  void reloadFile() {

    char filename[PATH_MAX];
    fz_strlcpy (filename, docpath, PATH_MAX);
    close();
    open (filename, 1);
    }
  //}}}

  //{{{
  void panView (int newx, int newy) {

    int image_w = 0;
    int image_h = 0;

    if (image) {
      image_w = fz_pixmap_width (ctx, image);
      image_h = fz_pixmap_height (ctx, image);
      }

    if (newx > 0)
      newx = 0;
    if (newy > 0)
      newy = 0;

    if (newx + image_w < winw)
      newx = winw - image_w;
    if (newy + image_h < winh)
      newy = winh - image_h;

    if (winw >= image_w)
      newx = (winw - image_w) / 2;
    if (winh >= image_h)
      newy = (winh - image_h) / 2;

    if (newx != panx || newy != pany)
      InvalidateRect (hwndview, NULL, 0);

    panx = newx;
    pany = newy;
    }
  //}}}
  //{{{
  void runPage (fz_device* dev, const fz_matrix ctm, fz_rect scissor, fz_cookie* cookie) {

    if (page_list)
      fz_run_display_list (ctx, page_list, dev, ctm, scissor, cookie);

    if (annotations_list)
      fz_run_display_list (ctx, annotations_list, dev, ctm, scissor, cookie);
    }
  //}}}
  //{{{
  void loadPage (int no_cache) {

    errored = 0;
    fz_cookie cookie = { 0 };

    fz_device *mdev = NULL;
    fz_var (mdev);

    fz_drop_display_list (ctx, page_list);
    fz_drop_display_list (ctx, annotations_list);
    fz_drop_stext_page (ctx, page_text);
    fz_drop_link (ctx, page_links);
    fz_drop_page (ctx, page);

    page_list = NULL;
    annotations_list = NULL;
    page_text = NULL;
    page_links = NULL;
    page = NULL;
    page_bbox.x0 = 0;
    page_bbox.y0 = 0;
    page_bbox.x1 = 100;
    page_bbox.y1 = 100;

    incomplete = 0;

    fz_try(ctx) {
      page = fz_load_page (ctx, doc, pageno - 1);
      page_bbox = fz_bound_page (ctx, page);
      }
    fz_catch(ctx) {
      if (fz_caught (ctx) == FZ_ERROR_TRYLATER)
        incomplete = 1;
      else
        winWarn ("Cannot load page");
      return;
      }

    fz_try (ctx) {
      /* Create display lists */
      page_list = fz_new_display_list (ctx, fz_infinite_rect);
      mdev = fz_new_list_device (ctx, page_list);
      if (no_cache)
        fz_enable_device_hints (ctx, mdev, FZ_NO_CACHE);
      cookie.incomplete_ok = 1;
      fz_run_page_contents (ctx, page, mdev, fz_identity, &cookie);
      fz_close_device (ctx, mdev);
      fz_drop_device (ctx, mdev);

      mdev = NULL;
      annotations_list = fz_new_display_list (ctx, fz_infinite_rect);
      mdev = fz_new_list_device (ctx, annotations_list);

      fz_annot* annot;
      for (annot = fz_first_annot (ctx, page); annot; annot = fz_next_annot(ctx, annot))
        fz_run_annot (ctx, annot, mdev, fz_identity, &cookie);
      if (cookie.incomplete)
        incomplete = 1;
      else if (cookie.errors) {
        winWarn ("Errors found on page");
        errored = 1;
        }
      fz_close_device (ctx, mdev);
      }
    fz_always (ctx) {
      fz_drop_device (ctx, mdev);
      }
    fz_catch (ctx) {
      if (fz_caught (ctx) == FZ_ERROR_TRYLATER)
        incomplete = 1;
      else {
        winWarn ("Cannot load page");
        errored = 1;
        }
      }

    fz_try (ctx) {
      page_links = fz_load_links (ctx, page);
      }
    fz_catch (ctx) {
      if (fz_caught (ctx) == FZ_ERROR_TRYLATER)
        incomplete = 1;
      else if (!errored)
        winWarn ("Cannot load page");
      }

    errored = errored;
    }
  //}}}
  //{{{
  void showPage (int load, int draw, int repaint, int searching) {

    char buf[MAX_TITLE];
    fz_device* idev = NULL;
    fz_device* tdev;
    fz_colorspace* colorspace;
    fz_matrix ctm;
    fz_rect bounds;
    fz_irect ibounds;
    fz_cookie cookie = { 0 };

    if (!nowaitcursor)
      winCursor (WAIT);

    if (load) {
      fz_rect mediabox;
      loadPage (searching);

      /* Zero search hit position */
      hit_count = 0;

      /* Extract text */
      mediabox = fz_bound_page (ctx, page);
      page_text = fz_new_stext_page (ctx, mediabox);

      if (page_list || annotations_list) {
        tdev = fz_new_stext_device (ctx, page_text, NULL);
        fz_try(ctx) {
          runPage (tdev, fz_identity, fz_infinite_rect, &cookie);
          fz_close_device (ctx, tdev);
          }
        fz_always (ctx)
          fz_drop_device (ctx, tdev);
        fz_catch (ctx)
          fz_rethrow (ctx);
        }
      }

    if (draw) {
      char buf2[64];
      size_t len;

      sprintf (buf2, " - %d/%d (%d dpi)", pageno, pagecount, resolution);
      len = MAX_TITLE-strlen (buf2);
      if (strlen (doctitle) > len) {
        fz_strlcpy (buf, doctitle, len-3);
        fz_strlcat (buf, "...", MAX_TITLE);
        fz_strlcat (buf, buf2, MAX_TITLE);
        }
      else
        sprintf (buf, "%s%s", doctitle, buf2);
      winTitle (buf);

      viewCtm (&ctm);
      bounds = fz_transform_rect (page_bbox, ctm);
      ibounds = fz_round_rect (bounds);
      bounds = fz_rect_from_irect (ibounds);

      // Draw
      fz_drop_pixmap (ctx, image);
      if (grayscale)
        colorspace = fz_device_gray (ctx);
      else
        colorspace = colorspace;

      image = NULL;
      fz_var (image);
      fz_var (idev);

      fz_try (ctx) {
        image = fz_new_pixmap_with_bbox (ctx, colorspace, ibounds, NULL, 1);
        fz_clear_pixmap_with_value (ctx, image, 255);
        if (page_list || annotations_list) {
          idev = fz_new_draw_device (ctx, fz_identity, image);
          runPage (idev, ctm, bounds, &cookie);
          fz_close_device (ctx, idev);
          }
        if (invert)
          fz_invert_pixmap (ctx, image);
        if (tint)
          fz_tint_pixmap (ctx, image, tint_r, tint_g, tint_b);
        }
      fz_always (ctx)
        fz_drop_device (ctx, idev);
      fz_catch (ctx)
        cookie.errors++;
      }

    if (repaint) {
      panView (panx, pany);

      if (!image) {
        /* there is no image to blit, but there might be an error message */
        winResize (layout_w, layout_h);
        }
      else if (shrinkwrap) {
        int w = fz_pixmap_width (ctx, image);
        int h = fz_pixmap_height (ctx, image);

        if (winw == w)
          panx = 0;
        if (winh == h)
          pany = 0;
        if (w > scrw * 90 / 100)
          w = scrw * 90 / 100;
        if (h > scrh * 90 / 100)
          h = scrh * 90 / 100;
        if (w != winw || h != winh)
          winResize (w, h);
        }

      InvalidateRect (hwndview, NULL, 0);
      winCursor (ARROW);
      }

    if (cookie.errors && errored == 0) {
      errored = 1;
      winWarn ("Errors found on page. Page rendering may be incomplete.");
      }

    fz_flush_warnings (ctx);
    }
  //}}}
  //{{{
  void recreate_annotationslist() {

    int errored = 0;
    fz_cookie cookie = { 0 };

    fz_device* mdev = NULL;
    fz_var (mdev);

    fz_drop_display_list (ctx, annotations_list);
    annotations_list = NULL;

    fz_try (ctx) {
      /* Create display list */
      annotations_list = fz_new_display_list(ctx, fz_infinite_rect);
      mdev = fz_new_list_device (ctx, annotations_list);

      fz_annot* annot;
      for (annot = fz_first_annot (ctx, page); annot; annot = fz_next_annot(ctx, annot))
        fz_run_annot (ctx, annot, mdev, fz_identity, &cookie);

      if (cookie.incomplete)
        incomplete = 1;
      else if (cookie.errors) {
        winWarn ("Errors found on page");
        errored = 1;
        }
      fz_close_device (ctx, mdev);
      }
    fz_always (ctx) {
      fz_drop_device (ctx, mdev);
      }
    fz_catch (ctx) {
      winWarn ("Cannot load page");
      errored = 1;
      }

    errored = errored;
    }
  //}}}
  //{{{
  void updatePage() {

    if (pdf_update_page (ctx, (pdf_page*)page)) {
      recreate_annotationslist();
      showPage (0, 1, 1, 0);
      }
    else
      showPage (0, 0, 1, 0);
    }
  //}}}

  fz_context* ctx;

  fz_document* doc;
  char* docpath;
  char* doctitle;
  fz_outline* outline;
  int outline_deferred;
  int pagecount;

  float layout_w;
  float layout_h;
  float layout_em;
  char* layout_css;
  int layout_use_doc_css;

  /* current view params */
  int resolution;
  int rotate;
  fz_pixmap* image;
  int grayscale;
  fz_colorspace* colorspace;
  int invert;
  int tint, tint_r, tint_g, tint_b;

  clock_t start_time;

  /* current page params */
  int pageno;
  fz_page* page;
  fz_rect page_bbox;
  fz_display_list* page_list;
  fz_display_list* annotations_list;
  fz_stext_page* page_text;
  fz_link* page_links;
  int errored;
  int incomplete;

  /* window system sizes */
  int winw, winh;
  int scrw, scrh;
  int shrinkwrap;
  int fullscreen;

  /* event handling state */
  char number[256];
  int numberlen;

  int ispanning;
  int panx, pany;

  int iscopying;
  int selx, sely;
  /* TODO - While sely keeps track of the relative change in
   * cursor position between two ticks/events, beyondy shall keep
   * track of the relative change in cursor position from the
   * point where the user hits a scrolling limit. This is ugly.
   * Used in pdfapp.c:apponmouse.
   */
  int beyondy;
  fz_rect selr;

  int nowaitcursor;

  /* search state */
  int issearching;
  int searchdir;
  char search[512];
  int searchpage;
  fz_quad hit_bbox[512];
  int hit_count;
  };
//}}}

//{{{
INT_PTR CALLBACK dloginfoproc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {

  char buf[256];
  wchar_t bufx[256];
  fz_context* ctx = gApp->ctx;
  fz_document* doc = gApp->doc;

  switch (message) {
    case WM_INITDIALOG:
      SetDlgItemTextW (hwnd, 0x10, wbuf);
      if (fz_lookup_metadata (ctx, doc, FZ_META_FORMAT, buf, sizeof buf) >= 0)
        SetDlgItemTextA (hwnd, 0x11, buf);
      else {
        SetDlgItemTextA (hwnd, 0x11, "Unknown");
        SetDlgItemTextA (hwnd, 0x12, "None");
        SetDlgItemTextA (hwnd, 0x13, "n/a");
        return TRUE;
        }

      if (fz_lookup_metadata (ctx, doc, FZ_META_ENCRYPTION, buf, sizeof buf) >= 0)
        SetDlgItemTextA (hwnd, 0x12, buf);
      else
        SetDlgItemTextA (hwnd, 0x12, "None");

      buf[0] = 0;
      if (fz_has_permission (ctx, doc, FZ_PERMISSION_PRINT))
        strcat (buf, "print, ");
      if (fz_has_permission (ctx, doc, FZ_PERMISSION_COPY))
        strcat (buf, "copy, ");
      if (fz_has_permission (ctx, doc, FZ_PERMISSION_EDIT))
        strcat (buf, "edit, ");
      if (fz_has_permission (ctx, doc, FZ_PERMISSION_ANNOTATE))
        strcat (buf, "annotate, ");
      if (strlen (buf) > 2)
        buf[strlen (buf)-2] = 0;
      else
        strcpy (buf, "none");
      SetDlgItemTextA (hwnd, 0x13, buf);

      #define SETUTF8(ID, STRING) \
        if (fz_lookup_metadata (ctx, doc, "info:" STRING, buf, sizeof buf) >= 0) {  \
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

    case WM_COMMAND:
      EndDialog (hwnd, 1);
      return TRUE;
    }

  return FALSE;
  }
//}}}
//{{{
void winInfo() {
  if (DialogBoxW (NULL, L"IDD_DLOGINFO", hwndframe, dloginfoproc) <= 0)
    winError ("cannot create info dialog");
  }
//}}}

static const int zoomList[] = { 18, 24, 36, 54, 72, 96, 120, 144, 180, 216, 288 };
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
void appreloadpage (cApp* app) {

  if (app->outline_deferred == appOUTLINE_LOAD_NOW) {
    fz_try(app->ctx)
      app->outline = fz_load_outline (app->ctx, app->doc);
    fz_catch(app->ctx)
      app->outline = NULL;
    app->outline_deferred = 0;
    }

  app->showPage (1, 1, 1, 0);
  }
//}}}
//{{{
void appgotopage (cApp* app, int number) {

  app->issearching = 0;
  InvalidateRect (hwndview, NULL, 0);

  if (number < 1)
    number = 1;
  if (number > app->pagecount)
    number = app->pagecount;

  if (number == app->pageno)
    return;

  app->pageno = number;
  app->showPage (1, 1, 1, 0);
  }
//}}}
//{{{
void apponresize (cApp* app, int w, int h) {

  if (app->winw != w || app->winh != h) {
    app->winw = w;
    app->winh = h;
    app->panView (app->panx, app->pany);
    InvalidateRect (hwndview, NULL, 0);
    }
  }
//}}}
//{{{
void appautozoom_vertical (cApp* app) {

  app->resolution *= (float) app->winh / fz_pixmap_height(app->ctx, app->image);
  if (app->resolution > MAXRES)
    app->resolution = MAXRES;
  else if (app->resolution < MINRES)
    app->resolution = MINRES;

  app->showPage (0, 1, 1, 0);
  }
//}}}
//{{{
void appautozoom_horizontal (cApp* app) {

  app->resolution *= (float) app->winw / fz_pixmap_width(app->ctx, app->image);
  if (app->resolution > MAXRES)
    app->resolution = MAXRES;
  else if (app->resolution < MINRES)
    app->resolution = MINRES;

  app->showPage (0, 1, 1, 0);
  }
//}}}
//{{{
void appautozoom (cApp* app) {

  float page_aspect = (float) fz_pixmap_width (app->ctx, app->image) / fz_pixmap_height (app->ctx, app->image);
  float win_aspect = (float) app->winw / app->winh;
  if (page_aspect > win_aspect)
    appautozoom_horizontal (app);
  else
    appautozoom_vertical (app);
  }
//}}}
//{{{
void appsearch (cApp* app, enum panning* panto, int dir) {

  /* abort if no search string */
  if (app->search[0] == 0) {
    InvalidateRect (hwndview, NULL, 0);
    return;
    }

  winCursor (WAIT);

  int firstpage = app->pageno;

  int page;
  if (app->searchpage == app->pageno)
    page = app->pageno + dir;
  else
    page = app->pageno;
  if (page < 1)
    page = app->pagecount;
  if (page > app->pagecount)
    page = 1;

  do {
    if (page != app->pageno) {
      app->pageno = page;
      app->showPage (1, 0, 0, 1);
      }

    app->hit_count = fz_search_stext_page (app->ctx, app->page_text, app->search, app->hit_bbox, nelem(app->hit_bbox));
    if (app->hit_count > 0) {
      *panto = dir == 1 ? PAN_TO_TOP : PAN_TO_BOTTOM;
      app->searchpage = app->pageno;
      winCursor (HAND);
      InvalidateRect (hwndview, NULL, 0);
      return;
      }

    page += dir;
    if (page < 1)
      page = app->pagecount;
    if (page > app->pagecount)
      page = 1;
    } while (page != firstpage);

    winWarn ("String '%s' not found.", app->search);

  app->pageno = firstpage;
  app->showPage (1, 0, 0, 0);
  winCursor (HAND);
  InvalidateRect (hwndview, NULL, 0);
  }
//}}}

//{{{
void apponcopy (cApp* app, unsigned short *ucsbuf, int ucslen) {

  fz_stext_page *page = app->page_text;
  int p, need_newline;
  fz_stext_block *block;
  fz_stext_line *line;
  fz_stext_char *ch;

  fz_matrix ctm;
  app->viewCtm (&ctm);
  ctm = fz_invert_matrix (ctm);

  fz_rect sel;
  sel = fz_transform_rect (app->selr, ctm);

  p = 0;
  need_newline = 0;

  for (block = page->first_block; block; block = block->next) {
    if (block->type != FZ_STEXT_BLOCK_TEXT) continue;

    for (line = block->u.t.first_line; line; line = line->next) {
      int saw_text = 0;
      for (ch = line->first_char; ch; ch = ch->next) {
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
void winDoCopy (cApp* app) {

  if (!OpenClipboard (hwndframe))
    return;

  EmptyClipboard();

  HGLOBAL handle = GlobalAlloc (GMEM_MOVEABLE, 4096 * sizeof(unsigned short));
  if (!handle) {
    CloseClipboard();
    return;
    }

  unsigned short* ucsbuf = (unsigned short*)GlobalLock (handle);
  apponcopy (app, ucsbuf, 4096);
  GlobalUnlock (handle);

  SetClipboardData (CF_UNICODETEXT, handle);
  CloseClipboard();

  justcopied = 1; /* keep inversion around for a while... */
  }
//}}}

//{{{
void onKey (cApp* app, int c, int modifiers) {

  int oldpage = app->pageno;
  enum panning panto = PAN_TO_TOP;
  int loadpage = 1;

  if (app->issearching) {
    //{{{  searching
    size_t n = strlen(app->search);
    if (c < ' ') {
      if (c == '\b' && n > 0) {
        app->search[n - 1] = 0;
        InvalidateRect (hwndview, NULL, 0);
        }
      if (c == '\n' || c == '\r') {
        app->issearching = 0;
        if (n > 0) {
          InvalidateRect (hwndview, NULL, 0);
          if (app->searchdir < 0) {
            if (app->pageno == 1)
              app->pageno = app->pagecount;
            else
              app->pageno--;
            app->showPage(1, 1, 0, 1);
            }

          onKey (app, 'n', 0);
          }
        else
          InvalidateRect (hwndview, NULL, 0);
        }
      if (c == '\033') {
        app->issearching = 0;
        InvalidateRect (hwndview, NULL, 0);
        }
      }
    else {
      if (n + 2 < sizeof app->search) {
        app->search[n] = c;
        app->search[n + 1] = 0;
        InvalidateRect(hwndview, NULL, 0);
        }
      }
    return;
    }
    //}}}
  if (c >= '0' && c <= '9') {
    //{{{  number
    app->number[app->numberlen++] = c;
    app->number[app->numberlen] = '\0';
    }
    //}}}

  switch (c) {
    case 'q':
    //{{{
    case 0x1B: {
      fz_context* ctx = gApp->ctx;
      gApp->close();
      free (dibinf);
      fz_drop_context (ctx);
      exit (0);
      break;
      }
    //}}}

    //{{{
    case '[':
      if (app->layout_em > 8) {
        float percent = (float)app->pageno / app->pagecount;
        app->layout_em -= 2;
        fz_layout_document(app->ctx, app->doc, app->layout_w, app->layout_h, app->layout_em);
        app->pagecount = fz_count_pages(app->ctx, app->doc);
        app->pageno = app->pagecount * percent + 0.1f;
        app->showPage (1, 1, 1, 0);
      }
      break;
    //}}}
    //{{{
    case ']':
      if (app->layout_em < 36) {
        float percent = (float)app->pageno / app->pagecount;
        app->layout_em += 2;
        fz_layout_document(app->ctx, app->doc, app->layout_w, app->layout_h, app->layout_em);
        app->pagecount = fz_count_pages(app->ctx, app->doc);
        app->pageno = app->pagecount * percent + 0.1f;
        app->showPage (1, 1, 1, 0);
      }
      break;
    //}}}
    case '+':
    //{{{
    case '=':
      app->resolution = zoomIn (app->resolution);
      app->showPage (0, 1, 1, 0);
      break;
    //}}}
    //{{{
    case '-':
      app->resolution = zoomOut (app->resolution);
      app->showPage (0, 1, 1, 0);
      break;
    //}}}
    //{{{
    case 'W':
      appautozoom_horizontal(app);
      break;
    //}}}
    //{{{
    case 'H':
      appautozoom_vertical(app);
      break;
    //}}}
    //{{{
    case 'Z':
      appautozoom(app);
      break;
    //}}}
    //{{{
    case 'L':
      app->rotate -= 90;
      app->showPage (0, 1, 1, 0);
      break;
    //}}}
    //{{{
    case 'R':
      app->rotate += 90;
      app->showPage (0, 1, 1, 0);
      break;
    //}}}
    //{{{
    case 'C':
      app->tint ^= 1;
      app->showPage (0, 1, 1, 0);
      break;
    //}}}
    //{{{
    case 'c':
      app->grayscale ^= 1;
      app->showPage (0, 1, 1, 0);
      break;
    //}}}
    //{{{
    case 'i':
      app->invert ^= 1;
      app->showPage (0, 1, 1, 0);
      break;
    //}}}
    //{{{
    case 'a':
      app->rotate -= 15;
      app->showPage (0, 1, 1, 0);
      break;
    //}}}
    //{{{
    case 's':
      app->rotate += 15;
      app->showPage (0, 1, 1, 0);
      break;
    //}}}
    //{{{
    case 'f':
      app->shrinkwrap = 0;
      winFullScreen (!app->fullscreen);
      app->fullscreen = !app->fullscreen;
      break;
    //}}}
    //{{{
    case 'w':
      if (app->fullscreen) {
        winFullScreen (0);
        app->fullscreen = 0;
        }

      app->shrinkwrap = 1;
      app->panx = app->pany = 0;
      app->showPage (0, 0, 1, 0);
      break;
    //}}}
    //{{{
    case 'h':
      app->panx += fz_pixmap_width (app->ctx, app->image) / 10;
      app->showPage (0, 0, 1, 0);
      break;
    //}}}
    //{{{
    case 'j':
      {
        int h = fz_pixmap_height(app->ctx, app->image);
        if (h <= app->winh || app->pany <= app->winh - h) {
          panto = PAN_TO_TOP;
          app->pageno++;
        }
        else {
          app->pany -= h / 10;
          app->showPage (0, 0, 1, 0);
        }
        break;
      }
    //}}}
    //{{{
    case 'k':
      {
        int h = fz_pixmap_height(app->ctx, app->image);
        if (h <= app->winh || app->pany == 0) {
          panto = PAN_TO_BOTTOM;
          app->pageno--;
        }
        else {
          app->pany += h / 10;
          app->showPage ( 0, 0, 1, 0);
        }
        break;
      }
    //}}}
    //{{{
    case 'l':
      app->panx -= fz_pixmap_width(app->ctx, app->image) / 10;
      app->showPage (0, 0, 1, 0);
      break;
    //}}}

    case 'g':
    case '\n':
    //{{{
    case '\r':
      if (app->numberlen > 0)
        appgotopage(app, atoi(app->number));
      else
        appgotopage(app, 1);
      break;
    //}}}
    //{{{
    case 'G':
      appgotopage(app, app->pagecount);
      break;
    //}}}
    //{{{
    case ',':
      panto = PAN_TO_BOTTOM;
      if (app->numberlen > 0)
        app->pageno -= atoi(app->number);
      else
        app->pageno--;
      break;
    //}}}
    //{{{
    case '.':
      panto = PAN_TO_TOP;
      if (app->numberlen > 0)
        app->pageno += atoi(app->number);
      else
        app->pageno++;
      break;
    //}}}

    case '\b':
    //{{{
    case 'b':
      panto = DONT_PAN;
      if (app->numberlen > 0)
        app->pageno -= atoi(app->number);
      else
        app->pageno--;
      break;
    //}}}
    //{{{
    case ' ':
      panto = DONT_PAN;
      if (modifiers & 1)
      {
        if (app->numberlen > 0)
          app->pageno -= atoi(app->number);
        else
          app->pageno--;
      }
      else
      { if (app->numberlen > 0)
          app->pageno += atoi(app->number);
        else
          app->pageno++;
      }
      break;
    //}}}
    //{{{
    case '<':
      panto = PAN_TO_TOP;
      app->pageno -= 10;
      break;
    //}}}
    //{{{
    case '>':
      panto = PAN_TO_TOP;
      app->pageno += 10;
      break;
    //}}}
    //{{{
    case 'r':
      panto = DONT_PAN;
      oldpage = -1;
      app->reloadFile();
      break;
    //}}}
    //{{{
    case '?':
      app->issearching = 1;
      app->searchdir = -1;
      app->search[0] = 0;
      app->hit_count = 0;
      app->searchpage = -1;
      InvalidateRect (hwndview, NULL, 0);
      break;
    //}}}
    //{{{
    case '/':
      app->issearching = 1;
      app->searchdir = 1;
      app->search[0] = 0;
      app->hit_count = 0;
      app->searchpage = -1;
      InvalidateRect (hwndview, NULL, 0);
      break;
    //}}}
    //{{{
    case 'n':
      if (app->searchdir > 0)
        appsearch (app, &panto, 1);
      else
        appsearch (app, &panto, -1);
      loadpage = 0;
      break;
    //}}}
    //{{{
    case 'N':
      if (app->searchdir > 0)
        appsearch (app, &panto, -1);
      else
        appsearch (app, &panto, 1);
      loadpage = 0;
      break;
    //}}}
    }

  if (c < '0' || c > '9')
    app->numberlen = 0;

  if (app->pageno < 1)
    app->pageno = 1;
  if (app->pageno > app->pagecount)
    app->pageno = app->pagecount;

  if (app->pageno != oldpage) {
    switch (panto) {
      //{{{
      case PAN_TO_TOP:
        app->pany = 0;
        break;
      //}}}
      //{{{
      case PAN_TO_BOTTOM:
        app->pany = -2000;
        break;
      //}}}
      //{{{
      case DONT_PAN:
        break;
      //}}}
      }
    app->showPage (loadpage, 1, 1, 0);
    }
  }
//}}}
//{{{
void handleKey (int c) {

  int modifier = (GetAsyncKeyState(VK_SHIFT) < 0);
  modifier |= ((GetAsyncKeyState(VK_CONTROL) < 0) << 2);

  if (GetCapture() == hwndview)
    return;

  if (justcopied) {
    justcopied = 0;
    InvalidateRect (hwndview, NULL, 0);
    }

  /* translate VK into ASCII equivalents */
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

  onKey (gApp, c, modifier);
  InvalidateRect (hwndview, NULL, 0);
  }
//}}}

//{{{
void handleScroll (cApp* app, int modifiers, int dir) {

  app->ispanning = app->iscopying = 0;
  if (modifiers & (1<<2)) {
    /* zoom in/out if ctrl is pressed */
    if (dir < 0)
      app->resolution = zoomIn (app->resolution);
    else
      app->resolution = zoomOut (app->resolution);
    if (app->resolution > MAXRES)
      app->resolution = MAXRES;
    if (app->resolution < MINRES)
      app->resolution = MINRES;
    app->showPage (0, 1, 1, 0);
  }
  else {
    /* scroll up/down, or left/right if
    shift is pressed */
    int w = fz_pixmap_width(app->ctx, app->image);
    int h = fz_pixmap_height(app->ctx, app->image);
    int xstep = 0;
    int ystep = 0;
    int pagestep = 0;
    if (modifiers & (1<<0)) {
      if (dir > 0 && app->panx >= 0)
        pagestep = -1;
      else if (dir < 0 && app->panx <= app->winw - w)
        pagestep = 1;
      else
        xstep = 20 * dir;
    }
    else {
      if (dir > 0 && app->pany >= 0)
        pagestep = -1;
      else if (dir < 0 && app->pany <= app->winh - h)
        pagestep = 1;
      else
        ystep = 20 * dir;
    }
    if (pagestep == 0)
      app->panView (app->panx + xstep, app->pany + ystep);
    else if (pagestep > 0 && app->pageno < app->pagecount) {
      app->pageno++;
      app->pany = 0;
      app->showPage (1, 1, 1, 0);
    }
    else if (pagestep < 0 && app->pageno > 1) {
      app->pageno--;
      app->pany = INT_MIN;
      app->showPage (1, 1, 1, 0);
    }
  }
}
//}}}
//{{{
void onMouse (cApp* app, int x, int y, int btn, int modifiers, int state) {

  fz_context* ctx = app->ctx;

  fz_point p;
  fz_irect irect = { 0, 0, (int)app->layout_w, (int)app->layout_h };
  if (app->image)
    irect = fz_pixmap_bbox (app->ctx, app->image);
  p.x = x - app->panx + irect.x0;
  p.y = y - app->pany + irect.y0;

  fz_matrix ctm;
  app->viewCtm (&ctm);
  ctm = fz_invert_matrix (ctm);

  p = fz_transform_point (p, ctm);

  fz_link* link;
  int processed = 0;
  if (btn == 1 && (state == 1 || state == -1)) {
    pdf_ui_event event;
    pdf_document* idoc = pdf_specifics (app->ctx, app->doc);

    event.etype = PDF_EVENT_TYPE_POINTER;
    event.event.pointer.pt = p;
    if (state == 1)
      event.event.pointer.ptype = PDF_POINTER_DOWN;
    else /* state == -1 */
      event.event.pointer.ptype = PDF_POINTER_UP;

    if (idoc && pdf_pass_event (ctx, idoc, (pdf_page*)app->page, &event)) {
      pdf_widget* widget = pdf_focused_widget (ctx, idoc);
      app->nowaitcursor = 1;
      app->updatePage();
      if (widget) {
        switch (pdf_widget_type (ctx, widget)) {
          //{{{
          case PDF_WIDGET_TYPE_TEXT:
            {
            char* text = pdf_text_widget_text (ctx, idoc, widget);
            char* current_text = text;
            int retry = 0;

            do {
              current_text = winTextInput (current_text, retry);
              retry = 1;
              } while (current_text && !pdf_text_widget_set_text (ctx, idoc, widget, current_text));

            fz_free(app->ctx, text);
            app->updatePage();
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

            fz_try(ctx) {
              nopts = pdf_choice_widget_options(ctx, idoc, widget, 0, NULL);
              opts = (const char**)fz_malloc (ctx, nopts * sizeof(*opts));
              (void)pdf_choice_widget_options (ctx, idoc, widget, 0, opts);

              nvals = pdf_choice_widget_value (ctx, idoc, widget, NULL);
              vals = (const char**)fz_malloc (ctx, MAX(nvals,nopts) * sizeof(*vals));
              (void)pdf_choice_widget_value (ctx, idoc, widget, vals);

              if (winChoiceInput (nopts, opts, &nvals, vals)) {
                pdf_choice_widget_set_value (ctx, idoc, widget, nvals, vals);
                app->updatePage();
                }
              }
            fz_always(ctx) {
              fz_free (ctx, opts);
              fz_free (ctx, vals);
              }
            fz_catch(ctx) {
              winWarn ("setting of choice failed");
              }
            }
            break;
          //}}}
          //{{{
          case PDF_WIDGET_TYPE_SIGNATURE:
            if (state == -1) {
              char ebuf[256];

              if (pdf_dict_get (ctx, ((pdf_annot *)widget)->obj, PDF_NAME(V))) {
                /* Signature is signed. Check the signature */
                ebuf[0] = 0;
                if (pdf_check_signature (ctx, idoc, widget, ebuf, sizeof(ebuf)))
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
      app->nowaitcursor = 0;
      processed = 1;
      }
    }

  for (link = app->page_links; link; link = link->next)
    if (p.x >= link->rect.x0 && p.x <= link->rect.x1)
      if (p.y >= link->rect.y0 && p.y <= link->rect.y1)
        break;

  if (link) {
    //{{{  link
    winCursor (HAND);
    if (btn == 1 && state == 1 && !processed) {
      if (fz_is_external_link (ctx, link->uri)) {
        //appgotouri (app, link->uri);
        }
      else
        appgotopage (app, fz_resolve_link(ctx, app->doc, link->uri, NULL, NULL) + 1);
      return;
      }
    }
    //}}}
  else {
    fz_annot *annot;
    for (annot = fz_first_annot(app->ctx, app->page); annot; annot = fz_next_annot(app->ctx, annot)) {
      fz_rect rect = fz_bound_annot(app->ctx, annot);
      if (x >= rect.x0 && x < rect.x1)
        if (y >= rect.y0 && y < rect.y1)
          break;
      }
    if (annot)
      winCursor (CARET);
    else
      winCursor (ARROW);
    }

  if (state == 1 && !processed) {
    if (btn == 1 && !app->iscopying) {
      app->ispanning = 1;
      app->selx = x;
      app->sely = y;
      app->beyondy = 0;
      }
    if (btn == 3 && !app->ispanning) {
      app->iscopying = 1;
      app->selx = x;
      app->sely = y;
      app->selr.x0 = x;
      app->selr.x1 = x;
      app->selr.y0 = y;
      app->selr.y1 = y;
      }
    if (btn == 4 || btn == 5) /* scroll wheel */
      handleScroll(app, modifiers, btn == 4 ? 1 : -1);
    if (btn == 6 || btn == 7) /* scroll wheel (horizontal) */
      /* scroll left/right or up/down if shift is pressed */
      handleScroll (app, modifiers ^ (1<<0), btn == 6 ? 1 : -1);
    }

  else if (state == -1) {
    if (app->iscopying) {
      //{{{  copying
      app->iscopying = 0;
      app->selr.x0 = fz_mini (app->selx, x) - app->panx + irect.x0;
      app->selr.x1 = fz_maxi (app->selx, x) - app->panx + irect.x0;
      app->selr.y0 = fz_mini (app->sely, y) - app->pany + irect.y0;
      app->selr.y1 = fz_maxi (app->sely, y) - app->pany + irect.y0;
      InvalidateRect (hwndview, NULL, 0);
      if (app->selr.x0 < app->selr.x1 && app->selr.y0 < app->selr.y1)
        winDoCopy (app);
      }
      //}}}
    app->ispanning = 0;
    }
  else if (app->ispanning) {
    //{{{  panning
    int newx = app->panx + x - app->selx;
    int newy = app->pany + y - app->sely;
    int imgh = app->winh;
    if (app->image)
      imgh = fz_pixmap_height (app->ctx, app->image);

    /* Scrolling beyond limits implies flipping pages */
    /* Are we requested to scroll beyond limits? */
    if (newy + imgh < app->winh || newy > 0) {
      /* Yes. We can assume that deltay != 0 */
      int deltay = y - app->sely;
      /* Check whether the panning has occurred in the direction that we are already crossing the
       * limit it. If not, we can conclude that we have switched ends of the page and will thus start over counting */
      if (app->beyondy == 0 || (app->beyondy ^ deltay) >= 0) {
        /* Updating how far we are beyond and flipping pages if beyond threshold */
        app->beyondy += deltay;
        if (app->beyondy > BEYOND_THRESHHOLD) {
          if( app->pageno > 1) {
            app->pageno--;
            app->showPage (1, 1, 1, 0);
            if (app->image)
              newy = -fz_pixmap_height (app->ctx, app->image);
            }
          app->beyondy = 0;
          }
        else if (app->beyondy < -BEYOND_THRESHHOLD) {
          if( app->pageno < app->pagecount) {
            app->pageno++;
            app->showPage (1, 1, 1, 0);
            newy = 0;
            }
          app->beyondy = 0;
          }
        }
      else
        app->beyondy = 0;
      }
    /* Although at this point we've already determined that or that no scrolling will be performed in
     * y-direction, the x-direction has not yet been taken care off. Therefore */
    app->panView (newx, newy);

    app->selx = x;
    app->sely = y;
    }
    //}}}
  else if (app->iscopying) {
    //{{{  copying
    app->selr.x0 = fz_mini (app->selx, x) - app->panx + irect.x0;
    app->selr.x1 = fz_maxi (app->selx, x) - app->panx + irect.x0;
    app->selr.y0 = fz_mini (app->sely, y) - app->pany + irect.y0;
    app->selr.y1 = fz_maxi (app->sely, y) - app->pany + irect.y0;
    InvalidateRect (hwndview, NULL, 0);
    }
    //}}}
  }
//}}}
//{{{
void handleMouse (int x, int y, int btn, int state) {

  int modifier = GetAsyncKeyState(VK_SHIFT) < 0;
  modifier |= (GetAsyncKeyState (VK_CONTROL) < 0) << 2;

  if (state != 0 && justcopied) {
    justcopied = 0;
    InvalidateRect (hwndview, NULL, 0);
    }

  if (state == 1)
    SetCapture (hwndview);
  if (state == -1)
    ReleaseCapture();

  onMouse (gApp, x, y, btn, modifier, state);
  }
//}}}

//{{{
LRESULT CALLBACK frameproc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {

  switch(message) {
    case WM_SETFOCUS:
      PostMessage (hwnd, WM_APP+5, 0, 0);
      return 0;

    case WM_APP+5:
      SetFocus (hwndview);
      return 0;

    case WM_DESTROY:
      PostQuitMessage (0);
      return 0;

    case WM_SYSCOMMAND:
      if (wParam == ID_ABOUT) {
        winHelp (gApp);
        return 0;
        }
      if (wParam == ID_DOCINFO) {
        winInfo();
        return 0;
        }
      break;

    case WM_SIZE: {
      // More generally, you should use GetEffectiveClientRect
      // if you have a toolbar etc.
      RECT rect;
      GetClientRect (hwnd, &rect);
      MoveWindow (hwndview, rect.left, rect.top,
      rect.right-rect.left, rect.bottom-rect.top, TRUE);
      if (wParam == SIZE_MAXIMIZED)
        gApp->shrinkwrap = 0;
      return 0;
      }

    case WM_SIZING:
      gApp->shrinkwrap = 0;
      break;

    case WM_NOTIFY:
    case WM_COMMAND:
      return SendMessage (hwndview, message, wParam, lParam);

    case WM_CLOSE:
      return 0;
    }

  return DefWindowProc (hwnd, message, wParam, lParam);
  }
//}}}
//{{{
LRESULT CALLBACK viewproc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {

  static int oldx = 0;
  static int oldy = 0;

  int x = (signed short) LOWORD(lParam);
  int y = (signed short) HIWORD(lParam);

  switch (message) {
    //{{{
    case WM_SIZE:
      if (wParam == SIZE_MINIMIZED)
        return 0;
      if (wParam == SIZE_MAXIMIZED)
        gApp->shrinkwrap = 0;

      apponresize (gApp, LOWORD(lParam), HIWORD(lParam));
      break;
    //}}}
    //{{{
    /* Paint events are low priority and automagically catenated
     * so we don't need to do any fancy waiting to defer repainting.
     */
    case WM_PAINT:
      {
      //puts("WM_PAINT");
      PAINTSTRUCT ps;
      hdc = BeginPaint (hwnd, &ps);
      gApp->blit();
      hdc = NULL;
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
      SetFocus (hwndview);
      oldx = x;
      oldy = y;
      handleMouse (x, y, 1, 1);
      return 0;
    //}}}
    //{{{
    case WM_MBUTTONDOWN:
      SetFocus (hwndview);
      oldx = x;
      oldy = y;
      handleMouse (x, y, 2, 1);
      return 0;
    //}}}
    //{{{
    case WM_RBUTTONDOWN:
      SetFocus (hwndview);
      oldx = x;
      oldy = y;
      handleMouse (x, y, 3, 1);
      return 0;
    //}}}

    //{{{
    case WM_LBUTTONUP:
      oldx = x;
      oldy = y;
      handleMouse (x, y, 1, -1);
      return 0;
    //}}}
    //{{{
    case WM_MBUTTONUP:
      oldx = x;
      oldy = y;
      handleMouse (x, y, 2, -1);
      return 0;
    //}}}
    //{{{
    case WM_RBUTTONUP:
      oldx = x;
      oldy = y;
      handleMouse(x, y, 3, -1);
      return 0;
    //}}}

    //{{{
    case WM_MOUSEMOVE:
      oldx = x;
      oldy = y;
      handleMouse(x, y, 0, 0);
      return 0;
    //}}}
    //{{{
    case WM_MOUSEWHEEL:
      if ((signed short)HIWORD(wParam) <= 0) {
        handleMouse (oldx, oldy, 4, 1);
        handleMouse(oldx, oldy, 4, -1);
        }
      else {
        handleMouse(oldx, oldy, 5, 1);
        handleMouse(oldx, oldy, 5, -1);
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
          handleMouse (oldx, oldy, 0, 0);  /* update cursor */
          return 0;
        }
      return 1;
    //}}}
    //{{{
    /* unicode encoded chars, including escape, backspace etc... */
    case WM_CHAR:
      if (wParam < 256) {
        handleKey (wParam);
        handleMouse (oldx, oldy, 0, 0);  /* update cursor */
        }
      return 0;
    //}}}

    //{{{
    /* We use WM_APP to trigger a reload and repaint of a page */
    case WM_APP:
      appreloadpage (gApp);
      break;
    //}}}
    }

  fflush (stdout);

  /* Pass on unhandled events to Windows */
  return DefWindowProc (hwnd, message, wParam, lParam);
  }
//}}}

typedef BOOL (SetProcessDPIAwareFn)(void);
//{{{
int get_system_dpi() {

  HMODULE hUser32 = LoadLibrary (TEXT("user32.dll"));
  SetProcessDPIAwareFn* ptr = (SetProcessDPIAwareFn*)GetProcAddress (hUser32, "SetProcessDPIAware");
  if (ptr != NULL)
    ptr();
  FreeLibrary (hUser32);

  HDC desktopDC = GetDC (NULL);
  int hdpi = GetDeviceCaps (desktopDC, LOGPIXELSX);
  int vdpi = GetDeviceCaps (desktopDC, LOGPIXELSY);

  /* hdpi,vdpi = 100 means 96dpi. */
  return ((hdpi + vdpi) * 96 + 0.5f) / 200;
  }
//}}}

//{{{
int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {

  gApp = new cApp();

  int argc;
  LPWSTR* wargv = CommandLineToArgvW (GetCommandLineW(), &argc);
  char** argv = fz_argv_from_wargv (argc, wargv);

  int displayRes = get_system_dpi();

  int c;
  while ((c = fz_getopt(argc, argv, "Ip:r:A:C:W:H:S:U:Xb:")) != -1) {
    switch (c) {
      case 'C':
        c = strtol (fz_optarg, NULL, 16);
        gApp->tint = 1;
        gApp->tint_r = (c >> 16) & 255;
        gApp->tint_g = (c >> 8) & 255;
        gApp->tint_b = (c) & 255;
        break;
      case 'r': displayRes = fz_atoi (fz_optarg); break;
      case 'I': gApp->invert = 1; break;
      case 'A': fz_set_aa_level (gApp->ctx, fz_atoi(fz_optarg)); break;
      case 'W': gApp->layout_w = fz_atoi (fz_optarg); break;
      case 'H': gApp->layout_h = fz_atoi (fz_optarg); break;
      case 'S': gApp->layout_em = fz_atoi (fz_optarg); break;
      case 'U': gApp->layout_css = fz_optarg; break;
      case 'X': gApp->layout_use_doc_css = 0; break;
      }
    }

  gApp->setResolution (displayRes);

  char argv0[256];
  GetModuleFileNameA (NULL, argv0, sizeof argv0);
  installApp (argv0);

  //{{{  create and register window frame class
  WNDCLASS wc;
  memset(&wc, 0, sizeof(wc));
  wc.style = 0;
  wc.lpfnWndProc = frameproc;
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
  //{{{  create and register window view class
  memset(&wc, 0, sizeof(wc));
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = viewproc;
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
  gApp->scrw = r.right - r.left;
  gApp->scrh = r.bottom - r.top;
  //}}}
  //{{{  create cursors
  arrowcurs = LoadCursor (NULL, IDC_ARROW);
  handcurs = LoadCursor (NULL, IDC_HAND);
  waitcurs = LoadCursor (NULL, IDC_WAIT);
  caretcurs = LoadCursor (NULL, IDC_IBEAM);
  //}}}
  //{{{  background color
  bgbrush = CreateSolidBrush (RGB (0x70,0x70,0x70));
  shbrush = CreateSolidBrush (RGB (0x40,0x40,0x40));
  //}}}
  //{{{  init DIB info for buffer
  dibinf = (BITMAPINFO*)malloc (sizeof(BITMAPINFO) + 12);
  dibinf->bmiHeader.biSize = sizeof(dibinf->bmiHeader);
  dibinf->bmiHeader.biPlanes = 1;
  dibinf->bmiHeader.biBitCount = 32;
  dibinf->bmiHeader.biCompression = BI_RGB;
  dibinf->bmiHeader.biXPelsPerMeter = 2834;
  dibinf->bmiHeader.biYPelsPerMeter = 2834;
  dibinf->bmiHeader.biClrUsed = 0;
  dibinf->bmiHeader.biClrImportant = 0;
  dibinf->bmiHeader.biClrUsed = 0;
  //}}}
  //{{{  Create window
  hwndframe = CreateWindowW (L"FrameWindow", // window class name
                             NULL, // window caption
                             WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                             CW_USEDEFAULT, CW_USEDEFAULT, // initial position
                             300, // initial x size
                             300, // initial y size
                             0, // parent window handle
                             0, // window menu handle
                             0, // program instance handle
                             0); // creation parameters
  if (!hwndframe)
    winError ("cannot create frame");

  hwndview = CreateWindowW (L"ViewWindow", // window class name
                            NULL,
                            WS_VISIBLE | WS_CHILD,
                            CW_USEDEFAULT, CW_USEDEFAULT,
                            CW_USEDEFAULT, CW_USEDEFAULT,
                            hwndframe, 0, 0, 0);
  if (!hwndview)
    winError ("cannot create view");
  //}}}
  hdc = NULL;
  SetWindowTextW (hwndframe, L"MuPDF");
  HMENU menu = GetSystemMenu (hwndframe, 0);
  AppendMenuW (menu, MF_SEPARATOR, 0, NULL);
  AppendMenuW (menu, MF_STRING, ID_ABOUT, L"About MuPDF...");
  AppendMenuW (menu, MF_STRING, ID_DOCINFO, L"Document Properties...");
  SetCursor (arrowcurs);

  if (fz_optind < argc) {
    strcpy (filename, argv[fz_optind++]);
    if (fz_optind < argc)
      gApp->pageno = atoi (argv[fz_optind++]);
    gApp->open (filename, 0);

    MSG msg;
    while (GetMessage (&msg, NULL, 0, 0)) {
      TranslateMessage (&msg);
      DispatchMessage (&msg);
      }
    }

  fz_free_argv (argc, argv);

  gApp->close();
  free (dibinf);
  fz_drop_context (gApp->ctx);

  return 0;
  }
//}}}
