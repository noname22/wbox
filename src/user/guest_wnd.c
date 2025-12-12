/*
 * WBOX Guest WND Implementation
 */
#include "guest_wnd.h"
#include "desktop_heap.h"
#include "guest_cls.h"
#include <stdio.h>
#include <string.h>

/* Forward declaration - we need user_window.h which already includes WBOX_WND */

uint32_t guest_wnd_create(WBOX_WND *host_wnd)
{
    if (!host_wnd) {
        return 0;
    }

    desktop_heap_t *heap = desktop_heap_get();
    if (!heap) {
        fprintf(stderr, "guest_wnd_create: desktop heap not initialized\n");
        return 0;
    }

    /* Calculate total size: base WND + cbWndExtra */
    uint32_t wnd_size = WND_BASE_SIZE;
    if (host_wnd->cbWndExtra > 0) {
        wnd_size += host_wnd->cbWndExtra;
    }

    /* Allocate from desktop heap */
    uint32_t guest_va = desktop_heap_alloc(wnd_size);
    if (guest_va == 0) {
        fprintf(stderr, "guest_wnd_create: failed to allocate %u bytes\n", wnd_size);
        return 0;
    }

    /* Initialize THRDESKHEAD (first 20 bytes) */
    desktop_heap_write32(guest_va + WND_HEAD_H, host_wnd->hwnd);
    desktop_heap_write32(guest_va + WND_HEAD_CLOCKOBJ, 1);  /* Lock count */
    desktop_heap_write32(guest_va + WND_HEAD_PTI, 0);       /* ThreadInfo - we don't have this */
    desktop_heap_write32(guest_va + WND_HEAD_RPDESK, 0);    /* Desktop - we'll set this later */
    desktop_heap_write32(guest_va + WND_HEAD_PSELF, guest_va);  /* Self pointer */

    /* State flags */
    desktop_heap_write32(guest_va + WND_STATE, host_wnd->state);
    desktop_heap_write32(guest_va + WND_STATE2, host_wnd->state2);

    /* Styles */
    desktop_heap_write32(guest_va + WND_EXSTYLE, host_wnd->exStyle);
    desktop_heap_write32(guest_va + WND_STYLE, host_wnd->style);

    /* Module and FNID */
    desktop_heap_write32(guest_va + WND_HMODULE, host_wnd->hInstance);
    desktop_heap_write32(guest_va + WND_FNID, host_wnd->pcls ? host_wnd->pcls->fnid : 0);

    /* Hierarchy pointers - initialized to 0, will be updated by guest_wnd_update_hierarchy */
    desktop_heap_write32(guest_va + WND_SPWNDNEXT, 0);
    desktop_heap_write32(guest_va + WND_SPWNDPREV, 0);
    desktop_heap_write32(guest_va + WND_SPWNDPARENT, 0);
    desktop_heap_write32(guest_va + WND_SPWNDCHILD, 0);
    desktop_heap_write32(guest_va + WND_SPWNDOWNER, 0);

    /* Window rectangles */
    desktop_heap_write32(guest_va + WND_RCWINDOW + 0, (uint32_t)host_wnd->rcWindow.left);
    desktop_heap_write32(guest_va + WND_RCWINDOW + 4, (uint32_t)host_wnd->rcWindow.top);
    desktop_heap_write32(guest_va + WND_RCWINDOW + 8, (uint32_t)host_wnd->rcWindow.right);
    desktop_heap_write32(guest_va + WND_RCWINDOW + 12, (uint32_t)host_wnd->rcWindow.bottom);

    desktop_heap_write32(guest_va + WND_RCCLIENT + 0, (uint32_t)host_wnd->rcClient.left);
    desktop_heap_write32(guest_va + WND_RCCLIENT + 4, (uint32_t)host_wnd->rcClient.top);
    desktop_heap_write32(guest_va + WND_RCCLIENT + 8, (uint32_t)host_wnd->rcClient.right);
    desktop_heap_write32(guest_va + WND_RCCLIENT + 12, (uint32_t)host_wnd->rcClient.bottom);

    /* Window procedure */
    desktop_heap_write32(guest_va + WND_LPFNWNDPROC, host_wnd->lpfnWndProc);

    /* Class pointer - get guest CLS VA */
    uint32_t guest_cls_va = 0;
    if (host_wnd->pcls) {
        guest_cls_va = guest_cls_get_va(host_wnd->pcls);
    }
    desktop_heap_write32(guest_va + WND_PCLS, guest_cls_va);

    /* Update region and properties */
    desktop_heap_write32(guest_va + WND_HRGNUPDATE, host_wnd->hrgnUpdate);
    desktop_heap_write32(guest_va + WND_PROPLISTHEAD + 0, 0);  /* Flink */
    desktop_heap_write32(guest_va + WND_PROPLISTHEAD + 4, 0);  /* Blink */
    desktop_heap_write32(guest_va + WND_PROPLISTITEMS, 0);

    /* Scroll info and menus */
    desktop_heap_write32(guest_va + WND_PSBINFO, 0);
    desktop_heap_write32(guest_va + WND_SYSTEMMENU, 0);
    desktop_heap_write32(guest_va + WND_IDMENU, host_wnd->IDMenu);

    /* Clipping regions */
    desktop_heap_write32(guest_va + WND_HRGNCLIP, 0);
    desktop_heap_write32(guest_va + WND_HRGNNEWFRAME, 0);

    /* Window name - LARGE_UNICODE_STRING
     * For now, set to empty. Full string support would require allocating
     * string buffer in desktop heap as well. */
    desktop_heap_write32(guest_va + WND_STRNAME + 0, 0);  /* Length */
    desktop_heap_write32(guest_va + WND_STRNAME + 4, 0);  /* MaxLength */
    desktop_heap_write32(guest_va + WND_STRNAME + 8, 0);  /* Buffer/flags */

    /* Extra bytes count */
    desktop_heap_write32(guest_va + WND_CBWNDEXTRA, host_wnd->cbWndExtra);

    /* Last active, IMC, user data */
    desktop_heap_write32(guest_va + WND_SPWNDLASTACTIVE, 0);
    desktop_heap_write32(guest_va + WND_HIMC, 0);
    desktop_heap_write32(guest_va + WND_DWUSERDATA, host_wnd->dwUserData);

    /* Activation context and clipboard */
    desktop_heap_write32(guest_va + WND_PACTCTX, 0);
    desktop_heap_write32(guest_va + WND_SPWNDCLIPBOARD, 0);

    /* Extended style 2 */
    desktop_heap_write32(guest_va + WND_EXSTYLE2, 0);

    /* Internal position structure (28 bytes) - zero fill */
    for (int i = 0; i < 28; i += 4) {
        desktop_heap_write32(guest_va + WND_INTERNALPOS + i, 0);
    }

    /* Flags and scroll info extended */
    desktop_heap_write32(guest_va + WND_FLAGS, 0);
    desktop_heap_write32(guest_va + WND_PSBINFOEX, 0);

    /* Thread list entry */
    desktop_heap_write32(guest_va + WND_THREADLISTENTRY + 0, 0);
    desktop_heap_write32(guest_va + WND_THREADLISTENTRY + 4, 0);

    /* Dialog pointer - critical for dialogs! */
    desktop_heap_write32(guest_va + WND_DIALOGPOINTER, 0);

    /* Zero out extra window bytes if present */
    if (host_wnd->cbWndExtra > 0) {
        for (int i = 0; i < host_wnd->cbWndExtra; i++) {
            desktop_heap_write8(guest_va + WND_BASE_SIZE + i, 0);
        }
    }

    printf("USER: Created guest WND at 0x%08X for hwnd 0x%08X (size %u)\n",
           guest_va, host_wnd->hwnd, wnd_size);

    return guest_va;
}

void guest_wnd_destroy(uint32_t guest_va)
{
    /* With a bump allocator, we can't really free memory.
     * In a real implementation we'd use a proper heap allocator.
     * For now, just mark the WND as destroyed by clearing the handle. */
    if (guest_va && desktop_heap_contains(guest_va)) {
        desktop_heap_write32(guest_va + WND_HEAD_H, 0);
    }
}

void guest_wnd_sync(WBOX_WND *host_wnd)
{
    if (!host_wnd || !host_wnd->guest_wnd_va) {
        return;
    }

    uint32_t guest_va = host_wnd->guest_wnd_va;

    /* Update state */
    desktop_heap_write32(guest_va + WND_STATE, host_wnd->state);
    desktop_heap_write32(guest_va + WND_STATE2, host_wnd->state2);

    /* Update styles */
    desktop_heap_write32(guest_va + WND_EXSTYLE, host_wnd->exStyle);
    desktop_heap_write32(guest_va + WND_STYLE, host_wnd->style);

    /* Update rectangles */
    desktop_heap_write32(guest_va + WND_RCWINDOW + 0, (uint32_t)host_wnd->rcWindow.left);
    desktop_heap_write32(guest_va + WND_RCWINDOW + 4, (uint32_t)host_wnd->rcWindow.top);
    desktop_heap_write32(guest_va + WND_RCWINDOW + 8, (uint32_t)host_wnd->rcWindow.right);
    desktop_heap_write32(guest_va + WND_RCWINDOW + 12, (uint32_t)host_wnd->rcWindow.bottom);

    desktop_heap_write32(guest_va + WND_RCCLIENT + 0, (uint32_t)host_wnd->rcClient.left);
    desktop_heap_write32(guest_va + WND_RCCLIENT + 4, (uint32_t)host_wnd->rcClient.top);
    desktop_heap_write32(guest_va + WND_RCCLIENT + 8, (uint32_t)host_wnd->rcClient.right);
    desktop_heap_write32(guest_va + WND_RCCLIENT + 12, (uint32_t)host_wnd->rcClient.bottom);

    /* Update window procedure */
    desktop_heap_write32(guest_va + WND_LPFNWNDPROC, host_wnd->lpfnWndProc);

    /* Update user data */
    desktop_heap_write32(guest_va + WND_DWUSERDATA, host_wnd->dwUserData);

    /* Update menu */
    desktop_heap_write32(guest_va + WND_IDMENU, host_wnd->IDMenu);
}

void guest_wnd_update_hierarchy(WBOX_WND *host_wnd)
{
    if (!host_wnd || !host_wnd->guest_wnd_va) {
        return;
    }

    uint32_t guest_va = host_wnd->guest_wnd_va;

    /* Get guest VAs for linked windows */
    uint32_t next_va = guest_wnd_get_va(host_wnd->spwndNext);
    uint32_t prev_va = guest_wnd_get_va(host_wnd->spwndPrev);
    uint32_t parent_va = guest_wnd_get_va(host_wnd->spwndParent);
    uint32_t child_va = guest_wnd_get_va(host_wnd->spwndChild);
    uint32_t owner_va = guest_wnd_get_va(host_wnd->spwndOwner);

    desktop_heap_write32(guest_va + WND_SPWNDNEXT, next_va);
    desktop_heap_write32(guest_va + WND_SPWNDPREV, prev_va);
    desktop_heap_write32(guest_va + WND_SPWNDPARENT, parent_va);
    desktop_heap_write32(guest_va + WND_SPWNDCHILD, child_va);
    desktop_heap_write32(guest_va + WND_SPWNDOWNER, owner_va);
}

void guest_wnd_set_dialog_pointer(uint32_t guest_va, uint32_t dialog_info)
{
    if (guest_va && desktop_heap_contains(guest_va)) {
        desktop_heap_write32(guest_va + WND_DIALOGPOINTER, dialog_info);
    }
}

uint32_t guest_wnd_get_va(WBOX_WND *host_wnd)
{
    if (!host_wnd) {
        return 0;
    }
    return host_wnd->guest_wnd_va;
}
