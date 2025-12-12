/*
 * WBOX USER Window Management
 * Window object (WND) structure and operations
 */
#ifndef WBOX_USER_WINDOW_H
#define WBOX_USER_WINDOW_H

#include <stdint.h>
#include <stdbool.h>
#include <wchar.h>
#include "user_class.h"

/* Forward declaration */
struct _WBOX_WND;

/* Rectangle structure */
typedef struct _WBOX_RECT {
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
} WBOX_RECT;

/* Window style flags (WS_*) */
#define WS_OVERLAPPED       0x00000000
#define WS_POPUP            0x80000000
#define WS_CHILD            0x40000000
#define WS_MINIMIZE         0x20000000
#define WS_VISIBLE          0x10000000
#define WS_DISABLED         0x08000000
#define WS_CLIPSIBLINGS     0x04000000
#define WS_CLIPCHILDREN     0x02000000
#define WS_MAXIMIZE         0x01000000
#define WS_CAPTION          0x00C00000
#define WS_BORDER           0x00800000
#define WS_DLGFRAME         0x00400000
#define WS_VSCROLL          0x00200000
#define WS_HSCROLL          0x00100000
#define WS_SYSMENU          0x00080000
#define WS_THICKFRAME       0x00040000
#define WS_GROUP            0x00020000
#define WS_TABSTOP          0x00010000
#define WS_MINIMIZEBOX      0x00020000
#define WS_MAXIMIZEBOX      0x00010000
#define WS_TILED            WS_OVERLAPPED
#define WS_ICONIC           WS_MINIMIZE
#define WS_SIZEBOX          WS_THICKFRAME
#define WS_OVERLAPPEDWINDOW (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
#define WS_POPUPWINDOW      (WS_POPUP | WS_BORDER | WS_SYSMENU)
#define WS_CHILDWINDOW      WS_CHILD

/* Extended window style flags (WS_EX_*) */
#define WS_EX_DLGMODALFRAME     0x00000001
#define WS_EX_NOPARENTNOTIFY    0x00000004
#define WS_EX_TOPMOST           0x00000008
#define WS_EX_ACCEPTFILES       0x00000010
#define WS_EX_TRANSPARENT       0x00000020
#define WS_EX_MDICHILD          0x00000040
#define WS_EX_TOOLWINDOW        0x00000080
#define WS_EX_WINDOWEDGE        0x00000100
#define WS_EX_CLIENTEDGE        0x00000200
#define WS_EX_CONTEXTHELP       0x00000400
#define WS_EX_RIGHT             0x00001000
#define WS_EX_LEFT              0x00000000
#define WS_EX_RTLREADING        0x00002000
#define WS_EX_LTRREADING        0x00000000
#define WS_EX_LEFTSCROLLBAR     0x00004000
#define WS_EX_RIGHTSCROLLBAR    0x00000000
#define WS_EX_CONTROLPARENT     0x00010000
#define WS_EX_STATICEDGE        0x00020000
#define WS_EX_APPWINDOW         0x00040000
#define WS_EX_OVERLAPPEDWINDOW  (WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE)
#define WS_EX_PALETTEWINDOW     (WS_EX_WINDOWEDGE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST)
#define WS_EX_LAYERED           0x00080000
#define WS_EX_NOINHERITLAYOUT   0x00100000
#define WS_EX_LAYOUTRTL         0x00400000
#define WS_EX_COMPOSITED        0x02000000
#define WS_EX_NOACTIVATE        0x08000000

/* CW_USEDEFAULT for window position/size */
#define CW_USEDEFAULT       ((int32_t)0x80000000)

/* ShowWindow commands */
#define SW_HIDE             0
#define SW_SHOWNORMAL       1
#define SW_NORMAL           1
#define SW_SHOWMINIMIZED    2
#define SW_SHOWMAXIMIZED    3
#define SW_MAXIMIZE         3
#define SW_SHOWNOACTIVATE   4
#define SW_SHOW             5
#define SW_MINIMIZE         6
#define SW_SHOWMINNOACTIVE  7
#define SW_SHOWNA           8
#define SW_RESTORE          9
#define SW_SHOWDEFAULT      10
#define SW_FORCEMINIMIZE    11

/* Internal window state flags */
#define WNDS_VISIBLE            0x00000001
#define WNDS_DISABLED           0x00000002
#define WNDS_MAXIMIZED          0x00000004
#define WNDS_MINIMIZED          0x00000008
#define WNDS_ACTIVEFRAME        0x00000010
#define WNDS_HASMENU            0x00000020
#define WNDS_DESTROYED          0x00000040
#define WNDS_SENDNCPAINT        0x00000080
#define WNDS_SENDERASEBACKGROUND 0x00000100
#define WNDS_NONCPAINT          0x00000200
#define WNDS_ERASEBACKGROUND    0x00000400

/*
 * Window object structure (WND)
 */
typedef struct _WBOX_WND {
    /* Handle */
    uint32_t hwnd;                      /* USER handle */

    /* Class */
    WBOX_CLS *pcls;                     /* Pointer to window class */
    uint32_t lpfnWndProc;               /* Window procedure (may override class) */

    /* Styles */
    uint32_t style;                     /* WS_* flags */
    uint32_t exStyle;                   /* WS_EX_* flags */

    /* Geometry */
    WBOX_RECT rcWindow;                 /* Window rect (screen coords) */
    WBOX_RECT rcClient;                 /* Client rect (screen coords) */

    /* Hierarchy */
    struct _WBOX_WND *spwndParent;      /* Parent window */
    struct _WBOX_WND *spwndChild;       /* First child window */
    struct _WBOX_WND *spwndNext;        /* Next sibling */
    struct _WBOX_WND *spwndPrev;        /* Previous sibling */
    struct _WBOX_WND *spwndOwner;       /* Owner window (for popups) */

    /* State */
    uint32_t state;                     /* Internal state flags (WNDS_*) */
    uint32_t state2;                    /* Additional state */

    /* Window text */
    wchar_t *strName;                   /* Window title (heap allocated) */

    /* Extra bytes */
    int cbWndExtra;                     /* Size of extra bytes */
    uint8_t *extraBytes;                /* Extra window bytes (if cbWndExtra > 0) */

    /* Identifiers */
    uint32_t hInstance;                 /* HINSTANCE */
    uint32_t IDMenu;                    /* Menu ID or HMENU */

    /* GDI state */
    uint32_t hrgnUpdate;                /* Update region (HRGN) */
    uint32_t hdc;                       /* Class/private DC if CS_OWNDC/CS_CLASSDC */

    /* User data */
    uint32_t dwUserData;                /* SetWindowLong(GWL_USERDATA) */

    /* Creation parameters (saved for WM_CREATE) */
    uint32_t dwExtraParam;              /* lpParam from CreateWindowEx */
} WBOX_WND;

/*
 * Initialize the window subsystem
 * Creates the desktop window
 */
int user_window_init(void);

/*
 * Shutdown window subsystem
 */
void user_window_shutdown(void);

/*
 * Create a window object
 * Returns the created window, or NULL on failure
 */
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
    uint32_t dwExtraParam
);

/*
 * Destroy a window
 */
void user_window_destroy(WBOX_WND *wnd);

/*
 * Find window by handle
 * Returns NULL if not found
 */
WBOX_WND *user_window_from_hwnd(uint32_t hwnd);

/*
 * Get the desktop window
 */
WBOX_WND *user_window_get_desktop(void);

/*
 * Link a window as a child of parent
 */
void user_window_link_child(WBOX_WND *parent, WBOX_WND *child);

/*
 * Unlink a window from hierarchy
 */
void user_window_unlink(WBOX_WND *wnd);

/*
 * Set window position and size
 */
void user_window_set_pos(WBOX_WND *wnd, int x, int y, int cx, int cy, uint32_t flags);

/*
 * Calculate client rect from window rect
 */
void user_window_calc_client_rect(WBOX_WND *wnd);

/*
 * Show or hide a window
 */
void user_window_show(WBOX_WND *wnd, int nCmdShow);

/*
 * Check if window is visible
 */
bool user_window_is_visible(WBOX_WND *wnd);

/*
 * Set window text
 */
void user_window_set_text(WBOX_WND *wnd, const wchar_t *text);

/*
 * Get window text
 */
const wchar_t *user_window_get_text(WBOX_WND *wnd);

/*
 * Get window long value
 */
uint32_t user_window_get_long(WBOX_WND *wnd, int index);

/*
 * Set window long value
 */
uint32_t user_window_set_long(WBOX_WND *wnd, int index, uint32_t value);

/* GetWindowLong indices */
#define GWL_WNDPROC     (-4)
#define GWL_HINSTANCE   (-6)
#define GWL_HWNDPARENT  (-8)
#define GWL_STYLE       (-16)
#define GWL_EXSTYLE     (-20)
#define GWL_USERDATA    (-21)
#define GWL_ID          (-12)

/* GetWindowLong aliases */
#define GWLP_WNDPROC    GWL_WNDPROC
#define GWLP_HINSTANCE  GWL_HINSTANCE
#define GWLP_HWNDPARENT GWL_HWNDPARENT
#define GWLP_USERDATA   GWL_USERDATA
#define GWLP_ID         GWL_ID

/* Dialog-specific offsets */
#define DWL_MSGRESULT   0
#define DWL_DLGPROC     4
#define DWL_USER        8

#endif /* WBOX_USER_WINDOW_H */
