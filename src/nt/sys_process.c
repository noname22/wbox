/*
 * WBOX NT Process System Calls
 * NtTerminateProcess, NtQueryPerformanceCounter implementations
 */
#include "syscalls.h"
#include "../cpu/cpu.h"
#include "../cpu/mem.h"
#include "../vm/vm.h"

#include <stdio.h>
#include <time.h>

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

/*
 * NtQueryPerformanceCounter - Query high-resolution performance counter
 *
 * Arguments from user stack (EDX points to stack):
 *   [EDX+0]  = return address
 *   [EDX+4]  = PerformanceCounter pointer (receives counter value)
 *   [EDX+8]  = PerformanceFrequency pointer (optional, receives frequency)
 *
 * Returns: STATUS_SUCCESS or error code
 */
ntstatus_t sys_NtQueryPerformanceCounter(void)
{
    uint32_t args = EDX;

    /* Read arguments from user stack */
    uint32_t counter_ptr   = readmemll(args + 4);
    uint32_t frequency_ptr = readmemll(args + 8);

    /* Get current time in nanoseconds using clock_gettime */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    /* Convert to 100-nanosecond units (Windows format) */
    uint64_t counter = (uint64_t)ts.tv_sec * 10000000ULL +
                       (uint64_t)ts.tv_nsec / 100ULL;

    /* Write counter value (LARGE_INTEGER = 64-bit) */
    if (counter_ptr) {
        writememll(counter_ptr + 0, (uint32_t)(counter & 0xFFFFFFFF));       /* LowPart */
        writememll(counter_ptr + 4, (uint32_t)((counter >> 32) & 0xFFFFFFFF)); /* HighPart */
    }

    /* Write frequency if requested (10 MHz = 100ns units) */
    if (frequency_ptr) {
        uint64_t frequency = 10000000ULL;  /* 10 MHz */
        writememll(frequency_ptr + 0, (uint32_t)(frequency & 0xFFFFFFFF));
        writememll(frequency_ptr + 4, (uint32_t)((frequency >> 32) & 0xFFFFFFFF));
    }

    return STATUS_SUCCESS;
}
