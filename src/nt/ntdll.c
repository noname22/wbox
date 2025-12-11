/*
 * WBOX NT Syscall Dispatcher
 * Handles SYSENTER calls from userspace, dispatches to syscall implementations
 */
#include "syscalls.h"
#include "../cpu/cpu.h"
#include "../cpu/mem.h"
#include "../vm/vm.h"

#include <stdio.h>

/*
 * Return from syscall to user mode
 * Sets EAX to return value and jumps back to user code
 */
static void syscall_return(ntstatus_t status)
{
    /* Set return value */
    EAX = status;

    /* Read return address from user stack (EDX points to stack) */
    uint32_t return_addr = readmemll(EDX);
    cpu_state.pc = return_addr;

    /* Segment registers are already set for Ring 3 from VM setup */
}

/*
 * NT syscall handler - called when SYSENTER is executed
 * Returns 1 to skip the normal SYSENTER processing
 */
int nt_syscall_handler(void)
{
    uint32_t syscall_num = EAX;
    ntstatus_t result;

    /* Dispatch to specific syscall handler */
    switch (syscall_num) {
        case NtClose:
            result = sys_NtClose();
            syscall_return(result);
            return 1;

        case NtCreateFile:
            result = sys_NtCreateFile();
            syscall_return(result);
            return 1;

        case NtOpenFile:
            result = sys_NtOpenFile();
            syscall_return(result);
            return 1;

        case NtReadFile:
            result = sys_NtReadFile();
            syscall_return(result);
            return 1;

        case NtWriteFile:
            result = sys_NtWriteFile();
            syscall_return(result);
            return 1;

        case NtTerminateProcess:
            result = sys_NtTerminateProcess();
            /* NtTerminateProcess exits, no return to user mode */
            return 1;

        default:
            /* Unimplemented syscall - print info and exit */
            printf("\n=== UNIMPLEMENTED SYSCALL ===\n");
            printf("Number: 0x%03X (%d)\n", syscall_num, syscall_num);
            printf("Name:   %s\n", syscall_get_name(syscall_num));
            printf("\nRegisters:\n");
            printf("  EAX=%08X (syscall number)\n", EAX);
            printf("  EDX=%08X (args pointer)\n", EDX);
            printf("  ESP=%08X EBP=%08X\n", ESP, EBP);

            /* Exit with STATUS_NOT_IMPLEMENTED */
            EAX = STATUS_NOT_IMPLEMENTED;
            vm_context_t *vm = vm_get_context();
            if (vm) {
                vm_request_exit(vm, STATUS_NOT_IMPLEMENTED);
            }
            cpu_exit_requested = 1;
            return 1;
    }
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
