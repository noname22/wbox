/*
 * WBOX NT File System Calls
 * NtWriteFile, NtReadFile implementations
 */
#include "syscalls.h"
#include "handles.h"
#include "../cpu/cpu.h"
#include "../cpu/mem.h"
#include "../vm/vm.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * NtWriteFile - Write data to a file or console
 *
 * Arguments from user stack (EDX points to stack):
 *   [EDX+0]  = return address
 *   [EDX+4]  = FileHandle
 *   [EDX+8]  = Event (ignored for sync I/O)
 *   [EDX+12] = ApcRoutine (ignored)
 *   [EDX+16] = ApcContext (ignored)
 *   [EDX+20] = IoStatusBlock pointer
 *   [EDX+24] = Buffer pointer
 *   [EDX+28] = Length
 *   [EDX+32] = ByteOffset pointer (ignored)
 *   [EDX+36] = Key pointer (ignored)
 */
ntstatus_t sys_NtWriteFile(void)
{
    uint32_t args = EDX;

    /* Read arguments from user stack */
    uint32_t file_handle   = readmemll(args + 4);
    /* uint32_t event      = readmemll(args + 8);  */
    /* uint32_t apc_routine= readmemll(args + 12); */
    /* uint32_t apc_context= readmemll(args + 16); */
    uint32_t io_status_ptr = readmemll(args + 20);
    uint32_t buffer_ptr    = readmemll(args + 24);
    uint32_t length        = readmemll(args + 28);
    /* uint32_t byte_offset= readmemll(args + 32); */
    /* uint32_t key        = readmemll(args + 36); */

    /* Get VM context for handle table access */
    vm_context_t *vm = vm_get_context();
    if (!vm) {
        return STATUS_INVALID_HANDLE;
    }

    /* Resolve handle (supports pseudo-handles for stdout/stderr) */
    handle_entry_t *he = handles_resolve(&vm->handles, file_handle);
    if (!he) {
        return STATUS_INVALID_HANDLE;
    }

    /* Verify handle type allows writing */
    if (he->type != HANDLE_TYPE_FILE &&
        he->type != HANDLE_TYPE_CONSOLE_OUT &&
        he->type != HANDLE_TYPE_CONSOLE_ERR) {
        return STATUS_INVALID_HANDLE;
    }

    /* Validate length */
    if (length == 0) {
        if (io_status_ptr) {
            writememll(io_status_ptr + 0, STATUS_SUCCESS);
            writememll(io_status_ptr + 4, 0);
        }
        return STATUS_SUCCESS;
    }

    /* Read buffer from guest memory */
    char *buf = malloc(length);
    if (!buf) {
        return STATUS_NO_MEMORY;
    }

    for (uint32_t i = 0; i < length; i++) {
        buf[i] = readmembl(buffer_ptr + i);
    }

    /* Write to host file descriptor */
    ssize_t written = write(he->host_fd, buf, length);
    free(buf);

    if (written < 0) {
        return STATUS_IO_DEVICE_ERROR;
    }

    /* Fill IO_STATUS_BLOCK if provided */
    if (io_status_ptr) {
        writememll(io_status_ptr + 0, STATUS_SUCCESS);  /* Status */
        writememll(io_status_ptr + 4, (uint32_t)written); /* Information (bytes written) */
    }

    return STATUS_SUCCESS;
}
