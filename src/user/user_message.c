/*
 * WBOX User Message Queue Implementation
 */
#include "user_message.h"
#include "user_window.h"
#include "../vm/vm.h"
#include "../vm/paging.h"
#include "../cpu/mem.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

/* Global message queue */
WBOX_MSG_QUEUE g_msg_queue;

/* Start time for tick count */
static struct timeval g_start_time;
static bool g_time_initialized = false;

void msg_queue_init(void)
{
    memset(&g_msg_queue, 0, sizeof(g_msg_queue));

    /* Initialize start time for GetTickCount */
    if (!g_time_initialized) {
        gettimeofday(&g_start_time, NULL);
        g_time_initialized = true;
    }

    printf("Message queue initialized\n");
}

uint32_t msg_get_tick_count(void)
{
    if (!g_time_initialized) {
        gettimeofday(&g_start_time, NULL);
        g_time_initialized = true;
    }

    struct timeval now;
    gettimeofday(&now, NULL);

    uint32_t ms = (uint32_t)((now.tv_sec - g_start_time.tv_sec) * 1000 +
                              (now.tv_usec - g_start_time.tv_usec) / 1000);
    return ms;
}

bool msg_queue_post(uint32_t hwnd, uint32_t message, uint32_t wParam, uint32_t lParam)
{
    if (g_msg_queue.count >= 256) {
        fprintf(stderr, "msg_queue_post: queue full!\n");
        return false;
    }

    WBOX_MSG *msg = &g_msg_queue.messages[g_msg_queue.tail];
    msg->hwnd = hwnd;
    msg->message = message;
    msg->wParam = wParam;
    msg->lParam = lParam;
    msg->time = msg_get_tick_count();
    msg->pt_x = g_msg_queue.mouseX;
    msg->pt_y = g_msg_queue.mouseY;

    g_msg_queue.tail = (g_msg_queue.tail + 1) % 256;
    g_msg_queue.count++;

    return true;
}

void msg_queue_post_quit(int exitCode)
{
    g_msg_queue.quitPosted = true;
    g_msg_queue.exitCode = exitCode;

    /* Post WM_QUIT to the queue */
    msg_queue_post(0, WM_QUIT, exitCode, 0);
}

/*
 * Check if a message matches the filter criteria
 */
static bool msg_matches_filter(const WBOX_MSG *msg, uint32_t hwndFilter,
                               uint32_t msgFilterMin, uint32_t msgFilterMax)
{
    /* hwndFilter: 0 = all, -1 = thread-specific, else = specific window */
    if (hwndFilter != 0 && hwndFilter != (uint32_t)-1) {
        if (msg->hwnd != hwndFilter) {
            /* Also check if hwndFilter is parent of msg->hwnd */
            WBOX_WND *filter_wnd = user_window_from_hwnd(hwndFilter);
            WBOX_WND *msg_wnd = user_window_from_hwnd(msg->hwnd);
            if (!filter_wnd || !msg_wnd) {
                return false;
            }
            /* Check if msg_wnd is a child of filter_wnd */
            bool is_child = false;
            WBOX_WND *parent = msg_wnd->spwndParent;
            while (parent) {
                if (parent == filter_wnd) {
                    is_child = true;
                    break;
                }
                parent = parent->spwndParent;
            }
            if (!is_child) {
                return false;
            }
        }
    }

    /* Message filter: 0,0 = no filter */
    if (msgFilterMin != 0 || msgFilterMax != 0) {
        if (msg->message < msgFilterMin || msg->message > msgFilterMax) {
            return false;
        }
    }

    return true;
}

/*
 * Find the first window that needs painting
 */
static WBOX_WND *find_window_needing_paint(void)
{
    /* Walk all windows and check for update region */
    WBOX_WND *desktop = user_window_get_desktop();
    if (!desktop) return NULL;

    /* Simple DFS through window hierarchy */
    WBOX_WND *wnd = desktop->spwndChild;
    while (wnd) {
        /* Check if this window needs painting */
        if (user_window_is_visible(wnd) &&
            (wnd->state & (WNDS_SENDNCPAINT | WNDS_SENDERASEBACKGROUND | WNDS_NONCPAINT | WNDS_ERASEBACKGROUND))) {
            return wnd;
        }

        /* Check children first (depth-first) */
        if (wnd->spwndChild) {
            wnd = wnd->spwndChild;
        } else if (wnd->spwndNext) {
            wnd = wnd->spwndNext;
        } else {
            /* Go back up and find next sibling */
            while (wnd->spwndParent && wnd->spwndParent != desktop && !wnd->spwndParent->spwndNext) {
                wnd = wnd->spwndParent;
            }
            if (wnd->spwndParent && wnd->spwndParent != desktop) {
                wnd = wnd->spwndParent->spwndNext;
            } else {
                break;
            }
        }
    }

    return NULL;
}

bool msg_queue_peek(WBOX_MSG *out_msg, uint32_t hwndFilter,
                    uint32_t msgFilterMin, uint32_t msgFilterMax,
                    uint32_t flags)
{
    bool remove = (flags & PM_REMOVE) != 0;

    /* First, check posted messages */
    int i = g_msg_queue.head;
    int checked = 0;
    while (checked < g_msg_queue.count) {
        WBOX_MSG *msg = &g_msg_queue.messages[i];

        if (msg_matches_filter(msg, hwndFilter, msgFilterMin, msgFilterMax)) {
            /* Found a matching message */
            if (out_msg) {
                *out_msg = *msg;
            }

            if (remove) {
                /* Remove from queue - shift remaining messages */
                if (i == g_msg_queue.head) {
                    g_msg_queue.head = (g_msg_queue.head + 1) % 256;
                } else {
                    /* Need to compact the queue - not ideal but simple */
                    int src = (i + 1) % 256;
                    int dst = i;
                    int remaining = g_msg_queue.count - checked - 1;
                    for (int j = 0; j < remaining; j++) {
                        g_msg_queue.messages[dst] = g_msg_queue.messages[src];
                        dst = (dst + 1) % 256;
                        src = (src + 1) % 256;
                    }
                    g_msg_queue.tail = (g_msg_queue.tail - 1 + 256) % 256;
                }
                g_msg_queue.count--;
            }

            return true;
        }

        i = (i + 1) % 256;
        checked++;
    }

    /* If no filter or filter includes WM_PAINT, check for windows needing paint */
    if (msgFilterMin == 0 && msgFilterMax == 0 ||
        (WM_PAINT >= msgFilterMin && WM_PAINT <= msgFilterMax)) {

        WBOX_WND *paint_wnd = find_window_needing_paint();
        if (paint_wnd && (hwndFilter == 0 || hwndFilter == paint_wnd->hwnd)) {
            /* Synthesize WM_PAINT message */
            if (out_msg) {
                out_msg->hwnd = paint_wnd->hwnd;
                out_msg->message = WM_PAINT;
                out_msg->wParam = 0;
                out_msg->lParam = 0;
                out_msg->time = msg_get_tick_count();
                out_msg->pt_x = g_msg_queue.mouseX;
                out_msg->pt_y = g_msg_queue.mouseY;
            }
            /* Note: WM_PAINT is not removed - it persists until validated */
            return true;
        }
    }

    return false;
}

bool msg_queue_has_messages(void)
{
    return g_msg_queue.count > 0 || find_window_needing_paint() != NULL;
}

void msg_write_to_guest(vm_context_t *vm, uint32_t guest_addr, const WBOX_MSG *msg)
{
    if (!vm || guest_addr == 0 || !msg) return;

    /* MSG structure layout (28 bytes):
     * +0: HWND hwnd
     * +4: UINT message
     * +8: WPARAM wParam
     * +12: LPARAM lParam
     * +16: DWORD time
     * +20: POINT pt (x, y - 4 bytes each)
     */
    uint32_t phys = paging_get_phys(&vm->paging, guest_addr);
    if (!phys) return;

    mem_writel_phys(phys + 0, msg->hwnd);
    mem_writel_phys(phys + 4, msg->message);
    mem_writel_phys(phys + 8, msg->wParam);
    mem_writel_phys(phys + 12, msg->lParam);
    mem_writel_phys(phys + 16, msg->time);
    mem_writel_phys(phys + 20, (uint32_t)msg->pt_x);
    mem_writel_phys(phys + 24, (uint32_t)msg->pt_y);
}

void msg_read_from_guest(vm_context_t *vm, uint32_t guest_addr, WBOX_MSG *msg)
{
    if (!vm || guest_addr == 0 || !msg) return;

    uint32_t phys = paging_get_phys(&vm->paging, guest_addr);
    if (!phys) return;

    msg->hwnd = mem_readl_phys(phys + 0);
    msg->message = mem_readl_phys(phys + 4);
    msg->wParam = mem_readl_phys(phys + 8);
    msg->lParam = mem_readl_phys(phys + 12);
    msg->time = mem_readl_phys(phys + 16);
    msg->pt_x = (int32_t)mem_readl_phys(phys + 20);
    msg->pt_y = (int32_t)mem_readl_phys(phys + 24);
}
