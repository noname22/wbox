/*
 * WBOX Virtual Filesystem Implementation
 * Secure path translation for Windows-to-host filesystem mapping
 * with multi-drive letter support
 */
#include "vfs_jail.h"
#include "../cpu/mem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libgen.h>
#include <errno.h>

/* Static buffer for translated paths (legacy API) */
static char translated_path[VFS_MAX_PATH];

/* Convert drive letter to index (0-25) */
static inline int drive_to_index(char drive)
{
    if (drive >= 'A' && drive <= 'Z') {
        return drive - 'A';
    }
    if (drive >= 'a' && drive <= 'z') {
        return drive - 'a';
    }
    return -1;
}

/* Convert index to uppercase drive letter */
static inline char index_to_drive(int index)
{
    if (index >= 0 && index < VFS_NUM_DRIVES) {
        return 'A' + index;
    }
    return '\0';
}

void vfs_init(vfs_jail_t *vfs)
{
    if (!vfs) return;
    memset(vfs, 0, sizeof(*vfs));
    vfs->initialized = true;
}

int vfs_map_drive(vfs_jail_t *vfs, char drive_letter, const char *host_path)
{
    if (!vfs || !host_path) {
        return -1;
    }

    int index = drive_to_index(drive_letter);
    if (index < 0) {
        fprintf(stderr, "VFS: Invalid drive letter '%c'\n", drive_letter);
        return -1;
    }

    /* Resolve to absolute path */
    char resolved[PATH_MAX];
    if (realpath(host_path, resolved) == NULL) {
        fprintf(stderr, "VFS: Cannot resolve path '%s': %s\n",
                host_path, strerror(errno));
        return -1;
    }

    /* Verify it's a directory */
    struct stat st;
    if (stat(resolved, &st) != 0) {
        fprintf(stderr, "VFS: Cannot stat '%s': %s\n",
                resolved, strerror(errno));
        return -1;
    }

    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "VFS: '%s' is not a directory\n", resolved);
        return -1;
    }

    /* Store normalized path (ensure no trailing slash except for root) */
    size_t len = strlen(resolved);
    if (len > 1 && resolved[len - 1] == '/') {
        resolved[len - 1] = '\0';
        len--;
    }

    if (len >= VFS_MAX_PATH) {
        fprintf(stderr, "VFS: Path too long\n");
        return -1;
    }

    strncpy(vfs->drives[index].host_path, resolved, VFS_MAX_PATH - 1);
    vfs->drives[index].host_path[VFS_MAX_PATH - 1] = '\0';
    vfs->drives[index].host_path_len = len;
    vfs->drives[index].mapped = true;
    vfs->initialized = true;

    printf("VFS: Mapped %c: -> %s\n", index_to_drive(index), resolved);
    return 0;
}

int vfs_unmap_drive(vfs_jail_t *vfs, char drive_letter)
{
    if (!vfs) return -1;

    int index = drive_to_index(drive_letter);
    if (index < 0 || !vfs->drives[index].mapped) {
        return -1;
    }

    vfs->drives[index].mapped = false;
    vfs->drives[index].host_path[0] = '\0';
    vfs->drives[index].host_path_len = 0;
    return 0;
}

bool vfs_drive_is_mapped(const vfs_jail_t *vfs, char drive_letter)
{
    if (!vfs) return false;

    int index = drive_to_index(drive_letter);
    if (index < 0) return false;

    return vfs->drives[index].mapped;
}

const char *vfs_get_drive_path(const vfs_jail_t *vfs, char drive_letter)
{
    if (!vfs) return NULL;

    int index = drive_to_index(drive_letter);
    if (index < 0 || !vfs->drives[index].mapped) {
        return NULL;
    }

    return vfs->drives[index].host_path;
}

int vfs_jail_init(vfs_jail_t *jail, const char *root_path)
{
    if (!jail || !root_path) {
        return -1;
    }

    /* Initialize VFS and map root_path to C: drive */
    vfs_init(jail);
    return vfs_map_drive(jail, 'C', root_path);
}

wchar_t *vfs_read_unicode_string(uint32_t unicode_string_ptr, uint16_t *out_len)
{
    if (unicode_string_ptr == 0 || out_len == NULL) {
        return NULL;
    }

    /*
     * UNICODE_STRING layout:
     *   +0: USHORT Length (bytes, not including null)
     *   +2: USHORT MaximumLength
     *   +4: PWSTR Buffer
     */
    uint16_t byte_length = readmemwl(unicode_string_ptr);
    uint32_t buffer_ptr = readmemll(unicode_string_ptr + 4);

    if (byte_length == 0 || buffer_ptr == 0) {
        return NULL;
    }

    /* Length is in bytes, each wchar_t is 2 bytes */
    uint16_t wchar_count = byte_length / 2;

    /* Sanity check */
    if (wchar_count > 32768) {
        return NULL;
    }

    wchar_t *result = malloc((wchar_count + 1) * sizeof(wchar_t));
    if (!result) {
        return NULL;
    }

    for (uint16_t i = 0; i < wchar_count; i++) {
        result[i] = (wchar_t)readmemwl(buffer_ptr + i * 2);
    }
    result[wchar_count] = L'\0';

    *out_len = wchar_count;
    return result;
}

vfs_error_t vfs_translate_path_ex(vfs_jail_t *vfs, const wchar_t *win_path,
                                  size_t win_path_len, char *out_path)
{
    if (!vfs || !vfs->initialized || !win_path || win_path_len == 0 || !out_path) {
        return VFS_ERROR_INVALID_PATH;
    }

    /* Convert wide string to UTF-8 path */
    char path_utf8[VFS_MAX_PATH];
    size_t i = 0;
    size_t j = 0;
    char drive_letter = 'C';  /* Default to C: */

    /* Skip NT path prefix \??\ */
    if (win_path_len >= 4 &&
        win_path[0] == L'\\' && win_path[1] == L'?' &&
        win_path[2] == L'?' && win_path[3] == L'\\') {
        i = 4;
    }

    /* Reject device paths */
    if (i + 7 <= win_path_len &&
        (win_path[i] == L'\\' || i == 0) &&
        ((win_path[i] == L'\\') ? 1 : 0) + i < win_path_len) {
        /* Check for \Device\ */
        size_t check_start = (win_path[i] == L'\\') ? i + 1 : i;
        if (win_path_len - check_start >= 7 &&
            (win_path[check_start] == L'D' || win_path[check_start] == L'd') &&
            (win_path[check_start + 1] == L'e' || win_path[check_start + 1] == L'E') &&
            (win_path[check_start + 2] == L'v' || win_path[check_start + 2] == L'V') &&
            (win_path[check_start + 3] == L'i' || win_path[check_start + 3] == L'I') &&
            (win_path[check_start + 4] == L'c' || win_path[check_start + 4] == L'C') &&
            (win_path[check_start + 5] == L'e' || win_path[check_start + 5] == L'E') &&
            win_path[check_start + 6] == L'\\') {
            /* Device path - reject */
            return VFS_ERROR_DEVICE_PATH;
        }
    }

    /* Reject UNC paths \\server\share */
    if (win_path_len >= 2 && win_path[0] == L'\\' && win_path[1] == L'\\') {
        return VFS_ERROR_UNC_PATH;
    }

    /* Extract drive letter (C:, D:, etc.) */
    if (i + 1 < win_path_len &&
        ((win_path[i] >= L'A' && win_path[i] <= L'Z') ||
         (win_path[i] >= L'a' && win_path[i] <= L'z')) &&
        win_path[i + 1] == L':') {
        drive_letter = (char)win_path[i];
        if (drive_letter >= 'a') {
            drive_letter = drive_letter - 'a' + 'A';  /* Uppercase */
        }
        i += 2;
    }

    /* Check if drive is mapped */
    int drive_index = drive_to_index(drive_letter);
    if (drive_index < 0 || !vfs->drives[drive_index].mapped) {
        return VFS_ERROR_UNMAPPED_DRIVE;
    }

    const vfs_drive_t *drive = &vfs->drives[drive_index];

    /* Convert remaining path, resolving . and .. */
    int depth = 0;
    bool last_was_sep = true;
    size_t component_start = 0;

    for (; i < win_path_len && j < VFS_MAX_PATH - 1; i++) {
        wchar_t wc = win_path[i];

        /* Convert backslash to forward slash */
        if (wc == L'\\' || wc == L'/') {
            if (!last_was_sep && j > component_start) {
                /* Check the component we just finished */
                size_t comp_len = j - component_start;

                if (comp_len == 2 &&
                    path_utf8[component_start] == '.' &&
                    path_utf8[component_start + 1] == '.') {
                    /* ".." - go up one directory */
                    depth--;
                    if (depth < 0) {
                        /* Attempted escape from drive! */
                        return VFS_ERROR_PATH_ESCAPE;
                    }
                    /* Remove the ".." we just wrote */
                    j = component_start;
                    /* Remove previous component (find last /) */
                    if (j > 0) {
                        j--;  /* Remove current slash position */
                        while (j > 0 && path_utf8[j - 1] != '/') {
                            j--;
                        }
                    }
                } else if (comp_len == 1 && path_utf8[component_start] == '.') {
                    /* "." - stay in current directory, remove it */
                    j = component_start;
                    if (j > 0) {
                        j--;  /* Remove the slash before . */
                    }
                } else if (comp_len > 0) {
                    /* Normal component */
                    depth++;
                    path_utf8[j++] = '/';
                }
            }
            last_was_sep = true;
            component_start = j;
            continue;
        }

        /* First non-separator after separator */
        if (last_was_sep) {
            component_start = j;
            last_was_sep = false;
        }

        /* Reject null bytes */
        if (wc == L'\0') {
            break;
        }

        /* Convert to UTF-8 */
        if (wc <= 0x7F) {
            /* ASCII */
            path_utf8[j++] = (char)wc;
        } else if (wc <= 0x7FF) {
            /* 2-byte UTF-8 */
            if (j + 2 >= VFS_MAX_PATH) break;
            path_utf8[j++] = (char)(0xC0 | (wc >> 6));
            path_utf8[j++] = (char)(0x80 | (wc & 0x3F));
        } else {
            /* 3-byte UTF-8 */
            if (j + 3 >= VFS_MAX_PATH) break;
            path_utf8[j++] = (char)(0xE0 | (wc >> 12));
            path_utf8[j++] = (char)(0x80 | ((wc >> 6) & 0x3F));
            path_utf8[j++] = (char)(0x80 | (wc & 0x3F));
        }
    }

    /* Handle final component */
    if (!last_was_sep && j > component_start) {
        size_t comp_len = j - component_start;

        if (comp_len == 2 &&
            path_utf8[component_start] == '.' &&
            path_utf8[component_start + 1] == '.') {
            depth--;
            if (depth < 0) {
                return VFS_ERROR_PATH_ESCAPE;
            }
            j = component_start;
            if (j > 0) {
                j--;
                while (j > 0 && path_utf8[j - 1] != '/') {
                    j--;
                }
            }
        } else if (comp_len == 1 && path_utf8[component_start] == '.') {
            j = component_start;
            if (j > 0) {
                j--;
            }
        }
    }

    /* Remove trailing slash if present (but keep leading slash) */
    if (j > 1 && path_utf8[j - 1] == '/') {
        j--;
    }

    path_utf8[j] = '\0';

    /* Build final path: drive_path + / + translated_path */
    int written = snprintf(out_path, VFS_MAX_PATH, "%s/%s",
                           drive->host_path, path_utf8);
    if (written < 0 || written >= VFS_MAX_PATH) {
        return VFS_ERROR_PATH_TOO_LONG;
    }

    return VFS_OK;
}

const char *vfs_translate_path(vfs_jail_t *jail, const wchar_t *win_path, size_t win_path_len)
{
    vfs_error_t err = vfs_translate_path_ex(jail, win_path, win_path_len, translated_path);
    if (err != VFS_OK) {
        return NULL;
    }
    return translated_path;
}

static bool check_path_in_drive(const vfs_drive_t *drive, const char *resolved)
{
    return (strncmp(resolved, drive->host_path, drive->host_path_len) == 0 &&
            (resolved[drive->host_path_len] == '/' ||
             resolved[drive->host_path_len] == '\0'));
}

bool vfs_path_is_safe_for_drive(vfs_jail_t *vfs, char drive_letter, const char *host_path)
{
    if (!vfs || !vfs->initialized || !host_path) {
        return false;
    }

    int index = drive_to_index(drive_letter);
    if (index < 0 || !vfs->drives[index].mapped) {
        return false;
    }

    const vfs_drive_t *drive = &vfs->drives[index];
    char resolved[PATH_MAX];

    /* Try to resolve the path (follows symlinks) */
    if (realpath(host_path, resolved) != NULL) {
        return check_path_in_drive(drive, resolved);
    }

    /* File doesn't exist - check parent directory */
    char *path_copy = strdup(host_path);
    if (!path_copy) {
        return false;
    }

    char *parent = dirname(path_copy);
    bool result = false;

    if (realpath(parent, resolved) != NULL) {
        result = check_path_in_drive(drive, resolved);
    }

    free(path_copy);
    return result;
}

bool vfs_path_is_safe(vfs_jail_t *vfs, const char *host_path)
{
    if (!vfs || !vfs->initialized || !host_path) {
        return false;
    }

    char resolved[PATH_MAX];
    char *path_copy = NULL;

    /* Try to resolve the path (follows symlinks) */
    if (realpath(host_path, resolved) == NULL) {
        /* File doesn't exist - check parent directory */
        path_copy = strdup(host_path);
        if (!path_copy) {
            return false;
        }
        char *parent = dirname(path_copy);
        if (realpath(parent, resolved) == NULL) {
            free(path_copy);
            return false;
        }
    }

    /* Check if resolved path is within any mapped drive */
    bool result = false;
    for (int i = 0; i < VFS_NUM_DRIVES; i++) {
        if (vfs->drives[i].mapped && check_path_in_drive(&vfs->drives[i], resolved)) {
            result = true;
            break;
        }
    }

    if (path_copy) {
        free(path_copy);
    }
    return result;
}

int vfs_find_dll(vfs_jail_t *vfs, const char *dll_name, char *out_path)
{
    if (!vfs || !vfs->initialized || !dll_name || !out_path) {
        return -1;
    }

    /* Search C: drive first (system drive) */
    if (!vfs->drives[2].mapped) {  /* C: = index 2 */
        return -1;
    }

    const char *c_drive = vfs->drives[2].host_path;
    struct stat st;

    /* Try WINDOWS\system32\dll_name (case-insensitive search) */
    static const char *system_paths[] = {
        "WINDOWS/system32",
        "WINDOWS/System32",
        "Windows/system32",
        "Windows/System32",
        "windows/system32",
        NULL
    };

    for (const char **path = system_paths; *path != NULL; path++) {
        snprintf(out_path, VFS_MAX_PATH, "%s/%s/%s", c_drive, *path, dll_name);
        if (stat(out_path, &st) == 0 && S_ISREG(st.st_mode)) {
            return 0;
        }
    }

    /* Try WINDOWS\dll_name */
    static const char *windows_paths[] = {
        "WINDOWS",
        "Windows",
        "windows",
        NULL
    };

    for (const char **path = windows_paths; *path != NULL; path++) {
        snprintf(out_path, VFS_MAX_PATH, "%s/%s/%s", c_drive, *path, dll_name);
        if (stat(out_path, &st) == 0 && S_ISREG(st.st_mode)) {
            return 0;
        }
    }

    return -1;
}
