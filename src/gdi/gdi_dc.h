/*
 * WBOX GDI Device Context Operations
 */
#ifndef WBOX_GDI_DC_H
#define WBOX_GDI_DC_H

#include "gdi_objects.h"
#include "gdi_handle_table.h"
#include "display.h"

/*
 * DC creation and deletion
 */

/* Create a display DC (for the primary screen) */
uint32_t gdi_create_display_dc(gdi_handle_table_t *table, display_context_t *display);

/* Create a window DC (associated with a guest HWND) */
uint32_t gdi_create_window_dc(gdi_handle_table_t *table, display_context_t *display, uint32_t hwnd);

/* Create a compatible (memory) DC */
uint32_t gdi_create_compatible_dc(gdi_handle_table_t *table, uint32_t hdc_ref);

/* Delete a DC */
bool gdi_delete_dc(gdi_handle_table_t *table, uint32_t hdc);

/* Release a window DC (for GetDC/ReleaseDC pair) */
int gdi_release_dc(gdi_handle_table_t *table, uint32_t hwnd, uint32_t hdc);

/*
 * Object selection
 */

/* Select an object into a DC, returns previous object handle */
uint32_t gdi_select_object(gdi_handle_table_t *table, uint32_t hdc, uint32_t hobject);

/* Select brush into DC */
uint32_t gdi_select_brush(gdi_handle_table_t *table, uint32_t hdc, uint32_t hbrush);

/* Select pen into DC */
uint32_t gdi_select_pen(gdi_handle_table_t *table, uint32_t hdc, uint32_t hpen);

/* Select font into DC */
uint32_t gdi_select_font(gdi_handle_table_t *table, uint32_t hdc, uint32_t hfont);

/* Select bitmap into memory DC */
uint32_t gdi_select_bitmap(gdi_handle_table_t *table, uint32_t hdc, uint32_t hbitmap);

/* Select palette into DC */
uint32_t gdi_select_palette(gdi_handle_table_t *table, uint32_t hdc, uint32_t hpalette, bool force_background);

/*
 * DC attributes
 */

/* Set text color, returns previous color */
COLORREF gdi_set_text_color(gdi_dc_t *dc, COLORREF color);

/* Get text color */
COLORREF gdi_get_text_color(gdi_dc_t *dc);

/* Set background color, returns previous color */
COLORREF gdi_set_bk_color(gdi_dc_t *dc, COLORREF color);

/* Get background color */
COLORREF gdi_get_bk_color(gdi_dc_t *dc);

/* Set background mode (TRANSPARENT or OPAQUE), returns previous mode */
int gdi_set_bk_mode(gdi_dc_t *dc, int mode);

/* Get background mode */
int gdi_get_bk_mode(gdi_dc_t *dc);

/* Set ROP2 (binary raster operation), returns previous */
int gdi_set_rop2(gdi_dc_t *dc, int rop2);

/* Get ROP2 */
int gdi_get_rop2(gdi_dc_t *dc);

/* Set map mode, returns previous */
int gdi_set_map_mode(gdi_dc_t *dc, int mode);

/* Get map mode */
int gdi_get_map_mode(gdi_dc_t *dc);

/* Set text alignment, returns previous */
int gdi_set_text_align(gdi_dc_t *dc, int align);

/* Get text alignment */
int gdi_get_text_align(gdi_dc_t *dc);

/* Set brush origin */
bool gdi_set_brush_org(gdi_dc_t *dc, int x, int y, POINT *prev);

/* Get brush origin */
bool gdi_get_brush_org(gdi_dc_t *dc, POINT *point);

/*
 * Viewport and window
 */

/* Set viewport origin */
bool gdi_set_viewport_org(gdi_dc_t *dc, int x, int y, POINT *prev);

/* Get viewport origin */
bool gdi_get_viewport_org(gdi_dc_t *dc, POINT *point);

/* Set window origin */
bool gdi_set_window_org(gdi_dc_t *dc, int x, int y, POINT *prev);

/* Get window origin */
bool gdi_get_window_org(gdi_dc_t *dc, POINT *point);

/* Offset viewport origin */
bool gdi_offset_viewport_org(gdi_dc_t *dc, int x, int y, POINT *prev);

/* Offset window origin */
bool gdi_offset_window_org(gdi_dc_t *dc, int x, int y, POINT *prev);

/*
 * Current position
 */

/* Move to position */
bool gdi_move_to(gdi_dc_t *dc, int x, int y, POINT *prev);

/* Get current position */
bool gdi_get_current_position(gdi_dc_t *dc, POINT *point);

/*
 * DC state save/restore
 */

/* Save DC state, returns save level */
int gdi_save_dc(gdi_handle_table_t *table, uint32_t hdc);

/* Restore DC state */
bool gdi_restore_dc(gdi_handle_table_t *table, uint32_t hdc, int level);

/*
 * Device capabilities
 */

/* Get device capability value */
int gdi_get_device_caps(gdi_dc_t *dc, int cap_index);

/*
 * Coordinate transformation helpers
 */

/* Transform logical coordinates to device coordinates */
void gdi_lp_to_dp(gdi_dc_t *dc, POINT *points, int count);

/* Transform device coordinates to logical coordinates */
void gdi_dp_to_lp(gdi_dc_t *dc, POINT *points, int count);

/*
 * Utility functions
 */

/* Get DC object from handle (convenience wrapper) */
gdi_dc_t *gdi_get_dc(gdi_handle_table_t *table, uint32_t hdc);

/* Initialize DC with default values */
void gdi_init_dc_defaults(gdi_dc_t *dc);

/* Set DC surface (pixels, dimensions) */
void gdi_set_dc_surface(gdi_dc_t *dc, uint32_t *pixels, int width, int height, int pitch);

#endif /* WBOX_GDI_DC_H */
