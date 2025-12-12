/*
 * WBOX Guest CLS Implementation
 */
#include "guest_cls.h"
#include "desktop_heap.h"
#include <stdio.h>
#include <string.h>

uint32_t guest_cls_create(WBOX_CLS *host_cls)
{
    if (!host_cls) {
        return 0;
    }

    desktop_heap_t *heap = desktop_heap_get();
    if (!heap) {
        fprintf(stderr, "guest_cls_create: desktop heap not initialized\n");
        return 0;
    }

    /* Allocate CLS structure from desktop heap */
    uint32_t guest_va = desktop_heap_alloc(CLS_SIZE);
    if (guest_va == 0) {
        fprintf(stderr, "guest_cls_create: failed to allocate %u bytes\n", CLS_SIZE);
        return 0;
    }

    /* Initialize CLS fields */
    desktop_heap_write32(guest_va + CLS_PCLSNEXT, 0);  /* We don't link classes in guest */
    desktop_heap_write16(guest_va + CLS_ATOMCLASSNAME, host_cls->atomClassName);
    desktop_heap_write16(guest_va + CLS_ATOMNVCLASSNAME, host_cls->atomNVClassName);
    desktop_heap_write32(guest_va + CLS_STYLE, host_cls->style);
    desktop_heap_write32(guest_va + CLS_LPFNWNDPROC, host_cls->lpfnWndProc);
    desktop_heap_write32(guest_va + CLS_CBCLSEXTRA, host_cls->cbClsExtra);
    desktop_heap_write32(guest_va + CLS_CBWNDEXTRA, host_cls->cbWndExtra);
    desktop_heap_write32(guest_va + CLS_HMODULE, host_cls->hModule);
    desktop_heap_write32(guest_va + CLS_SPICN, 0);  /* Icon cursor - internal pointers */
    desktop_heap_write32(guest_va + CLS_SPICNSM, 0);
    desktop_heap_write32(guest_va + CLS_HICON, host_cls->hIcon);
    desktop_heap_write32(guest_va + CLS_HICONSM, host_cls->hIconSm);
    desktop_heap_write32(guest_va + CLS_HCURSOR, host_cls->hCursor);
    desktop_heap_write32(guest_va + CLS_HBRBACKGROUND, host_cls->hbrBackground);
    desktop_heap_write32(guest_va + CLS_LPSZMENUNAME, 0);  /* Menu name string - would need heap allocation */
    desktop_heap_write32(guest_va + CLS_LPSZANSICLASSNAME, 0);  /* ANSI class name */
    desktop_heap_write32(guest_va + CLS_SPCPDCFIRST, 0);  /* Call proc data */
    desktop_heap_write32(guest_va + CLS_PCLSBASE, guest_va);  /* Self-reference for base class */
    desktop_heap_write32(guest_va + CLS_CWNDREFERENCECOUNT, host_cls->cWndReferenceCount);
    desktop_heap_write32(guest_va + CLS_FNID, host_cls->fnid);
    desktop_heap_write32(guest_va + CLS_CSF_FLAGS, host_cls->flags);
    desktop_heap_write32(guest_va + CLS_LPFNWNDPROCEXTRA, host_cls->lpfnWndProc);  /* User-mode wndproc */

    printf("USER: Created guest CLS at 0x%08X for atom 0x%04X\n",
           guest_va, host_cls->atomClassName);

    return guest_va;
}

void guest_cls_destroy(uint32_t guest_va)
{
    /* With a bump allocator, we can't really free memory.
     * Just mark as destroyed by clearing the atom. */
    if (guest_va && desktop_heap_contains(guest_va)) {
        desktop_heap_write16(guest_va + CLS_ATOMCLASSNAME, 0);
    }
}

void guest_cls_sync(WBOX_CLS *host_cls)
{
    if (!host_cls || !host_cls->guest_cls_va) {
        return;
    }

    uint32_t guest_va = host_cls->guest_cls_va;

    /* Update mutable fields */
    desktop_heap_write32(guest_va + CLS_STYLE, host_cls->style);
    desktop_heap_write32(guest_va + CLS_LPFNWNDPROC, host_cls->lpfnWndProc);
    desktop_heap_write32(guest_va + CLS_HICON, host_cls->hIcon);
    desktop_heap_write32(guest_va + CLS_HICONSM, host_cls->hIconSm);
    desktop_heap_write32(guest_va + CLS_HCURSOR, host_cls->hCursor);
    desktop_heap_write32(guest_va + CLS_HBRBACKGROUND, host_cls->hbrBackground);
    desktop_heap_write32(guest_va + CLS_CWNDREFERENCECOUNT, host_cls->cWndReferenceCount);
    desktop_heap_write32(guest_va + CLS_LPFNWNDPROCEXTRA, host_cls->lpfnWndProc);
}

uint32_t guest_cls_get_va(WBOX_CLS *host_cls)
{
    if (!host_cls) {
        return 0;
    }
    return host_cls->guest_cls_va;
}
