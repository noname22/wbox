/*
 * WBOX NT File System Calls
 * NtClose, NtCreateFile, NtOpenFile, NtReadFile, NtWriteFile implementations
 */
#include "syscalls.h"
#include "handles.h"
#include "vfs_jail.h"
#include "../cpu/cpu.h"
#include "../cpu/mem.h"
#include "../vm/vm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

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

/*
 * NtClose - Close a handle
 *
 * Arguments:
 *   [EDX+4] = Handle
 */
ntstatus_t sys_NtClose(void)
{
    uint32_t args = EDX;
    uint32_t handle = readmemll(args + 4);

    vm_context_t *vm = vm_get_context();
    if (!vm) {
        return STATUS_INVALID_HANDLE;
    }

    /* Get handle entry (don't resolve pseudo-handles) */
    handle_entry_t *he = handles_get(&vm->handles, handle);
    if (!he) {
        return STATUS_INVALID_HANDLE;
    }

    /* Close host file descriptor for regular files */
    if (he->type == HANDLE_TYPE_FILE && he->host_fd >= 0) {
        close(he->host_fd);
    }

    /* Remove from handle table */
    handles_remove(&vm->handles, handle);

    return STATUS_SUCCESS;
}

/*
 * NtReadFile - Read data from a file
 *
 * Arguments from user stack (EDX points to stack):
 *   [EDX+0]  = return address
 *   [EDX+4]  = FileHandle
 *   [EDX+8]  = Event (ignored for sync I/O)
 *   [EDX+12] = ApcRoutine (ignored)
 *   [EDX+16] = ApcContext (ignored)
 *   [EDX+20] = IoStatusBlock pointer
 *   [EDX+24] = Buffer pointer (output)
 *   [EDX+28] = Length
 *   [EDX+32] = ByteOffset pointer (optional)
 *   [EDX+36] = Key pointer (ignored)
 */
ntstatus_t sys_NtReadFile(void)
{
    uint32_t args = EDX;

    uint32_t file_handle    = readmemll(args + 4);
    uint32_t io_status_ptr  = readmemll(args + 20);
    uint32_t buffer_ptr     = readmemll(args + 24);
    uint32_t length         = readmemll(args + 28);
    uint32_t byte_offset_ptr = readmemll(args + 32);

    vm_context_t *vm = vm_get_context();
    if (!vm) {
        return STATUS_INVALID_HANDLE;
    }

    /* Resolve handle */
    handle_entry_t *he = handles_resolve(&vm->handles, file_handle);
    if (!he) {
        return STATUS_INVALID_HANDLE;
    }

    /* Verify handle type allows reading */
    if (he->type != HANDLE_TYPE_FILE && he->type != HANDLE_TYPE_CONSOLE_IN) {
        return STATUS_INVALID_HANDLE;
    }

    /* Handle zero-length read */
    if (length == 0) {
        if (io_status_ptr) {
            writememll(io_status_ptr + 0, STATUS_SUCCESS);
            writememll(io_status_ptr + 4, 0);
        }
        return STATUS_SUCCESS;
    }

    /* Handle byte offset if provided */
    if (byte_offset_ptr != 0) {
        uint32_t offset_low = readmemll(byte_offset_ptr);
        uint32_t offset_high = readmemll(byte_offset_ptr + 4);
        int64_t offset = (int64_t)offset_low | ((int64_t)offset_high << 32);

        /* -1 means use current file position */
        if (offset >= 0) {
            if (lseek(he->host_fd, offset, SEEK_SET) < 0) {
                return STATUS_IO_DEVICE_ERROR;
            }
            he->file_offset = offset;
        }
    }

    /* Allocate buffer and read */
    char *buf = malloc(length);
    if (!buf) {
        return STATUS_NO_MEMORY;
    }

    ssize_t bytes_read = read(he->host_fd, buf, length);

    if (bytes_read < 0) {
        free(buf);
        return STATUS_IO_DEVICE_ERROR;
    }

    /* Copy data to guest memory */
    for (ssize_t i = 0; i < bytes_read; i++) {
        writemembl(buffer_ptr + i, buf[i]);
    }

    free(buf);

    /* Update file offset */
    he->file_offset += bytes_read;

    /* Fill IO_STATUS_BLOCK */
    ntstatus_t status = (bytes_read == 0) ? STATUS_END_OF_FILE : STATUS_SUCCESS;
    if (io_status_ptr) {
        writememll(io_status_ptr + 0, status);
        writememll(io_status_ptr + 4, (uint32_t)bytes_read);
    }

    return status;
}

/*
 * Helper function to map errno to NTSTATUS
 */
static ntstatus_t errno_to_ntstatus(int err, bool must_exist)
{
    switch (err) {
        case ENOENT:
            return must_exist ? STATUS_OBJECT_NAME_NOT_FOUND : STATUS_OBJECT_PATH_NOT_FOUND;
        case EEXIST:
            return STATUS_OBJECT_NAME_COLLISION;
        case EACCES:
        case EPERM:
            return STATUS_ACCESS_DENIED;
        case ENOMEM:
            return STATUS_NO_MEMORY;
        case ENOTDIR:
            return STATUS_OBJECT_PATH_NOT_FOUND;
        case EISDIR:
            return STATUS_OBJECT_TYPE_MISMATCH;
        default:
            return STATUS_IO_DEVICE_ERROR;
    }
}

/*
 * NtCreateFile - Create or open a file
 *
 * Arguments from user stack (EDX points to stack):
 *   [EDX+0]  = return address
 *   [EDX+4]  = FileHandle pointer (output)
 *   [EDX+8]  = DesiredAccess
 *   [EDX+12] = ObjectAttributes pointer
 *   [EDX+16] = IoStatusBlock pointer (output)
 *   [EDX+20] = AllocationSize pointer (ignored)
 *   [EDX+24] = FileAttributes (ignored)
 *   [EDX+28] = ShareAccess (ignored)
 *   [EDX+32] = CreateDisposition
 *   [EDX+36] = CreateOptions
 *   [EDX+40] = EaBuffer (ignored)
 *   [EDX+44] = EaLength (ignored)
 */
ntstatus_t sys_NtCreateFile(void)
{
    uint32_t args = EDX;

    uint32_t file_handle_ptr = readmemll(args + 4);
    uint32_t desired_access  = readmemll(args + 8);
    uint32_t obj_attr_ptr    = readmemll(args + 12);
    uint32_t io_status_ptr   = readmemll(args + 16);
    uint32_t create_disp     = readmemll(args + 32);
    uint32_t create_options  = readmemll(args + 36);

    vm_context_t *vm = vm_get_context();
    if (!vm) {
        return STATUS_INVALID_HANDLE;
    }

    /* Check if VFS jail is initialized */
    if (!vm->vfs_jail.initialized) {
        return STATUS_ACCESS_DENIED;
    }

    /* Read OBJECT_ATTRIBUTES -> ObjectName pointer (at offset +8) */
    uint32_t unicode_str_ptr = readmemll(obj_attr_ptr + 8);
    if (unicode_str_ptr == 0) {
        return STATUS_OBJECT_NAME_INVALID;
    }

    /* Read UNICODE_STRING and translate path */
    uint16_t path_len;
    wchar_t *win_path = vfs_read_unicode_string(unicode_str_ptr, &path_len);
    if (!win_path) {
        return STATUS_OBJECT_NAME_INVALID;
    }

    const char *host_path = vfs_translate_path(&vm->vfs_jail, win_path, path_len);
    free(win_path);

    if (!host_path) {
        return STATUS_OBJECT_PATH_INVALID;
    }

    /* Determine open flags from CreateDisposition */
    int flags = 0;
    bool must_exist = false;
    uint32_t info_value = FILE_OPENED;

    switch (create_disp) {
        case FILE_SUPERSEDE:    /* 0: Replace/create */
            flags = O_CREAT | O_TRUNC;
            info_value = FILE_SUPERSEDED;
            break;
        case FILE_OPEN:         /* 1: Open existing only */
            must_exist = true;
            info_value = FILE_OPENED;
            break;
        case FILE_CREATE:       /* 2: Create new only */
            flags = O_CREAT | O_EXCL;
            info_value = FILE_CREATED;
            break;
        case FILE_OPEN_IF:      /* 3: Open or create */
            flags = O_CREAT;
            info_value = FILE_OPENED;  /* May be FILE_CREATED */
            break;
        case FILE_OVERWRITE:    /* 4: Overwrite existing */
            flags = O_TRUNC;
            must_exist = true;
            info_value = FILE_OVERWRITTEN;
            break;
        case FILE_OVERWRITE_IF: /* 5: Overwrite or create */
            flags = O_CREAT | O_TRUNC;
            info_value = FILE_OVERWRITTEN;
            break;
        default:
            return STATUS_INVALID_PARAMETER;
    }

    /* Determine read/write mode */
    bool want_read = (desired_access & (GENERIC_READ | FILE_READ_DATA)) != 0;
    bool want_write = (desired_access & (GENERIC_WRITE | FILE_WRITE_DATA | FILE_APPEND_DATA)) != 0;

    if (want_read && want_write) {
        flags |= O_RDWR;
    } else if (want_write) {
        flags |= O_WRONLY;
    } else {
        flags |= O_RDONLY;
    }

    /* Check for directory */
    bool is_directory = (create_options & FILE_DIRECTORY_FILE) != 0;
    if (is_directory) {
        flags |= O_DIRECTORY;
    }

    /* Check if file exists (for FILE_OPEN_IF info value) */
    struct stat st;
    bool existed = (stat(host_path, &st) == 0);

    /* Open the file */
    int fd = open(host_path, flags, 0666);
    if (fd < 0) {
        return errno_to_ntstatus(errno, must_exist);
    }

    /* Final security check: verify path after open */
    if (!vfs_path_is_safe(&vm->vfs_jail, host_path)) {
        close(fd);
        return STATUS_ACCESS_DENIED;
    }

    /* Update info value for FILE_OPEN_IF */
    if (create_disp == FILE_OPEN_IF) {
        info_value = existed ? FILE_OPENED : FILE_CREATED;
    }

    /* Add to handle table */
    uint32_t handle = handles_add(&vm->handles, HANDLE_TYPE_FILE, fd);
    if (handle == 0) {
        close(fd);
        return STATUS_NO_MEMORY;
    }

    /* Update handle entry */
    handle_entry_t *he = handles_get(&vm->handles, handle);
    if (he) {
        he->access_mask = desired_access;
        he->file_offset = 0;
    }

    /* Write output handle */
    writememll(file_handle_ptr, handle);

    /* Write IO_STATUS_BLOCK */
    if (io_status_ptr) {
        writememll(io_status_ptr + 0, STATUS_SUCCESS);
        writememll(io_status_ptr + 4, info_value);
    }

    return STATUS_SUCCESS;
}

/*
 * NtOpenFile - Open an existing file
 *
 * Arguments from user stack (EDX points to stack):
 *   [EDX+0]  = return address
 *   [EDX+4]  = FileHandle pointer (output)
 *   [EDX+8]  = DesiredAccess
 *   [EDX+12] = ObjectAttributes pointer
 *   [EDX+16] = IoStatusBlock pointer (output)
 *   [EDX+20] = ShareAccess (ignored)
 *   [EDX+24] = OpenOptions
 */
ntstatus_t sys_NtOpenFile(void)
{
    uint32_t args = EDX;

    uint32_t file_handle_ptr = readmemll(args + 4);
    uint32_t desired_access  = readmemll(args + 8);
    uint32_t obj_attr_ptr    = readmemll(args + 12);
    uint32_t io_status_ptr   = readmemll(args + 16);
    uint32_t open_options    = readmemll(args + 24);

    vm_context_t *vm = vm_get_context();
    if (!vm) {
        return STATUS_INVALID_HANDLE;
    }

    /* Check if VFS jail is initialized */
    if (!vm->vfs_jail.initialized) {
        return STATUS_ACCESS_DENIED;
    }

    /* Read OBJECT_ATTRIBUTES -> ObjectName pointer */
    uint32_t unicode_str_ptr = readmemll(obj_attr_ptr + 8);
    if (unicode_str_ptr == 0) {
        return STATUS_OBJECT_NAME_INVALID;
    }

    /* Read UNICODE_STRING and translate path */
    uint16_t path_len;
    wchar_t *win_path = vfs_read_unicode_string(unicode_str_ptr, &path_len);
    if (!win_path) {
        return STATUS_OBJECT_NAME_INVALID;
    }

    const char *host_path = vfs_translate_path(&vm->vfs_jail, win_path, path_len);
    free(win_path);

    if (!host_path) {
        return STATUS_OBJECT_PATH_INVALID;
    }

    /* Determine flags */
    int flags = 0;
    bool want_read = (desired_access & (GENERIC_READ | FILE_READ_DATA)) != 0;
    bool want_write = (desired_access & (GENERIC_WRITE | FILE_WRITE_DATA | FILE_APPEND_DATA)) != 0;

    if (want_read && want_write) {
        flags = O_RDWR;
    } else if (want_write) {
        flags = O_WRONLY;
    } else {
        flags = O_RDONLY;
    }

    if (open_options & FILE_DIRECTORY_FILE) {
        flags |= O_DIRECTORY;
    }

    /* Open file (must exist) */
    int fd = open(host_path, flags);
    if (fd < 0) {
        return errno_to_ntstatus(errno, true);
    }

    /* Security check */
    if (!vfs_path_is_safe(&vm->vfs_jail, host_path)) {
        close(fd);
        return STATUS_ACCESS_DENIED;
    }

    /* Add handle */
    uint32_t handle = handles_add(&vm->handles, HANDLE_TYPE_FILE, fd);
    if (handle == 0) {
        close(fd);
        return STATUS_NO_MEMORY;
    }

    /* Update handle entry */
    handle_entry_t *he = handles_get(&vm->handles, handle);
    if (he) {
        he->access_mask = desired_access;
        he->file_offset = 0;
    }

    /* Write output handle */
    writememll(file_handle_ptr, handle);

    /* Write IO_STATUS_BLOCK */
    if (io_status_ptr) {
        writememll(io_status_ptr + 0, STATUS_SUCCESS);
        writememll(io_status_ptr + 4, FILE_OPENED);
    }

    return STATUS_SUCCESS;
}
