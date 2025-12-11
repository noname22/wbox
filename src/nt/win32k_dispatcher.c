/*
 * WBOX Win32k Syscall Dispatcher Implementation
 */
#include "win32k_dispatcher.h"
#include "win32k_syscalls.h"
#include "../cpu/cpu.h"
#include "../cpu/mem.h"
#include "../vm/vm.h"
#include "../vm/paging.h"
#include "../gdi/gdi_dc.h"
#include "../gdi/gdi_drawing.h"
#include "../gdi/gdi_text.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Global state */
static gdi_handle_table_t g_gdi_handles;
static display_context_t *g_display = NULL;
static bool g_initialized = false;

/* Helper to read guest memory */
static inline uint32_t read_stack_arg(int index)
{
    return readmemll(ESP + 4 + (index * 4));
}

/* Helper to read guest string (Unicode) */
static int read_guest_unicode(uint32_t guest_ptr, uint16_t *buf, int max_chars)
{
    if (!guest_ptr) return 0;

    vm_context_t *vm = vm_get_context();
    if (!vm) return 0;

    int count = 0;
    while (count < max_chars) {
        uint32_t phys = paging_get_phys(&vm->paging, guest_ptr + count * 2);
        if (!phys) break;

        uint16_t ch = mem_readw_phys(phys);
        buf[count] = ch;
        if (ch == 0) break;
        count++;
    }

    return count;
}

/* Helper to write guest memory */
static void write_guest_dword(uint32_t guest_ptr, uint32_t value)
{
    if (!guest_ptr) return;

    vm_context_t *vm = vm_get_context();
    if (!vm) return;

    uint32_t phys = paging_get_phys(&vm->paging, guest_ptr);
    if (phys) {
        mem_writel_phys(phys, value);
    }
}

/* Initialize win32k */
int win32k_init(display_context_t *display)
{
    if (g_initialized) return 0;

    if (gdi_handle_table_init(&g_gdi_handles) < 0) {
        fprintf(stderr, "win32k: Failed to initialize GDI handle table\n");
        return -1;
    }

    g_display = display;
    g_initialized = true;

    printf("Win32k subsystem initialized\n");
    return 0;
}

/* Shutdown win32k */
void win32k_shutdown(void)
{
    if (!g_initialized) return;

    gdi_handle_table_shutdown(&g_gdi_handles);
    g_display = NULL;
    g_initialized = false;
}

/* Get handle table */
gdi_handle_table_t *win32k_get_handle_table(void)
{
    return &g_gdi_handles;
}

/* Get display */
display_context_t *win32k_get_display(void)
{
    return g_display;
}

/*
 * GDI Syscall Implementations
 */

/* NtGdiGetStockObject */
ntstatus_t sys_NtGdiGetStockObject(void)
{
    int index = read_stack_arg(0);

    uint32_t handle = gdi_get_stock_object(&g_gdi_handles, index);

    EAX = handle;
    return STATUS_SUCCESS;
}

/* NtGdiCreateCompatibleDC */
ntstatus_t sys_NtGdiCreateCompatibleDC(void)
{
    uint32_t hdc_ref = read_stack_arg(0);

    uint32_t hdc = gdi_create_compatible_dc(&g_gdi_handles, hdc_ref);

    EAX = hdc;
    return STATUS_SUCCESS;
}

/* NtGdiDeleteObjectApp */
ntstatus_t sys_NtGdiDeleteObjectApp(void)
{
    uint32_t hobject = read_stack_arg(0);

    /* Check object type */
    uint8_t type;
    void *obj = gdi_get_object_any(&g_gdi_handles, hobject, &type);
    if (!obj) {
        EAX = 0;
        return STATUS_SUCCESS;
    }

    /* Cannot delete stock objects */
    if (GDI_HANDLE_IS_STOCK(hobject)) {
        EAX = 1;
        return STATUS_SUCCESS;
    }

    /* Free based on type */
    bool success = false;
    switch (type) {
        case GDI_OBJ_DC:
            success = gdi_delete_dc(&g_gdi_handles, hobject);
            break;
        case GDI_OBJ_BRUSH:
            success = gdi_free_handle(&g_gdi_handles, hobject);
            if (success) gdi_free_brush(&g_gdi_handles, obj);
            break;
        case GDI_OBJ_PEN:
            success = gdi_free_handle(&g_gdi_handles, hobject);
            if (success) gdi_free_pen(&g_gdi_handles, obj);
            break;
        case GDI_OBJ_FONT:
            success = gdi_free_handle(&g_gdi_handles, hobject);
            if (success) gdi_free_font(&g_gdi_handles, obj);
            break;
        case GDI_OBJ_BITMAP:
            success = gdi_free_handle(&g_gdi_handles, hobject);
            if (success) gdi_free_bitmap(&g_gdi_handles, obj);
            break;
        case GDI_OBJ_REGION:
            success = gdi_free_handle(&g_gdi_handles, hobject);
            if (success) gdi_free_region(&g_gdi_handles, obj);
            break;
        default:
            success = gdi_free_handle(&g_gdi_handles, hobject);
            break;
    }

    EAX = success ? 1 : 0;
    return STATUS_SUCCESS;
}

/* NtGdiSelectBrush */
ntstatus_t sys_NtGdiSelectBrush(void)
{
    uint32_t hdc = read_stack_arg(0);
    uint32_t hbrush = read_stack_arg(1);

    uint32_t prev = gdi_select_brush(&g_gdi_handles, hdc, hbrush);

    EAX = prev;
    return STATUS_SUCCESS;
}

/* NtGdiSelectPen */
ntstatus_t sys_NtGdiSelectPen(void)
{
    uint32_t hdc = read_stack_arg(0);
    uint32_t hpen = read_stack_arg(1);

    uint32_t prev = gdi_select_pen(&g_gdi_handles, hdc, hpen);

    EAX = prev;
    return STATUS_SUCCESS;
}

/* NtGdiSelectFont */
ntstatus_t sys_NtGdiSelectFont(void)
{
    uint32_t hdc = read_stack_arg(0);
    uint32_t hfont = read_stack_arg(1);

    uint32_t prev = gdi_select_font(&g_gdi_handles, hdc, hfont);

    EAX = prev;
    return STATUS_SUCCESS;
}

/* NtGdiSelectBitmap */
ntstatus_t sys_NtGdiSelectBitmap(void)
{
    uint32_t hdc = read_stack_arg(0);
    uint32_t hbitmap = read_stack_arg(1);

    uint32_t prev = gdi_select_bitmap(&g_gdi_handles, hdc, hbitmap);

    EAX = prev;
    return STATUS_SUCCESS;
}

/* NtGdiGetAndSetDCDword - handles SetTextColor, SetBkColor, SetBkMode, etc. */
ntstatus_t sys_NtGdiGetAndSetDCDword(void)
{
    uint32_t hdc = read_stack_arg(0);
    uint32_t index = read_stack_arg(1);
    uint32_t value = read_stack_arg(2);
    uint32_t result_ptr = read_stack_arg(3);

    gdi_dc_t *dc = gdi_get_dc(&g_gdi_handles, hdc);
    if (!dc) {
        EAX = 0;
        return STATUS_SUCCESS;
    }

    uint32_t old_value = 0;

    /* Attribute indices from ReactOS dcutil.c */
    switch (index) {
        case 0:  /* SetTextColor / GetTextColor */
            old_value = dc->text_color;
            dc->text_color = value & 0x00FFFFFF;
            break;
        case 1:  /* SetBkColor / GetBkColor */
            old_value = dc->bk_color;
            dc->bk_color = value & 0x00FFFFFF;
            break;
        case 2:  /* SetBkMode / GetBkMode */
            old_value = dc->bk_mode;
            dc->bk_mode = value;
            break;
        case 3:  /* SetMapMode / GetMapMode */
            old_value = dc->map_mode;
            dc->map_mode = value;
            break;
        case 4:  /* SetTextAlign / GetTextAlign */
            old_value = dc->text_align;
            dc->text_align = value;
            break;
        case 5:  /* SetROP2 / GetROP2 */
            old_value = dc->rop2;
            dc->rop2 = value;
            break;
        case 6:  /* SetStretchBltMode / GetStretchBltMode */
            old_value = dc->stretch_mode;
            dc->stretch_mode = value;
            break;
        case 7:  /* SetPolyFillMode / GetPolyFillMode */
            old_value = dc->poly_fill_mode;
            dc->poly_fill_mode = value;
            break;
        default:
            EAX = 0;
            return STATUS_SUCCESS;
    }

    /* Write old value to result pointer */
    if (result_ptr) {
        write_guest_dword(result_ptr, old_value);
    }

    EAX = 1;  /* Success */
    return STATUS_SUCCESS;
}

/* NtGdiPatBlt */
ntstatus_t sys_NtGdiPatBlt(void)
{
    uint32_t hdc = read_stack_arg(0);
    int x = (int)read_stack_arg(1);
    int y = (int)read_stack_arg(2);
    int width = (int)read_stack_arg(3);
    int height = (int)read_stack_arg(4);
    uint32_t rop = read_stack_arg(5);

    gdi_dc_t *dc = gdi_get_dc(&g_gdi_handles, hdc);
    if (!dc) {
        EAX = 0;
        return STATUS_SUCCESS;
    }

    bool success = gdi_pat_blt(dc, x, y, width, height, rop);

    /* Mark display dirty if drawing to screen */
    if (success && dc->dc_type == DCTYPE_DIRECT && g_display) {
        g_display->dirty = true;
    }

    EAX = success ? 1 : 0;
    return STATUS_SUCCESS;
}

/* NtGdiBitBlt */
ntstatus_t sys_NtGdiBitBlt(void)
{
    uint32_t hdc_dest = read_stack_arg(0);
    int x_dest = (int)read_stack_arg(1);
    int y_dest = (int)read_stack_arg(2);
    int width = (int)read_stack_arg(3);
    int height = (int)read_stack_arg(4);
    uint32_t hdc_src = read_stack_arg(5);
    int x_src = (int)read_stack_arg(6);
    int y_src = (int)read_stack_arg(7);
    uint32_t rop = read_stack_arg(8);
    /* args 9, 10 are crBack and reserved */

    gdi_dc_t *dst_dc = gdi_get_dc(&g_gdi_handles, hdc_dest);
    if (!dst_dc) {
        EAX = 0;
        return STATUS_SUCCESS;
    }

    gdi_dc_t *src_dc = gdi_get_dc(&g_gdi_handles, hdc_src);

    bool success = gdi_bit_blt(dst_dc, x_dest, y_dest, width, height,
                                src_dc, x_src, y_src, rop);

    if (success && dst_dc->dc_type == DCTYPE_DIRECT && g_display) {
        g_display->dirty = true;
    }

    EAX = success ? 1 : 0;
    return STATUS_SUCCESS;
}

/* NtGdiExtTextOutW */
ntstatus_t sys_NtGdiExtTextOutW(void)
{
    uint32_t hdc = read_stack_arg(0);
    int x = (int)read_stack_arg(1);
    int y = (int)read_stack_arg(2);
    uint32_t options = read_stack_arg(3);
    uint32_t rect_ptr = read_stack_arg(4);
    uint32_t str_ptr = read_stack_arg(5);
    int count = (int)read_stack_arg(6);
    uint32_t dx_ptr = read_stack_arg(7);
    /* arg 8 is reserved */

    gdi_dc_t *dc = gdi_get_dc(&g_gdi_handles, hdc);
    if (!dc) {
        EAX = 0;
        return STATUS_SUCCESS;
    }

    /* Read string from guest memory */
    uint16_t str_buf[256];
    if (count > 255) count = 255;
    int actual_count = read_guest_unicode(str_ptr, str_buf, count);

    /* Read optional rect */
    RECT rect = {0};
    if (rect_ptr) {
        vm_context_t *vm = vm_get_context();
        if (vm) {
            uint32_t phys = paging_get_phys(&vm->paging, rect_ptr);
            if (phys) {
                rect.left = (int32_t)mem_readl_phys(phys);
                rect.top = (int32_t)mem_readl_phys(phys + 4);
                rect.right = (int32_t)mem_readl_phys(phys + 8);
                rect.bottom = (int32_t)mem_readl_phys(phys + 12);
            }
        }
    }

    /* TODO: Read dx array if needed */

    bool success = gdi_ext_text_out(dc, x, y, options,
                                     rect_ptr ? &rect : NULL,
                                     str_buf, actual_count, NULL);

    if (success && dc->dc_type == DCTYPE_DIRECT && g_display) {
        g_display->dirty = true;
    }

    EAX = success ? 1 : 0;
    return STATUS_SUCCESS;
}

/* NtGdiGetTextExtent */
ntstatus_t sys_NtGdiGetTextExtent(void)
{
    uint32_t hdc = read_stack_arg(0);
    uint32_t str_ptr = read_stack_arg(1);
    int count = (int)read_stack_arg(2);
    uint32_t size_ptr = read_stack_arg(3);
    /* arg 4 is flags */

    gdi_dc_t *dc = gdi_get_dc(&g_gdi_handles, hdc);
    if (!dc) {
        EAX = 0;
        return STATUS_SUCCESS;
    }

    uint16_t str_buf[256];
    if (count > 255) count = 255;
    int actual_count = read_guest_unicode(str_ptr, str_buf, count);

    SIZE size;
    bool success = gdi_get_text_extent(dc, str_buf, actual_count, &size);

    if (success && size_ptr) {
        write_guest_dword(size_ptr, size.cx);
        write_guest_dword(size_ptr + 4, size.cy);
    }

    EAX = success ? 1 : 0;
    return STATUS_SUCCESS;
}

/* NtGdiGetTextExtentExW */
ntstatus_t sys_NtGdiGetTextExtentExW(void)
{
    uint32_t hdc = read_stack_arg(0);
    uint32_t str_ptr = read_stack_arg(1);
    int count = (int)read_stack_arg(2);
    int max_extent = (int)read_stack_arg(3);
    uint32_t fit_ptr = read_stack_arg(4);
    uint32_t dx_ptr = read_stack_arg(5);
    uint32_t size_ptr = read_stack_arg(6);
    /* args 7-8 are flags */

    gdi_dc_t *dc = gdi_get_dc(&g_gdi_handles, hdc);
    if (!dc) {
        EAX = 0;
        return STATUS_SUCCESS;
    }

    uint16_t str_buf[256];
    if (count > 255) count = 255;
    int actual_count = read_guest_unicode(str_ptr, str_buf, count);

    SIZE size;
    int fit = 0;
    bool success = gdi_get_text_extent_ex(dc, str_buf, actual_count, max_extent,
                                           fit_ptr ? &fit : NULL, NULL, &size);

    if (success) {
        if (fit_ptr) write_guest_dword(fit_ptr, fit);
        if (size_ptr) {
            write_guest_dword(size_ptr, size.cx);
            write_guest_dword(size_ptr + 4, size.cy);
        }
    }

    EAX = success ? 1 : 0;
    return STATUS_SUCCESS;
}

/* NtGdiCreateSolidBrush */
ntstatus_t sys_NtGdiCreateSolidBrush(void)
{
    uint32_t color = read_stack_arg(0);
    /* arg 1 is reserved */

    uint32_t handle = gdi_create_solid_brush(&g_gdi_handles, color);

    EAX = handle;
    return STATUS_SUCCESS;
}

/* NtGdiCreatePen */
ntstatus_t sys_NtGdiCreatePen(void)
{
    int style = (int)read_stack_arg(0);
    int width = (int)read_stack_arg(1);
    uint32_t color = read_stack_arg(2);
    /* arg 3 is reserved */

    uint32_t handle = gdi_create_pen(&g_gdi_handles, style, width, color);

    EAX = handle;
    return STATUS_SUCCESS;
}

/* NtGdiCreateRectRgn */
ntstatus_t sys_NtGdiCreateRectRgn(void)
{
    int left = (int)read_stack_arg(0);
    int top = (int)read_stack_arg(1);
    int right = (int)read_stack_arg(2);
    int bottom = (int)read_stack_arg(3);

    uint32_t handle = gdi_create_rect_rgn(&g_gdi_handles, left, top, right, bottom);

    EAX = handle;
    return STATUS_SUCCESS;
}

/* NtGdiFillRgn */
ntstatus_t sys_NtGdiFillRgn(void)
{
    uint32_t hdc = read_stack_arg(0);
    uint32_t hrgn = read_stack_arg(1);
    uint32_t hbrush = read_stack_arg(2);

    gdi_dc_t *dc = gdi_get_dc(&g_gdi_handles, hdc);
    if (!dc) {
        EAX = 0;
        return STATUS_SUCCESS;
    }

    bool success = gdi_fill_rgn(&g_gdi_handles, dc, hrgn, hbrush);

    if (success && dc->dc_type == DCTYPE_DIRECT && g_display) {
        g_display->dirty = true;
    }

    EAX = success ? 1 : 0;
    return STATUS_SUCCESS;
}

/* NtGdiRectangle */
ntstatus_t sys_NtGdiRectangle(void)
{
    uint32_t hdc = read_stack_arg(0);
    int left = (int)read_stack_arg(1);
    int top = (int)read_stack_arg(2);
    int right = (int)read_stack_arg(3);
    int bottom = (int)read_stack_arg(4);

    gdi_dc_t *dc = gdi_get_dc(&g_gdi_handles, hdc);
    if (!dc) {
        EAX = 0;
        return STATUS_SUCCESS;
    }

    bool success = gdi_rectangle(dc, left, top, right, bottom);

    if (success && dc->dc_type == DCTYPE_DIRECT && g_display) {
        g_display->dirty = true;
    }

    EAX = success ? 1 : 0;
    return STATUS_SUCCESS;
}

/* NtGdiGetDeviceCaps */
ntstatus_t sys_NtGdiGetDeviceCaps(void)
{
    uint32_t hdc = read_stack_arg(0);
    int index = (int)read_stack_arg(1);

    gdi_dc_t *dc = gdi_get_dc(&g_gdi_handles, hdc);
    if (!dc) {
        EAX = 0;
        return STATUS_SUCCESS;
    }

    int result = gdi_get_device_caps(dc, index);

    EAX = result;
    return STATUS_SUCCESS;
}

/* NtGdiSetPixel */
ntstatus_t sys_NtGdiSetPixel(void)
{
    uint32_t hdc = read_stack_arg(0);
    int x = (int)read_stack_arg(1);
    int y = (int)read_stack_arg(2);
    uint32_t color = read_stack_arg(3);

    gdi_dc_t *dc = gdi_get_dc(&g_gdi_handles, hdc);
    if (!dc) {
        EAX = (uint32_t)-1;
        return STATUS_SUCCESS;
    }

    COLORREF result = gdi_set_pixel(dc, x, y, color);

    if (dc->dc_type == DCTYPE_DIRECT && g_display) {
        g_display->dirty = true;
    }

    EAX = result;
    return STATUS_SUCCESS;
}

/* NtGdiGetPixel */
ntstatus_t sys_NtGdiGetPixel(void)
{
    uint32_t hdc = read_stack_arg(0);
    int x = (int)read_stack_arg(1);
    int y = (int)read_stack_arg(2);

    gdi_dc_t *dc = gdi_get_dc(&g_gdi_handles, hdc);
    if (!dc) {
        EAX = (uint32_t)-1;
        return STATUS_SUCCESS;
    }

    COLORREF result = gdi_get_pixel(dc, x, y);

    EAX = result;
    return STATUS_SUCCESS;
}

/* NtGdiMoveTo */
ntstatus_t sys_NtGdiMoveTo(void)
{
    uint32_t hdc = read_stack_arg(0);
    int x = (int)read_stack_arg(1);
    int y = (int)read_stack_arg(2);
    uint32_t point_ptr = read_stack_arg(3);

    gdi_dc_t *dc = gdi_get_dc(&g_gdi_handles, hdc);
    if (!dc) {
        EAX = 0;
        return STATUS_SUCCESS;
    }

    POINT prev;
    bool success = gdi_move_to(dc, x, y, &prev);

    if (success && point_ptr) {
        write_guest_dword(point_ptr, prev.x);
        write_guest_dword(point_ptr + 4, prev.y);
    }

    EAX = success ? 1 : 0;
    return STATUS_SUCCESS;
}

/* NtGdiLineTo */
ntstatus_t sys_NtGdiLineTo(void)
{
    uint32_t hdc = read_stack_arg(0);
    int x = (int)read_stack_arg(1);
    int y = (int)read_stack_arg(2);

    gdi_dc_t *dc = gdi_get_dc(&g_gdi_handles, hdc);
    if (!dc) {
        EAX = 0;
        return STATUS_SUCCESS;
    }

    bool success = gdi_line_to(dc, x, y);

    if (success && dc->dc_type == DCTYPE_DIRECT && g_display) {
        g_display->dirty = true;
    }

    EAX = success ? 1 : 0;
    return STATUS_SUCCESS;
}

/* NtGdiSaveDC */
ntstatus_t sys_NtGdiSaveDC(void)
{
    uint32_t hdc = read_stack_arg(0);

    int result = gdi_save_dc(&g_gdi_handles, hdc);

    EAX = result;
    return STATUS_SUCCESS;
}

/* NtGdiRestoreDC */
ntstatus_t sys_NtGdiRestoreDC(void)
{
    uint32_t hdc = read_stack_arg(0);
    int level = (int)read_stack_arg(1);

    bool success = gdi_restore_dc(&g_gdi_handles, hdc, level);

    EAX = success ? 1 : 0;
    return STATUS_SUCCESS;
}

/* NtGdiOpenDCW - create a DC (simplified) */
ntstatus_t sys_NtGdiOpenDCW(void)
{
    /* Create a display DC */
    uint32_t hdc = gdi_create_display_dc(&g_gdi_handles, g_display);

    EAX = hdc;
    return STATUS_SUCCESS;
}

/* NtGdiGetDCPoint */
ntstatus_t sys_NtGdiGetDCPoint(void)
{
    uint32_t hdc = read_stack_arg(0);
    int type = (int)read_stack_arg(1);
    uint32_t point_ptr = read_stack_arg(2);

    gdi_dc_t *dc = gdi_get_dc(&g_gdi_handles, hdc);
    if (!dc) {
        EAX = 0;
        return STATUS_SUCCESS;
    }

    POINT point = {0};
    bool success = true;

    switch (type) {
        case 1:  /* Viewport origin */
            gdi_get_viewport_org(dc, &point);
            break;
        case 2:  /* Window origin */
            gdi_get_window_org(dc, &point);
            break;
        case 3:  /* Current position */
            gdi_get_current_position(dc, &point);
            break;
        case 4:  /* Brush origin */
            gdi_get_brush_org(dc, &point);
            break;
        default:
            success = false;
            break;
    }

    if (success && point_ptr) {
        write_guest_dword(point_ptr, point.x);
        write_guest_dword(point_ptr + 4, point.y);
    }

    EAX = success ? 1 : 0;
    return STATUS_SUCCESS;
}

/* NtGdiSetBrushOrg */
ntstatus_t sys_NtGdiSetBrushOrg(void)
{
    uint32_t hdc = read_stack_arg(0);
    int x = (int)read_stack_arg(1);
    int y = (int)read_stack_arg(2);
    uint32_t point_ptr = read_stack_arg(3);

    gdi_dc_t *dc = gdi_get_dc(&g_gdi_handles, hdc);
    if (!dc) {
        EAX = 0;
        return STATUS_SUCCESS;
    }

    POINT prev;
    bool success = gdi_set_brush_org(dc, x, y, &prev);

    if (success && point_ptr) {
        write_guest_dword(point_ptr, prev.x);
        write_guest_dword(point_ptr + 4, prev.y);
    }

    EAX = success ? 1 : 0;
    return STATUS_SUCCESS;
}

/* NtGdiHfontCreate - create font (simplified) */
ntstatus_t sys_NtGdiHfontCreate(void)
{
    /* For now, return a stock font */
    uint32_t handle = gdi_get_stock_object(&g_gdi_handles, GDI_STOCK_DEFAULT_GUI_FONT);

    EAX = handle;
    return STATUS_SUCCESS;
}

/* NtGdiExtGetObjectW */
ntstatus_t sys_NtGdiExtGetObjectW(void)
{
    uint32_t hobject = read_stack_arg(0);
    int count = (int)read_stack_arg(1);
    uint32_t buffer_ptr = read_stack_arg(2);

    uint8_t type;
    void *obj = gdi_get_object_any(&g_gdi_handles, hobject, &type);
    if (!obj) {
        EAX = 0;
        return STATUS_SUCCESS;
    }

    /* Return size needed if buffer is NULL */
    if (!buffer_ptr) {
        switch (type) {
            case GDI_OBJ_PEN:
                EAX = 16;  /* LOGPEN size */
                break;
            case GDI_OBJ_BRUSH:
                EAX = 12;  /* LOGBRUSH size */
                break;
            case GDI_OBJ_FONT:
                EAX = 92;  /* LOGFONTW size */
                break;
            case GDI_OBJ_BITMAP:
                EAX = 24;  /* BITMAP size */
                break;
            default:
                EAX = 0;
                break;
        }
        return STATUS_SUCCESS;
    }

    /* Fill buffer based on object type */
    /* Simplified - just return success with minimal data */
    EAX = count > 0 ? count : 0;
    return STATUS_SUCCESS;
}

/* NtGdiFlush */
ntstatus_t sys_NtGdiFlush(void)
{
    /* Force display update */
    if (g_display && g_display->dirty) {
        display_present(g_display);
    }

    EAX = 1;
    return STATUS_SUCCESS;
}

/* NtGdiInit */
ntstatus_t sys_NtGdiInit(void)
{
    EAX = 1;
    return STATUS_SUCCESS;
}

/*
 * User Syscall Implementations
 */

/* NtUserGetDC */
ntstatus_t sys_NtUserGetDC(void)
{
    uint32_t hwnd = read_stack_arg(0);

    uint32_t hdc = gdi_create_window_dc(&g_gdi_handles, g_display, hwnd);

    EAX = hdc;
    return STATUS_SUCCESS;
}

/* NtUserGetDCEx */
ntstatus_t sys_NtUserGetDCEx(void)
{
    uint32_t hwnd = read_stack_arg(0);
    uint32_t hrgnClip = read_stack_arg(1);
    uint32_t flags = read_stack_arg(2);

    (void)hrgnClip;
    (void)flags;

    uint32_t hdc = gdi_create_window_dc(&g_gdi_handles, g_display, hwnd);

    EAX = hdc;
    return STATUS_SUCCESS;
}

/* NtUserGetWindowDC */
ntstatus_t sys_NtUserGetWindowDC(void)
{
    uint32_t hwnd = read_stack_arg(0);

    uint32_t hdc = gdi_create_window_dc(&g_gdi_handles, g_display, hwnd);

    EAX = hdc;
    return STATUS_SUCCESS;
}

/* NtUserReleaseDC */
ntstatus_t sys_NtUserReleaseDC(void)
{
    uint32_t hdc = read_stack_arg(0);

    int result = gdi_release_dc(&g_gdi_handles, 0, hdc);

    EAX = result;
    return STATUS_SUCCESS;
}

/* NtUserBeginPaint */
ntstatus_t sys_NtUserBeginPaint(void)
{
    uint32_t hwnd = read_stack_arg(0);
    uint32_t ps_ptr = read_stack_arg(1);  /* PAINTSTRUCT* */

    /* Create DC for painting */
    uint32_t hdc = gdi_create_window_dc(&g_gdi_handles, g_display, hwnd);

    /* Fill PAINTSTRUCT if provided */
    if (ps_ptr) {
        write_guest_dword(ps_ptr, hdc);           /* hdc */
        write_guest_dword(ps_ptr + 4, 1);         /* fErase */
        write_guest_dword(ps_ptr + 8, 0);         /* rcPaint.left */
        write_guest_dword(ps_ptr + 12, 0);        /* rcPaint.top */
        write_guest_dword(ps_ptr + 16, g_display ? g_display->width : 800);   /* rcPaint.right */
        write_guest_dword(ps_ptr + 20, g_display ? g_display->height : 600);  /* rcPaint.bottom */
        write_guest_dword(ps_ptr + 24, 0);        /* fRestore */
        write_guest_dword(ps_ptr + 28, 0);        /* fIncUpdate */
        /* rgbReserved[32] left as zeros */
    }

    EAX = hdc;
    return STATUS_SUCCESS;
}

/* NtUserEndPaint */
ntstatus_t sys_NtUserEndPaint(void)
{
    uint32_t hwnd = read_stack_arg(0);
    uint32_t ps_ptr = read_stack_arg(1);

    (void)hwnd;

    /* Get DC from PAINTSTRUCT and release it */
    if (ps_ptr) {
        vm_context_t *vm = vm_get_context();
        if (vm) {
            uint32_t phys = paging_get_phys(&vm->paging, ps_ptr);
            if (phys) {
                uint32_t hdc = mem_readl_phys(phys);
                gdi_release_dc(&g_gdi_handles, hwnd, hdc);
            }
        }
    }

    /* Force display update */
    if (g_display && g_display->dirty) {
        display_present(g_display);
    }

    EAX = 1;
    return STATUS_SUCCESS;
}

/* NtUserInvalidateRect */
ntstatus_t sys_NtUserInvalidateRect(void)
{
    uint32_t hwnd = read_stack_arg(0);
    uint32_t rect_ptr = read_stack_arg(1);
    uint32_t erase = read_stack_arg(2);

    (void)hwnd;
    (void)rect_ptr;
    (void)erase;

    /* Mark display as dirty */
    if (g_display) {
        g_display->dirty = true;
    }

    EAX = 1;
    return STATUS_SUCCESS;
}

/* NtUserFillWindow */
ntstatus_t sys_NtUserFillWindow(void)
{
    uint32_t hwnd_parent = read_stack_arg(0);
    uint32_t hwnd = read_stack_arg(1);
    uint32_t hdc = read_stack_arg(2);
    uint32_t hbrush = read_stack_arg(3);

    (void)hwnd_parent;
    (void)hwnd;

    gdi_dc_t *dc = gdi_get_dc(&g_gdi_handles, hdc);
    if (!dc) {
        EAX = 0;
        return STATUS_SUCCESS;
    }

    gdi_brush_t *brush = gdi_get_object(&g_gdi_handles, hbrush, GDI_OBJ_BRUSH);
    if (!brush) {
        EAX = 0;
        return STATUS_SUCCESS;
    }

    RECT rect = { 0, 0, dc->width, dc->height };
    int result = gdi_fill_rect(dc, &rect, brush);

    if (result && dc->dc_type == DCTYPE_DIRECT && g_display) {
        g_display->dirty = true;
    }

    EAX = result;
    return STATUS_SUCCESS;
}

/* NtUserCallNoParam */
ntstatus_t sys_NtUserCallNoParam(void)
{
    uint32_t routine = read_stack_arg(0);

    /* Various no-parameter user routines */
    switch (routine) {
        case 0:  /* NOPARAM_ROUTINE_CREATEMENU */
        case 1:  /* NOPARAM_ROUTINE_CREATEPOPUPMENU */
            EAX = 0;  /* No menu support yet */
            break;
        case 2:  /* NOPARAM_ROUTINE_GETMESSAGEEXTRAINFO */
            EAX = 0;
            break;
        case 3:  /* NOPARAM_ROUTINE_MSLOADED */
            EAX = 0;
            break;
        default:
            EAX = 0;
            break;
    }

    return STATUS_SUCCESS;
}

/* NtUserCallOneParam */
ntstatus_t sys_NtUserCallOneParam(void)
{
    uint32_t param = read_stack_arg(0);
    uint32_t routine = read_stack_arg(1);

    (void)param;

    switch (routine) {
        case 21:  /* ONEPARAM_ROUTINE_GETINPUTEVENT */
            EAX = 0;
            break;
        case 22:  /* ONEPARAM_ROUTINE_GETKEYBOARDLAYOUT */
            EAX = 0x04090409;  /* US English */
            break;
        case 23:  /* ONEPARAM_ROUTINE_GETKEYBOARDTYPE */
            EAX = 4;  /* Enhanced 101/102-key keyboard */
            break;
        default:
            EAX = 0;
            break;
    }

    return STATUS_SUCCESS;
}

/* NtUserCallTwoParam */
ntstatus_t sys_NtUserCallTwoParam(void)
{
    uint32_t param1 = read_stack_arg(0);
    uint32_t param2 = read_stack_arg(1);
    uint32_t routine = read_stack_arg(2);

    (void)param1;
    (void)param2;

    switch (routine) {
        default:
            EAX = 0;
            break;
    }

    return STATUS_SUCCESS;
}

/* NtUserSelectPalette */
ntstatus_t sys_NtUserSelectPalette(void)
{
    uint32_t hdc = read_stack_arg(0);
    uint32_t hpal = read_stack_arg(1);
    uint32_t force_bg = read_stack_arg(2);

    uint32_t prev = gdi_select_palette(&g_gdi_handles, hdc, hpal, force_bg != 0);

    EAX = prev;
    return STATUS_SUCCESS;
}

/* NtUserGetThreadState */
ntstatus_t sys_NtUserGetThreadState(void)
{
    uint32_t routine = read_stack_arg(0);

    switch (routine) {
        case 0:  /* THREADSTATE_GETINPUTSTATE */
            EAX = 0;
            break;
        case 4:  /* THREADSTATE_GETMESSAGEEXTRAINFO */
            EAX = 0;
            break;
        default:
            EAX = 0;
            break;
    }

    return STATUS_SUCCESS;
}

/*
 * Main dispatcher
 */
ntstatus_t win32k_syscall_dispatch(uint32_t syscall_num)
{
    /* Ensure initialized */
    if (!g_initialized) {
        if (win32k_init(NULL) < 0) {
            return STATUS_UNSUCCESSFUL;
        }
    }

    uint32_t index = syscall_num - WIN32K_SYSCALL_BASE;

    switch (index) {
        /* GDI Syscalls */
        case NtGdiBitBlt - WIN32K_SYSCALL_BASE:
            return sys_NtGdiBitBlt();
        case NtGdiCreateCompatibleDC - WIN32K_SYSCALL_BASE:
            return sys_NtGdiCreateCompatibleDC();
        case NtGdiCreatePen - WIN32K_SYSCALL_BASE:
            return sys_NtGdiCreatePen();
        case NtGdiCreateRectRgn - WIN32K_SYSCALL_BASE:
            return sys_NtGdiCreateRectRgn();
        case NtGdiCreateSolidBrush - WIN32K_SYSCALL_BASE:
            return sys_NtGdiCreateSolidBrush();
        case NtGdiDeleteObjectApp - WIN32K_SYSCALL_BASE:
            return sys_NtGdiDeleteObjectApp();
        case NtGdiExtGetObjectW - WIN32K_SYSCALL_BASE:
            return sys_NtGdiExtGetObjectW();
        case NtGdiExtTextOutW - WIN32K_SYSCALL_BASE:
            return sys_NtGdiExtTextOutW();
        case NtGdiFillRgn - WIN32K_SYSCALL_BASE:
            return sys_NtGdiFillRgn();
        case NtGdiFlush - WIN32K_SYSCALL_BASE:
            return sys_NtGdiFlush();
        case NtGdiGetAndSetDCDword - WIN32K_SYSCALL_BASE:
            return sys_NtGdiGetAndSetDCDword();
        case NtGdiGetDeviceCaps - WIN32K_SYSCALL_BASE:
            return sys_NtGdiGetDeviceCaps();
        case NtGdiGetDCPoint - WIN32K_SYSCALL_BASE:
            return sys_NtGdiGetDCPoint();
        case NtGdiGetPixel - WIN32K_SYSCALL_BASE:
            return sys_NtGdiGetPixel();
        case NtGdiGetStockObject - WIN32K_SYSCALL_BASE:
            return sys_NtGdiGetStockObject();
        case NtGdiGetTextExtent - WIN32K_SYSCALL_BASE:
            return sys_NtGdiGetTextExtent();
        case NtGdiGetTextExtentExW - WIN32K_SYSCALL_BASE:
            return sys_NtGdiGetTextExtentExW();
        case NtGdiHfontCreate - WIN32K_SYSCALL_BASE:
            return sys_NtGdiHfontCreate();
        case NtGdiInit - WIN32K_SYSCALL_BASE:
            return sys_NtGdiInit();
        case NtGdiLineTo - WIN32K_SYSCALL_BASE:
            return sys_NtGdiLineTo();
        case NtGdiMoveTo - WIN32K_SYSCALL_BASE:
            return sys_NtGdiMoveTo();
        case NtGdiOpenDCW - WIN32K_SYSCALL_BASE:
            return sys_NtGdiOpenDCW();
        case NtGdiPatBlt - WIN32K_SYSCALL_BASE:
            return sys_NtGdiPatBlt();
        case NtGdiRectangle - WIN32K_SYSCALL_BASE:
            return sys_NtGdiRectangle();
        case NtGdiRestoreDC - WIN32K_SYSCALL_BASE:
            return sys_NtGdiRestoreDC();
        case NtGdiSaveDC - WIN32K_SYSCALL_BASE:
            return sys_NtGdiSaveDC();
        case NtGdiSelectBitmap - WIN32K_SYSCALL_BASE:
            return sys_NtGdiSelectBitmap();
        case NtGdiSelectBrush - WIN32K_SYSCALL_BASE:
            return sys_NtGdiSelectBrush();
        case NtGdiSelectFont - WIN32K_SYSCALL_BASE:
            return sys_NtGdiSelectFont();
        case NtGdiSelectPen - WIN32K_SYSCALL_BASE:
            return sys_NtGdiSelectPen();
        case NtGdiSetBrushOrg - WIN32K_SYSCALL_BASE:
            return sys_NtGdiSetBrushOrg();
        case NtGdiSetPixel - WIN32K_SYSCALL_BASE:
            return sys_NtGdiSetPixel();

        /* User Syscalls */
        case NtUserBeginPaint - WIN32K_SYSCALL_BASE:
            return sys_NtUserBeginPaint();
        case NtUserCallNoParam - WIN32K_SYSCALL_BASE:
            return sys_NtUserCallNoParam();
        case NtUserCallOneParam - WIN32K_SYSCALL_BASE:
            return sys_NtUserCallOneParam();
        case NtUserCallTwoParam - WIN32K_SYSCALL_BASE:
            return sys_NtUserCallTwoParam();
        case NtUserEndPaint - WIN32K_SYSCALL_BASE:
            return sys_NtUserEndPaint();
        case NtUserFillWindow - WIN32K_SYSCALL_BASE:
            return sys_NtUserFillWindow();
        case NtUserGetDC - WIN32K_SYSCALL_BASE:
            return sys_NtUserGetDC();
        case NtUserGetDCEx - WIN32K_SYSCALL_BASE:
            return sys_NtUserGetDCEx();
        case NtUserGetThreadState - WIN32K_SYSCALL_BASE:
            return sys_NtUserGetThreadState();
        case NtUserGetWindowDC - WIN32K_SYSCALL_BASE:
            return sys_NtUserGetWindowDC();
        case NtUserInvalidateRect - WIN32K_SYSCALL_BASE:
            return sys_NtUserInvalidateRect();
        case NtUserReleaseDC - WIN32K_SYSCALL_BASE:
            return sys_NtUserReleaseDC();
        case NtUserSelectPalette - WIN32K_SYSCALL_BASE:
            return sys_NtUserSelectPalette();

        default:
            /* Unknown syscall - log and return success with 0 */
            printf("win32k: Unimplemented syscall 0x%X (%s)\n",
                   syscall_num, syscall_get_name(syscall_num));
            EAX = 0;
            return STATUS_SUCCESS;
    }
}
