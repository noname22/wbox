/*
 * WBOX NT Process System Calls
 * NtTerminateProcess implementation
 */
#include "syscalls.h"
#include "../cpu/cpu.h"
#include "../cpu/mem.h"
#include "../vm/vm.h"

#include <stdio.h>

/*
 * NtTerminateProcess - Terminate a process
 *
 * Arguments from user stack (EDX points to stack):
 *   [EDX+0]  = return address
 *   [EDX+4]  = ProcessHandle (NULL or -1 for current process)
 *   [EDX+8]  = ExitStatus
 */
ntstatus_t sys_NtTerminateProcess(void)
{
    uint32_t args = EDX;

    /* Read arguments from user stack */
    uint32_t process_handle = readmemll(args + 4);
    uint32_t exit_status    = readmemll(args + 8);

    /* NULL (0) or -1 (0xFFFFFFFF) means current process */
    if (process_handle == 0 || process_handle == 0xFFFFFFFF) {
        /* Get VM context and request exit */
        vm_context_t *vm = vm_get_context();
        if (vm) {
            vm_request_exit(vm, exit_status);
        }

        /* Signal CPU to exit immediately */
        cpu_exit_requested = 1;

        return STATUS_SUCCESS;
    }

    /* Other process handles not supported */
    return STATUS_INVALID_HANDLE;
}
