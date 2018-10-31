// muTest.c
//{{{  includes
#include "mupdf/fitz.h" /* for pdf output */
#include "mupdf/pdf.h" /* for pdf output */
#include "mupdf/helpers/mu-threads.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>

struct timeval;
struct timezone;

#ifdef __cplusplus
extern "C" {
#endif
int gettimeofday(struct timeval *tv, struct timezone *tz);
#ifdef __cplusplus
}
#endif
//}}}

#define DEBUG_THREADS(A) do { } while (0)
//{{{
enum {
  OUT_NONE,
  OUT_PNG, OUT_TGA, OUT_PNM, OUT_PGM, OUT_PPM, OUT_PAM,
  OUT_PBM, OUT_PKM, OUT_PWG, OUT_PCL, OUT_PS, OUT_PSD,
  OUT_TEXT, OUT_HTML, OUT_XHTML, OUT_STEXT, OUT_PCLM,
  OUT_TRACE, OUT_SVG, OUT_PDF, OUT_GPROOF
  };
//}}}
enum { CS_INVALID, CS_UNSET, CS_MONO, CS_GRAY, CS_GRAY_ALPHA, CS_RGB, CS_RGB_ALPHA, CS_CMYK, CS_CMYK_ALPHA, CS_ICC };
enum { SPOTS_NONE, SPOTS_OVERPRINT_SIM, SPOTS_FULL };

//{{{  struct cs_name
typedef struct {
  const char *name;
  int colorspace;
  } cs_name_t;
//}}}
//{{{
static const cs_name_t cs_name_table[] = {
  { "m", CS_MONO },
  { "mono", CS_MONO },
  { "g", CS_GRAY },
  { "gray", CS_GRAY },
  { "grey", CS_GRAY },
  { "ga", CS_GRAY_ALPHA },
  { "grayalpha", CS_GRAY_ALPHA },
  { "greyalpha", CS_GRAY_ALPHA },
  { "rgb", CS_RGB },
  { "rgba", CS_RGB_ALPHA },
  { "rgbalpha", CS_RGB_ALPHA },
  { "cmyk", CS_CMYK },
  { "cmyka", CS_CMYK_ALPHA },
  { "cmykalpha", CS_CMYK_ALPHA },
  };
//}}}
//{{{  struct format_cs_table_t
typedef struct {
  int format;
  int default_cs;
  int permitted_cs[7];
  } format_cs_table_t;
//}}}
//{{{
static const format_cs_table_t format_cs_table[] = {
  { OUT_PNG, CS_RGB, { CS_GRAY, CS_GRAY_ALPHA, CS_RGB, CS_RGB_ALPHA, CS_ICC } },
  { OUT_PPM, CS_RGB, { CS_GRAY, CS_RGB } },
  { OUT_PNM, CS_GRAY, { CS_GRAY, CS_RGB } },
  { OUT_PAM, CS_RGB_ALPHA, { CS_GRAY, CS_GRAY_ALPHA, CS_RGB, CS_RGB_ALPHA, CS_CMYK, CS_CMYK_ALPHA } },
  { OUT_PGM, CS_GRAY, { CS_GRAY, CS_RGB } },
  { OUT_PBM, CS_MONO, { CS_MONO } },
  { OUT_PKM, CS_CMYK, { CS_CMYK } },
  { OUT_PWG, CS_RGB, { CS_MONO, CS_GRAY, CS_RGB, CS_CMYK } },
  { OUT_PCL, CS_MONO, { CS_MONO, CS_RGB } },
  { OUT_PCLM, CS_RGB, { CS_RGB, CS_GRAY } },
  { OUT_PS, CS_RGB, { CS_GRAY, CS_RGB, CS_CMYK } },
  { OUT_PSD, CS_CMYK, { CS_GRAY, CS_GRAY_ALPHA, CS_RGB, CS_RGB_ALPHA, CS_CMYK, CS_CMYK_ALPHA, CS_ICC } },
  { OUT_TGA, CS_RGB, { CS_GRAY, CS_GRAY_ALPHA, CS_RGB, CS_RGB_ALPHA } },

  { OUT_TRACE, CS_RGB, { CS_RGB } },
  { OUT_SVG, CS_RGB, { CS_RGB } },
  { OUT_PDF, CS_RGB, { CS_RGB } },
  { OUT_GPROOF, CS_RGB, { CS_RGB } },

  { OUT_TEXT, CS_RGB, { CS_RGB } },
  { OUT_HTML, CS_RGB, { CS_RGB } },
  { OUT_XHTML, CS_RGB, { CS_RGB } },
  { OUT_STEXT, CS_RGB, { CS_RGB } },
  };
//}}}

//{{{  static vars
const char* output = NULL;
fz_output* out = NULL;

int output_pagenum = 0;
int output_file_per_page = 0;

char* format = NULL;
int output_format = OUT_NONE;

float rotation = 0;
float resolution = 72;
int res_specified = 0;
int width = 0;
int height = 0;
int fit = 0;

float layout_w = 450;
float layout_h = 600;
float layout_em = 12;
char* layout_css = NULL;
int layout_use_doc_css = 1;
float min_line_width = 0.0f;

pdf_document* pdfout = NULL;

int ignore_errors = 0;

int out_cs = CS_UNSET;
const char *proof_filename = NULL;
fz_colorspace* proof_cs = NULL;
const char* icc_filename = NULL;
float gamma_value = 1;
int invert = 0;
int band_height = 0;

int errored = 0;
fz_colorspace *colorspace;
fz_colorspace* oi = NULL;
int spots = SPOTS_NONE;
int alpha;
char* filename;
int files = 0;
fz_band_writer* bander = NULL;

fz_cmm_engine* icc_engine = &fz_cmm_engine_lcms;

static const char* layer_config = NULL;
//}}}

//{{{  struct timing
static struct {
  int count, total;
  int min, max;
  int mininterp, maxinterp;
  int minpage, maxpage;

  const char* minfilename;
  const char* maxfilename;
  } timing;
//}}}

//{{{
int gettime() {

  static struct timeval first;
  static int once = 1;

  if (once) {
    gettimeofday (&first, NULL);
    once = 0;
    }

  struct timeval now;
  gettimeofday (&now, NULL);

  return (now.tv_sec - first.tv_sec) * 1000 + (now.tv_usec - first.tv_usec) / 1000;
  }
//}}}
//{{{
int has_percent_d (const char* s) {

  /* find '%[0-9]*d' */
  while (*s) {
    if (*s++ == '%') {
      while (*s >= '0' && *s <= '9')
        ++s;
      if (*s == 'd')
        return 1;
      }
    }

  return 0;
  }
//}}}
//{{{
/* Output file level (as opposed to page level) headers */
void file_level_headers (fz_context* ctx) {

  if (output_format == OUT_STEXT || output_format == OUT_TRACE)
    fz_write_printf (ctx, out, "<?xml version=\"1.0\"?>\n");

  if (output_format == OUT_HTML)
    fz_print_stext_header_as_html (ctx, out);
  if (output_format == OUT_XHTML)
    fz_print_stext_header_as_xhtml (ctx, out);

  if (output_format == OUT_STEXT || output_format == OUT_TRACE)
    fz_write_printf (ctx, out, "<document name=\"%s\">\n", filename);

  if (output_format == OUT_PS)
    fz_write_ps_file_header (ctx, out);

  if (output_format == OUT_PWG)
    fz_write_pwg_file_header (ctx, out);

  if (output_format == OUT_PCLM) {
    fz_pclm_options opts = { 0 };
    fz_parse_pclm_options (ctx, &opts, "compression=flate");
    bander = fz_new_pclm_band_writer (ctx, out, &opts);
    }
  }
//}}}
//{{{
void file_level_trailers (fz_context* ctx) {

  if (output_format == OUT_STEXT || output_format == OUT_TRACE)
    fz_write_printf(ctx, out, "</document>\n");

  if (output_format == OUT_HTML)
    fz_print_stext_trailer_as_html (ctx, out);
  if (output_format == OUT_XHTML)
    fz_print_stext_trailer_as_xhtml (ctx, out);

  if (output_format == OUT_PS)
    fz_write_ps_file_trailer (ctx, out, output_pagenum);

  if (output_format == OUT_PCLM)
    fz_drop_band_writer (ctx, bander);
  }
//}}}

//{{{
void drawBand (fz_context* ctx, fz_page* page,
               fz_display_list* list, fz_matrix ctm, fz_rect tbounds, fz_cookie* cookie,
               int band_start, fz_pixmap* pix, fz_bitmap** bit) {

  fz_device* dev = NULL;
  fz_var (dev);

  *bit = NULL;

  fz_try (ctx) {
    if (pix->alpha)
      fz_clear_pixmap (ctx, pix);
    else
      fz_clear_pixmap_with_value (ctx, pix, 255);

    dev = fz_new_draw_device_with_proof (ctx, fz_identity, pix, proof_cs);
    if (list)
      fz_run_display_list (ctx, list, dev, ctm, tbounds, cookie);
    else
      fz_run_page (ctx, page, dev, ctm, cookie);
    fz_close_device (ctx, dev);
    fz_drop_device (ctx, dev);
    dev = NULL;

    if (invert)
      fz_invert_pixmap (ctx, pix);
    if (gamma_value != 1)
      fz_gamma_pixmap (ctx, pix, gamma_value);

    if (((output_format == OUT_PCL || output_format == OUT_PWG) && out_cs == CS_MONO) ||
         (output_format == OUT_PBM) || (output_format == OUT_PKM))
      *bit = fz_new_bitmap_from_pixmap_band(ctx, pix, NULL, band_start);
    }

  fz_catch(ctx) {
    fz_drop_device(ctx, dev);
    fz_rethrow(ctx);
    }
  }
//}}}
//{{{
void doDrawPage (fz_context* ctx, fz_page* page, fz_display_list* list, int pagenum,
                 fz_cookie* cookie, int start, int interptime, char* filename, int bg, fz_separations* seps) {


  fz_device* dev = NULL;
  fz_var(dev);

  if (output_file_per_page)
    file_level_headers (ctx);

  fz_rect mediabox;
  fz_try(ctx) {
    if (list)
      mediabox = fz_bound_display_list (ctx, list);
    else
      mediabox = fz_bound_page (ctx, page);
    }
  fz_catch(ctx) {
    fz_drop_separations (ctx, seps);
    fz_drop_page (ctx, page);
    fz_rethrow (ctx);
    }

  if (output_format == OUT_TRACE) {
    fz_try (ctx) {
      fz_write_printf (ctx, out, "<page mediabox=\"%g %g %g %g\">\n",
                       mediabox.x0, mediabox.y0, mediabox.x1, mediabox.y1);
      dev = fz_new_trace_device (ctx, out);
      if (list)
        fz_run_display_list (ctx, list, dev, fz_identity, fz_infinite_rect, cookie);
      else
        fz_run_page (ctx, page, dev, fz_identity, cookie);
      fz_write_printf (ctx, out, "</page>\n");
      fz_close_device (ctx, dev);
      }
    fz_always(ctx) {
      fz_drop_device (ctx, dev);
      }
    fz_catch(ctx) {
      fz_drop_display_list (ctx, list);
      fz_drop_separations (ctx, seps);
      fz_drop_page (ctx, page);
      fz_rethrow (ctx);
      }
    }

  else if (output_format == OUT_TEXT || output_format == OUT_HTML ||
           output_format == OUT_XHTML || output_format == OUT_STEXT) {
    float zoom = resolution / 72;
    fz_matrix ctm = fz_pre_scale (fz_rotate(rotation), zoom, zoom);

    fz_stext_page* text = NULL;
    fz_var (text);

    fz_try (ctx) {
      fz_stext_options stext_options;

      stext_options.flags = (output_format == OUT_HTML || output_format == OUT_XHTML) ? FZ_STEXT_PRESERVE_IMAGES : 0;
      text = fz_new_stext_page (ctx, mediabox);
      dev = fz_new_stext_device (ctx,  text, &stext_options);
      if (list)
        fz_run_display_list (ctx, list, dev, ctm, fz_infinite_rect, cookie);
      else
        fz_run_page (ctx, page, dev, ctm, cookie);
      fz_close_device (ctx, dev);
      fz_drop_device (ctx, dev);
      dev = NULL;

      if (output_format == OUT_STEXT)
        fz_print_stext_page_as_xml(ctx, out, text);
      else if (output_format == OUT_HTML)
        fz_print_stext_page_as_html(ctx, out, text);
      else if (output_format == OUT_XHTML)
        fz_print_stext_page_as_xhtml(ctx, out, text);
      else if (output_format == OUT_TEXT) {
        fz_print_stext_page_as_text(ctx, out, text);
        fz_write_printf(ctx, out, "\f\n");
        }
      }
    fz_always(ctx) {
      fz_drop_device(ctx, dev);
      fz_drop_stext_page(ctx, text);
      }
   fz_catch(ctx) {
      fz_drop_display_list(ctx, list);
      fz_drop_separations(ctx, seps);
      fz_drop_page(ctx, page);
      fz_rethrow(ctx);
      }
    }

  else if (output_format == OUT_PDF) {
    fz_buffer* contents = NULL;
    fz_var (contents);
    pdf_obj* resources = NULL;
    fz_var (resources);

    fz_try (ctx) {
      pdf_obj* page_obj;
      dev = pdf_page_write (ctx, pdfout, mediabox, &resources, &contents);
      if (list)
        fz_run_display_list (ctx, list, dev, fz_identity, fz_infinite_rect, cookie);
      else
        fz_run_page (ctx, page, dev, fz_identity, cookie);
      fz_close_device (ctx, dev);
      fz_drop_device (ctx, dev);
      dev = NULL;

      page_obj = pdf_add_page (ctx, pdfout, mediabox, rotation, resources, contents);
      pdf_insert_page (ctx, pdfout, -1, page_obj);
      pdf_drop_obj (ctx, page_obj);
      }

    fz_always(ctx) {
      pdf_drop_obj (ctx, resources);
      fz_drop_buffer (ctx, contents);
      fz_drop_device (ctx, dev);
      }
    fz_catch(ctx) {
      fz_drop_display_list (ctx, list);
      fz_drop_separations (ctx, seps);
      fz_drop_page (ctx, page);
      fz_rethrow (ctx);
      }
    }

  else if (output_format == OUT_SVG) {
    fz_matrix ctm;
    fz_rect tbounds;
    char buf[512];

    fz_output *out = NULL;
    fz_var (out);

    float zoom = resolution / 72;
    ctm = fz_pre_rotate (fz_scale (zoom, zoom), rotation);
    tbounds = fz_transform_rect (mediabox, ctm);

    fz_try (ctx) {
      if (!output || !strcmp (output, "-"))
        out = fz_stdout (ctx);
      else {
        fz_snprintf (buf, sizeof(buf), output, pagenum);
        out = fz_new_output_with_path (ctx, buf, 0);
        }

      dev = fz_new_svg_device (ctx, out, tbounds.x1-tbounds.x0, tbounds.y1-tbounds.y0, FZ_SVG_TEXT_AS_PATH, 1);
      if (list)
        fz_run_display_list (ctx, list, dev, ctm, tbounds, cookie);
      else
        fz_run_page (ctx, page, dev, ctm, cookie);
      fz_close_device (ctx, dev);
      fz_close_output (ctx, out);
      }
    fz_always(ctx) {
      fz_drop_device (ctx, dev);
      fz_drop_output (ctx, out);
      }
    fz_catch(ctx) {
      fz_drop_display_list (ctx, list);
      fz_drop_separations (ctx, seps);
      fz_drop_page (ctx, page);
      fz_rethrow (ctx);
      }
    }
  else {
    float zoom;
    fz_matrix ctm;
    fz_rect tbounds;
    fz_irect ibounds;
    fz_pixmap *pix = NULL;
    int w, h;
    fz_bitmap *bit = NULL;

    fz_var(pix);
    fz_var(bander);
    fz_var(bit);

    zoom = resolution / 72;
    ctm = fz_pre_scale (fz_rotate (rotation), zoom, zoom);

    if (output_format == OUT_TGA)
      ctm = fz_pre_scale (fz_pre_translate(ctm, 0, -height), 1, -1);

    tbounds = fz_transform_rect (mediabox, ctm);
    ibounds = fz_round_rect (tbounds);

    /* Make local copies of our width/height */
    w = width;
    h = height;

    /* If a resolution is specified, check to see whether w/h are exceeded; if not, unset them. */
    if (res_specified) {
      int t;
      t = ibounds.x1 - ibounds.x0;
      if (w && t <= w)
        w = 0;
      t = ibounds.y1 - ibounds.y0;
      if (h && t <= h)
        h = 0;
      }

    /* Now w or h will be 0 unless they need to be enforced. */
    if (w || h) {
      float scalex = w / (tbounds.x1 - tbounds.x0);
      float scaley = h / (tbounds.y1 - tbounds.y0);
      fz_matrix scale_mat;

      if (fit) {
        if (w == 0)
          scalex = 1.0f;
        if (h == 0)
          scaley = 1.0f;
        }
      else {
        if (w == 0)
          scalex = scaley;
        if (h == 0)
          scaley = scalex;
        }
      if (!fit) {
        if (scalex > scaley)
          scalex = scaley;
        else
          scaley = scalex;
        }
      scale_mat = fz_scale (scalex, scaley);
      ctm = fz_concat (ctm, scale_mat);
      tbounds = fz_transform_rect (mediabox, ctm);
      }
    ibounds = fz_round_rect (tbounds);
    tbounds = fz_rect_from_irect (ibounds);

    fz_try(ctx) {
      fz_irect band_ibounds = ibounds;
      int band, bands = 1;
      int totalheight = ibounds.y1 - ibounds.y0;
      int drawheight = totalheight;

      pix = fz_new_pixmap_with_bbox (ctx, colorspace, band_ibounds, seps, alpha);
      fz_set_pixmap_resolution (ctx, pix, resolution, resolution);

      /* Output any page level headers (for banded formats) */
      if (output) {
        if (output_format == OUT_PGM || output_format == OUT_PPM || output_format == OUT_PNM)
          bander = fz_new_pnm_band_writer (ctx, out);
        else if (output_format == OUT_PAM)
          bander = fz_new_pam_band_writer (ctx, out);
        else if (output_format == OUT_PNG)
          bander = fz_new_png_band_writer (ctx, out);
        else if (output_format == OUT_PBM)
          bander = fz_new_pbm_band_writer (ctx, out);
        else if (output_format == OUT_PKM)
          bander = fz_new_pkm_band_writer (ctx, out);
        else if (output_format == OUT_PS)
          bander = fz_new_ps_band_writer (ctx, out);
        else if (output_format == OUT_PSD)
          bander = fz_new_psd_band_writer (ctx, out);
        else if (output_format == OUT_TGA)
          bander = fz_new_tga_band_writer (ctx, out, fz_colorspace_is_bgr(ctx, colorspace));
        else if (output_format == OUT_PWG) {
          if (out_cs == CS_MONO)
            bander = fz_new_mono_pwg_band_writer (ctx, out, NULL);
          else
            bander = fz_new_pwg_band_writer (ctx, out, NULL);
          }
        else if (output_format == OUT_PCL) {
          if (out_cs == CS_MONO)
            bander = fz_new_mono_pcl_band_writer (ctx, out, NULL);
          else
            bander = fz_new_color_pcl_band_writer (ctx, out, NULL);
          }
        if (bander)
          fz_write_header (ctx, bander, pix->w, totalheight, pix->n, pix->alpha, pix->xres, pix->yres, output_pagenum++, pix->colorspace, pix->seps);
        }

      for (band = 0; band < bands; band++) {
        drawBand (ctx, page, list, ctm, tbounds, cookie, band * band_height, pix, &bit);
        if (output) {
          if (bander)
            fz_write_band (ctx, bander, bit ? bit->stride : pix->stride, drawheight, bit ? bit->samples : pix->samples);
          fz_drop_bitmap (ctx, bit);
          bit = NULL;
          }
        ctm.f -= drawheight;
        }
      }
    fz_always(ctx) {
      if (output_format != OUT_PCLM)
        fz_drop_band_writer (ctx, bander);
      fz_drop_bitmap (ctx, bit);
      bit = NULL;
      fz_drop_pixmap (ctx, pix);
      }

    fz_catch(ctx) {
      fz_drop_display_list (ctx, list);
      fz_drop_separations (ctx, seps);
      fz_drop_page (ctx, page);
      fz_rethrow (ctx);
      }
    }

  fz_drop_display_list (ctx, list);

  if (output_file_per_page)
    file_level_trailers (ctx);

  fz_drop_separations (ctx, seps);
  fz_drop_page (ctx, page);

  int end = gettime();
  int diff = end - start;

  if (bg) {
    if (diff + interptime < timing.min) {
      timing.min = diff + interptime;
      timing.mininterp = interptime;
      timing.minpage = pagenum;
      timing.minfilename = filename;
      }
    if (diff + interptime > timing.max) {
      timing.max = diff + interptime;
      timing.maxinterp = interptime;
      timing.maxpage = pagenum;
      timing.maxfilename = filename;
      }
    timing.count ++;

    fprintf (stderr, " %dms (interpretation) %dms (rendering) %dms (total)", interptime, diff, diff + interptime);
    }
  else {
    if (diff < timing.min) {
      timing.min = diff;
      timing.minpage = pagenum;
      timing.minfilename = filename;
      }
    if (diff > timing.max) {
      timing.max = diff;
      timing.maxpage = pagenum;
      timing.maxfilename = filename;
      }
    timing.total += diff;
    timing.count ++;

    fprintf(stderr, " %dms", diff);
    }

  fprintf (stderr, "\n");

  fz_dump_glyph_cache_stats (ctx);

  fz_flush_warnings (ctx);

  if (cookie->errors)
    errored = 1;
  }
//}}}
//{{{
void drawPage (fz_context* ctx, fz_document* doc, int pagenum) {

  fz_cookie cookie = { 0 };

  fz_display_list* list = NULL;
  fz_var (list);

  fz_device* dev = NULL;
  fz_var (dev);

  fz_separations* seps = NULL;
  fz_var (seps);

  int start = gettime();

  fz_page* page = fz_load_page (ctx, doc, pagenum - 1);

  if (spots != SPOTS_NONE) {
    fz_try (ctx) {
      seps = fz_page_separations (ctx, page);
      if (seps) {
        int i, n = fz_count_separations (ctx, seps);
        if (spots == SPOTS_FULL)
          for (i = 0; i < n; i++)
            fz_set_separation_behavior (ctx, seps, i, FZ_SEPARATION_SPOT);
        else
          for (i = 0; i < n; i++)
            fz_set_separation_behavior (ctx, seps, i, FZ_SEPARATION_COMPOSITE);
        }
      else if (fz_page_uses_overprint (ctx, page))
        /* This page uses overprint, so we need an empty sep object to force the overprint simulation on. */
        seps = fz_new_separations (ctx, 0);
      else if (oi && fz_colorspace_n (ctx, oi) != fz_colorspace_n(ctx, colorspace))
        /* We have an output intent, and it's incompatible  with the colorspace our device needs. Force the
         * overprint simulation on, because this ensures that  we 'simulate' the output intent too. */
        seps = fz_new_separations (ctx, 0);
      }
    fz_catch (ctx) {
      fz_drop_page (ctx, page);
      fz_rethrow (ctx);
      }
    }

  // use display list
  fz_try (ctx) {
    list = fz_new_display_list (ctx, fz_bound_page (ctx, page));
    dev = fz_new_list_device (ctx, list);
    fz_run_page (ctx, page, dev, fz_identity, &cookie);
    fz_close_device (ctx, dev);
    }
  fz_always (ctx) {
    fz_drop_device (ctx, dev);
    dev = NULL;
    }
  fz_catch (ctx) {
    fz_drop_display_list (ctx, list);
    fz_drop_separations (ctx, seps);
    fz_drop_page (ctx, page);
    fz_rethrow (ctx);
    }

  int iscolor;
  dev = fz_new_test_device (ctx, &iscolor, 0.02f, 0, NULL);
  fz_try (ctx) {
    if (list)
      fz_run_display_list (ctx, list, dev, fz_identity, fz_infinite_rect, NULL);
    else
      fz_run_page (ctx, page, dev, fz_identity, &cookie);
    fz_close_device (ctx, dev);
    }
  fz_always(ctx) {
    fz_drop_device (ctx, dev);
    dev = NULL;
    }
  fz_catch(ctx) {
    fz_drop_display_list (ctx, list);
    fz_drop_separations (ctx, seps);
    fz_drop_page (ctx, page);
    fz_rethrow (ctx);
    }
  fprintf (stderr, " %s", iscolor ? "color" : "grayscale");

  if (output_file_per_page) {
    char text_buffer[512];
    if (out) {
      fz_close_output (ctx, out);
      fz_drop_output (ctx, out);
      }
    fz_snprintf (text_buffer, sizeof(text_buffer), output, pagenum);
    out = fz_new_output_with_path (ctx, text_buffer, 0);
    }

  fprintf (stderr, "page %s %d", filename, pagenum);
  doDrawPage (ctx, page, list, pagenum, &cookie, start, 0, filename, 0, seps);
  }
//}}}
//{{{
void drawRange (fz_context* ctx, fz_document* doc, const char* range) {


  int pagecount = fz_count_pages(ctx, doc);

  int page, spage, epage;
  while ((range = fz_parse_page_range (ctx, range, &spage, &epage, pagecount))) {
    if (spage < epage)
      for (page = spage; page <= epage; page++) {
        fz_try (ctx)
          drawPage (ctx, doc, page);
        fz_catch(ctx) {
          if (ignore_errors)
            fz_warn (ctx, "ignoring error on page %d in '%s'", page, filename);
          else
            fz_rethrow (ctx);
          }
        }
    else
      for (page = spage; page >= epage; page--) {
        fz_try (ctx)
          drawPage (ctx, doc, page);
        fz_catch(ctx) {
          if (ignore_errors)
            fz_warn (ctx, "ignoring error on page %d in '%s'", page, filename);
          else
            fz_rethrow (ctx);
          }
        }
    }
  }
//}}}

//{{{
int parse_colorspace (const char* name) {

  int i;
  for (i = 0; i < nelem(cs_name_table); i++)
    if (!strcmp(name, cs_name_table[i].name))
      return cs_name_table[i].colorspace;

  /* Assume ICC. We will error out later if not the case. */
  icc_filename = name;
  return CS_ICC;
  }
//}}}

//{{{  struct trace_header
typedef struct {
  size_t size;
  size_t align;
  } trace_header;
//}}}
//{{{  struct trace_info
typedef struct {
  size_t current;
  size_t peak;
  size_t total;
  } trace_info;
//}}}

//{{{
void* trace_malloc (void* arg, size_t size) {

  trace_info* info = (trace_info *) arg;
  trace_header* p;

  if (size == 0)
    return NULL;

  p = (trace_header*)malloc (size + sizeof(trace_header));
  if (p == NULL)
    return NULL;

  p[0].size = size;
  info->current += size;
  info->total += size;
  if (info->current > info->peak)
    info->peak = info->current;

  return (void *)&p[1];
  }
//}}}
//{{{
void trace_free (void* arg, void* p_) {

  trace_info* info = (trace_info*) arg;
  trace_header* p = (trace_header*)p_;

  if (p == NULL)
    return;
  info->current -= p[-1].size;
  free (&p[-1]);
  }
//}}}
//{{{
void* trace_realloc (void* arg, void* p_, size_t size) {

  trace_info* info = (trace_info*) arg;
  trace_header* p = (trace_header*)p_;
  size_t oldsize;

  if (size == 0) {
    trace_free (arg, p_);
    return NULL;
    }

  if (p == NULL)
    return trace_malloc (arg, size);

  oldsize = p[-1].size;
  p = (trace_header*)realloc(&p[-1], size + sizeof(trace_header));
  if (p == NULL)
    return NULL;

  info->current += size - oldsize;
  if (size > oldsize)
    info->total += size - oldsize;

  if (info->current > info->peak)
    info->peak = info->current;

  p[0].size = size;

  return &p[1];
  }
//}}}

//{{{
inline int isWhite (int ch) {
  return ch == '\011' || ch == '\012' || ch == '\014' || ch == '\015' || ch == '\040';
  }
//}}}
//{{{
void apply_layer_config (fz_context* ctx, fz_document* doc, const char* lc) {


  int config = -1;
  int n, j;
  pdf_layer_config info;

  pdf_document* pdoc = pdf_specifics(ctx, doc);
  if (!pdoc) {
    fz_warn(ctx, "Only PDF files have layers");
    return;
    }

  while (isWhite(*lc))
    lc++;

  if (*lc == 0 || *lc == 'l') {
    int num_configs = pdf_count_layer_configs(ctx, pdoc);

    fprintf(stderr, "Layer configs:\n");
    for (config = 0; config < num_configs; config++) {
      fprintf (stderr, " %s%d:", config < 10 ? " " : "", config);
      pdf_layer_config_info (ctx, pdoc, config, &info);
      if (info.name)
        fprintf (stderr, " Name=\"%s\"", info.name);
      if (info.creator)
        fprintf (stderr, " Creator=\"%s\"", info.creator);
      fprintf (stderr, "\n");
      }
    return;
    }

  while (*lc) {
    int i;
    if (*lc < '0' || *lc > '9') {
      fprintf (stderr, "cannot find number expected for -y\n");
      return;
      }
    i = fz_atoi (lc);
    pdf_select_layer_config (ctx, pdoc, i);

    if (config < 0)
      config = i;

    while (*lc >= '0' && *lc <= '9')
      lc++;
    while (isWhite (*lc))
      lc++;
    if (*lc == ',') {
      lc++;
      while (isWhite (*lc))
        lc++;
      }
    else if (*lc) {
      fprintf (stderr, "cannot find comma expected for -y\n");
      return;
      }
    }

  /* Now list the final state of the config */
  fprintf (stderr, "Layer Config %d:\n", config);
  pdf_layer_config_info (ctx, pdoc, config, &info);
  if (info.name)
    fprintf(stderr, " Name=\"%s\"", info.name);
  if (info.creator)
    fprintf(stderr, " Creator=\"%s\"", info.creator);
  fprintf(stderr, "\n");
  n = pdf_count_layer_config_ui (ctx, pdoc);
  for (j = 0; j < n; j++) {
    pdf_layer_config_ui ui;

    pdf_layer_config_ui_info(ctx, pdoc, j, &ui);
    fprintf(stderr, "%s%d: ", j < 10 ? " " : "", j);
    while (ui.depth > 0) {
      ui.depth--;
      fprintf(stderr, "  ");
      }
    if (ui.type == PDF_LAYER_UI_CHECKBOX)
      fprintf (stderr, " [%c] ", ui.selected ? 'x' : ' ');
    else if (ui.type == PDF_LAYER_UI_RADIOBOX)
      fprintf (stderr, " (%c) ", ui.selected ? 'x' : ' ');
    if (ui.text)
      fprintf (stderr, "%s", ui.text);
    if (ui.type != PDF_LAYER_UI_LABEL && ui.locked)
      fprintf (stderr, " <locked>");
    fprintf (stderr, "\n");
    }
  }
//}}}

//{{{
int main (int argc, char** argv) {

  int c;
  while ((c = fz_getopt (argc, argv, "p:o:F:R:r:w:h:fB:c:e:G:Is:A:DiW:H:S:T:U:XLvPl:y:NO:")) != -1) {
    switch (c) {
      //{{{  switch cases
      case 'R': rotation = fz_atof(fz_optarg); break;
      case 'r': resolution = fz_atof(fz_optarg); res_specified = 1; break;
      case 'w': width = fz_atof(fz_optarg); break;
      case 'h': height = fz_atof(fz_optarg); break;
      case 'f': fit = 1; break;

      case 'c': out_cs = parse_colorspace(fz_optarg); break;
      case 'e': proof_filename = fz_optarg; break;
      case 'G': gamma_value = fz_atof(fz_optarg); break;
      case 'I': invert++; break;

      case 'W': layout_w = fz_atof(fz_optarg); break;
      case 'H': layout_h = fz_atof(fz_optarg); break;
      case 'S': layout_em = fz_atof(fz_optarg); break;
      case 'U': layout_css = fz_optarg; break;
      case 'X': layout_use_doc_css = 0; break;

      case 'O': spots = fz_atof(fz_optarg);
        fprintf(stderr, "Spot rendering/Overprint/Overprint simulation not enabled in this build\n");
        spots = SPOTS_NONE;
        break;

      case 'l': min_line_width = fz_atof(fz_optarg); break;
      case 'i': ignore_errors = 1; break;
      case 'N': icc_engine = NULL; break;

      case 'y': layer_config = fz_optarg; break;
      }
      //}}}
    }

  trace_info info = { 0, 0, 0 };
  fz_alloc_context alloc_ctx = { &info, trace_malloc, trace_realloc, trace_free };
  fz_context* ctx = fz_new_context (&alloc_ctx, NULL, FZ_STORE_DEFAULT);
  if (!ctx) {
    fprintf (stderr, "cannot initialise context\n");
    exit (1);
    }

  if (proof_filename)
    proof_cs = fz_new_icc_colorspace_from_file (ctx, FZ_COLORSPACE_NONE, proof_filename);

  fz_set_text_aa_level (ctx, 8);
  fz_set_graphics_aa_level (ctx, 8);
  fz_set_graphics_min_line_width (ctx, min_line_width);
  fz_set_cmm_engine (ctx, icc_engine);

  if (layout_css) {
    fz_buffer* buf = fz_read_file (ctx, layout_css);
    fz_set_user_css (ctx, fz_string_from_buffer(ctx, buf));
    fz_drop_buffer (ctx, buf);
    }

  fz_set_use_document_css (ctx, layout_use_doc_css);

  output_format = OUT_PNG;
  output = "out.png";

  {
    int i, j;
    for (i = 0; i < nelem(format_cs_table); i++) {
      if (format_cs_table[i].format == output_format) {
        if (out_cs == CS_UNSET)
          out_cs = format_cs_table[i].default_cs;
        for (j = 0; j < nelem (format_cs_table[i].permitted_cs); j++) {
          if (format_cs_table[i].permitted_cs[j] == out_cs)
            break;
          }
        if (j == nelem (format_cs_table[i].permitted_cs)) {
          fprintf(stderr, "Unsupported colorspace for this format\n");
          exit(1);
          }
        }
      }
    }

  alpha = 1;
  switch (out_cs) {
    //{{{  case cs
    case CS_MONO:
    case CS_GRAY:
    case CS_GRAY_ALPHA:
      colorspace = fz_device_gray(ctx);
      alpha = (out_cs == CS_GRAY_ALPHA);
      break;
    case CS_RGB:
    case CS_RGB_ALPHA:
      colorspace = fz_device_rgb(ctx);
      alpha = (out_cs == CS_RGB_ALPHA);
      break;
    case CS_CMYK:
    case CS_CMYK_ALPHA:
      colorspace = fz_device_cmyk(ctx);
      alpha = (out_cs == CS_CMYK_ALPHA);
      break;
    case CS_ICC:
      fz_try(ctx)
        colorspace = fz_new_icc_colorspace_from_file(ctx, FZ_COLORSPACE_NONE, icc_filename);
      fz_catch(ctx)
      {
        fprintf(stderr, "Invalid ICC destination color space\n");
        exit(1);
      }
      if (colorspace == NULL) {
        fprintf(stderr, "Invalid ICC destination color space\n");
        exit(1);
      }
      alpha = 0;
      break;
    default:
      fprintf(stderr, "Unknown colorspace!\n");
      exit(1);
      break;
    }
    //}}}

  if (out_cs != CS_ICC)
    colorspace = fz_keep_colorspace (ctx, colorspace);
  else {
    int i, j, okay;

    /* Check to make sure this icc profile is ok with the output format */
    okay = 0;
    for (i = 0; i < nelem (format_cs_table); i++) {
      if (format_cs_table[i].format == output_format) {
        for (j = 0; j < nelem (format_cs_table[i].permitted_cs); j++) {
          switch (format_cs_table[i].permitted_cs[j]) {
            case CS_MONO:
            case CS_GRAY:
            case CS_GRAY_ALPHA:
              if (fz_colorspace_is_gray (ctx, colorspace))
                okay = 1;
              break;
            case CS_RGB:
            case CS_RGB_ALPHA:
              if (fz_colorspace_is_rgb (ctx, colorspace))
                okay = 1;
              break;
            case CS_CMYK:
            case CS_CMYK_ALPHA:
              if (fz_colorspace_is_cmyk (ctx, colorspace))
                okay = 1;
              break;
            }
          }
        }
      }

    if (!okay) {
      fprintf (stderr, "ICC profile uses a colorspace that cannot be used for this format\n");
      exit (1);
      }
    }

  if (output_format == OUT_PDF)
    pdfout = pdf_create_document (ctx);
  else if (output_format == OUT_GPROOF) {
    /* GPROOF files are saved direct. Do not open "output". */
    }
  else if (output_format == OUT_SVG) {
    /* SVG files are always opened for each page. Do not open "output". */
    }
  else {
    if (has_percent_d (output))
      output_file_per_page = 1;
    else
      out = fz_new_output_with_path (ctx, output, 0);
    }

  if (!output_file_per_page)
    file_level_headers (ctx);

  timing.count = 0;
  timing.total = 0;
  timing.min = 1 << 30;
  timing.max = 0;
  timing.mininterp = 1 << 30;
  timing.maxinterp = 0;
  timing.minpage = 0;
  timing.maxpage = 0;
  timing.minfilename = "";
  timing.maxfilename = "";

  fz_document* doc = NULL;
  fz_var (doc);

  fz_try (ctx) {
    fz_register_document_handlers (ctx);

    while (fz_optind < argc) {
      fz_try (ctx) {
        filename = argv[fz_optind++];
        files++;
        doc = fz_open_document (ctx, filename);

        /* Once document is open check for output intent colorspace */
        oi = fz_document_output_intent (ctx, doc);
        if (oi) {
          /* See if we had explicitly set a profile to render */
          if (out_cs != CS_ICC) {
            /* In this case, we want to render to the output intent color space if the number of channels is the same */
            if (fz_colorspace_n (ctx, oi) == fz_colorspace_n (ctx, colorspace)) {
              fz_drop_colorspace (ctx, colorspace);
              colorspace = fz_keep_colorspace (ctx, oi);
              }
            }
          }

        fz_layout_document (ctx, doc, layout_w, layout_h, layout_em);

        if (layer_config)
          apply_layer_config (ctx, doc, layer_config);

        if (output_format == OUT_GPROOF)
          fz_save_gproof (ctx, filename, doc, output, resolution, "", "");
        else {
          if (fz_optind == argc || !fz_is_page_range (ctx, argv[fz_optind]))
            drawRange (ctx, doc, "1-N");
          if (fz_optind < argc && fz_is_page_range (ctx, argv[fz_optind]))
            drawRange (ctx, doc, argv[fz_optind++]);
          }

        fz_drop_document (ctx, doc);
        doc = NULL;
        }

      fz_catch(ctx) {
        fz_drop_document (ctx, doc);
        doc = NULL;

        if (!ignore_errors)
          fz_rethrow (ctx);

        fz_warn (ctx, "ignoring error in '%s'", filename);
        }
      }
    }

  fz_catch(ctx) {
    fz_drop_document (ctx, doc);
    fprintf (stderr, "error: cannot draw '%s'\n", filename);
    errored = 1;
    }

  if (!output_file_per_page)
    file_level_trailers (ctx);

  fz_close_output (ctx, out);
  fz_drop_output (ctx, out);
  out = NULL;

  if (files == 1) {
    fprintf (stderr, "total %dms / %d pages for an average of %dms\n",
      timing.total, timing.count, timing.total / timing.count);
    fprintf (stderr, "fastest page %d: %dms\n", timing.minpage, timing.min);
    fprintf (stderr, "slowest page %d: %dms\n", timing.maxpage, timing.max);
    }
  else {
    fprintf (stderr, "total %dms / %d pages for an average of %dms in %d files\n",
             timing.total, timing.count, timing.total / timing.count, files);
    fprintf (stderr, "fastest page %d: %dms (%s)\n", timing.minpage, timing.min, timing.minfilename);
    fprintf (stderr, "slowest page %d: %dms (%s)\n", timing.maxpage, timing.max, timing.maxfilename);
    }

  fz_drop_colorspace (ctx, colorspace);
  fz_drop_colorspace (ctx, proof_cs);
  fz_drop_context (ctx);

  char buf[100];
  fz_snprintf (buf, sizeof buf, "Memory use total=%zu peak=%zu current=%zu", info.total, info.peak, info.current);
  fprintf (stderr, "%s\n", buf);

  Sleep (2000);
  return (errored != 0);
  }
//}}}
