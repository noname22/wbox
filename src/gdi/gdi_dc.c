/*
 * WBOX GDI Device Context Implementation
 */
#include "gdi_dc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Device capabilities values */
#define CAPS_HORZRES        800
#define CAPS_VERTRES        600
#define CAPS_BITSPIXEL      32
#define CAPS_PLANES         1
#define CAPS_LOGPIXELSX     96
#define CAPS_LOGPIXELSY     96

/* Initialize DC with default attribute values */
void gdi_init_dc_defaults(gdi_dc_t *dc)
{
    /* Clear and set defaults */
    dc->cur_x = 0;
    dc->cur_y = 0;
    dc->text_color = RGB(0, 0, 0);      /* Black */
    dc->bk_color = RGB(255, 255, 255);  /* White */
    dc->bk_mode = OPAQUE;
    dc->map_mode = 1;                    /* MM_TEXT */
    dc->text_align = 0;                  /* TA_LEFT | TA_TOP */
    dc->rop2 = 13;                       /* R2_COPYPEN */
    dc->stretch_mode = 1;                /* BLACKONWHITE */
    dc->poly_fill_mode = 1;              /* ALTERNATE */

    /* Viewport and window defaults */
    dc->vp_org_x = 0;
    dc->vp_org_y = 0;
    dc->vp_ext_x = 1;
    dc->vp_ext_y = 1;
    dc->win_org_x = 0;
    dc->win_org_y = 0;
    dc->win_ext_x = 1;
    dc->win_ext_y = 1;

    dc->brush_org_x = 0;
    dc->brush_org_y = 0;

    dc->save_level = 0;
    dc->saved_dc = NULL;
    dc->dirty = false;
}

/* Set DC surface */
void gdi_set_dc_surface(gdi_dc_t *dc, uint32_t *pixels, int width, int height, int pitch)
{
    dc->pixels = pixels;
    dc->width = width;
    dc->height = height;
    dc->pitch = pitch;
    dc->bits_per_pixel = 32;
}

/* Create display DC */
uint32_t gdi_create_display_dc(gdi_handle_table_t *table, display_context_t *display)
{
    gdi_dc_t *dc = gdi_alloc_dc(table);
    if (!dc) return 0;

    gdi_init_dc_defaults(dc);
    dc->dc_type = DCTYPE_DIRECT;
    dc->hwnd = 0;  /* Desktop */

    /* Link to display framebuffer */
    if (display) {
        dc->pixels = display->pixels;
        dc->width = display->width;
        dc->height = display->height;
        dc->pitch = display->pitch;
    } else {
        dc->pixels = NULL;
        dc->width = CAPS_HORZRES;
        dc->height = CAPS_VERTRES;
        dc->pitch = CAPS_HORZRES * 4;
    }
    dc->bits_per_pixel = 32;

    /* Select default stock objects */
    dc->brush = &table->stock_brushes[GDI_STOCK_WHITE_BRUSH];
    dc->pen = &table->stock_pens[1];  /* BLACK_PEN */
    dc->font = &table->stock_fonts[3]; /* SYSTEM_FONT */

    dc->prev_brush_handle = table->stock_handles[GDI_STOCK_WHITE_BRUSH];
    dc->prev_pen_handle = table->stock_handles[GDI_STOCK_BLACK_PEN];
    dc->prev_font_handle = table->stock_handles[GDI_STOCK_SYSTEM_FONT];

    /* Allocate handle */
    uint32_t handle = gdi_alloc_handle(table, dc, GDI_OBJ_DC);
    if (!handle) {
        gdi_free_dc(table, dc);
        return 0;
    }

    dc->handle = handle;
    return handle;
}

/* Create window DC */
uint32_t gdi_create_window_dc(gdi_handle_table_t *table, display_context_t *display, uint32_t hwnd)
{
    uint32_t hdc = gdi_create_display_dc(table, display);
    if (hdc) {
        gdi_dc_t *dc = gdi_get_object(table, hdc, GDI_OBJ_DC);
        if (dc) {
            dc->hwnd = hwnd;
        }
    }
    return hdc;
}

/* Create compatible (memory) DC */
uint32_t gdi_create_compatible_dc(gdi_handle_table_t *table, uint32_t hdc_ref)
{
    gdi_dc_t *dc = gdi_alloc_dc(table);
    if (!dc) return 0;

    gdi_init_dc_defaults(dc);
    dc->dc_type = DCTYPE_MEMORY;
    dc->hwnd = 0;

    /* Memory DCs start with 1x1 monochrome bitmap */
    dc->pixels = NULL;
    dc->width = 1;
    dc->height = 1;
    dc->pitch = 4;
    dc->bits_per_pixel = 32;

    /* Copy attributes from reference DC if provided */
    if (hdc_ref) {
        gdi_dc_t *ref_dc = gdi_get_object(table, hdc_ref, GDI_OBJ_DC);
        if (ref_dc) {
            dc->text_color = ref_dc->text_color;
            dc->bk_color = ref_dc->bk_color;
            dc->bk_mode = ref_dc->bk_mode;
            dc->map_mode = ref_dc->map_mode;
            dc->bits_per_pixel = ref_dc->bits_per_pixel;
        }
    }

    /* Select default stock objects */
    dc->brush = &table->stock_brushes[GDI_STOCK_WHITE_BRUSH];
    dc->pen = &table->stock_pens[1];
    dc->font = &table->stock_fonts[3];

    dc->prev_brush_handle = table->stock_handles[GDI_STOCK_WHITE_BRUSH];
    dc->prev_pen_handle = table->stock_handles[GDI_STOCK_BLACK_PEN];
    dc->prev_font_handle = table->stock_handles[GDI_STOCK_SYSTEM_FONT];

    /* Allocate handle */
    uint32_t handle = gdi_alloc_handle(table, dc, GDI_OBJ_DC);
    if (!handle) {
        gdi_free_dc(table, dc);
        return 0;
    }

    dc->handle = handle;
    return handle;
}

/* Delete DC */
bool gdi_delete_dc(gdi_handle_table_t *table, uint32_t hdc)
{
    gdi_dc_t *dc = gdi_get_object(table, hdc, GDI_OBJ_DC);
    if (!dc) return false;

    /* Free saved DC chain */
    gdi_dc_t *saved = dc->saved_dc;
    while (saved) {
        gdi_dc_t *next = saved->saved_dc;
        gdi_free_dc(table, saved);
        saved = next;
    }

    /* Free the DC */
    gdi_free_handle(table, hdc);
    gdi_free_dc(table, dc);

    return true;
}

/* Release window DC */
int gdi_release_dc(gdi_handle_table_t *table, uint32_t hwnd, uint32_t hdc)
{
    (void)hwnd;  /* Not used for now */

    gdi_dc_t *dc = gdi_get_object(table, hdc, GDI_OBJ_DC);
    if (!dc) return 0;

    /* Only release DCs that were obtained via GetDC */
    if (dc->dc_type != DCTYPE_DIRECT) return 0;

    /* Delete the DC */
    gdi_delete_dc(table, hdc);
    return 1;
}

/* Get DC object from handle */
gdi_dc_t *gdi_get_dc(gdi_handle_table_t *table, uint32_t hdc)
{
    return gdi_get_object(table, hdc, GDI_OBJ_DC);
}

/*
 * Object selection
 */

uint32_t gdi_select_brush(gdi_handle_table_t *table, uint32_t hdc, uint32_t hbrush)
{
    gdi_dc_t *dc = gdi_get_object(table, hdc, GDI_OBJ_DC);
    if (!dc) return 0;

    gdi_brush_t *brush = gdi_get_object(table, hbrush, GDI_OBJ_BRUSH);
    if (!brush) return 0;

    uint32_t prev_handle = dc->prev_brush_handle;
    dc->brush = brush;
    dc->prev_brush_handle = hbrush;

    return prev_handle;
}

uint32_t gdi_select_pen(gdi_handle_table_t *table, uint32_t hdc, uint32_t hpen)
{
    gdi_dc_t *dc = gdi_get_object(table, hdc, GDI_OBJ_DC);
    if (!dc) return 0;

    gdi_pen_t *pen = gdi_get_object(table, hpen, GDI_OBJ_PEN);
    if (!pen) return 0;

    uint32_t prev_handle = dc->prev_pen_handle;
    dc->pen = pen;
    dc->prev_pen_handle = hpen;

    return prev_handle;
}

uint32_t gdi_select_font(gdi_handle_table_t *table, uint32_t hdc, uint32_t hfont)
{
    gdi_dc_t *dc = gdi_get_object(table, hdc, GDI_OBJ_DC);
    if (!dc) return 0;

    gdi_font_t *font = gdi_get_object(table, hfont, GDI_OBJ_FONT);
    if (!font) return 0;

    uint32_t prev_handle = dc->prev_font_handle;
    dc->font = font;
    dc->prev_font_handle = hfont;

    return prev_handle;
}

uint32_t gdi_select_bitmap(gdi_handle_table_t *table, uint32_t hdc, uint32_t hbitmap)
{
    gdi_dc_t *dc = gdi_get_object(table, hdc, GDI_OBJ_DC);
    if (!dc) return 0;

    /* Only memory DCs can have bitmaps selected */
    if (dc->dc_type != DCTYPE_MEMORY) return 0;

    gdi_bitmap_t *bitmap = gdi_get_object(table, hbitmap, GDI_OBJ_BITMAP);
    if (!bitmap) return 0;

    /* Check if bitmap is already selected into another DC */
    if (bitmap->hdc != 0 && bitmap->hdc != hdc) {
        return 0;  /* Cannot select bitmap into multiple DCs */
    }

    /* Get previous bitmap handle */
    uint32_t prev_handle = dc->prev_bitmap_handle;

    /* Deselect previous bitmap */
    if (dc->bitmap) {
        dc->bitmap->hdc = 0;
    }

    /* Select new bitmap */
    dc->bitmap = bitmap;
    dc->prev_bitmap_handle = hbitmap;
    bitmap->hdc = hdc;

    /* Update DC surface to bitmap */
    dc->pixels = bitmap->pixels;
    dc->width = bitmap->width;
    dc->height = bitmap->height;
    dc->pitch = bitmap->pitch;
    dc->bits_per_pixel = bitmap->bits_per_pixel;

    return prev_handle;
}

uint32_t gdi_select_palette(gdi_handle_table_t *table, uint32_t hdc, uint32_t hpalette, bool force_background)
{
    (void)force_background;

    gdi_dc_t *dc = gdi_get_object(table, hdc, GDI_OBJ_DC);
    if (!dc) return 0;

    gdi_palette_t *palette = gdi_get_object(table, hpalette, GDI_OBJ_PALETTE);
    if (!palette) return 0;

    /* For now, just return the default palette handle */
    return table->stock_handles[GDI_STOCK_DEFAULT_PALETTE];
}

/* Generic select object */
uint32_t gdi_select_object(gdi_handle_table_t *table, uint32_t hdc, uint32_t hobject)
{
    uint8_t type;
    void *obj = gdi_get_object_any(table, hobject, &type);
    if (!obj) return 0;

    switch (type) {
        case GDI_OBJ_BRUSH:
            return gdi_select_brush(table, hdc, hobject);
        case GDI_OBJ_PEN:
            return gdi_select_pen(table, hdc, hobject);
        case GDI_OBJ_FONT:
            return gdi_select_font(table, hdc, hobject);
        case GDI_OBJ_BITMAP:
            return gdi_select_bitmap(table, hdc, hobject);
        case GDI_OBJ_PALETTE:
            return gdi_select_palette(table, hdc, hobject, false);
        default:
            return 0;
    }
}

/*
 * DC attributes
 */

COLORREF gdi_set_text_color(gdi_dc_t *dc, COLORREF color)
{
    COLORREF prev = dc->text_color;
    dc->text_color = color & 0x00FFFFFF;  /* Mask out alpha */
    return prev;
}

COLORREF gdi_get_text_color(gdi_dc_t *dc)
{
    return dc->text_color;
}

COLORREF gdi_set_bk_color(gdi_dc_t *dc, COLORREF color)
{
    COLORREF prev = dc->bk_color;
    dc->bk_color = color & 0x00FFFFFF;
    return prev;
}

COLORREF gdi_get_bk_color(gdi_dc_t *dc)
{
    return dc->bk_color;
}

int gdi_set_bk_mode(gdi_dc_t *dc, int mode)
{
    int prev = dc->bk_mode;
    dc->bk_mode = mode;
    return prev;
}

int gdi_get_bk_mode(gdi_dc_t *dc)
{
    return dc->bk_mode;
}

int gdi_set_rop2(gdi_dc_t *dc, int rop2)
{
    int prev = dc->rop2;
    dc->rop2 = rop2;
    return prev;
}

int gdi_get_rop2(gdi_dc_t *dc)
{
    return dc->rop2;
}

int gdi_set_map_mode(gdi_dc_t *dc, int mode)
{
    int prev = dc->map_mode;
    dc->map_mode = mode;
    return prev;
}

int gdi_get_map_mode(gdi_dc_t *dc)
{
    return dc->map_mode;
}

int gdi_set_text_align(gdi_dc_t *dc, int align)
{
    int prev = dc->text_align;
    dc->text_align = align;
    return prev;
}

int gdi_get_text_align(gdi_dc_t *dc)
{
    return dc->text_align;
}

bool gdi_set_brush_org(gdi_dc_t *dc, int x, int y, POINT *prev)
{
    if (prev) {
        prev->x = dc->brush_org_x;
        prev->y = dc->brush_org_y;
    }
    dc->brush_org_x = x;
    dc->brush_org_y = y;
    return true;
}

bool gdi_get_brush_org(gdi_dc_t *dc, POINT *point)
{
    if (!point) return false;
    point->x = dc->brush_org_x;
    point->y = dc->brush_org_y;
    return true;
}

/*
 * Viewport and window
 */

bool gdi_set_viewport_org(gdi_dc_t *dc, int x, int y, POINT *prev)
{
    if (prev) {
        prev->x = dc->vp_org_x;
        prev->y = dc->vp_org_y;
    }
    dc->vp_org_x = x;
    dc->vp_org_y = y;
    return true;
}

bool gdi_get_viewport_org(gdi_dc_t *dc, POINT *point)
{
    if (!point) return false;
    point->x = dc->vp_org_x;
    point->y = dc->vp_org_y;
    return true;
}

bool gdi_set_window_org(gdi_dc_t *dc, int x, int y, POINT *prev)
{
    if (prev) {
        prev->x = dc->win_org_x;
        prev->y = dc->win_org_y;
    }
    dc->win_org_x = x;
    dc->win_org_y = y;
    return true;
}

bool gdi_get_window_org(gdi_dc_t *dc, POINT *point)
{
    if (!point) return false;
    point->x = dc->win_org_x;
    point->y = dc->win_org_y;
    return true;
}

bool gdi_offset_viewport_org(gdi_dc_t *dc, int x, int y, POINT *prev)
{
    if (prev) {
        prev->x = dc->vp_org_x;
        prev->y = dc->vp_org_y;
    }
    dc->vp_org_x += x;
    dc->vp_org_y += y;
    return true;
}

bool gdi_offset_window_org(gdi_dc_t *dc, int x, int y, POINT *prev)
{
    if (prev) {
        prev->x = dc->win_org_x;
        prev->y = dc->win_org_y;
    }
    dc->win_org_x += x;
    dc->win_org_y += y;
    return true;
}

/*
 * Current position
 */

bool gdi_move_to(gdi_dc_t *dc, int x, int y, POINT *prev)
{
    if (prev) {
        prev->x = dc->cur_x;
        prev->y = dc->cur_y;
    }
    dc->cur_x = x;
    dc->cur_y = y;
    return true;
}

bool gdi_get_current_position(gdi_dc_t *dc, POINT *point)
{
    if (!point) return false;
    point->x = dc->cur_x;
    point->y = dc->cur_y;
    return true;
}

/*
 * DC state save/restore
 */

int gdi_save_dc(gdi_handle_table_t *table, uint32_t hdc)
{
    gdi_dc_t *dc = gdi_get_object(table, hdc, GDI_OBJ_DC);
    if (!dc) return 0;

    /* Allocate saved state */
    gdi_dc_t *saved = gdi_alloc_dc(table);
    if (!saved) return 0;

    /* Copy current state */
    *saved = *dc;
    saved->saved_dc = dc->saved_dc;

    /* Link to chain */
    dc->saved_dc = saved;
    dc->save_level++;

    return dc->save_level;
}

bool gdi_restore_dc(gdi_handle_table_t *table, uint32_t hdc, int level)
{
    gdi_dc_t *dc = gdi_get_object(table, hdc, GDI_OBJ_DC);
    if (!dc) return false;

    /* Negative level means relative to current */
    if (level < 0) {
        level = dc->save_level + level + 1;
    }

    if (level <= 0 || level > dc->save_level) {
        return false;
    }

    /* Pop states until we reach the target level */
    while (dc->save_level >= level && dc->saved_dc) {
        gdi_dc_t *saved = dc->saved_dc;

        /* Restore state (preserving some fields) */
        uint32_t handle = dc->handle;
        gdi_dc_t *next_saved = saved->saved_dc;
        int new_save_level = saved->save_level;

        /* Copy saved state back */
        *dc = *saved;
        dc->handle = handle;
        dc->saved_dc = next_saved;
        dc->save_level = new_save_level;

        /* Free the saved state */
        gdi_free_dc(table, saved);
    }

    return true;
}

/*
 * Device capabilities
 */

int gdi_get_device_caps(gdi_dc_t *dc, int cap_index)
{
    switch (cap_index) {
        case HORZRES:
            return dc->width;
        case VERTRES:
            return dc->height;
        case BITSPIXEL:
            return dc->bits_per_pixel;
        case PLANES:
            return CAPS_PLANES;
        case NUMCOLORS:
            return -1;  /* True color */
        case LOGPIXELSX:
            return CAPS_LOGPIXELSX;
        case LOGPIXELSY:
            return CAPS_LOGPIXELSY;
        default:
            return 0;
    }
}

/*
 * Coordinate transformation
 */

void gdi_lp_to_dp(gdi_dc_t *dc, POINT *points, int count)
{
    /* Simple MM_TEXT transformation */
    for (int i = 0; i < count; i++) {
        points[i].x = points[i].x - dc->win_org_x + dc->vp_org_x;
        points[i].y = points[i].y - dc->win_org_y + dc->vp_org_y;
    }
}

void gdi_dp_to_lp(gdi_dc_t *dc, POINT *points, int count)
{
    /* Simple MM_TEXT transformation */
    for (int i = 0; i < count; i++) {
        points[i].x = points[i].x - dc->vp_org_x + dc->win_org_x;
        points[i].y = points[i].y - dc->vp_org_y + dc->win_org_y;
    }
}
