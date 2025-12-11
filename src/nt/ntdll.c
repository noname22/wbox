/*
 * WBOX NT Syscall Dispatcher
 * Handles SYSENTER calls from userspace, prints syscall name, and exits
 */
#include "syscalls.h"
#include "../cpu/cpu.h"
#include "../vm/vm.h"

#include <stdio.h>

/*
 * NT syscall handler - called when SYSENTER is executed
 * Returns 1 to skip the normal SYSENTER processing
 */
int nt_syscall_handler(void)
{
    uint32_t syscall_num = EAX;
    const char *name = syscall_get_name(syscall_num);

    printf("\n=== SYSCALL ===\n");
    printf("Number: 0x%03X (%d)\n", syscall_num, syscall_num);
    printf("Name:   %s\n", name);
    printf("\nRegisters:\n");
    printf("  EAX=%08X (syscall number)\n", EAX);
    printf("  ECX=%08X (arg pointer/stack)\n", ECX);
    printf("  EDX=%08X (return address)\n", EDX);
    printf("  EBX=%08X ESI=%08X EDI=%08X\n", EBX, ESI, EDI);
    printf("  ESP=%08X EBP=%08X\n", ESP, EBP);
    printf("  EIP=%08X (before SYSENTER)\n", cpu_state.pc);

    /* Set return value to STATUS_NOT_IMPLEMENTED */
    EAX = STATUS_NOT_IMPLEMENTED;

    /* Request VM exit */
    vm_context_t *vm = vm_get_context();
    if (vm) {
        vm_request_exit(vm, STATUS_NOT_IMPLEMENTED);
    }

    /* Signal CPU loop to exit immediately */
    cpu_exit_requested = 1;

    /* Return 1 to indicate we handled the syscall and should skip normal processing */
    return 1;
}

/*
 * Install the syscall handler
 * This sets up the SYSENTER callback in the CPU emulator
 */
void nt_install_syscall_handler(void)
{
    printf("Installing NT syscall handler\n");
    sysenter_callback = nt_syscall_handler;
}

/*
 * Remove the syscall handler
 */
void nt_remove_syscall_handler(void)
{
    sysenter_callback = NULL;
}
