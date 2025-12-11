/*
 * WBOX GDI Handle Table
 * Manages GDI object handles on the host
 */
#ifndef WBOX_GDI_HANDLE_TABLE_H
#define WBOX_GDI_HANDLE_TABLE_H

#include "gdi_objects.h"
#include <stdint.h>
#include <stdbool.h>

/* Handle table configuration */
#define GDI_MAX_HANDLES         4096
#define GDI_HANDLE_INDEX_MASK   0x0000FFFF
#define GDI_HANDLE_TYPE_SHIFT   16
#define GDI_HANDLE_TYPE_MASK    0x007F0000
#define GDI_HANDLE_STOCK_FLAG   0x80000000
#define GDI_HANDLE_REUSE_SHIFT  24
#define GDI_HANDLE_REUSE_MASK   0x7F000000

/* Stock object indices (used with GetStockObject) */
#define GDI_STOCK_WHITE_BRUSH       0
#define GDI_STOCK_LTGRAY_BRUSH      1
#define GDI_STOCK_GRAY_BRUSH        2
#define GDI_STOCK_DKGRAY_BRUSH      3
#define GDI_STOCK_BLACK_BRUSH       4
#define GDI_STOCK_NULL_BRUSH        5
#define GDI_STOCK_WHITE_PEN         6
#define GDI_STOCK_BLACK_PEN         7
#define GDI_STOCK_NULL_PEN          8
#define GDI_STOCK_OEM_FIXED_FONT    10
#define GDI_STOCK_ANSI_FIXED_FONT   11
#define GDI_STOCK_ANSI_VAR_FONT     12
#define GDI_STOCK_SYSTEM_FONT       13
#define GDI_STOCK_DEVICE_DEFAULT_FONT 14
#define GDI_STOCK_DEFAULT_PALETTE   15
#define GDI_STOCK_SYSTEM_FIXED_FONT 16
#define GDI_STOCK_DEFAULT_GUI_FONT  17
#define GDI_STOCK_DC_BRUSH          18
#define GDI_STOCK_DC_PEN            19
#define GDI_STOCK_COUNT             20

/* Handle table entry */
typedef struct gdi_handle_entry {
    void *object;               /* Pointer to host object */
    uint8_t type;               /* Object type (GDI_OBJ_*) */
    uint8_t flags;              /* Entry flags */
    uint16_t reuse_count;       /* For handle validation */
    bool in_use;
} gdi_handle_entry_t;

/* Handle entry flags */
#define GDI_ENTRY_STOCK     0x01    /* Stock object, cannot be deleted */

/* Handle table state */
typedef struct gdi_handle_table {
    gdi_handle_entry_t entries[GDI_MAX_HANDLES];
    int next_free;              /* Next free index hint */
    int handle_count;           /* Number of allocated handles */

    /* Stock objects storage */
    gdi_brush_t stock_brushes[6];   /* WHITE, LTGRAY, GRAY, DKGRAY, BLACK, NULL */
    gdi_pen_t stock_pens[3];        /* WHITE, BLACK, NULL */
    gdi_font_t stock_fonts[8];      /* Various stock fonts */
    gdi_palette_t stock_palette;    /* Default palette */

    /* DC_BRUSH and DC_PEN colors (per-DC, but stored here for simplicity) */
    COLORREF dc_brush_color;
    COLORREF dc_pen_color;

    /* Stock object handles (cached) */
    uint32_t stock_handles[GDI_STOCK_COUNT];

    /* Object pools (to avoid frequent malloc) */
    gdi_dc_t *dc_pool;
    int dc_pool_size;
    gdi_brush_t *brush_pool;
    int brush_pool_size;
    gdi_pen_t *pen_pool;
    int pen_pool_size;
    gdi_font_t *font_pool;
    int font_pool_size;
    gdi_bitmap_t *bitmap_pool;
    int bitmap_pool_size;
    gdi_region_t *region_pool;
    int region_pool_size;
} gdi_handle_table_t;

/*
 * Handle table API
 */

/* Initialize handle table and create stock objects */
int gdi_handle_table_init(gdi_handle_table_t *table);

/* Shutdown and free all resources */
void gdi_handle_table_shutdown(gdi_handle_table_t *table);

/* Allocate a handle for an object */
uint32_t gdi_alloc_handle(gdi_handle_table_t *table, void *object, uint8_t type);

/* Get object pointer from handle (returns NULL if invalid or wrong type) */
void *gdi_get_object(gdi_handle_table_t *table, uint32_t handle, uint8_t expected_type);

/* Get object pointer from handle (any type) */
void *gdi_get_object_any(gdi_handle_table_t *table, uint32_t handle, uint8_t *out_type);

/* Free a handle (returns false if stock object or invalid) */
bool gdi_free_handle(gdi_handle_table_t *table, uint32_t handle);

/* Get stock object handle */
uint32_t gdi_get_stock_object(gdi_handle_table_t *table, int index);

/* Check if handle is valid */
bool gdi_handle_is_valid(gdi_handle_table_t *table, uint32_t handle);

/* Get object type from handle */
uint8_t gdi_handle_get_type(uint32_t handle);

/*
 * Object allocation helpers
 */

/* Allocate a new DC */
gdi_dc_t *gdi_alloc_dc(gdi_handle_table_t *table);

/* Free a DC */
void gdi_free_dc(gdi_handle_table_t *table, gdi_dc_t *dc);

/* Allocate a new brush */
gdi_brush_t *gdi_alloc_brush(gdi_handle_table_t *table);

/* Free a brush */
void gdi_free_brush(gdi_handle_table_t *table, gdi_brush_t *brush);

/* Allocate a new pen */
gdi_pen_t *gdi_alloc_pen(gdi_handle_table_t *table);

/* Free a pen */
void gdi_free_pen(gdi_handle_table_t *table, gdi_pen_t *pen);

/* Allocate a new font */
gdi_font_t *gdi_alloc_font(gdi_handle_table_t *table);

/* Free a font */
void gdi_free_font(gdi_handle_table_t *table, gdi_font_t *font);

/* Allocate a new bitmap */
gdi_bitmap_t *gdi_alloc_bitmap(gdi_handle_table_t *table);

/* Free a bitmap */
void gdi_free_bitmap(gdi_handle_table_t *table, gdi_bitmap_t *bitmap);

/* Allocate a new region */
gdi_region_t *gdi_alloc_region(gdi_handle_table_t *table);

/* Free a region */
void gdi_free_region(gdi_handle_table_t *table, gdi_region_t *region);

/*
 * Handle manipulation macros
 */

/* Build a handle from components */
#define GDI_MAKE_HANDLE(index, type, reuse) \
    (((uint32_t)(index) & GDI_HANDLE_INDEX_MASK) | \
     (((uint32_t)(type) << GDI_HANDLE_TYPE_SHIFT) & GDI_HANDLE_TYPE_MASK) | \
     (((uint32_t)(reuse) << GDI_HANDLE_REUSE_SHIFT) & GDI_HANDLE_REUSE_MASK))

/* Extract index from handle */
#define GDI_HANDLE_INDEX(h)  ((h) & GDI_HANDLE_INDEX_MASK)

/* Extract type from handle */
#define GDI_HANDLE_TYPE(h)   (((h) & GDI_HANDLE_TYPE_MASK) >> GDI_HANDLE_TYPE_SHIFT)

/* Check if handle is stock object */
#define GDI_HANDLE_IS_STOCK(h) (((h) & GDI_HANDLE_STOCK_FLAG) != 0)

#endif /* WBOX_GDI_HANDLE_TABLE_H */
