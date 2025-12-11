/*
 * WBOX GDI Drawing Operations
 */
#ifndef WBOX_GDI_DRAWING_H
#define WBOX_GDI_DRAWING_H

#include "gdi_objects.h"
#include "gdi_handle_table.h"
#include "gdi_dc.h"

/*
 * Rectangle operations
 */

/* Fill rectangle with brush */
int gdi_fill_rect(gdi_dc_t *dc, const RECT *rect, gdi_brush_t *brush);

/* Frame rectangle (draw border) */
int gdi_frame_rect(gdi_dc_t *dc, const RECT *rect, gdi_brush_t *brush);

/* Draw rectangle outline with current pen */
bool gdi_rectangle(gdi_dc_t *dc, int left, int top, int right, int bottom);

/* Invert rectangle */
bool gdi_invert_rect(gdi_dc_t *dc, const RECT *rect);

/*
 * BitBlt operations
 */

/* Pattern block transfer (fill with brush using ROP) */
bool gdi_pat_blt(gdi_dc_t *dc, int x, int y, int width, int height, uint32_t rop);

/* Bit block transfer (copy pixels) */
bool gdi_bit_blt(gdi_dc_t *dst_dc, int dst_x, int dst_y, int width, int height,
                  gdi_dc_t *src_dc, int src_x, int src_y, uint32_t rop);

/* Stretch bit block transfer */
bool gdi_stretch_blt(gdi_dc_t *dst_dc, int dst_x, int dst_y, int dst_w, int dst_h,
                      gdi_dc_t *src_dc, int src_x, int src_y, int src_w, int src_h,
                      uint32_t rop);

/*
 * Line drawing
 */

/* Draw line from current position to point */
bool gdi_line_to(gdi_dc_t *dc, int x, int y);

/* Draw polyline */
bool gdi_polyline(gdi_dc_t *dc, const POINT *points, int count);

/* Draw multiple polylines */
bool gdi_poly_polyline(gdi_dc_t *dc, const POINT *points, const int *counts, int num_polys);

/*
 * Region operations
 */

/* Fill region with brush */
bool gdi_fill_rgn(gdi_handle_table_t *table, gdi_dc_t *dc, uint32_t hrgn, uint32_t hbrush);

/* Frame region (draw border) */
bool gdi_frame_rgn(gdi_handle_table_t *table, gdi_dc_t *dc, uint32_t hrgn, uint32_t hbrush, int width, int height);

/* Invert region */
bool gdi_invert_rgn(gdi_handle_table_t *table, gdi_dc_t *dc, uint32_t hrgn);

/* Paint region (fill with current brush) */
bool gdi_paint_rgn(gdi_handle_table_t *table, gdi_dc_t *dc, uint32_t hrgn);

/*
 * Shape drawing
 */

/* Draw ellipse */
bool gdi_ellipse(gdi_dc_t *dc, int left, int top, int right, int bottom);

/* Draw rounded rectangle */
bool gdi_round_rect(gdi_dc_t *dc, int left, int top, int right, int bottom, int width, int height);

/* Draw polygon */
bool gdi_polygon(gdi_dc_t *dc, const POINT *points, int count);

/* Draw arc */
bool gdi_arc(gdi_dc_t *dc, int left, int top, int right, int bottom,
              int x_start, int y_start, int x_end, int y_end);

/*
 * Pixel operations
 */

/* Set pixel color */
COLORREF gdi_set_pixel(gdi_dc_t *dc, int x, int y, COLORREF color);

/* Get pixel color */
COLORREF gdi_get_pixel(gdi_dc_t *dc, int x, int y);

/*
 * Raster operations
 */

/* Apply ROP3 operation */
uint32_t gdi_apply_rop3(uint32_t dst, uint32_t src, uint32_t pat, uint32_t rop);

/* Apply ROP2 operation */
uint32_t gdi_apply_rop2(uint32_t dst, uint32_t src, int rop2);

/*
 * Brush operations for object creation
 */

/* Create solid brush */
uint32_t gdi_create_solid_brush(gdi_handle_table_t *table, COLORREF color);

/* Create pen */
uint32_t gdi_create_pen(gdi_handle_table_t *table, int style, int width, COLORREF color);

/*
 * Region creation
 */

/* Create rectangular region */
uint32_t gdi_create_rect_rgn(gdi_handle_table_t *table, int left, int top, int right, int bottom);

/* Set region to rectangle */
bool gdi_set_rect_rgn(gdi_handle_table_t *table, uint32_t hrgn, int left, int top, int right, int bottom);

/* Combine regions */
int gdi_combine_rgn(gdi_handle_table_t *table, uint32_t hrgnDest, uint32_t hrgnSrc1, uint32_t hrgnSrc2, int mode);

/* Get region bounding box */
int gdi_get_rgn_box(gdi_handle_table_t *table, uint32_t hrgn, RECT *rect);

/*
 * Clipping helpers
 */

/* Clip rectangle to DC bounds */
bool gdi_clip_rect(gdi_dc_t *dc, int *x, int *y, int *width, int *height);

/* Check if point is visible in DC */
bool gdi_pt_visible(gdi_dc_t *dc, int x, int y);

/* Check if rectangle intersects DC visible region */
bool gdi_rect_visible(gdi_dc_t *dc, const RECT *rect);

#endif /* WBOX_GDI_DRAWING_H */
