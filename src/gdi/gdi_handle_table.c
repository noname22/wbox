/*
 * WBOX GDI Handle Table Implementation
 */
#include "gdi_handle_table.h"
#include "../process/process.h"  /* For WBOX_PROCESS_ID */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Update shared table entry when a handle is allocated */
static void update_shared_entry(gdi_handle_table_t *table, int index, uint8_t type, uint16_t reuse)
{
    if (table->shared_table == NULL || index >= GDI_MAX_SHARED_HANDLES) {
        return;
    }
    gdi_shared_handle_entry_t *entry = &table->shared_table[index];
    entry->pKernelAddress = 0;      /* Not used by guest */
    entry->wProcessId = WBOX_PROCESS_ID;
    entry->wCount = 1;              /* Reference count */
    entry->wUpper = reuse & 0x7F;   /* Upper bits of handle */
    entry->wType = type;            /* Object type */
    entry->pUserAddress = 0;        /* Not used by guest */
}

/* Clear shared table entry when a handle is freed */
static void clear_shared_entry(gdi_handle_table_t *table, int index)
{
    if (table->shared_table == NULL || index >= GDI_MAX_SHARED_HANDLES) {
        return;
    }
    memset(&table->shared_table[index], 0, sizeof(gdi_shared_handle_entry_t));
}

/* Object pool sizes */
#define DC_POOL_SIZE        64
#define BRUSH_POOL_SIZE     256
#define PEN_POOL_SIZE       128
#define FONT_POOL_SIZE      64
#define BITMAP_POOL_SIZE    128
#define REGION_POOL_SIZE    128

/* Initialize stock brushes */
static void init_stock_brushes(gdi_handle_table_t *table)
{
    /* WHITE_BRUSH */
    table->stock_brushes[0].style = BS_SOLID;
    table->stock_brushes[0].color = RGB(255, 255, 255);
    table->stock_brushes[0].in_use = true;

    /* LTGRAY_BRUSH */
    table->stock_brushes[1].style = BS_SOLID;
    table->stock_brushes[1].color = RGB(192, 192, 192);
    table->stock_brushes[1].in_use = true;

    /* GRAY_BRUSH */
    table->stock_brushes[2].style = BS_SOLID;
    table->stock_brushes[2].color = RGB(128, 128, 128);
    table->stock_brushes[2].in_use = true;

    /* DKGRAY_BRUSH */
    table->stock_brushes[3].style = BS_SOLID;
    table->stock_brushes[3].color = RGB(64, 64, 64);
    table->stock_brushes[3].in_use = true;

    /* BLACK_BRUSH */
    table->stock_brushes[4].style = BS_SOLID;
    table->stock_brushes[4].color = RGB(0, 0, 0);
    table->stock_brushes[4].in_use = true;

    /* NULL_BRUSH (HOLLOW_BRUSH) */
    table->stock_brushes[5].style = BS_NULL;
    table->stock_brushes[5].color = 0;
    table->stock_brushes[5].in_use = true;
}

/* Initialize stock pens */
static void init_stock_pens(gdi_handle_table_t *table)
{
    /* WHITE_PEN */
    table->stock_pens[0].style = PS_SOLID;
    table->stock_pens[0].width = 1;
    table->stock_pens[0].color = RGB(255, 255, 255);
    table->stock_pens[0].in_use = true;

    /* BLACK_PEN */
    table->stock_pens[1].style = PS_SOLID;
    table->stock_pens[1].width = 1;
    table->stock_pens[1].color = RGB(0, 0, 0);
    table->stock_pens[1].in_use = true;

    /* NULL_PEN */
    table->stock_pens[2].style = PS_NULL;
    table->stock_pens[2].width = 0;
    table->stock_pens[2].color = 0;
    table->stock_pens[2].in_use = true;
}

/* Initialize stock fonts */
static void init_stock_fonts(gdi_handle_table_t *table)
{
    /* Common font settings */
    for (int i = 0; i < 8; i++) {
        table->stock_fonts[i].height = 16;
        table->stock_fonts[i].width = 8;
        table->stock_fonts[i].weight = 400;  /* FW_NORMAL */
        table->stock_fonts[i].char_set = 0;  /* ANSI_CHARSET */
        table->stock_fonts[i].italic = false;
        table->stock_fonts[i].underline = false;
        table->stock_fonts[i].strikeout = false;
        table->stock_fonts[i].in_use = true;
    }

    /* OEM_FIXED_FONT (index 0) */
    strncpy(table->stock_fonts[0].face_name, "Terminal", 31);
    table->stock_fonts[0].pitch_and_family = 0x31;  /* FIXED_PITCH | FF_MODERN */

    /* ANSI_FIXED_FONT (index 1) */
    strncpy(table->stock_fonts[1].face_name, "Courier", 31);
    table->stock_fonts[1].pitch_and_family = 0x31;

    /* ANSI_VAR_FONT (index 2) */
    strncpy(table->stock_fonts[2].face_name, "MS Sans Serif", 31);
    table->stock_fonts[2].pitch_and_family = 0x22;  /* VARIABLE_PITCH | FF_SWISS */

    /* SYSTEM_FONT (index 3) */
    strncpy(table->stock_fonts[3].face_name, "System", 31);
    table->stock_fonts[3].weight = 700;  /* FW_BOLD */
    table->stock_fonts[3].pitch_and_family = 0x22;

    /* DEVICE_DEFAULT_FONT (index 4) */
    strncpy(table->stock_fonts[4].face_name, "System", 31);
    table->stock_fonts[4].pitch_and_family = 0x22;

    /* SYSTEM_FIXED_FONT (index 5) */
    strncpy(table->stock_fonts[5].face_name, "Fixedsys", 31);
    table->stock_fonts[5].pitch_and_family = 0x31;

    /* DEFAULT_GUI_FONT (index 6) */
    strncpy(table->stock_fonts[6].face_name, "MS Shell Dlg", 31);
    table->stock_fonts[6].height = 13;
    table->stock_fonts[6].pitch_and_family = 0x22;

    /* Extra slot (index 7) - unused */
    table->stock_fonts[7].in_use = false;
}

/* Initialize default palette */
static void init_stock_palette(gdi_handle_table_t *table)
{
    table->stock_palette.entry_count = 0;
    table->stock_palette.entries = NULL;
    table->stock_palette.in_use = true;
}

/* Create stock object handles */
static void init_stock_handles(gdi_handle_table_t *table)
{
    /* Brushes: indices 0-5 */
    for (int i = 0; i <= 5; i++) {
        table->stock_handles[i] = GDI_HANDLE_STOCK_FLAG |
                                   (GDI_OBJ_BRUSH << GDI_HANDLE_TYPE_SHIFT) |
                                   i;
        table->stock_brushes[i].handle = table->stock_handles[i];
    }

    /* Pens: indices 6-8 */
    table->stock_handles[6] = GDI_HANDLE_STOCK_FLAG |
                               (GDI_OBJ_PEN << GDI_HANDLE_TYPE_SHIFT) |
                               0;  /* WHITE_PEN */
    table->stock_pens[0].handle = table->stock_handles[6];

    table->stock_handles[7] = GDI_HANDLE_STOCK_FLAG |
                               (GDI_OBJ_PEN << GDI_HANDLE_TYPE_SHIFT) |
                               1;  /* BLACK_PEN */
    table->stock_pens[1].handle = table->stock_handles[7];

    table->stock_handles[8] = GDI_HANDLE_STOCK_FLAG |
                               (GDI_OBJ_PEN << GDI_HANDLE_TYPE_SHIFT) |
                               2;  /* NULL_PEN */
    table->stock_pens[2].handle = table->stock_handles[8];

    /* Fonts: indices 10-17 */
    int font_indices[] = { 10, 11, 12, 13, 14, 15, 16, 17 };
    for (int i = 0; i < 8; i++) {
        int stock_idx = font_indices[i];
        if (stock_idx < GDI_STOCK_COUNT) {
            table->stock_handles[stock_idx] = GDI_HANDLE_STOCK_FLAG |
                                               (GDI_OBJ_FONT << GDI_HANDLE_TYPE_SHIFT) |
                                               i;
            table->stock_fonts[i].handle = table->stock_handles[stock_idx];
        }
    }

    /* Default palette: index 15 */
    table->stock_handles[15] = GDI_HANDLE_STOCK_FLAG |
                                (GDI_OBJ_PALETTE << GDI_HANDLE_TYPE_SHIFT) |
                                0;
    table->stock_palette.handle = table->stock_handles[15];

    /* DC_BRUSH and DC_PEN: indices 18-19 */
    table->stock_handles[18] = GDI_HANDLE_STOCK_FLAG |
                                (GDI_OBJ_BRUSH << GDI_HANDLE_TYPE_SHIFT) |
                                18;
    table->stock_handles[19] = GDI_HANDLE_STOCK_FLAG |
                                (GDI_OBJ_PEN << GDI_HANDLE_TYPE_SHIFT) |
                                19;

    table->dc_brush_color = RGB(255, 255, 255);  /* Default white */
    table->dc_pen_color = RGB(0, 0, 0);          /* Default black */
}

/* Allocate object pools */
static int init_object_pools(gdi_handle_table_t *table)
{
    table->dc_pool = calloc(DC_POOL_SIZE, sizeof(gdi_dc_t));
    if (!table->dc_pool) return -1;
    table->dc_pool_size = DC_POOL_SIZE;

    table->brush_pool = calloc(BRUSH_POOL_SIZE, sizeof(gdi_brush_t));
    if (!table->brush_pool) return -1;
    table->brush_pool_size = BRUSH_POOL_SIZE;

    table->pen_pool = calloc(PEN_POOL_SIZE, sizeof(gdi_pen_t));
    if (!table->pen_pool) return -1;
    table->pen_pool_size = PEN_POOL_SIZE;

    table->font_pool = calloc(FONT_POOL_SIZE, sizeof(gdi_font_t));
    if (!table->font_pool) return -1;
    table->font_pool_size = FONT_POOL_SIZE;

    table->bitmap_pool = calloc(BITMAP_POOL_SIZE, sizeof(gdi_bitmap_t));
    if (!table->bitmap_pool) return -1;
    table->bitmap_pool_size = BITMAP_POOL_SIZE;

    table->region_pool = calloc(REGION_POOL_SIZE, sizeof(gdi_region_t));
    if (!table->region_pool) return -1;
    table->region_pool_size = REGION_POOL_SIZE;

    return 0;
}

/* Initialize handle table */
int gdi_handle_table_init(gdi_handle_table_t *table)
{
    memset(table, 0, sizeof(*table));

    /* Initialize handle entries */
    for (int i = 0; i < GDI_MAX_HANDLES; i++) {
        table->entries[i].object = NULL;
        table->entries[i].type = 0;
        table->entries[i].flags = 0;
        table->entries[i].reuse_count = 0;
        table->entries[i].in_use = false;
    }

    /* Reserve index 0 (NULL handle) */
    table->entries[0].in_use = true;
    table->next_free = 1;
    table->handle_count = 1;

    /* Initialize stock objects */
    init_stock_brushes(table);
    init_stock_pens(table);
    init_stock_fonts(table);
    init_stock_palette(table);
    init_stock_handles(table);

    /* Allocate object pools */
    if (init_object_pools(table) < 0) {
        gdi_handle_table_shutdown(table);
        return -1;
    }

    printf("GDI handle table initialized (%d handles, %d stock objects)\n",
           GDI_MAX_HANDLES, GDI_STOCK_COUNT);
    return 0;
}

/* Shutdown handle table */
void gdi_handle_table_shutdown(gdi_handle_table_t *table)
{
    /* Free object pools */
    free(table->dc_pool);
    free(table->brush_pool);
    free(table->pen_pool);
    free(table->font_pool);
    free(table->bitmap_pool);
    free(table->region_pool);

    /* Free any dynamically allocated bitmap pixels */
    for (int i = 0; i < GDI_MAX_HANDLES; i++) {
        if (table->entries[i].in_use && table->entries[i].type == GDI_OBJ_BITMAP) {
            gdi_bitmap_t *bmp = table->entries[i].object;
            if (bmp && bmp->pixels) {
                free(bmp->pixels);
            }
        }
    }

    /* Free stock palette entries if allocated */
    free(table->stock_palette.entries);

    memset(table, 0, sizeof(*table));
}

/* Allocate a handle */
uint32_t gdi_alloc_handle(gdi_handle_table_t *table, void *object, uint8_t type)
{
    /* Find free slot */
    int index = -1;
    for (int i = table->next_free; i < GDI_MAX_HANDLES; i++) {
        if (!table->entries[i].in_use) {
            index = i;
            break;
        }
    }
    if (index < 0) {
        /* Wrap around and search from beginning */
        for (int i = 1; i < table->next_free; i++) {
            if (!table->entries[i].in_use) {
                index = i;
                break;
            }
        }
    }

    if (index < 0) {
        fprintf(stderr, "GDI: Handle table exhausted\n");
        return 0;
    }

    /* Fill entry */
    gdi_handle_entry_t *entry = &table->entries[index];
    entry->object = object;
    entry->type = type;
    entry->flags = 0;
    entry->reuse_count++;
    entry->in_use = true;

    /* Update hint for next allocation */
    table->next_free = index + 1;
    if (table->next_free >= GDI_MAX_HANDLES) {
        table->next_free = 1;
    }
    table->handle_count++;

    /* Update shared table entry for guest */
    update_shared_entry(table, index, type, entry->reuse_count);

    /* Build handle value */
    return GDI_MAKE_HANDLE(index, type, entry->reuse_count);
}

/* Get object from handle */
void *gdi_get_object(gdi_handle_table_t *table, uint32_t handle, uint8_t expected_type)
{
    if (handle == 0) return NULL;

    /* Check for stock object */
    if (GDI_HANDLE_IS_STOCK(handle)) {
        uint8_t type = GDI_HANDLE_TYPE(handle);
        int index = handle & 0xFF;  /* Stock index in low byte */

        if (type != expected_type) return NULL;

        switch (type) {
            case GDI_OBJ_BRUSH:
                if (index < 6) return &table->stock_brushes[index];
                if (index == 18) {
                    /* DC_BRUSH - return a special brush with dc_brush_color */
                    static gdi_brush_t dc_brush;
                    dc_brush.style = BS_SOLID;
                    dc_brush.color = table->dc_brush_color;
                    dc_brush.handle = handle;
                    return &dc_brush;
                }
                break;
            case GDI_OBJ_PEN:
                if (index < 3) return &table->stock_pens[index];
                if (index == 19) {
                    static gdi_pen_t dc_pen;
                    dc_pen.style = PS_SOLID;
                    dc_pen.width = 1;
                    dc_pen.color = table->dc_pen_color;
                    dc_pen.handle = handle;
                    return &dc_pen;
                }
                break;
            case GDI_OBJ_FONT:
                if (index < 8) return &table->stock_fonts[index];
                break;
            case GDI_OBJ_PALETTE:
                if (index == 0) return &table->stock_palette;
                break;
        }
        return NULL;
    }

    /* Regular handle */
    int index = GDI_HANDLE_INDEX(handle);
    if (index >= GDI_MAX_HANDLES) return NULL;

    gdi_handle_entry_t *entry = &table->entries[index];
    if (!entry->in_use) return NULL;
    if (entry->type != expected_type) return NULL;

    /* Validate reuse count */
    uint16_t reuse = (handle >> GDI_HANDLE_REUSE_SHIFT) & 0x7F;
    if ((entry->reuse_count & 0x7F) != reuse) return NULL;

    return entry->object;
}

/* Get object from handle (any type) */
void *gdi_get_object_any(gdi_handle_table_t *table, uint32_t handle, uint8_t *out_type)
{
    if (handle == 0) return NULL;

    /* Check for stock object */
    if (GDI_HANDLE_IS_STOCK(handle)) {
        uint8_t type = GDI_HANDLE_TYPE(handle);
        if (out_type) *out_type = type;
        return gdi_get_object(table, handle, type);
    }

    /* Regular handle */
    int index = GDI_HANDLE_INDEX(handle);
    if (index >= GDI_MAX_HANDLES) return NULL;

    gdi_handle_entry_t *entry = &table->entries[index];
    if (!entry->in_use) return NULL;

    if (out_type) *out_type = entry->type;
    return entry->object;
}

/* Free a handle */
bool gdi_free_handle(gdi_handle_table_t *table, uint32_t handle)
{
    if (handle == 0) return false;

    /* Cannot delete stock objects */
    if (GDI_HANDLE_IS_STOCK(handle)) {
        return false;
    }

    int index = GDI_HANDLE_INDEX(handle);
    if (index >= GDI_MAX_HANDLES) return false;

    gdi_handle_entry_t *entry = &table->entries[index];
    if (!entry->in_use) return false;

    /* Validate type matches handle */
    uint8_t type = GDI_HANDLE_TYPE(handle);
    if (entry->type != type) return false;

    /* Mark as free */
    entry->in_use = false;
    entry->object = NULL;
    table->handle_count--;

    /* Clear shared table entry */
    clear_shared_entry(table, index);

    /* Update free hint */
    if (index < table->next_free) {
        table->next_free = index;
    }

    return true;
}

/* Get stock object handle */
uint32_t gdi_get_stock_object(gdi_handle_table_t *table, int index)
{
    if (index < 0 || index >= GDI_STOCK_COUNT) {
        return 0;
    }
    return table->stock_handles[index];
}

/* Check if handle is valid */
bool gdi_handle_is_valid(gdi_handle_table_t *table, uint32_t handle)
{
    if (handle == 0) return false;

    if (GDI_HANDLE_IS_STOCK(handle)) {
        return true;  /* Stock handles are always valid */
    }

    int index = GDI_HANDLE_INDEX(handle);
    if (index >= GDI_MAX_HANDLES) return false;

    return table->entries[index].in_use;
}

/* Get object type from handle */
uint8_t gdi_handle_get_type(uint32_t handle)
{
    return GDI_HANDLE_TYPE(handle);
}

/*
 * Object allocation helpers
 */

gdi_dc_t *gdi_alloc_dc(gdi_handle_table_t *table)
{
    for (int i = 0; i < table->dc_pool_size; i++) {
        if (!table->dc_pool[i].in_use) {
            memset(&table->dc_pool[i], 0, sizeof(gdi_dc_t));
            table->dc_pool[i].in_use = true;
            return &table->dc_pool[i];
        }
    }
    /* Pool exhausted, allocate dynamically */
    gdi_dc_t *dc = calloc(1, sizeof(gdi_dc_t));
    if (dc) dc->in_use = true;
    return dc;
}

void gdi_free_dc(gdi_handle_table_t *table, gdi_dc_t *dc)
{
    if (!dc) return;

    /* Check if it's from the pool */
    if (dc >= table->dc_pool && dc < table->dc_pool + table->dc_pool_size) {
        dc->in_use = false;
    } else {
        free(dc);
    }
}

gdi_brush_t *gdi_alloc_brush(gdi_handle_table_t *table)
{
    for (int i = 0; i < table->brush_pool_size; i++) {
        if (!table->brush_pool[i].in_use) {
            memset(&table->brush_pool[i], 0, sizeof(gdi_brush_t));
            table->brush_pool[i].in_use = true;
            return &table->brush_pool[i];
        }
    }
    gdi_brush_t *brush = calloc(1, sizeof(gdi_brush_t));
    if (brush) brush->in_use = true;
    return brush;
}

void gdi_free_brush(gdi_handle_table_t *table, gdi_brush_t *brush)
{
    if (!brush) return;
    if (brush >= table->brush_pool && brush < table->brush_pool + table->brush_pool_size) {
        brush->in_use = false;
    } else {
        free(brush);
    }
}

gdi_pen_t *gdi_alloc_pen(gdi_handle_table_t *table)
{
    for (int i = 0; i < table->pen_pool_size; i++) {
        if (!table->pen_pool[i].in_use) {
            memset(&table->pen_pool[i], 0, sizeof(gdi_pen_t));
            table->pen_pool[i].in_use = true;
            return &table->pen_pool[i];
        }
    }
    gdi_pen_t *pen = calloc(1, sizeof(gdi_pen_t));
    if (pen) pen->in_use = true;
    return pen;
}

void gdi_free_pen(gdi_handle_table_t *table, gdi_pen_t *pen)
{
    if (!pen) return;
    if (pen >= table->pen_pool && pen < table->pen_pool + table->pen_pool_size) {
        pen->in_use = false;
    } else {
        free(pen);
    }
}

gdi_font_t *gdi_alloc_font(gdi_handle_table_t *table)
{
    for (int i = 0; i < table->font_pool_size; i++) {
        if (!table->font_pool[i].in_use) {
            memset(&table->font_pool[i], 0, sizeof(gdi_font_t));
            table->font_pool[i].in_use = true;
            return &table->font_pool[i];
        }
    }
    gdi_font_t *font = calloc(1, sizeof(gdi_font_t));
    if (font) font->in_use = true;
    return font;
}

void gdi_free_font(gdi_handle_table_t *table, gdi_font_t *font)
{
    if (!font) return;
    if (font >= table->font_pool && font < table->font_pool + table->font_pool_size) {
        font->in_use = false;
    } else {
        free(font);
    }
}

gdi_bitmap_t *gdi_alloc_bitmap(gdi_handle_table_t *table)
{
    for (int i = 0; i < table->bitmap_pool_size; i++) {
        if (!table->bitmap_pool[i].in_use) {
            memset(&table->bitmap_pool[i], 0, sizeof(gdi_bitmap_t));
            table->bitmap_pool[i].in_use = true;
            return &table->bitmap_pool[i];
        }
    }
    gdi_bitmap_t *bmp = calloc(1, sizeof(gdi_bitmap_t));
    if (bmp) bmp->in_use = true;
    return bmp;
}

void gdi_free_bitmap(gdi_handle_table_t *table, gdi_bitmap_t *bitmap)
{
    if (!bitmap) return;

    /* Free pixel buffer */
    free(bitmap->pixels);
    bitmap->pixels = NULL;

    if (bitmap >= table->bitmap_pool && bitmap < table->bitmap_pool + table->bitmap_pool_size) {
        bitmap->in_use = false;
    } else {
        free(bitmap);
    }
}

gdi_region_t *gdi_alloc_region(gdi_handle_table_t *table)
{
    for (int i = 0; i < table->region_pool_size; i++) {
        if (!table->region_pool[i].in_use) {
            memset(&table->region_pool[i], 0, sizeof(gdi_region_t));
            table->region_pool[i].in_use = true;
            return &table->region_pool[i];
        }
    }
    gdi_region_t *rgn = calloc(1, sizeof(gdi_region_t));
    if (rgn) rgn->in_use = true;
    return rgn;
}

void gdi_free_region(gdi_handle_table_t *table, gdi_region_t *region)
{
    if (!region) return;

    /* Free rectangle list if allocated */
    free(region->rects);
    region->rects = NULL;

    if (region >= table->region_pool && region < table->region_pool + table->region_pool_size) {
        region->in_use = false;
    } else {
        free(region);
    }
}

/* Set guest-mapped shared table pointer */
void gdi_set_shared_table(gdi_handle_table_t *table, void *host_ptr, uint32_t guest_addr)
{
    table->shared_table = (gdi_shared_handle_entry_t *)host_ptr;
    table->shared_table_guest_addr = guest_addr;

    if (host_ptr) {
        /* Clear the entire shared table */
        memset(host_ptr, 0, GDI_SHARED_TABLE_SIZE);
        printf("GDI: Shared table set at guest 0x%08X (host %p)\n", guest_addr, host_ptr);
    }
}
