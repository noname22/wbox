/*
 * WBOX GDI Text Rendering
 */
#ifndef WBOX_GDI_TEXT_H
#define WBOX_GDI_TEXT_H

#include "gdi_objects.h"
#include "gdi_handle_table.h"
#include "gdi_dc.h"

/* Text alignment flags */
#define TA_LEFT         0x0000
#define TA_RIGHT        0x0002
#define TA_CENTER       0x0006
#define TA_TOP          0x0000
#define TA_BOTTOM       0x0008
#define TA_BASELINE     0x0018
#define TA_NOUPDATECP   0x0000
#define TA_UPDATECP     0x0001

/* ExtTextOut options */
#define ETO_OPAQUE      0x0002
#define ETO_CLIPPED     0x0004

/* DrawText flags */
#define DT_TOP          0x00000000
#define DT_LEFT         0x00000000
#define DT_CENTER       0x00000001
#define DT_RIGHT        0x00000002
#define DT_VCENTER      0x00000004
#define DT_BOTTOM       0x00000008
#define DT_WORDBREAK    0x00000010
#define DT_SINGLELINE   0x00000020
#define DT_EXPANDTABS   0x00000040
#define DT_NOCLIP       0x00000100
#define DT_CALCRECT     0x00000400

/*
 * Text output functions
 */

/* Extended text output */
bool gdi_ext_text_out(gdi_dc_t *dc, int x, int y, uint32_t options,
                       const RECT *rect, const uint16_t *str, int count,
                       const int *dx);

/* Get text extent (measure string) */
bool gdi_get_text_extent(gdi_dc_t *dc, const uint16_t *str, int count, SIZE *size);

/* Get text extent with extra info */
bool gdi_get_text_extent_ex(gdi_dc_t *dc, const uint16_t *str, int count,
                             int max_extent, int *fit, int *dx, SIZE *size);

/* Get character widths */
bool gdi_get_char_width(gdi_dc_t *dc, uint32_t first, uint32_t last, int *widths);

/* Get text metrics */
bool gdi_get_text_metrics(gdi_dc_t *dc, void *tm);  /* TEXTMETRICW* */

/*
 * Font creation
 */

/* Create font from LOGFONT */
uint32_t gdi_create_font(gdi_handle_table_t *table, int height, int width,
                          int escapement, int orientation, int weight,
                          bool italic, bool underline, bool strikeout,
                          int charset, int out_precision, int clip_precision,
                          int quality, int pitch_and_family, const char *face_name);

/*
 * Built-in font
 */

/* Get built-in font character width */
int gdi_builtin_font_char_width(void);

/* Get built-in font character height */
int gdi_builtin_font_char_height(void);

/* Get built-in font glyph data */
const uint8_t *gdi_builtin_font_get_glyph(int codepoint);

#endif /* WBOX_GDI_TEXT_H */
