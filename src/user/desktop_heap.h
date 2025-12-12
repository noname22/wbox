/*
 * WBOX Desktop Heap
 * Shared memory region for WND and CLS structures accessible from user mode
 *
 * The desktop heap is a shared memory region where window (WND) and class (CLS)
 * structures are allocated. This allows user32.dll's ValidateHwnd() to directly
 * access window information without making syscalls.
 */
#ifndef WBOX_DESKTOP_HEAP_H
#define WBOX_DESKTOP_HEAP_H

#include <stdint.h>
#include <stdbool.h>
#include "../vm/vm.h"

/* Desktop heap memory layout */
#define DESKTOP_HEAP_BASE_VA    0x01000000  /* Guest virtual address */
#define DESKTOP_HEAP_SIZE       (1024 * 1024)  /* 1MB */
#define DESKTOP_HEAP_LIMIT_VA   (DESKTOP_HEAP_BASE_VA + DESKTOP_HEAP_SIZE)

/*
 * Guest WND structure offsets (must match ReactOS exactly)
 *
 * THRDESKHEAD layout (20 bytes):
 *   HEAD:        h (4) + cLockObj (4) = 8
 *   THROBJHEAD:  +pti (4) = 12
 *   THRDESKHEAD: +rpdesk (4) + pSelf (4) = 20
 */
#define WND_HEAD_H              0x00  /* HANDLE (same as hwnd) */
#define WND_HEAD_CLOCKOBJ       0x04  /* DWORD lock count */
#define WND_HEAD_PTI            0x08  /* PTHREADINFO */
#define WND_HEAD_RPDESK         0x0C  /* PDESKTOP */
#define WND_HEAD_PSELF          0x10  /* PVOID self pointer */
#define WND_STATE               0x14  /* DWORD state flags */
#define WND_STATE2              0x18  /* DWORD state2 */
#define WND_EXSTYLE             0x1C  /* DWORD ExStyle */
#define WND_STYLE               0x20  /* DWORD style */
#define WND_HMODULE             0x24  /* HINSTANCE */
#define WND_FNID                0x28  /* DWORD fnid */
#define WND_SPWNDNEXT           0x2C  /* WND* spwndNext */
#define WND_SPWNDPREV           0x30  /* WND* spwndPrev */
#define WND_SPWNDPARENT         0x34  /* WND* spwndParent */
#define WND_SPWNDCHILD          0x38  /* WND* spwndChild */
#define WND_SPWNDOWNER          0x3C  /* WND* spwndOwner */
#define WND_RCWINDOW            0x40  /* RECT (16 bytes) */
#define WND_RCCLIENT            0x50  /* RECT (16 bytes) */
#define WND_LPFNWNDPROC         0x60  /* WNDPROC */
#define WND_PCLS                0x64  /* PCLS */
#define WND_HRGNUPDATE          0x68  /* HRGN */
#define WND_PROPLISTHEAD        0x6C  /* LIST_ENTRY (8 bytes) */
#define WND_PROPLISTITEMS       0x74  /* ULONG */
#define WND_PSBINFO             0x78  /* PSBINFO */
#define WND_SYSTEMMENU          0x7C  /* HMENU */
#define WND_IDMENU              0x80  /* UINT_PTR */
#define WND_HRGNCLIP            0x84  /* HRGN */
#define WND_HRGNNEWFRAME        0x88  /* HRGN */
#define WND_STRNAME             0x8C  /* LARGE_UNICODE_STRING (12 bytes) */
#define WND_CBWNDEXTRA          0x98  /* ULONG */
#define WND_SPWNDLASTACTIVE     0x9C  /* WND* */
#define WND_HIMC                0xA0  /* HIMC */
#define WND_DWUSERDATA          0xA4  /* LONG_PTR */
#define WND_PACTCTX             0xA8  /* PVOID */
#define WND_SPWNDCLIPBOARD      0xAC  /* WND* */
#define WND_EXSTYLE2            0xB0  /* DWORD */
#define WND_INTERNALPOS         0xB4  /* INTERNALPOS (28 bytes) */
#define WND_FLAGS               0xD0  /* UINT bitfield */
#define WND_PSBINFOEX           0xD4  /* PSBINFOEX */
#define WND_THREADLISTENTRY     0xD8  /* LIST_ENTRY (8 bytes) */
#define WND_DIALOGPOINTER       0xE0  /* PVOID */
#define WND_BASE_SIZE           0xE4  /* 228 bytes base (+ cbwndExtra) */

/*
 * Guest CLS structure offsets (must match ReactOS)
 */
#define CLS_PCLSNEXT            0x00  /* PCLS pclsNext */
#define CLS_ATOMCLASSNAME       0x04  /* RTL_ATOM atomClassName */
#define CLS_ATOMNVCLASSNAME     0x06  /* RTL_ATOM atomNVClassName */
#define CLS_STYLE               0x08  /* UINT style */
#define CLS_LPFNWNDPROC         0x0C  /* WNDPROC lpfnWndProc (kernel) */
#define CLS_CBCLSEXTRA          0x10  /* INT cbclsExtra */
#define CLS_CBWNDEXTRA          0x14  /* INT cbwndExtra */
#define CLS_HMODULE             0x18  /* HINSTANCE hModule */
#define CLS_SPICN               0x1C  /* PCURSOR spicn */
#define CLS_SPICNSM             0x20  /* PCURSOR spicnSm */
#define CLS_HICON               0x24  /* HANDLE hIcon */
#define CLS_HICONSM             0x28  /* HANDLE hIconSm */
#define CLS_HCURSOR             0x2C  /* HANDLE hCursor */
#define CLS_HBRBACKGROUND       0x30  /* HBRUSH hbrBackground */
#define CLS_LPSZMENUNAME        0x34  /* PWSTR lpszMenuName */
#define CLS_LPSZANSICLASSNAME   0x38  /* PSTR lpszAnsiClassName */
#define CLS_SPCPDCFIRST         0x3C  /* struct _CALLPROCDATA *spcpdcFirst */
#define CLS_PCLSBASE            0x40  /* struct _CLS *pclsBase */
#define CLS_CWNDREFERENCECOUNT  0x44  /* INT cWndReferenceCount */
#define CLS_FNID                0x48  /* UINT fnid */
#define CLS_CSF_FLAGS           0x4C  /* UINT CSF_flags */
#define CLS_LPFNWNDPROCEXTRA    0x50  /* WNDPROC lpfnWndProcExtra (user) */
#define CLS_SIZE                0x54  /* 84 bytes base */

/*
 * LARGE_UNICODE_STRING structure (12 bytes on 32-bit)
 */
#define LUNISTR_LENGTH          0x00  /* ULONG Length */
#define LUNISTR_MAXLENGTH       0x04  /* ULONG MaximumLength */
#define LUNISTR_FLAGS           0x08  /* ULONG bAnsi flag */
#define LUNISTR_BUFFER          0x08  /* PWSTR Buffer (overlaps with bAnsi in union) */
/* Actually the union makes this tricky - we need to check ReactOS layout carefully */

/*
 * Desktop heap context
 */
typedef struct {
    uint32_t base_va;       /* Guest virtual address (0x01000000) */
    uint32_t limit_va;      /* End of heap (0x01100000) */
    uint32_t phys_base;     /* Physical memory base */
    uint32_t alloc_offset;  /* Current allocation offset from base */
    bool initialized;
} desktop_heap_t;

/*
 * Initialize the desktop heap
 * Allocates physical memory and maps it into guest address space
 * Returns 0 on success, -1 on failure
 */
int desktop_heap_init(vm_context_t *vm);

/*
 * Shutdown the desktop heap
 */
void desktop_heap_shutdown(void);

/*
 * Get the global desktop heap context
 */
desktop_heap_t *desktop_heap_get(void);

/*
 * Allocate memory from the desktop heap
 * size: Bytes to allocate (will be 4-byte aligned)
 * Returns guest virtual address, or 0 on failure
 */
uint32_t desktop_heap_alloc(uint32_t size);

/*
 * Write data to the desktop heap at a specific offset
 * va: Guest virtual address in desktop heap
 * data: Data to write
 * size: Number of bytes to write
 */
void desktop_heap_write(uint32_t va, const void *data, uint32_t size);

/*
 * Write a 32-bit value to the desktop heap
 */
void desktop_heap_write32(uint32_t va, uint32_t value);

/*
 * Write a 16-bit value to the desktop heap
 */
void desktop_heap_write16(uint32_t va, uint16_t value);

/*
 * Write an 8-bit value to the desktop heap
 */
void desktop_heap_write8(uint32_t va, uint8_t value);

/*
 * Check if an address is within the desktop heap
 */
bool desktop_heap_contains(uint32_t va);

#endif /* WBOX_DESKTOP_HEAP_H */
