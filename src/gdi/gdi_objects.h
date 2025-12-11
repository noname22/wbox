/*
 * WBOX GDI Object Definitions
 * Host-side structures for GDI objects
 */
#ifndef WBOX_GDI_OBJECTS_H
#define WBOX_GDI_OBJECTS_H

#include "gdi_types.h"
#include <stdint.h>
#include <stdbool.h>

/* GDI object type tags (matches Windows GDI handle encoding) */
#define GDI_OBJ_DC          0x01
#define GDI_OBJ_REGION      0x04
#define GDI_OBJ_BITMAP      0x05
#define GDI_OBJ_PALETTE     0x08
#define GDI_OBJ_FONT        0x0A
#define GDI_OBJ_BRUSH       0x10
#define GDI_OBJ_PEN         0x30

/* DC types */
#define DCTYPE_DIRECT       0   /* Screen/window DC */
#define DCTYPE_MEMORY       1   /* Memory DC (for bitmaps) */
#define DCTYPE_INFO         2   /* Information DC */

/* Forward declarations */
struct gdi_brush;
struct gdi_pen;
struct gdi_font;
struct gdi_bitmap;
struct gdi_region;
struct gdi_palette;

/*
 * Device Context - host-side representation
 */
typedef struct gdi_dc {
    uint32_t handle;            /* Handle value returned to guest */
    int dc_type;                /* DCTYPE_DIRECT, DCTYPE_MEMORY, DCTYPE_INFO */

    /* Surface info */
    int width;
    int height;
    uint32_t *pixels;           /* Points to display framebuffer or bitmap pixels */
    int pitch;                  /* Bytes per row */
    int bits_per_pixel;

    /* Current drawing position */
    int cur_x;
    int cur_y;

    /* DC attributes */
    COLORREF text_color;
    COLORREF bk_color;
    int bk_mode;                /* TRANSPARENT or OPAQUE */
    int map_mode;
    int text_align;
    int rop2;                   /* Binary raster operation */
    int stretch_mode;
    int poly_fill_mode;

    /* Viewport and window */
    int vp_org_x, vp_org_y;
    int vp_ext_x, vp_ext_y;
    int win_org_x, win_org_y;
    int win_ext_x, win_ext_y;

    /* Brush origin */
    int brush_org_x, brush_org_y;

    /* Selected objects (host pointers) */
    struct gdi_brush *brush;
    struct gdi_pen *pen;
    struct gdi_font *font;
    struct gdi_bitmap *bitmap;      /* For memory DCs */
    struct gdi_region *clip_region;
    struct gdi_palette *palette;

    /* Previous selected objects (for returning old object on select) */
    uint32_t prev_brush_handle;
    uint32_t prev_pen_handle;
    uint32_t prev_font_handle;
    uint32_t prev_bitmap_handle;

    /* Window association (for window DCs) */
    uint32_t hwnd;              /* Guest window handle */

    /* Save/restore stack */
    int save_level;
    struct gdi_dc *saved_dc;    /* Linked list of saved states */

    /* Flags */
    bool dirty;                 /* Needs display update */
    bool in_use;                /* Handle is allocated */
} gdi_dc_t;

/*
 * Brush object
 */
typedef struct gdi_brush {
    uint32_t handle;
    int style;                  /* BS_SOLID, BS_NULL, BS_HATCHED, etc. */
    COLORREF color;             /* Brush color (for solid brushes) */
    int hatch_style;            /* Hatch pattern (for BS_HATCHED) */
    struct gdi_bitmap *pattern; /* Pattern bitmap (for BS_PATTERN) */
    bool in_use;
} gdi_brush_t;

/*
 * Pen object
 */
typedef struct gdi_pen {
    uint32_t handle;
    int style;                  /* PS_SOLID, PS_DASH, PS_NULL, etc. */
    int width;
    COLORREF color;
    bool in_use;
} gdi_pen_t;

/*
 * Font object - simplified for basic text rendering
 */
typedef struct gdi_font {
    uint32_t handle;
    int height;
    int width;
    int weight;                 /* FW_NORMAL, FW_BOLD, etc. */
    int escapement;
    int orientation;
    bool italic;
    bool underline;
    bool strikeout;
    int char_set;
    int pitch_and_family;
    char face_name[32];         /* LF_FACESIZE */
    bool in_use;
} gdi_font_t;

/*
 * Bitmap/Surface object
 */
typedef struct gdi_bitmap {
    uint32_t handle;
    int width;
    int height;
    int bits_per_pixel;
    int planes;
    uint32_t *pixels;           /* Host-allocated pixel buffer (ARGB8888) */
    int pitch;                  /* Bytes per row */

    /* DIB info (if created as DIB) */
    bool is_dib;
    void *dib_bits;             /* Guest pointer to DIB bits */

    /* DC association */
    uint32_t hdc;               /* DC this bitmap is selected into (0 if none) */

    bool in_use;
} gdi_bitmap_t;

/*
 * Region object - simplified to rectangle list
 */
typedef struct gdi_region {
    uint32_t handle;
    RECT bounds;                /* Bounding rectangle */

    /* For complex regions, could add rectangle list */
    int rect_count;
    RECT *rects;                /* Array of rectangles (NULL for simple rect) */

    bool in_use;
} gdi_region_t;

/*
 * Palette object
 */
typedef struct gdi_palette {
    uint32_t handle;
    int entry_count;
    uint32_t *entries;          /* PALETTEENTRY array */
    bool in_use;
} gdi_palette_t;

/*
 * Helper macros
 */

/* Convert COLORREF (0x00BBGGRR) to ARGB8888 (0xAARRGGBB) */
static inline uint32_t colorref_to_argb(COLORREF cr)
{
    return 0xFF000000 |                     /* Alpha = 255 (opaque) */
           ((cr & 0x0000FF) << 16) |        /* R: move from bits 0-7 to 16-23 */
           (cr & 0x00FF00) |                /* G: stays at bits 8-15 */
           ((cr & 0xFF0000) >> 16);         /* B: move from bits 16-23 to 0-7 */
}

/* Convert ARGB8888 to COLORREF */
static inline COLORREF argb_to_colorref(uint32_t argb)
{
    return ((argb & 0x00FF0000) >> 16) |    /* R: from bits 16-23 to 0-7 */
           (argb & 0x0000FF00) |            /* G: stays at bits 8-15 */
           ((argb & 0x000000FF) << 16);     /* B: from bits 0-7 to 16-23 */
}

/* Check if handle is a stock object */
#define GDI_IS_STOCK_HANDLE(h)  (((h) & 0x80000000) != 0)

/* Stock object handle flag */
#define GDI_STOCK_HANDLE_FLAG   0x80000000

#endif /* WBOX_GDI_OBJECTS_H */
