/*
 * WBOX USER Window Management Implementation
 */
#include "user_window.h"
#include "user_handle_table.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Desktop window */
static WBOX_WND *g_desktop_window = NULL;

/* Default frame dimensions (non-client area) */
#define DEFAULT_BORDER_WIDTH    4
#define DEFAULT_CAPTION_HEIGHT  19
#define DEFAULT_MENU_HEIGHT     20

/* Internal: create window without linking to hierarchy */
static WBOX_WND *window_create_internal(
    WBOX_CLS *pcls,
    const wchar_t *windowName,
    uint32_t style,
    uint32_t exStyle,
    int x, int y,
    int cx, int cy,
    uint32_t hInstance,
    uint32_t hMenu,
    uint32_t dwExtraParam)
{
    WBOX_WND *wnd = calloc(1, sizeof(WBOX_WND));
    if (!wnd) {
        fprintf(stderr, "USER: Failed to allocate WND\n");
        return NULL;
    }

    /* Allocate handle */
    USER_HANDLE_TABLE *htable = user_get_handle_table();
    uint32_t hwnd = user_handle_alloc(htable, wnd, USER_TYPE_WINDOW, NULL);
    if (hwnd == 0) {
        fprintf(stderr, "USER: Failed to allocate HWND\n");
        free(wnd);
        return NULL;
    }

    wnd->hwnd = hwnd;
    wnd->pcls = pcls;
    wnd->lpfnWndProc = pcls ? pcls->lpfnWndProc : 0;
    wnd->style = style;
    wnd->exStyle = exStyle;
    wnd->hInstance = hInstance;
    wnd->IDMenu = hMenu;
    wnd->dwExtraParam = dwExtraParam;

    /* Set geometry */
    wnd->rcWindow.left = x;
    wnd->rcWindow.top = y;
    wnd->rcWindow.right = x + cx;
    wnd->rcWindow.bottom = y + cy;

    /* Calculate client rect */
    user_window_calc_client_rect(wnd);

    /* Allocate extra bytes if needed */
    int cbExtra = pcls ? pcls->cbWndExtra : 0;
    if (cbExtra > 0) {
        wnd->extraBytes = calloc(1, cbExtra);
        if (!wnd->extraBytes) {
            fprintf(stderr, "USER: Failed to allocate window extra bytes\n");
            user_handle_free(htable, hwnd);
            free(wnd);
            return NULL;
        }
        wnd->cbWndExtra = cbExtra;
    }

    /* Set window name */
    if (windowName && windowName[0]) {
        user_window_set_text(wnd, windowName);
    }

    /* Set initial state */
    if (style & WS_VISIBLE) {
        wnd->state |= WNDS_VISIBLE;
    }
    if (style & WS_DISABLED) {
        wnd->state |= WNDS_DISABLED;
    }
    if (style & WS_MAXIMIZE) {
        wnd->state |= WNDS_MAXIMIZED;
    }
    if (style & WS_MINIMIZE) {
        wnd->state |= WNDS_MINIMIZED;
    }

    /* Add class reference */
    if (pcls) {
        user_class_add_ref(pcls);
    }

    return wnd;
}

int user_window_init(void)
{
    /* Create desktop window */
    WBOX_CLS *desktop_cls = user_class_get_system_class(ICLS_DIALOG);

    g_desktop_window = window_create_internal(
        desktop_cls,
        L"",
        WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN,
        0,
        0, 0, 800, 600,  /* Match default framebuffer size */
        0, 0, 0
    );

    if (!g_desktop_window) {
        fprintf(stderr, "USER: Failed to create desktop window\n");
        return -1;
    }

    /* Desktop has special handle 0 */
    /* But keep real handle for internal use */

    printf("USER: Window subsystem initialized (desktop hwnd=0x%08X)\n",
           g_desktop_window->hwnd);
    return 0;
}

void user_window_shutdown(void)
{
    /* Destroy all windows (desktop will be destroyed last) */
    if (g_desktop_window) {
        /* Recursively destroy children */
        while (g_desktop_window->spwndChild) {
            user_window_destroy(g_desktop_window->spwndChild);
        }

        /* Free desktop resources */
        free(g_desktop_window->strName);
        free(g_desktop_window->extraBytes);

        USER_HANDLE_TABLE *htable = user_get_handle_table();
        user_handle_free(htable, g_desktop_window->hwnd);

        free(g_desktop_window);
        g_desktop_window = NULL;
    }
}

WBOX_WND *user_window_create(
    WBOX_CLS *pcls,
    const wchar_t *windowName,
    uint32_t style,
    uint32_t exStyle,
    int x, int y,
    int cx, int cy,
    WBOX_WND *parent,
    WBOX_WND *owner,
    uint32_t hInstance,
    uint32_t hMenu,
    uint32_t dwExtraParam)
{
    /* Handle CW_USEDEFAULT */
    if (x == CW_USEDEFAULT) x = 100;
    if (y == CW_USEDEFAULT) y = 100;
    if (cx == CW_USEDEFAULT) cx = 400;
    if (cy == CW_USEDEFAULT) cy = 300;

    /* Child windows must have a parent */
    if ((style & WS_CHILD) && !parent) {
        parent = g_desktop_window;
    }

    /* Non-child windows default parent to desktop */
    if (!(style & WS_CHILD) && !parent) {
        parent = g_desktop_window;
    }

    /* Create the window */
    WBOX_WND *wnd = window_create_internal(
        pcls, windowName, style, exStyle,
        x, y, cx, cy, hInstance, hMenu, dwExtraParam
    );

    if (!wnd) {
        return NULL;
    }

    /* Set owner for popup windows */
    wnd->spwndOwner = owner;

    /* Link to parent */
    if (parent) {
        user_window_link_child(parent, wnd);
    }

    printf("USER: Created window hwnd=0x%08X class='%ls' style=0x%08X pos=(%d,%d) size=(%d,%d)\n",
           wnd->hwnd,
           pcls ? pcls->szClassName : L"(null)",
           style, x, y, cx, cy);

    return wnd;
}

void user_window_destroy(WBOX_WND *wnd)
{
    if (!wnd) return;

    /* Don't destroy desktop */
    if (wnd == g_desktop_window) {
        fprintf(stderr, "USER: Cannot destroy desktop window\n");
        return;
    }

    /* Mark as destroyed */
    wnd->state |= WNDS_DESTROYED;

    /* Destroy children first */
    while (wnd->spwndChild) {
        user_window_destroy(wnd->spwndChild);
    }

    /* Unlink from hierarchy */
    user_window_unlink(wnd);

    /* Release class reference */
    if (wnd->pcls) {
        user_class_release(wnd->pcls);
    }

    /* Free resources */
    free(wnd->strName);
    free(wnd->extraBytes);

    /* Free handle */
    USER_HANDLE_TABLE *htable = user_get_handle_table();
    user_handle_free(htable, wnd->hwnd);

    printf("USER: Destroyed window hwnd=0x%08X\n", wnd->hwnd);

    free(wnd);
}

WBOX_WND *user_window_from_hwnd(uint32_t hwnd)
{
    /* Special case: HWND_DESKTOP (0) */
    if (hwnd == 0 || hwnd == HWND_DESKTOP) {
        return g_desktop_window;
    }

    /* Look up in handle table */
    USER_HANDLE_TABLE *htable = user_get_handle_table();
    return user_handle_get_typed(htable, hwnd, USER_TYPE_WINDOW);
}

WBOX_WND *user_window_get_desktop(void)
{
    return g_desktop_window;
}

void user_window_link_child(WBOX_WND *parent, WBOX_WND *child)
{
    if (!parent || !child) return;

    /* Remove from old parent first */
    user_window_unlink(child);

    /* Set parent */
    child->spwndParent = parent;

    /* Add to front of child list */
    child->spwndNext = parent->spwndChild;
    child->spwndPrev = NULL;

    if (parent->spwndChild) {
        parent->spwndChild->spwndPrev = child;
    }
    parent->spwndChild = child;
}

void user_window_unlink(WBOX_WND *wnd)
{
    if (!wnd) return;

    /* Update siblings */
    if (wnd->spwndPrev) {
        wnd->spwndPrev->spwndNext = wnd->spwndNext;
    }
    if (wnd->spwndNext) {
        wnd->spwndNext->spwndPrev = wnd->spwndPrev;
    }

    /* Update parent's child pointer if this was first child */
    if (wnd->spwndParent && wnd->spwndParent->spwndChild == wnd) {
        wnd->spwndParent->spwndChild = wnd->spwndNext;
    }

    /* Clear links */
    wnd->spwndParent = NULL;
    wnd->spwndNext = NULL;
    wnd->spwndPrev = NULL;
}

void user_window_set_pos(WBOX_WND *wnd, int x, int y, int cx, int cy, uint32_t flags)
{
    if (!wnd) return;

    /* Update window rect */
    wnd->rcWindow.left = x;
    wnd->rcWindow.top = y;
    wnd->rcWindow.right = x + cx;
    wnd->rcWindow.bottom = y + cy;

    /* Recalculate client rect */
    user_window_calc_client_rect(wnd);
}

void user_window_calc_client_rect(WBOX_WND *wnd)
{
    if (!wnd) return;

    /* Start with window rect */
    wnd->rcClient = wnd->rcWindow;

    /* Subtract non-client area */
    uint32_t style = wnd->style;
    uint32_t exStyle = wnd->exStyle;

    /* Border */
    if (style & WS_BORDER) {
        wnd->rcClient.left += 1;
        wnd->rcClient.top += 1;
        wnd->rcClient.right -= 1;
        wnd->rcClient.bottom -= 1;
    }

    /* Thick frame (resizable) */
    if (style & WS_THICKFRAME) {
        wnd->rcClient.left += DEFAULT_BORDER_WIDTH;
        wnd->rcClient.top += DEFAULT_BORDER_WIDTH;
        wnd->rcClient.right -= DEFAULT_BORDER_WIDTH;
        wnd->rcClient.bottom -= DEFAULT_BORDER_WIDTH;
    }

    /* Caption */
    if (style & WS_CAPTION) {
        wnd->rcClient.top += DEFAULT_CAPTION_HEIGHT;
    }

    /* Dialog frame */
    if (style & WS_DLGFRAME) {
        wnd->rcClient.left += DEFAULT_BORDER_WIDTH;
        wnd->rcClient.top += DEFAULT_BORDER_WIDTH;
        wnd->rcClient.right -= DEFAULT_BORDER_WIDTH;
        wnd->rcClient.bottom -= DEFAULT_BORDER_WIDTH;
    }

    /* Extended styles */
    if (exStyle & WS_EX_CLIENTEDGE) {
        wnd->rcClient.left += 2;
        wnd->rcClient.top += 2;
        wnd->rcClient.right -= 2;
        wnd->rcClient.bottom -= 2;
    }

    if (exStyle & WS_EX_WINDOWEDGE) {
        wnd->rcClient.left += 2;
        wnd->rcClient.top += 2;
        wnd->rcClient.right -= 2;
        wnd->rcClient.bottom -= 2;
    }

    /* Ensure valid rect */
    if (wnd->rcClient.right < wnd->rcClient.left) {
        wnd->rcClient.right = wnd->rcClient.left;
    }
    if (wnd->rcClient.bottom < wnd->rcClient.top) {
        wnd->rcClient.bottom = wnd->rcClient.top;
    }
}

void user_window_show(WBOX_WND *wnd, int nCmdShow)
{
    if (!wnd) return;

    switch (nCmdShow) {
        case SW_HIDE:
            wnd->state &= ~WNDS_VISIBLE;
            wnd->style &= ~WS_VISIBLE;
            break;

        case SW_SHOW:
        case SW_SHOWNORMAL:
        case SW_SHOWNA:
        case SW_SHOWNOACTIVATE:
            wnd->state |= WNDS_VISIBLE;
            wnd->style |= WS_VISIBLE;
            wnd->state &= ~(WNDS_MINIMIZED | WNDS_MAXIMIZED);
            break;

        case SW_SHOWMINIMIZED:
        case SW_MINIMIZE:
        case SW_SHOWMINNOACTIVE:
        case SW_FORCEMINIMIZE:
            wnd->state |= WNDS_VISIBLE | WNDS_MINIMIZED;
            wnd->style |= WS_VISIBLE;
            wnd->state &= ~WNDS_MAXIMIZED;
            break;

        case SW_SHOWMAXIMIZED:  /* Same as SW_MAXIMIZE */
            wnd->state |= WNDS_VISIBLE | WNDS_MAXIMIZED;
            wnd->style |= WS_VISIBLE;
            wnd->state &= ~WNDS_MINIMIZED;
            break;

        case SW_RESTORE:
            wnd->state |= WNDS_VISIBLE;
            wnd->style |= WS_VISIBLE;
            wnd->state &= ~(WNDS_MINIMIZED | WNDS_MAXIMIZED);
            break;
    }
}

bool user_window_is_visible(WBOX_WND *wnd)
{
    if (!wnd) return false;
    return (wnd->state & WNDS_VISIBLE) != 0;
}

void user_window_set_text(WBOX_WND *wnd, const wchar_t *text)
{
    if (!wnd) return;

    /* Free old text */
    free(wnd->strName);
    wnd->strName = NULL;

    /* Copy new text */
    if (text) {
        size_t len = wcslen(text);
        wnd->strName = malloc((len + 1) * sizeof(wchar_t));
        if (wnd->strName) {
            wcscpy(wnd->strName, text);
        }
    }
}

const wchar_t *user_window_get_text(WBOX_WND *wnd)
{
    if (!wnd) return L"";
    return wnd->strName ? wnd->strName : L"";
}

uint32_t user_window_get_long(WBOX_WND *wnd, int index)
{
    if (!wnd) return 0;

    switch (index) {
        case GWL_WNDPROC:
            return wnd->lpfnWndProc;
        case GWL_HINSTANCE:
            return wnd->hInstance;
        case GWL_HWNDPARENT:
            return wnd->spwndParent ? wnd->spwndParent->hwnd : 0;
        case GWL_STYLE:
            return wnd->style;
        case GWL_EXSTYLE:
            return wnd->exStyle;
        case GWL_USERDATA:
            return wnd->dwUserData;
        case GWL_ID:
            return wnd->IDMenu;
        default:
            /* Positive indices are window extra bytes */
            if (index >= 0 && index + 4 <= wnd->cbWndExtra && wnd->extraBytes) {
                return *(uint32_t *)(wnd->extraBytes + index);
            }
            return 0;
    }
}

uint32_t user_window_set_long(WBOX_WND *wnd, int index, uint32_t value)
{
    if (!wnd) return 0;

    uint32_t old = user_window_get_long(wnd, index);

    switch (index) {
        case GWL_WNDPROC:
            wnd->lpfnWndProc = value;
            break;
        case GWL_HINSTANCE:
            wnd->hInstance = value;
            break;
        case GWL_STYLE:
            wnd->style = value;
            /* Update state flags */
            if (value & WS_VISIBLE) wnd->state |= WNDS_VISIBLE;
            else wnd->state &= ~WNDS_VISIBLE;
            if (value & WS_DISABLED) wnd->state |= WNDS_DISABLED;
            else wnd->state &= ~WNDS_DISABLED;
            break;
        case GWL_EXSTYLE:
            wnd->exStyle = value;
            break;
        case GWL_USERDATA:
            wnd->dwUserData = value;
            break;
        case GWL_ID:
            wnd->IDMenu = value;
            break;
        default:
            /* Positive indices are window extra bytes */
            if (index >= 0 && index + 4 <= wnd->cbWndExtra && wnd->extraBytes) {
                *(uint32_t *)(wnd->extraBytes + index) = value;
            }
            break;
    }

    return old;
}
