/*
 * WBOX Win32k Syscall Dispatcher
 * Routes win32k syscalls (0x1000+) to GDI/USER implementations
 */
#ifndef WBOX_WIN32K_DISPATCHER_H
#define WBOX_WIN32K_DISPATCHER_H

#include "syscalls.h"
#include "../gdi/gdi_handle_table.h"
#include "../gdi/display.h"

/*
 * Initialize win32k subsystem
 * Must be called before any win32k syscalls
 */
int win32k_init(display_context_t *display);

/*
 * Shutdown win32k subsystem
 */
void win32k_shutdown(void);

/*
 * Get GDI handle table
 */
gdi_handle_table_t *win32k_get_handle_table(void);

/*
 * Get display context
 */
display_context_t *win32k_get_display(void);

/*
 * Main dispatcher - called from ntdll.c when syscall >= 0x1000
 * Returns NTSTATUS result
 */
ntstatus_t win32k_syscall_dispatch(uint32_t syscall_num);

/*
 * Individual syscall handlers
 */

/* GDI Syscalls */
ntstatus_t sys_NtGdiGetStockObject(void);
ntstatus_t sys_NtGdiCreateCompatibleDC(void);
ntstatus_t sys_NtGdiDeleteObjectApp(void);
ntstatus_t sys_NtGdiSelectBrush(void);
ntstatus_t sys_NtGdiSelectPen(void);
ntstatus_t sys_NtGdiSelectFont(void);
ntstatus_t sys_NtGdiSelectBitmap(void);
ntstatus_t sys_NtGdiGetAndSetDCDword(void);
ntstatus_t sys_NtGdiPatBlt(void);
ntstatus_t sys_NtGdiBitBlt(void);
ntstatus_t sys_NtGdiExtTextOutW(void);
ntstatus_t sys_NtGdiGetTextExtent(void);
ntstatus_t sys_NtGdiGetTextExtentExW(void);
ntstatus_t sys_NtGdiCreateSolidBrush(void);
ntstatus_t sys_NtGdiCreatePen(void);
ntstatus_t sys_NtGdiCreateRectRgn(void);
ntstatus_t sys_NtGdiFillRgn(void);
ntstatus_t sys_NtGdiRectangle(void);
ntstatus_t sys_NtGdiGetDeviceCaps(void);
ntstatus_t sys_NtGdiSetPixel(void);
ntstatus_t sys_NtGdiGetPixel(void);
ntstatus_t sys_NtGdiMoveTo(void);
ntstatus_t sys_NtGdiLineTo(void);
ntstatus_t sys_NtGdiSaveDC(void);
ntstatus_t sys_NtGdiRestoreDC(void);
ntstatus_t sys_NtGdiOpenDCW(void);
ntstatus_t sys_NtGdiGetDCPoint(void);
ntstatus_t sys_NtGdiSetBrushOrg(void);
ntstatus_t sys_NtGdiHfontCreate(void);
ntstatus_t sys_NtGdiExtGetObjectW(void);
ntstatus_t sys_NtGdiGetDCObject(void);
ntstatus_t sys_NtGdiFlush(void);
ntstatus_t sys_NtGdiInit(void);

/* User Syscalls */
ntstatus_t sys_NtUserGetDC(void);
ntstatus_t sys_NtUserGetDCEx(void);
ntstatus_t sys_NtUserGetWindowDC(void);
ntstatus_t sys_NtUserReleaseDC(void);
ntstatus_t sys_NtUserBeginPaint(void);
ntstatus_t sys_NtUserEndPaint(void);
ntstatus_t sys_NtUserInvalidateRect(void);
ntstatus_t sys_NtUserFillWindow(void);
ntstatus_t sys_NtUserCallNoParam(void);
ntstatus_t sys_NtUserCallOneParam(void);
ntstatus_t sys_NtUserCallTwoParam(void);
ntstatus_t sys_NtUserSelectPalette(void);
ntstatus_t sys_NtUserGetThreadState(void);

#endif /* WBOX_WIN32K_DISPATCHER_H */
