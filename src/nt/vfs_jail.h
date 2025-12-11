/*
 * WBOX Virtual Filesystem Jail
 * Secure path translation for Windows-to-host filesystem mapping
 */
#ifndef WBOX_VFS_JAIL_H
#define WBOX_VFS_JAIL_H

#include <stdint.h>
#include <stdbool.h>
#include <wchar.h>

/* Maximum path length */
#define VFS_MAX_PATH 4096

/* VFS jail context */
typedef struct {
    char jail_root[VFS_MAX_PATH];   /* Absolute path to jail root */
    size_t jail_root_len;           /* Cached length of jail_root */
    bool initialized;               /* Whether jail is configured */
} vfs_jail_t;

/*
 * Initialize the VFS jail with a root directory
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

/*
 * Translate a Windows NT path to a host path within the jail
 *
 * Handles the following path formats:
 *   - \??\C:\path\file.txt  -> {jail}/path/file.txt
 *   - C:\path\file.txt      -> {jail}/path/file.txt
 *   - \path\file.txt        -> {jail}/path/file.txt
 *   - path\file.txt         -> {jail}/path/file.txt
 *
 * Path traversal with .. is resolved in-place and rejected if it would
 * escape the jail root.
 *
 * Returns pointer to static buffer with translated path, or NULL on error
 * (path escapes jail, invalid characters, etc.)
 */
const char *vfs_translate_path(vfs_jail_t *jail, const wchar_t *win_path, size_t win_path_len);

/*
 * Validate that a path stays within the jail after resolution
 * This uses realpath() to follow symlinks and verify the final location
 *
 * For existing files: verifies the resolved path starts with jail_root
 * For new files: verifies the parent directory is within jail_root
 *
 * Returns true if path is safe, false if it escapes
 */
bool vfs_path_is_safe(vfs_jail_t *jail, const char *host_path);

#endif /* WBOX_VFS_JAIL_H */
