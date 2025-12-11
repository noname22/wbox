/*
 * WBOX GDI Drawing Operations Implementation
 */
#include "gdi_drawing.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Common ROP3 codes */
#define ROP_BLACKNESS   0x00000042
#define ROP_NOTSRCERASE 0x001100A6
#define ROP_NOTSRCCOPY  0x00330008
#define ROP_SRCERASE    0x00440328
#define ROP_DSTINVERT   0x00550009
#define ROP_PATINVERT   0x005A0049
#define ROP_SRCINVERT   0x00660046
#define ROP_SRCAND      0x008800C6
#define ROP_MERGEPAINT  0x00BB0226
#define ROP_MERGECOPY   0x00C000CA
#define ROP_SRCCOPY     0x00CC0020
#define ROP_SRCPAINT    0x00EE0086
#define ROP_PATCOPY     0x00F00021
#define ROP_PATPAINT    0x00FB0A09
#define ROP_WHITENESS   0x00FF0062

/* Region complexity results */
#define NULLREGION      1
#define SIMPLEREGION    2
#define COMPLEXREGION   3

/* Combine region modes */
#define RGN_AND         1
#define RGN_OR          2
#define RGN_XOR         3
#define RGN_DIFF        4
#define RGN_COPY        5

/*
 * Clipping helpers
 */

bool gdi_clip_rect(gdi_dc_t *dc, int *x, int *y, int *width, int *height)
{
    /* Apply DC origin */
    int ox = *x + dc->vp_org_x - dc->win_org_x;
    int oy = *y + dc->vp_org_y - dc->win_org_y;

    /* Clip to DC bounds */
    if (ox < 0) {
        *width += ox;
        ox = 0;
    }
    if (oy < 0) {
        *height += oy;
        oy = 0;
    }
    if (ox + *width > dc->width) {
        *width = dc->width - ox;
    }
    if (oy + *height > dc->height) {
        *height = dc->height - oy;
    }

    *x = ox;
    *y = oy;

    return (*width > 0 && *height > 0);
}

bool gdi_pt_visible(gdi_dc_t *dc, int x, int y)
{
    /* Apply DC origin */
    x = x + dc->vp_org_x - dc->win_org_x;
    y = y + dc->vp_org_y - dc->win_org_y;

    return (x >= 0 && x < dc->width && y >= 0 && y < dc->height);
}

bool gdi_rect_visible(gdi_dc_t *dc, const RECT *rect)
{
    int x = rect->left + dc->vp_org_x - dc->win_org_x;
    int y = rect->top + dc->vp_org_y - dc->win_org_y;
    int r = rect->right + dc->vp_org_x - dc->win_org_x;
    int b = rect->bottom + dc->vp_org_y - dc->win_org_y;

    return (x < dc->width && r > 0 && y < dc->height && b > 0);
}

/*
 * Raster operations
 */

uint32_t gdi_apply_rop3(uint32_t dst, uint32_t src, uint32_t pat, uint32_t rop)
{
    /* Extract the actual ROP code (byte index) */
    uint8_t code = (rop >> 16) & 0xFF;

    uint32_t result = 0;
    for (int bit = 0; bit < 32; bit++) {
        uint32_t mask = 1u << bit;
        int d = (dst & mask) ? 1 : 0;
        int s = (src & mask) ? 1 : 0;
        int p = (pat & mask) ? 1 : 0;

        /* Build index: bit 2 = dst, bit 1 = src, bit 0 = pat */
        int index = (d << 2) | (s << 1) | p;

        if (code & (1 << index)) {
            result |= mask;
        }
    }
    return result;
}

uint32_t gdi_apply_rop2(uint32_t dst, uint32_t src, int rop2)
{
    switch (rop2) {
        case 1:  return 0;                  /* R2_BLACK */
        case 2:  return ~(dst | src);       /* R2_NOTMERGEPEN */
        case 3:  return dst & ~src;         /* R2_MASKNOTPEN */
        case 4:  return ~src;               /* R2_NOTCOPYPEN */
        case 5:  return ~dst & src;         /* R2_MASKPENNOT */
        case 6:  return ~dst;               /* R2_NOT */
        case 7:  return dst ^ src;          /* R2_XORPEN */
        case 8:  return ~(dst & src);       /* R2_NOTMASKPEN */
        case 9:  return dst & src;          /* R2_MASKPEN */
        case 10: return ~(dst ^ src);       /* R2_NOTXORPEN */
        case 11: return dst;                /* R2_NOP */
        case 12: return dst | ~src;         /* R2_MERGENOTPEN */
        case 13: return src;                /* R2_COPYPEN */
        case 14: return ~dst | src;         /* R2_MERGEPENNOT */
        case 15: return dst | src;          /* R2_MERGEPEN */
        case 16: return 0xFFFFFFFF;         /* R2_WHITE */
        default: return src;                /* Default to R2_COPYPEN */
    }
}

/*
 * Rectangle operations
 */

int gdi_fill_rect(gdi_dc_t *dc, const RECT *rect, gdi_brush_t *brush)
{
    if (!dc || !rect || !brush) return 0;
    if (!dc->pixels) return 0;

    /* NULL brush = no fill */
    if (brush->style == BS_NULL) return 1;

    int x = rect->left;
    int y = rect->top;
    int width = rect->right - rect->left;
    int height = rect->bottom - rect->top;

    if (!gdi_clip_rect(dc, &x, &y, &width, &height)) {
        return 1;  /* Nothing to draw, but not an error */
    }

    uint32_t color = colorref_to_argb(brush->color);

    /* Fill rectangle */
    for (int row = y; row < y + height; row++) {
        uint32_t *dst = dc->pixels + row * (dc->pitch / 4) + x;
        for (int col = 0; col < width; col++) {
            *dst++ = color;
        }
    }

    dc->dirty = true;
    return 1;
}

int gdi_frame_rect(gdi_dc_t *dc, const RECT *rect, gdi_brush_t *brush)
{
    if (!dc || !rect || !brush) return 0;

    /* Draw top edge */
    RECT edge = { rect->left, rect->top, rect->right, rect->top + 1 };
    gdi_fill_rect(dc, &edge, brush);

    /* Draw bottom edge */
    edge.top = rect->bottom - 1;
    edge.bottom = rect->bottom;
    gdi_fill_rect(dc, &edge, brush);

    /* Draw left edge */
    edge.top = rect->top + 1;
    edge.bottom = rect->bottom - 1;
    edge.right = rect->left + 1;
    gdi_fill_rect(dc, &edge, brush);

    /* Draw right edge */
    edge.left = rect->right - 1;
    edge.right = rect->right;
    gdi_fill_rect(dc, &edge, brush);

    return 1;
}

bool gdi_rectangle(gdi_dc_t *dc, int left, int top, int right, int bottom)
{
    if (!dc) return false;

    /* Fill interior with brush */
    if (dc->brush && dc->brush->style != BS_NULL) {
        RECT r = { left + 1, top + 1, right - 1, bottom - 1 };
        gdi_fill_rect(dc, &r, dc->brush);
    }

    /* Draw border with pen */
    if (dc->pen && dc->pen->style != PS_NULL) {
        /* Create temporary brush from pen color */
        gdi_brush_t pen_brush;
        pen_brush.style = BS_SOLID;
        pen_brush.color = dc->pen->color;

        RECT r = { left, top, right, bottom };
        gdi_frame_rect(dc, &r, &pen_brush);
    }

    return true;
}

bool gdi_invert_rect(gdi_dc_t *dc, const RECT *rect)
{
    if (!dc || !rect || !dc->pixels) return false;

    int x = rect->left;
    int y = rect->top;
    int width = rect->right - rect->left;
    int height = rect->bottom - rect->top;

    if (!gdi_clip_rect(dc, &x, &y, &width, &height)) {
        return true;
    }

    for (int row = y; row < y + height; row++) {
        uint32_t *dst = dc->pixels + row * (dc->pitch / 4) + x;
        for (int col = 0; col < width; col++) {
            *dst = ~(*dst) | 0xFF000000;  /* Invert RGB, keep alpha */
            dst++;
        }
    }

    dc->dirty = true;
    return true;
}

/*
 * BitBlt operations
 */

bool gdi_pat_blt(gdi_dc_t *dc, int x, int y, int width, int height, uint32_t rop)
{
    if (!dc || !dc->pixels) return false;

    /* Get pattern (brush) color */
    uint32_t pat_color = 0xFFFFFFFF;  /* Default white */
    if (dc->brush) {
        if (dc->brush->style == BS_NULL) {
            /* NULL brush - check ROP */
            if ((rop >> 16) == 0x00) return true;  /* BLACKNESS */
            if ((rop >> 16) == 0xFF) {
                pat_color = 0xFFFFFFFF;  /* WHITENESS */
            }
        } else {
            pat_color = colorref_to_argb(dc->brush->color);
        }
    }

    if (!gdi_clip_rect(dc, &x, &y, &width, &height)) {
        return true;
    }

    /* Handle common ROP cases */
    uint8_t rop_code = (rop >> 16) & 0xFF;

    switch (rop) {
        case ROP_BLACKNESS:
            pat_color = 0xFF000000;
            goto fill;
        case ROP_WHITENESS:
            pat_color = 0xFFFFFFFF;
            goto fill;
        case ROP_PATCOPY:
        fill:
            for (int row = y; row < y + height; row++) {
                uint32_t *dst = dc->pixels + row * (dc->pitch / 4) + x;
                for (int col = 0; col < width; col++) {
                    *dst++ = pat_color;
                }
            }
            break;

        case ROP_PATINVERT:
            for (int row = y; row < y + height; row++) {
                uint32_t *dst = dc->pixels + row * (dc->pitch / 4) + x;
                for (int col = 0; col < width; col++) {
                    *dst = (*dst ^ pat_color) | 0xFF000000;
                    dst++;
                }
            }
            break;

        case ROP_DSTINVERT:
            for (int row = y; row < y + height; row++) {
                uint32_t *dst = dc->pixels + row * (dc->pitch / 4) + x;
                for (int col = 0; col < width; col++) {
                    *dst = (~*dst) | 0xFF000000;
                    dst++;
                }
            }
            break;

        default:
            /* General case using ROP3 */
            for (int row = y; row < y + height; row++) {
                uint32_t *dst = dc->pixels + row * (dc->pitch / 4) + x;
                for (int col = 0; col < width; col++) {
                    *dst = gdi_apply_rop3(*dst, 0, pat_color, rop);
                    dst++;
                }
            }
            break;
    }

    dc->dirty = true;
    return true;
}

bool gdi_bit_blt(gdi_dc_t *dst_dc, int dst_x, int dst_y, int width, int height,
                  gdi_dc_t *src_dc, int src_x, int src_y, uint32_t rop)
{
    if (!dst_dc || !dst_dc->pixels) return false;

    /* Handle pattern-only ROPs */
    if (!src_dc || !src_dc->pixels) {
        return gdi_pat_blt(dst_dc, dst_x, dst_y, width, height, rop);
    }

    /* Clip destination */
    int orig_dst_x = dst_x, orig_dst_y = dst_y;
    if (!gdi_clip_rect(dst_dc, &dst_x, &dst_y, &width, &height)) {
        return true;
    }

    /* Adjust source based on clipping */
    src_x += (dst_x - orig_dst_x);
    src_y += (dst_y - orig_dst_y);

    /* Clip source */
    if (src_x < 0) {
        width += src_x;
        dst_x -= src_x;
        src_x = 0;
    }
    if (src_y < 0) {
        height += src_y;
        dst_y -= src_y;
        src_y = 0;
    }
    if (src_x + width > src_dc->width) {
        width = src_dc->width - src_x;
    }
    if (src_y + height > src_dc->height) {
        height = src_dc->height - src_y;
    }

    if (width <= 0 || height <= 0) return true;

    /* Get pattern color */
    uint32_t pat_color = 0xFFFFFFFF;
    if (dst_dc->brush && dst_dc->brush->style != BS_NULL) {
        pat_color = colorref_to_argb(dst_dc->brush->color);
    }

    /* Perform blit */
    for (int row = 0; row < height; row++) {
        uint32_t *dst = dst_dc->pixels + (dst_y + row) * (dst_dc->pitch / 4) + dst_x;
        uint32_t *src = src_dc->pixels + (src_y + row) * (src_dc->pitch / 4) + src_x;

        switch (rop) {
            case ROP_SRCCOPY:
                memcpy(dst, src, width * 4);
                break;

            case ROP_SRCAND:
                for (int col = 0; col < width; col++) {
                    dst[col] = (dst[col] & src[col]) | 0xFF000000;
                }
                break;

            case ROP_SRCPAINT:
                for (int col = 0; col < width; col++) {
                    dst[col] = (dst[col] | src[col]) | 0xFF000000;
                }
                break;

            case ROP_SRCINVERT:
                for (int col = 0; col < width; col++) {
                    dst[col] = (dst[col] ^ src[col]) | 0xFF000000;
                }
                break;

            default:
                for (int col = 0; col < width; col++) {
                    dst[col] = gdi_apply_rop3(dst[col], src[col], pat_color, rop);
                }
                break;
        }
    }

    dst_dc->dirty = true;
    return true;
}

bool gdi_stretch_blt(gdi_dc_t *dst_dc, int dst_x, int dst_y, int dst_w, int dst_h,
                      gdi_dc_t *src_dc, int src_x, int src_y, int src_w, int src_h,
                      uint32_t rop)
{
    if (!dst_dc || !src_dc || !dst_dc->pixels || !src_dc->pixels) {
        return false;
    }

    /* Simple nearest-neighbor scaling */
    for (int dy = 0; dy < dst_h; dy++) {
        int sy = src_y + (dy * src_h) / dst_h;
        if (sy < 0 || sy >= src_dc->height) continue;

        for (int dx = 0; dx < dst_w; dx++) {
            int sx = src_x + (dx * src_w) / dst_w;
            if (sx < 0 || sx >= src_dc->width) continue;

            int dest_px = dst_x + dx;
            int dest_py = dst_y + dy;
            if (dest_px < 0 || dest_px >= dst_dc->width) continue;
            if (dest_py < 0 || dest_py >= dst_dc->height) continue;

            uint32_t src_pixel = src_dc->pixels[sy * (src_dc->pitch / 4) + sx];
            uint32_t *dst_pixel = &dst_dc->pixels[dest_py * (dst_dc->pitch / 4) + dest_px];

            if (rop == ROP_SRCCOPY) {
                *dst_pixel = src_pixel;
            } else {
                uint32_t pat = 0xFFFFFFFF;
                if (dst_dc->brush) {
                    pat = colorref_to_argb(dst_dc->brush->color);
                }
                *dst_pixel = gdi_apply_rop3(*dst_pixel, src_pixel, pat, rop);
            }
        }
    }

    dst_dc->dirty = true;
    return true;
}

/*
 * Line drawing
 */

bool gdi_line_to(gdi_dc_t *dc, int x, int y)
{
    if (!dc || !dc->pixels) return false;
    if (!dc->pen || dc->pen->style == PS_NULL) {
        dc->cur_x = x;
        dc->cur_y = y;
        return true;
    }

    int x0 = dc->cur_x;
    int y0 = dc->cur_y;
    int x1 = x;
    int y1 = y;

    /* Apply DC origin */
    x0 += dc->vp_org_x - dc->win_org_x;
    y0 += dc->vp_org_y - dc->win_org_y;
    x1 += dc->vp_org_x - dc->win_org_x;
    y1 += dc->vp_org_y - dc->win_org_y;

    uint32_t color = colorref_to_argb(dc->pen->color);

    /* Bresenham's line algorithm */
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    while (1) {
        if (x0 >= 0 && x0 < dc->width && y0 >= 0 && y0 < dc->height) {
            dc->pixels[y0 * (dc->pitch / 4) + x0] = color;
        }

        if (x0 == x1 && y0 == y1) break;

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }

    dc->cur_x = x;
    dc->cur_y = y;
    dc->dirty = true;
    return true;
}

bool gdi_polyline(gdi_dc_t *dc, const POINT *points, int count)
{
    if (!dc || !points || count < 2) return false;

    gdi_move_to(dc, points[0].x, points[0].y, NULL);
    for (int i = 1; i < count; i++) {
        gdi_line_to(dc, points[i].x, points[i].y);
    }

    return true;
}

bool gdi_poly_polyline(gdi_dc_t *dc, const POINT *points, const int *counts, int num_polys)
{
    if (!dc || !points || !counts) return false;

    int point_index = 0;
    for (int i = 0; i < num_polys; i++) {
        if (counts[i] >= 2) {
            gdi_polyline(dc, &points[point_index], counts[i]);
        }
        point_index += counts[i];
    }

    return true;
}

/*
 * Region operations
 */

bool gdi_fill_rgn(gdi_handle_table_t *table, gdi_dc_t *dc, uint32_t hrgn, uint32_t hbrush)
{
    gdi_region_t *rgn = gdi_get_object(table, hrgn, GDI_OBJ_REGION);
    if (!rgn) return false;

    gdi_brush_t *brush = gdi_get_object(table, hbrush, GDI_OBJ_BRUSH);
    if (!brush) return false;

    return gdi_fill_rect(dc, &rgn->bounds, brush) != 0;
}

bool gdi_frame_rgn(gdi_handle_table_t *table, gdi_dc_t *dc, uint32_t hrgn, uint32_t hbrush, int width, int height)
{
    (void)width;
    (void)height;

    gdi_region_t *rgn = gdi_get_object(table, hrgn, GDI_OBJ_REGION);
    if (!rgn) return false;

    gdi_brush_t *brush = gdi_get_object(table, hbrush, GDI_OBJ_BRUSH);
    if (!brush) return false;

    return gdi_frame_rect(dc, &rgn->bounds, brush) != 0;
}

bool gdi_invert_rgn(gdi_handle_table_t *table, gdi_dc_t *dc, uint32_t hrgn)
{
    gdi_region_t *rgn = gdi_get_object(table, hrgn, GDI_OBJ_REGION);
    if (!rgn) return false;

    return gdi_invert_rect(dc, &rgn->bounds);
}

bool gdi_paint_rgn(gdi_handle_table_t *table, gdi_dc_t *dc, uint32_t hrgn)
{
    gdi_region_t *rgn = gdi_get_object(table, hrgn, GDI_OBJ_REGION);
    if (!rgn || !dc->brush) return false;

    return gdi_fill_rect(dc, &rgn->bounds, dc->brush) != 0;
}

/*
 * Pixel operations
 */

COLORREF gdi_set_pixel(gdi_dc_t *dc, int x, int y, COLORREF color)
{
    if (!dc || !dc->pixels) return (COLORREF)-1;

    /* Apply DC origin */
    x += dc->vp_org_x - dc->win_org_x;
    y += dc->vp_org_y - dc->win_org_y;

    if (x < 0 || x >= dc->width || y < 0 || y >= dc->height) {
        return (COLORREF)-1;
    }

    uint32_t *pixel = &dc->pixels[y * (dc->pitch / 4) + x];
    COLORREF prev = argb_to_colorref(*pixel);
    *pixel = colorref_to_argb(color);

    dc->dirty = true;
    return prev;
}

COLORREF gdi_get_pixel(gdi_dc_t *dc, int x, int y)
{
    if (!dc || !dc->pixels) return (COLORREF)-1;

    x += dc->vp_org_x - dc->win_org_x;
    y += dc->vp_org_y - dc->win_org_y;

    if (x < 0 || x >= dc->width || y < 0 || y >= dc->height) {
        return (COLORREF)-1;
    }

    return argb_to_colorref(dc->pixels[y * (dc->pitch / 4) + x]);
}

/*
 * Object creation
 */

uint32_t gdi_create_solid_brush(gdi_handle_table_t *table, COLORREF color)
{
    gdi_brush_t *brush = gdi_alloc_brush(table);
    if (!brush) return 0;

    brush->style = BS_SOLID;
    brush->color = color & 0x00FFFFFF;
    brush->hatch_style = 0;
    brush->pattern = NULL;

    uint32_t handle = gdi_alloc_handle(table, brush, GDI_OBJ_BRUSH);
    if (!handle) {
        gdi_free_brush(table, brush);
        return 0;
    }

    brush->handle = handle;
    return handle;
}

uint32_t gdi_create_pen(gdi_handle_table_t *table, int style, int width, COLORREF color)
{
    gdi_pen_t *pen = gdi_alloc_pen(table);
    if (!pen) return 0;

    pen->style = style;
    pen->width = width > 0 ? width : 1;
    pen->color = color & 0x00FFFFFF;

    uint32_t handle = gdi_alloc_handle(table, pen, GDI_OBJ_PEN);
    if (!handle) {
        gdi_free_pen(table, pen);
        return 0;
    }

    pen->handle = handle;
    return handle;
}

/*
 * Bitmap creation
 */

uint32_t gdi_create_bitmap(gdi_handle_table_t *table, int width, int height,
                           uint32_t planes, uint32_t bpp)
{
    gdi_bitmap_t *bmp = gdi_alloc_bitmap(table);
    if (!bmp) return 0;

    /* Normalize parameters */
    if (width < 1) width = 1;
    if (height < 1) height = 1;
    if (planes < 1) planes = 1;
    if (bpp < 1) bpp = 1;

    bmp->width = width;
    bmp->height = height;
    bmp->bits_per_pixel = bpp;
    bmp->planes = planes;
    bmp->pitch = ((width * bpp + 31) / 32) * 4;  /* DWORD-aligned */

    /* Allocate pixel data */
    size_t size = bmp->pitch * height;
    bmp->pixels = calloc(1, size);
    if (!bmp->pixels) {
        gdi_free_bitmap(table, bmp);
        return 0;
    }

    uint32_t handle = gdi_alloc_handle(table, bmp, GDI_OBJ_BITMAP);
    if (!handle) {
        free(bmp->pixels);
        gdi_free_bitmap(table, bmp);
        return 0;
    }

    bmp->handle = handle;
    return handle;
}

uint32_t gdi_create_pattern_brush(gdi_handle_table_t *table, uint32_t hbitmap)
{
    gdi_brush_t *brush = gdi_alloc_brush(table);
    if (!brush) return 0;

    brush->style = BS_PATTERN;
    brush->color = 0;
    brush->hatch_style = 0;
    brush->pattern = (void *)(uintptr_t)hbitmap;  /* Store bitmap handle */

    uint32_t handle = gdi_alloc_handle(table, brush, GDI_OBJ_BRUSH);
    if (!handle) {
        gdi_free_brush(table, brush);
        return 0;
    }

    brush->handle = handle;
    return handle;
}

/*
 * Region creation
 */

uint32_t gdi_create_rect_rgn(gdi_handle_table_t *table, int left, int top, int right, int bottom)
{
    gdi_region_t *rgn = gdi_alloc_region(table);
    if (!rgn) return 0;

    rgn->bounds.left = left;
    rgn->bounds.top = top;
    rgn->bounds.right = right;
    rgn->bounds.bottom = bottom;
    rgn->rect_count = 1;
    rgn->rects = NULL;

    uint32_t handle = gdi_alloc_handle(table, rgn, GDI_OBJ_REGION);
    if (!handle) {
        gdi_free_region(table, rgn);
        return 0;
    }

    rgn->handle = handle;
    return handle;
}

bool gdi_set_rect_rgn(gdi_handle_table_t *table, uint32_t hrgn, int left, int top, int right, int bottom)
{
    gdi_region_t *rgn = gdi_get_object(table, hrgn, GDI_OBJ_REGION);
    if (!rgn) return false;

    rgn->bounds.left = left;
    rgn->bounds.top = top;
    rgn->bounds.right = right;
    rgn->bounds.bottom = bottom;

    return true;
}

int gdi_combine_rgn(gdi_handle_table_t *table, uint32_t hrgnDest, uint32_t hrgnSrc1, uint32_t hrgnSrc2, int mode)
{
    gdi_region_t *dest = gdi_get_object(table, hrgnDest, GDI_OBJ_REGION);
    gdi_region_t *src1 = gdi_get_object(table, hrgnSrc1, GDI_OBJ_REGION);

    if (!dest || !src1) return 0;

    if (mode == RGN_COPY) {
        dest->bounds = src1->bounds;
        return SIMPLEREGION;
    }

    gdi_region_t *src2 = gdi_get_object(table, hrgnSrc2, GDI_OBJ_REGION);
    if (!src2) return 0;

    /* Simplified: just use bounding box operations */
    switch (mode) {
        case RGN_AND:
            dest->bounds.left = (src1->bounds.left > src2->bounds.left) ? src1->bounds.left : src2->bounds.left;
            dest->bounds.top = (src1->bounds.top > src2->bounds.top) ? src1->bounds.top : src2->bounds.top;
            dest->bounds.right = (src1->bounds.right < src2->bounds.right) ? src1->bounds.right : src2->bounds.right;
            dest->bounds.bottom = (src1->bounds.bottom < src2->bounds.bottom) ? src1->bounds.bottom : src2->bounds.bottom;
            break;

        case RGN_OR:
            dest->bounds.left = (src1->bounds.left < src2->bounds.left) ? src1->bounds.left : src2->bounds.left;
            dest->bounds.top = (src1->bounds.top < src2->bounds.top) ? src1->bounds.top : src2->bounds.top;
            dest->bounds.right = (src1->bounds.right > src2->bounds.right) ? src1->bounds.right : src2->bounds.right;
            dest->bounds.bottom = (src1->bounds.bottom > src2->bounds.bottom) ? src1->bounds.bottom : src2->bounds.bottom;
            break;

        default:
            return 0;
    }

    if (dest->bounds.left >= dest->bounds.right || dest->bounds.top >= dest->bounds.bottom) {
        return NULLREGION;
    }

    return SIMPLEREGION;
}

int gdi_get_rgn_box(gdi_handle_table_t *table, uint32_t hrgn, RECT *rect)
{
    gdi_region_t *rgn = gdi_get_object(table, hrgn, GDI_OBJ_REGION);
    if (!rgn || !rect) return 0;

    *rect = rgn->bounds;

    if (rect->left >= rect->right || rect->top >= rect->bottom) {
        return NULLREGION;
    }

    return SIMPLEREGION;
}

/*
 * Shape drawing (stubs for now)
 */

bool gdi_ellipse(gdi_dc_t *dc, int left, int top, int right, int bottom)
{
    /* Simplified: draw as rectangle */
    return gdi_rectangle(dc, left, top, right, bottom);
}

bool gdi_round_rect(gdi_dc_t *dc, int left, int top, int right, int bottom, int width, int height)
{
    (void)width;
    (void)height;
    /* Simplified: draw as rectangle */
    return gdi_rectangle(dc, left, top, right, bottom);
}

bool gdi_polygon(gdi_dc_t *dc, const POINT *points, int count)
{
    if (!dc || !points || count < 3) return false;

    /* Draw outline */
    gdi_polyline(dc, points, count);
    /* Close the polygon */
    gdi_move_to(dc, points[count-1].x, points[count-1].y, NULL);
    gdi_line_to(dc, points[0].x, points[0].y);

    return true;
}

bool gdi_arc(gdi_dc_t *dc, int left, int top, int right, int bottom,
              int x_start, int y_start, int x_end, int y_end)
{
    (void)dc;
    (void)left;
    (void)top;
    (void)right;
    (void)bottom;
    (void)x_start;
    (void)y_start;
    (void)x_end;
    (void)y_end;
    /* Not implemented */
    return true;
}
