/*
 * WBOX Virtual Filesystem
 * Secure path translation for Windows-to-host filesystem mapping
 * with multi-drive letter support
 */
#ifndef WBOX_VFS_JAIL_H
#define WBOX_VFS_JAIL_H

#include <stdint.h>
#include <stdbool.h>
#include <wchar.h>

/* Maximum path length */
#define VFS_MAX_PATH 4096

/* Number of drive letters (A-Z) */
#define VFS_NUM_DRIVES 26

/* Drive mapping entry */
typedef struct {
    char host_path[VFS_MAX_PATH];   /* Absolute path to host directory */
    size_t host_path_len;           /* Cached length of host_path */
    bool mapped;                     /* Whether this drive is mapped */
} vfs_drive_t;

/* VFS context with multi-drive support */
typedef struct {
    vfs_drive_t drives[VFS_NUM_DRIVES];  /* Drive mappings (index 0=A, 1=B, ... 25=Z) */
    bool initialized;                     /* Whether VFS is configured */
} vfs_jail_t;

/*
 * Initialize the VFS (no drives mapped initially)
 */
void vfs_init(vfs_jail_t *vfs);

/*
 * Map a drive letter to a host directory
 * drive_letter: 'A'-'Z' or 'a'-'z'
 * host_path: Path to host directory (resolved to absolute path)
 * Returns 0 on success, -1 on error
 */
int vfs_map_drive(vfs_jail_t *vfs, char drive_letter, const char *host_path);

/*
 * Unmap a drive letter
 * Returns 0 on success, -1 if not mapped
 */
int vfs_unmap_drive(vfs_jail_t *vfs, char drive_letter);

/*
 * Check if a drive letter is mapped
 */
bool vfs_drive_is_mapped(const vfs_jail_t *vfs, char drive_letter);

/*
 * Get the host path for a drive letter
 * Returns NULL if drive is not mapped
 */
const char *vfs_get_drive_path(const vfs_jail_t *vfs, char drive_letter);

/*
 * Initialize the VFS jail with a root directory (legacy single-drive mode)
 * Maps the root_path to C: drive
 * The root path is resolved to an absolute path and verified to be a directory
 * Returns 0 on success, -1 on error
 */
int vfs_jail_init(vfs_jail_t *jail, const char *root_path);

/*
 * Read a UNICODE_STRING structure from guest memory
 *
 * UNICODE_STRING layout:
 *   +0: USHORT Length (byte length, not including null)
 *   +2: USHORT MaximumLength
 *   +4: PWSTR Buffer (pointer to wide char string)
 *
 * Returns allocated buffer (caller must free) or NULL on error
 * The out_len parameter receives the character count (not byte length)
 */
wchar_t *vfs_read_unicode_string(uint32_t unicode_string_ptr, uint16_t *out_len);

/* VFS translation error codes */
typedef enum {
    VFS_OK = 0,
    VFS_ERROR_INVALID_PATH = -1,      /* Invalid path format */
    VFS_ERROR_PATH_ESCAPE = -2,       /* Path attempts to escape drive root */
    VFS_ERROR_UNMAPPED_DRIVE = -3,    /* Drive letter not mapped */
    VFS_ERROR_PATH_TOO_LONG = -4,     /* Path exceeds maximum length */
    VFS_ERROR_DEVICE_PATH = -5,       /* Device path not allowed */
    VFS_ERROR_UNC_PATH = -6,          /* UNC path not allowed */
} vfs_error_t;

/*
 * Translate a Windows NT path to a host path
 *
 * Handles the following path formats:
 *   - \??\C:\path\file.txt  -> {drive_C}/path/file.txt
 *   - C:\path\file.txt      -> {drive_C}/path/file.txt
 *   - \path\file.txt        -> {drive_C}/path/file.txt (default to C:)
 *   - path\file.txt         -> {drive_C}/path/file.txt (default to C:)
 *
 * Path traversal with .. is resolved in-place and rejected if it would
 * escape the drive root.
 *
 * Parameters:
 *   vfs          - VFS context
 *   win_path     - Windows path (wide string)
 *   win_path_len - Length of win_path in characters
 *   out_path     - Output buffer for translated path (must be VFS_MAX_PATH bytes)
 *
 * Returns VFS_OK on success, negative error code on failure
 */
vfs_error_t vfs_translate_path_ex(vfs_jail_t *vfs, const wchar_t *win_path,
                                  size_t win_path_len, char *out_path);

/*
 * Translate a Windows NT path to a host path (legacy API)
 *
 * Returns pointer to static buffer with translated path, or NULL on error
 * (path escapes jail, invalid characters, unmapped drive, etc.)
 */
const char *vfs_translate_path(vfs_jail_t *jail, const wchar_t *win_path, size_t win_path_len);

/*
 * Validate that a path stays within any mapped drive after resolution
 * This uses realpath() to follow symlinks and verify the final location
 *
 * For existing files: verifies the resolved path starts with any drive root
 * For new files: verifies the parent directory is within any drive root
 *
 * host_path: The translated host path to verify
 *
 * Returns true if path is safe, false if it escapes all drives
 */
bool vfs_path_is_safe(vfs_jail_t *vfs, const char *host_path);

/*
 * Validate that a path stays within a specific drive after resolution
 */
bool vfs_path_is_safe_for_drive(vfs_jail_t *vfs, char drive_letter, const char *host_path);

/*
 * Resolve a DLL name to a host path via VFS
 *
 * Searches for the DLL in standard Windows locations on the specified drive:
 *   1. WINDOWS\system32\<dll_name>
 *   2. WINDOWS\<dll_name>
 *
 * Parameters:
 *   vfs          - VFS context
 *   dll_name     - DLL name (e.g., "ntdll.dll")
 *   out_path     - Output buffer for host path (must be VFS_MAX_PATH bytes)
 *
 * Returns 0 on success (DLL found), -1 if not found
 */
int vfs_find_dll(vfs_jail_t *vfs, const char *dll_name, char *out_path);

#endif /* WBOX_VFS_JAIL_H */
