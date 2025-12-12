/*
 * WBOX User Callback Mechanism
 * Allows kernel to invoke guest window procedures
 */
#ifndef WBOX_USER_CALLBACK_H
#define WBOX_USER_CALLBACK_H

#include <stdint.h>
#include <stdbool.h>

/* Forward declarations */
typedef struct vm_context vm_context_t;
typedef struct _WBOX_WND WBOX_WND;

/* Callback state - tracks nested callbacks */
typedef struct _WBOX_CALLBACK_STATE {
    /* Saved CPU state */
    uint32_t saved_eip;
    uint32_t saved_esp;
    uint32_t saved_eax;
    uint32_t saved_ebx;
    uint32_t saved_ecx;
    uint32_t saved_edx;
    uint32_t saved_esi;
    uint32_t saved_edi;
    uint32_t saved_ebp;

    /* Callback status */
    bool active;
    bool completed;
    uint32_t result;
} WBOX_CALLBACK_STATE;

/* Maximum callback nesting depth */
#define MAX_CALLBACK_DEPTH 16

/* Callback stack for nested calls */
extern WBOX_CALLBACK_STATE g_callback_stack[MAX_CALLBACK_DEPTH];
extern int g_callback_depth;

/*
 * Initialize callback subsystem
 * Creates the return stub in guest memory
 */
int user_callback_init(vm_context_t *vm);

/*
 * Call a window procedure
 * Saves CPU state, sets up stdcall frame, executes until return
 *
 * Parameters:
 *   vm      - VM context
 *   wnd     - Window object (contains lpfnWndProc)
 *   msg     - Message ID (WM_*)
 *   wParam  - Word parameter
 *   lParam  - Long parameter
 *
 * Returns: LRESULT from window procedure
 */
uint32_t user_call_wndproc(vm_context_t *vm, WBOX_WND *wnd,
                           uint32_t msg, uint32_t wParam, uint32_t lParam);

/*
 * Call a window procedure by address
 * Like user_call_wndproc but takes explicit wndproc address and hwnd
 */
uint32_t user_call_wndproc_addr(vm_context_t *vm, uint32_t wndproc,
                                uint32_t hwnd, uint32_t msg,
                                uint32_t wParam, uint32_t lParam);

/*
 * Handle callback return syscall
 * Called when guest code returns from WndProc
 */
void user_callback_return(uint32_t result);

/*
 * Check if we're currently in a callback
 */
bool user_callback_active(void);

/*
 * Get current callback depth
 */
int user_callback_get_depth(void);

#endif /* WBOX_USER_CALLBACK_H */
