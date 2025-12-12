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
 * Read syscall argument from user stack.
 * After SYSENTER, the stack layout is:
 *   ESP+0  = return address (from syscall stub)
 *   ESP+4  = return address (from NtXxx function)
 *   ESP+8  = arg0
 *   ESP+12 = arg1
 *   ...
 */
static inline uint32_t read_stack_arg(int index)
{
    return readmemll(ESP + 8 + (index * 4));
}

/*
 * NtTerminateProcess - Terminate a process
 *
 * Arguments:
 *   arg0 = ProcessHandle (NULL or -1 for current process)
 *   arg1 = ExitStatus
 */
ntstatus_t sys_NtTerminateProcess(void)
{
    /* Read arguments from user stack */
    uint32_t process_handle = read_stack_arg(0);
    uint32_t exit_status    = read_stack_arg(1);

    fprintf(stderr, "SYSCALL: NtTerminateProcess(handle=0x%X, exit_status=0x%X) at PC=0x%08X\n",
            process_handle, exit_status, cpu_state.pc);

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
 * Arguments:
 *   arg0 = PerformanceCounter pointer (receives counter value)
 *   arg1 = PerformanceFrequency pointer (optional, receives frequency)
 *
 * Returns: STATUS_SUCCESS or error code
 */
ntstatus_t sys_NtQueryPerformanceCounter(void)
{
    /* Read arguments from user stack */
    uint32_t counter_ptr   = read_stack_arg(0);
    uint32_t frequency_ptr = read_stack_arg(1);

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
