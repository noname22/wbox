/*
 * WBOX User Message Queue
 * Windows message queue infrastructure
 */
#ifndef WBOX_USER_MESSAGE_H
#define WBOX_USER_MESSAGE_H

#include <stdint.h>
#include <stdbool.h>

/* Forward declaration */
typedef struct vm_context vm_context_t;

/* MSG structure (28 bytes, matches Windows) */
typedef struct _WBOX_MSG {
    uint32_t hwnd;      /* Target window */
    uint32_t message;   /* Message ID (WM_*) */
    uint32_t wParam;    /* Word parameter */
    uint32_t lParam;    /* Long parameter */
    uint32_t time;      /* Timestamp (GetTickCount) */
    int32_t pt_x;       /* Cursor X */
    int32_t pt_y;       /* Cursor Y */
} WBOX_MSG;

/* Message queue (one per thread, but we only have one thread for now) */
typedef struct _WBOX_MSG_QUEUE {
    WBOX_MSG messages[256];  /* Circular buffer */
    int head;                /* Next message to read */
    int tail;                /* Next slot to write */
    int count;               /* Messages in queue */

    /* Focus/capture state */
    uint32_t hwndFocus;      /* Keyboard focus window */
    uint32_t hwndCapture;    /* Mouse capture window */
    uint32_t hwndActive;     /* Active window */

    /* Keyboard state */
    uint8_t keyState[256];   /* Per-key up/down state */

    /* Mouse position */
    int32_t mouseX;
    int32_t mouseY;

    /* Quit flag */
    bool quitPosted;
    int exitCode;
} WBOX_MSG_QUEUE;

/* PeekMessage flags */
#define PM_NOREMOVE     0x0000
#define PM_REMOVE       0x0001
#define PM_NOYIELD      0x0002

/* Common window messages */
#define WM_NULL         0x0000
#define WM_CREATE       0x0001
#define WM_DESTROY      0x0002
#define WM_MOVE         0x0003
#define WM_SIZE         0x0005
#define WM_ACTIVATE     0x0006
#define WM_SETFOCUS     0x0007
#define WM_KILLFOCUS    0x0008
#define WM_ENABLE       0x000A
#define WM_SETREDRAW    0x000B
#define WM_SETTEXT      0x000C
#define WM_GETTEXT      0x000D
#define WM_GETTEXTLENGTH 0x000E
#define WM_PAINT        0x000F
#define WM_CLOSE        0x0010
#define WM_QUERYENDSESSION 0x0011
#define WM_QUIT         0x0012
#define WM_QUERYOPEN    0x0013
#define WM_ERASEBKGND   0x0014
#define WM_SYSCOLORCHANGE 0x0015
#define WM_SHOWWINDOW   0x0018
#define WM_ACTIVATEAPP  0x001C
#define WM_SETCURSOR    0x0020
#define WM_MOUSEACTIVATE 0x0021
#define WM_GETMINMAXINFO 0x0024
#define WM_WINDOWPOSCHANGING 0x0046
#define WM_WINDOWPOSCHANGED 0x0047
#define WM_NCCREATE     0x0081
#define WM_NCDESTROY    0x0082
#define WM_NCCALCSIZE   0x0083
#define WM_NCHITTEST    0x0084
#define WM_NCPAINT      0x0085
#define WM_NCACTIVATE   0x0086
#define WM_KEYDOWN      0x0100
#define WM_KEYUP        0x0101
#define WM_CHAR         0x0102
#define WM_SYSKEYDOWN   0x0104
#define WM_SYSKEYUP     0x0105
#define WM_SYSCHAR      0x0106
#define WM_COMMAND      0x0111
#define WM_SYSCOMMAND   0x0112
#define WM_TIMER        0x0113
#define WM_MOUSEMOVE    0x0200
#define WM_LBUTTONDOWN  0x0201
#define WM_LBUTTONUP    0x0202
#define WM_LBUTTONDBLCLK 0x0203
#define WM_RBUTTONDOWN  0x0204
#define WM_RBUTTONUP    0x0205
#define WM_RBUTTONDBLCLK 0x0206
#define WM_MBUTTONDOWN  0x0207
#define WM_MBUTTONUP    0x0208
#define WM_MBUTTONDBLCLK 0x0209
#define WM_MOUSEWHEEL   0x020A
#define WM_USER         0x0400

/* WM_SIZE wParam values */
#define SIZE_RESTORED   0
#define SIZE_MINIMIZED  1
#define SIZE_MAXIMIZED  2

/* WM_ACTIVATE wParam values */
#define WA_INACTIVE     0
#define WA_ACTIVE       1
#define WA_CLICKACTIVE  2

/* Helper macros */
#define MAKELPARAM(l, h) ((uint32_t)(((uint16_t)(l)) | ((uint32_t)((uint16_t)(h))) << 16))
#define LOWORD(l)        ((uint16_t)((uint32_t)(l) & 0xffff))
#define HIWORD(l)        ((uint16_t)((uint32_t)(l) >> 16))
#define GET_X_LPARAM(lp) ((int16_t)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int16_t)HIWORD(lp))

/* Global message queue */
extern WBOX_MSG_QUEUE g_msg_queue;

/* Initialize message queue */
void msg_queue_init(void);

/* Post a message to the queue */
bool msg_queue_post(uint32_t hwnd, uint32_t message, uint32_t wParam, uint32_t lParam);

/* Post quit message */
void msg_queue_post_quit(int exitCode);

/* Peek at messages in the queue
 * Returns true if a message was found
 * If PM_REMOVE is set in flags, the message is removed from the queue
 */
bool msg_queue_peek(WBOX_MSG *out_msg, uint32_t hwndFilter,
                    uint32_t msgFilterMin, uint32_t msgFilterMax,
                    uint32_t flags);

/* Check if queue has any messages */
bool msg_queue_has_messages(void);

/* Get current tick count (for message timestamps) */
uint32_t msg_get_tick_count(void);

/* Write MSG structure to guest memory */
void msg_write_to_guest(vm_context_t *vm, uint32_t guest_addr, const WBOX_MSG *msg);

/* Read MSG structure from guest memory */
void msg_read_from_guest(vm_context_t *vm, uint32_t guest_addr, WBOX_MSG *msg);

#endif /* WBOX_USER_MESSAGE_H */
