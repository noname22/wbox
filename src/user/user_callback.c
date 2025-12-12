/*
 * WBOX User Callback Implementation
 * Invokes guest window procedures from kernel
 */
#include "user_callback.h"
#include "user_window.h"
#include "guest_wnd.h"
#include "../nt/syscalls.h"
#include "../vm/vm.h"
#include "../vm/paging.h"
#include "../process/process.h"
#include "../cpu/cpu.h"
#include "../cpu/mem.h"

#include <stdio.h>
#include <string.h>

/* TEB address (standard Windows) */
#define TEB_VA  0x7FFDF000

/*
 * Set the CallbackWnd cache in TEB.Win32ClientInfo
 * This allows ValidateHwnd() to find the WND during callbacks
 */
static void set_callbackwnd_cache(uint32_t hwnd, uint32_t guest_wnd_va)
{
    uint32_t clientinfo_va = TEB_VA + TEB_WIN32_CLIENT_INFO;
    writememll(clientinfo_va + CI_CALLBACKWND_HWND, hwnd);
    writememll(clientinfo_va + CI_CALLBACKWND_PWND, guest_wnd_va);
    writememll(clientinfo_va + CI_CALLBACKWND_PACTCTX, 0);
}

/*
 * Clear the CallbackWnd cache
 */
static void clear_callbackwnd_cache(void)
{
    uint32_t clientinfo_va = TEB_VA + TEB_WIN32_CLIENT_INFO;
    writememll(clientinfo_va + CI_CALLBACKWND_HWND, 0);
    writememll(clientinfo_va + CI_CALLBACKWND_PWND, 0);
    writememll(clientinfo_va + CI_CALLBACKWND_PACTCTX, 0);
}

/* Callback stack for nested calls */
WBOX_CALLBACK_STATE g_callback_stack[MAX_CALLBACK_DEPTH];
int g_callback_depth = 0;

/* Address of the WndProc return stub in guest memory */
static uint32_t g_wndproc_return_stub = 0;

/* Flag to request callback exit */
static volatile bool g_callback_exit_requested = false;

int user_callback_init(vm_context_t *vm)
{
    /* Create WndProc return stub in KUSD page
     * Code: B8 FD FF 00 00 (MOV EAX, 0xFFFD), 0F 34 (SYSENTER), CC (INT3)
     * The return value from WndProc will already be in EAX
     */
    uint32_t kusd_va = 0x7FFE0000;
    uint32_t stub_offset = 0x360;  /* After DLL init stub at 0x350 */
    uint32_t stub_va = kusd_va + stub_offset;

    uint32_t kusd_phys = paging_get_phys(&vm->paging, kusd_va);
    if (!kusd_phys) {
        fprintf(stderr, "user_callback_init: KUSD not mapped\n");
        return -1;
    }

    /* Write the stub code:
     * When WndProc returns via RET, EAX contains the return value.
     * We need to save it and pass it to the syscall handler.
     *
     *   89 C1       ; MOV ECX, EAX (save return value to ECX)
     *   B8 FD FF 00 00 ; MOV EAX, 0x0000FFFD (syscall number)
     *   0F 34       ; SYSENTER
     *   CC          ; INT3 - should never reach
     */
    mem_writeb_phys(kusd_phys + stub_offset + 0, 0x89);  /* MOV ECX, EAX */
    mem_writeb_phys(kusd_phys + stub_offset + 1, 0xC1);
    mem_writeb_phys(kusd_phys + stub_offset + 2, 0xB8);  /* MOV EAX, imm32 */
    mem_writeb_phys(kusd_phys + stub_offset + 3, 0xFD);  /* 0x0000FFFD low byte */
    mem_writeb_phys(kusd_phys + stub_offset + 4, 0xFF);
    mem_writeb_phys(kusd_phys + stub_offset + 5, 0x00);
    mem_writeb_phys(kusd_phys + stub_offset + 6, 0x00);
    mem_writeb_phys(kusd_phys + stub_offset + 7, 0x0F);  /* SYSENTER */
    mem_writeb_phys(kusd_phys + stub_offset + 8, 0x34);
    mem_writeb_phys(kusd_phys + stub_offset + 9, 0xCC);  /* INT3 - should never reach */

    g_wndproc_return_stub = stub_va;
    g_callback_depth = 0;

    printf("WndProc return stub at 0x%08X\n", stub_va);
    return 0;
}

uint32_t user_call_wndproc(vm_context_t *vm, WBOX_WND *wnd,
                           uint32_t msg, uint32_t wParam, uint32_t lParam)
{
    if (!wnd) return 0;

    /* Get WndProc address */
    uint32_t wndproc = wnd->lpfnWndProc;
    if (wndproc == 0 && wnd->pcls) {
        wndproc = wnd->pcls->lpfnWndProc;
    }

    if (wndproc == 0) {
        return 0;
    }

    return user_call_wndproc_addr(vm, wndproc, wnd->hwnd, msg, wParam, lParam);
}

/*
 * WINDOWPROC_CALLBACK_ARGUMENTS structure offsets (from ReactOS callback.h)
 * Total size: 32 bytes
 */
#define WPCB_PROC           0   /* WNDPROC - actual WndProc to call */
#define WPCB_ISANSIPROC     4   /* BOOL - FALSE for Unicode */
#define WPCB_WND            8   /* HWND - window handle */
#define WPCB_MSG           12   /* UINT - message */
#define WPCB_WPARAM        16   /* WPARAM */
#define WPCB_LPARAM        20   /* LPARAM */
#define WPCB_LPARAMBUFSIZE 24   /* INT - -1 for simple messages */
#define WPCB_RESULT        28   /* LRESULT - output result */
#define WPCB_SIZE          32

/* CREATESTRUCTW offsets (48 bytes total) */
#define CS_LPCREATEPARAMS   0x00
#define CS_HINSTANCE        0x04
#define CS_HMENU            0x08
#define CS_HWNDPARENT       0x0C
#define CS_CY               0x10
#define CS_CX               0x14
#define CS_Y                0x18
#define CS_X                0x1C
#define CS_STYLE            0x20
#define CS_LPSZNAME         0x24
#define CS_LPSZCLASS        0x28
#define CS_DWEXSTYLE        0x2C
#define CREATESTRUCTW_SIZE  0x30  /* 48 bytes */

/* Message IDs */
#define WM_CREATE           0x0001
#define WM_NCCREATE         0x0081

/* Helper to read wide string length from guest memory */
static uint32_t read_guest_wstr_len(uint32_t va)
{
    if (va == 0) return 0;
    /* Check if this is an atom (HIWORD == 0, LOWORD != 0) */
    if ((va & 0xFFFF0000) == 0 && (va & 0xFFFF) != 0) {
        return 0;  /* Atom, not a string */
    }
    uint32_t len = 0;
    while (len < 512) {  /* Safety limit */
        uint16_t ch = readmemwl(va + len * 2);
        if (ch == 0) break;
        len++;
    }
    return len;
}

/* Check if a value is an atom (HIWORD == 0, LOWORD != 0) */
static inline bool is_atom(uint32_t val)
{
    return (val & 0xFFFF0000) == 0 && (val & 0xFFFF) != 0;
}

/* PEB offset for KernelCallbackTable (32-bit) */
#define PEB_KERNELCALLBACKTABLE 0x2C

/* Callback index for window procedure */
#define USER32_CALLBACK_WINDOWPROC 0

uint32_t user_call_wndproc_addr(vm_context_t *vm, uint32_t wndproc,
                                uint32_t hwnd, uint32_t msg,
                                uint32_t wParam, uint32_t lParam)
{
    if (!vm || wndproc == 0) return 0;

    /* Check callback depth */
    if (g_callback_depth >= MAX_CALLBACK_DEPTH) {
        fprintf(stderr, "user_call_wndproc: Callback depth exceeded!\n");
        return 0;
    }

    /* Check that return stub is initialized */
    if (g_wndproc_return_stub == 0) {
        /* Initialize on first use */
        if (user_callback_init(vm) != 0) {
            return 0;
        }
    }

    /* Get PEB address from TEB (TEB+0x30 = PEB pointer) */
    uint32_t teb_va = 0x7FFDF000;  /* Standard TEB address */
    uint32_t peb_va = readmemll(teb_va + 0x30);
    if (peb_va == 0) {
        fprintf(stderr, "user_call_wndproc: Cannot read PEB address from TEB\n");
        return 0;
    }

    /* Read KernelCallbackTable from PEB */
    uint32_t callback_table = readmemll(peb_va + PEB_KERNELCALLBACKTABLE);
    if (callback_table == 0) {
        fprintf(stderr, "user_call_wndproc: KernelCallbackTable not set - falling back to direct call\n");
        /* Fall back to direct WndProc call (old behavior) */
        goto direct_call;
    }

    /* Read User32CallWindowProcFromKernel address from callback table */
    uint32_t callback_handler = readmemll(callback_table + USER32_CALLBACK_WINDOWPROC * 4);
    if (callback_handler == 0) {
        fprintf(stderr, "user_call_wndproc: WindowProc callback handler not set\n");
        goto direct_call;
    }

    fprintf(stderr, "USER: Using kernel callback mechanism (handler=0x%08X)\n", callback_handler);

    /* Get current callback state slot */
    WBOX_CALLBACK_STATE *state = &g_callback_stack[g_callback_depth];
    g_callback_depth++;

    /* Save current CPU state */
    state->saved_eip = cpu_state.pc;
    state->saved_esp = ESP;
    state->saved_eax = EAX;
    state->saved_ebx = EBX;
    state->saved_ecx = ECX;
    state->saved_edx = EDX;
    state->saved_esi = ESI;
    state->saved_edi = EDI;
    state->saved_ebp = EBP;
    state->active = true;
    state->completed = false;
    state->result = 0;

    /* Calculate argument size and allocate on stack
     * For WM_NCCREATE/WM_CREATE, we need to serialize CREATESTRUCT with strings
     * Buffer layout: [WPCB_ARGS(32)][CREATESTRUCT(48)][lpszName][lpszClass]
     */
    uint32_t args_va;
    uint32_t arg_length;
    int32_t lparam_buf_size = -1;  /* -1 = no extra buffer */

    if ((msg == WM_NCCREATE || msg == WM_CREATE) && lParam != 0) {
        /* Read CREATESTRUCT from guest memory */
        uint32_t cs_lpCreateParams = readmemll(lParam + CS_LPCREATEPARAMS);
        uint32_t cs_hInstance      = readmemll(lParam + CS_HINSTANCE);
        uint32_t cs_hMenu          = readmemll(lParam + CS_HMENU);
        uint32_t cs_hwndParent     = readmemll(lParam + CS_HWNDPARENT);
        uint32_t cs_cy             = readmemll(lParam + CS_CY);
        uint32_t cs_cx             = readmemll(lParam + CS_CX);
        uint32_t cs_y              = readmemll(lParam + CS_Y);
        uint32_t cs_x              = readmemll(lParam + CS_X);
        uint32_t cs_style          = readmemll(lParam + CS_STYLE);
        uint32_t cs_lpszName       = readmemll(lParam + CS_LPSZNAME);
        uint32_t cs_lpszClass      = readmemll(lParam + CS_LPSZCLASS);
        uint32_t cs_dwExStyle      = readmemll(lParam + CS_DWEXSTYLE);

        /* Check if lpszName or lpszClass is an atom */
        bool name_is_atom = is_atom(cs_lpszName);
        bool class_is_atom = is_atom(cs_lpszClass);

        fprintf(stderr, "USER: CREATESTRUCT lpszName=0x%X (atom=%d), lpszClass=0x%X (atom=%d)\n",
                cs_lpszName, name_is_atom, cs_lpszClass, class_is_atom);

        /* Calculate string sizes (including null terminators) */
        uint32_t name_len = name_is_atom ? 0 : read_guest_wstr_len(cs_lpszName);
        uint32_t class_len = class_is_atom ? 0 : read_guest_wstr_len(cs_lpszClass);
        uint32_t name_bytes = name_is_atom ? 0 : (name_len + 1) * 2;
        uint32_t class_bytes = class_is_atom ? 0 : (class_len + 1) * 2;

        /* Buffer size: CREATESTRUCT + strings */
        lparam_buf_size = CREATESTRUCTW_SIZE + name_bytes + class_bytes;
        arg_length = WPCB_SIZE + lparam_buf_size;

        /* Allocate buffer on stack */
        ESP -= arg_length;
        args_va = ESP;

        /* String offsets relative to start of extra data (after WPCB_ARGS)
         * For atoms, we keep the original atom value instead of an offset */
        uint32_t name_offset = name_is_atom ? cs_lpszName : CREATESTRUCTW_SIZE;
        uint32_t class_offset = class_is_atom ? cs_lpszClass : (CREATESTRUCTW_SIZE + name_bytes);

        /* Fill WINDOWPROC_CALLBACK_ARGUMENTS */
        writememll(args_va + WPCB_PROC, wndproc);
        writememll(args_va + WPCB_ISANSIPROC, 0);
        writememll(args_va + WPCB_WND, hwnd);
        writememll(args_va + WPCB_MSG, msg);
        writememll(args_va + WPCB_WPARAM, wParam);
        writememll(args_va + WPCB_LPARAM, lParam);  /* Original lParam */
        writememll(args_va + WPCB_LPARAMBUFSIZE, lparam_buf_size);
        writememll(args_va + WPCB_RESULT, 0);

        /* Write CREATESTRUCT at args_va + WPCB_SIZE */
        uint32_t cs_va = args_va + WPCB_SIZE;
        writememll(cs_va + CS_LPCREATEPARAMS, cs_lpCreateParams);
        writememll(cs_va + CS_HINSTANCE, cs_hInstance);
        writememll(cs_va + CS_HMENU, cs_hMenu);
        writememll(cs_va + CS_HWNDPARENT, cs_hwndParent);
        writememll(cs_va + CS_CY, cs_cy);
        writememll(cs_va + CS_CX, cs_cx);
        writememll(cs_va + CS_Y, cs_y);
        writememll(cs_va + CS_X, cs_x);
        writememll(cs_va + CS_STYLE, cs_style);
        writememll(cs_va + CS_LPSZNAME, name_offset);    /* Relative offset */
        writememll(cs_va + CS_LPSZCLASS, class_offset);  /* Relative offset */
        writememll(cs_va + CS_DWEXSTYLE, cs_dwExStyle);

        /* Copy lpszName string (skip if atom) */
        if (!name_is_atom && name_len > 0) {
            uint32_t dst = cs_va + CREATESTRUCTW_SIZE;  /* Fixed position for name */
            for (uint32_t i = 0; i <= name_len; i++) {
                uint16_t ch = (i < name_len) ? readmemwl(cs_lpszName + i * 2) : 0;
                writememwl(dst + i * 2, ch);
            }
        }

        /* Copy lpszClass string (skip if atom) */
        if (!class_is_atom && class_len > 0) {
            uint32_t dst = cs_va + CREATESTRUCTW_SIZE + name_bytes;  /* After name string */
            for (uint32_t i = 0; i <= class_len; i++) {
                uint16_t ch = (i < class_len) ? readmemwl(cs_lpszClass + i * 2) : 0;
                writememwl(dst + i * 2, ch);
            }
        }

        fprintf(stderr, "USER: Serialized CREATESTRUCT for msg=0x%X: name_off=%d, class_off=%d, buf_size=%d\n",
                msg, name_offset, class_offset, lparam_buf_size);
    } else {
        /* Simple message - no extra buffer */
        arg_length = WPCB_SIZE;
        ESP -= arg_length;
        args_va = ESP;

        writememll(args_va + WPCB_PROC, wndproc);
        writememll(args_va + WPCB_ISANSIPROC, 0);
        writememll(args_va + WPCB_WND, hwnd);
        writememll(args_va + WPCB_MSG, msg);
        writememll(args_va + WPCB_WPARAM, wParam);
        writememll(args_va + WPCB_LPARAM, lParam);
        writememll(args_va + WPCB_LPARAMBUFSIZE, -1);
        writememll(args_va + WPCB_RESULT, 0);
    }

    /* Store args_va in state for NtCallbackReturn to find */
    state->callback_args_va = args_va;

    /* Set up stdcall call to User32CallWindowProcFromKernel(Arguments, ArgumentLength)
     * Stack layout:
     *   [ESP+8]  ArgumentLength
     *   [ESP+4]  Arguments (pointer to WINDOWPROC_CALLBACK_ARGUMENTS)
     *   [ESP+0]  return address
     */
    ESP -= 4;
    writememll(ESP, arg_length);          /* ArgumentLength */
    ESP -= 4;
    writememll(ESP, args_va);             /* Arguments pointer */
    ESP -= 4;
    writememll(ESP, g_wndproc_return_stub);  /* Return address */

    /* Jump to callback handler */
    cpu_state.pc = callback_handler;

    fprintf(stderr, "USER: Calling callback 0x%08X(args=0x%X, len=%d) for WndProc 0x%08X(hwnd=0x%X, msg=0x%X)\n",
            callback_handler, args_va, arg_length, wndproc, hwnd, msg);

    /* Set CallbackWnd cache so ValidateHwnd can find the WND */
    WBOX_WND *wnd = user_window_from_hwnd(hwnd);
    uint32_t guest_wnd_va = wnd ? wnd->guest_wnd_va : 0;
    if (guest_wnd_va) {
        set_callbackwnd_cache(hwnd, guest_wnd_va);
        fprintf(stderr, "USER: Set CallbackWnd cache hwnd=0x%X pwnd=0x%X\n", hwnd, guest_wnd_va);
    }

    /* Clear exit flag */
    g_callback_exit_requested = false;

    /* Execute guest code until callback completes */
    int max_iterations = 10000000;  /* Safety limit */
    int iter = 0;

    while (!state->completed && !vm->exit_requested && iter < max_iterations) {
        exec386(1000);
        iter++;
    }

    if (iter >= max_iterations) {
        fprintf(stderr, "user_call_wndproc: Callback timeout! hwnd=0x%X msg=0x%X\n",
                hwnd, msg);
    }

    /* Get result (stored by user_callback_return) */
    uint32_t result = state->result;
    fprintf(stderr, "USER: WndProc returned 0x%X (iter=%d, completed=%d)\n",
            result, iter, state->completed);

    /* Clear CallbackWnd cache */
    if (guest_wnd_va) {
        clear_callbackwnd_cache();
    }

    /* Restore CPU state */
    cpu_state.pc = state->saved_eip;
    ESP = state->saved_esp;
    EBX = state->saved_ebx;
    ECX = state->saved_ecx;
    EDX = state->saved_edx;
    ESI = state->saved_esi;
    EDI = state->saved_edi;
    EBP = state->saved_ebp;

    state->active = false;
    g_callback_depth--;

    /* Reset CPU exit flag since we're returning from a controlled callback */
    cpu_exit_requested = 0;

    return result;

direct_call:
    /* Fallback: direct WndProc call (old behavior) */
    {
        WBOX_CALLBACK_STATE *state = &g_callback_stack[g_callback_depth];
        g_callback_depth++;

        state->saved_eip = cpu_state.pc;
        state->saved_esp = ESP;
        state->saved_eax = EAX;
        state->saved_ebx = EBX;
        state->saved_ecx = ECX;
        state->saved_edx = EDX;
        state->saved_esi = ESI;
        state->saved_edi = EDI;
        state->saved_ebp = EBP;
        state->active = true;
        state->completed = false;
        state->result = 0;
        state->callback_args_va = 0;

        /* Set up stdcall stack frame for WndProc(hwnd, msg, wParam, lParam) */
        ESP -= 4; writememll(ESP, lParam);
        ESP -= 4; writememll(ESP, wParam);
        ESP -= 4; writememll(ESP, msg);
        ESP -= 4; writememll(ESP, hwnd);
        ESP -= 4; writememll(ESP, g_wndproc_return_stub);

        cpu_state.pc = wndproc;

        fprintf(stderr, "USER: Direct call WndProc 0x%08X(hwnd=0x%X, msg=0x%X, wParam=0x%X, lParam=0x%X)\n",
                wndproc, hwnd, msg, wParam, lParam);

        /* Set CallbackWnd cache */
        WBOX_WND *dc_wnd = user_window_from_hwnd(hwnd);
        uint32_t dc_guest_wnd_va = dc_wnd ? dc_wnd->guest_wnd_va : 0;
        if (dc_guest_wnd_va) {
            set_callbackwnd_cache(hwnd, dc_guest_wnd_va);
        }

        g_callback_exit_requested = false;
        int max_iterations = 10000000;
        int iter = 0;

        while (!state->completed && !vm->exit_requested && iter < max_iterations) {
            exec386(1000);
            iter++;
        }

        uint32_t result = state->result;
        fprintf(stderr, "USER: WndProc returned 0x%X (iter=%d, completed=%d)\n",
                result, iter, state->completed);

        /* Clear CallbackWnd cache */
        if (dc_guest_wnd_va) {
            clear_callbackwnd_cache();
        }

        cpu_state.pc = state->saved_eip;
        ESP = state->saved_esp;
        EBX = state->saved_ebx;
        ECX = state->saved_ecx;
        EDX = state->saved_edx;
        ESI = state->saved_esi;
        EDI = state->saved_edi;
        EBP = state->saved_ebp;

        state->active = false;
        g_callback_depth--;
        cpu_exit_requested = 0;

        return result;
    }
}

void user_callback_return(uint32_t result)
{
    if (g_callback_depth <= 0) {
        fprintf(stderr, "user_callback_return: No active callback!\n");
        return;
    }

    /* Get current callback state */
    WBOX_CALLBACK_STATE *state = &g_callback_stack[g_callback_depth - 1];

    /* Store result and mark as completed */
    state->result = result;
    state->completed = true;

    /* Stop execution to return to the callback caller */
    cpu_exit_requested = 1;
}

bool user_callback_active(void)
{
    return g_callback_depth > 0;
}

int user_callback_get_depth(void)
{
    return g_callback_depth;
}
