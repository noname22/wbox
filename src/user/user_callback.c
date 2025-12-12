/*
 * WBOX User Callback Implementation
 * Invokes guest window procedures from kernel
 */
#include "user_callback.h"
#include "user_window.h"
#include "../nt/syscalls.h"
#include "../vm/vm.h"
#include "../vm/paging.h"
#include "../cpu/cpu.h"
#include "../cpu/mem.h"

#include <stdio.h>
#include <string.h>

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

    /* Write the stub code */
    mem_writeb_phys(kusd_phys + stub_offset + 0, 0xB8);  /* MOV EAX, imm32 */
    mem_writeb_phys(kusd_phys + stub_offset + 1, 0xFD);  /* 0x0000FFFD low byte */
    mem_writeb_phys(kusd_phys + stub_offset + 2, 0xFF);
    mem_writeb_phys(kusd_phys + stub_offset + 3, 0x00);
    mem_writeb_phys(kusd_phys + stub_offset + 4, 0x00);
    mem_writeb_phys(kusd_phys + stub_offset + 5, 0x0F);  /* SYSENTER */
    mem_writeb_phys(kusd_phys + stub_offset + 6, 0x34);
    mem_writeb_phys(kusd_phys + stub_offset + 7, 0xCC);  /* INT3 - should never reach */

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

    /* Set up stdcall stack frame for WndProc(hwnd, msg, wParam, lParam)
     * Stack layout (high to low):
     *   [ESP+16] lParam
     *   [ESP+12] wParam
     *   [ESP+8]  uMsg
     *   [ESP+4]  hwnd
     *   [ESP+0]  return address
     */
    ESP -= 4;
    writememll(ESP, lParam);
    ESP -= 4;
    writememll(ESP, wParam);
    ESP -= 4;
    writememll(ESP, msg);
    ESP -= 4;
    writememll(ESP, hwnd);
    ESP -= 4;
    writememll(ESP, g_wndproc_return_stub);  /* Return to our stub */

    /* Jump to WndProc */
    cpu_state.pc = wndproc;

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

    /* Restore CPU state */
    cpu_state.pc = state->saved_eip;
    ESP = state->saved_esp;
    /* Don't restore EAX - leave result there */
    EBX = state->saved_ebx;
    ECX = state->saved_ecx;
    EDX = state->saved_edx;
    ESI = state->saved_esi;
    EDI = state->saved_edi;
    EBP = state->saved_ebp;

    state->active = false;
    g_callback_depth--;

    return result;
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
