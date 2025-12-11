/*
 * WBOX Virtual Filesystem Jail Implementation
 * Secure path translation for Windows-to-host filesystem mapping
 */
#include "vfs_jail.h"
#include "../cpu/mem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libgen.h>
#include <errno.h>

/* Static buffer for translated paths */
static char translated_path[VFS_MAX_PATH];

int vfs_jail_init(vfs_jail_t *jail, const char *root_path)
{
    if (!jail || !root_path) {
        return -1;
    }

    /* Resolve to absolute path */
    char resolved[PATH_MAX];
    if (realpath(root_path, resolved) == NULL) {
        fprintf(stderr, "VFS jail: Cannot resolve path '%s': %s\n",
                root_path, strerror(errno));
        return -1;
    }

    /* Verify it's a directory */
    struct stat st;
    if (stat(resolved, &st) != 0) {
        fprintf(stderr, "VFS jail: Cannot stat '%s': %s\n",
                resolved, strerror(errno));
        return -1;
    }

    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "VFS jail: '%s' is not a directory\n", resolved);
        return -1;
    }

    /* Store normalized path (ensure no trailing slash except for root) */
    size_t len = strlen(resolved);
    if (len > 1 && resolved[len - 1] == '/') {
        resolved[len - 1] = '\0';
        len--;
    }

    if (len >= VFS_MAX_PATH) {
        fprintf(stderr, "VFS jail: Path too long\n");
        return -1;
    }

    strncpy(jail->jail_root, resolved, VFS_MAX_PATH - 1);
    jail->jail_root[VFS_MAX_PATH - 1] = '\0';
    jail->jail_root_len = len;
    jail->initialized = true;

    return 0;
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

const char *vfs_translate_path(vfs_jail_t *jail, const wchar_t *win_path, size_t win_path_len)
{
    if (!jail || !jail->initialized || !win_path || win_path_len == 0) {
        return NULL;
    }

    /* Convert wide string to UTF-8 path */
    char path_utf8[VFS_MAX_PATH];
    size_t i = 0;
    size_t j = 0;

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
            return NULL;
        }
    }

    /* Reject UNC paths \\server\share */
    if (win_path_len >= 2 && win_path[0] == L'\\' && win_path[1] == L'\\') {
        return NULL;
    }

    /* Skip drive letter (C:, D:, etc.) */
    if (i + 1 < win_path_len &&
        ((win_path[i] >= L'A' && win_path[i] <= L'Z') ||
         (win_path[i] >= L'a' && win_path[i] <= L'z')) &&
        win_path[i + 1] == L':') {
        i += 2;
    }

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
                        /* Attempted escape from jail! */
                        return NULL;
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
                return NULL;
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

    /* Build final path: jail_root + / + translated_path */
    /* Ensure there's a / separator between jail root and relative path */
    int written = snprintf(translated_path, VFS_MAX_PATH, "%s/%s",
                           jail->jail_root, path_utf8);
    if (written < 0 || written >= VFS_MAX_PATH) {
        return NULL;
    }

    return translated_path;
}

bool vfs_path_is_safe(vfs_jail_t *jail, const char *host_path)
{
    if (!jail || !jail->initialized || !host_path) {
        return false;
    }

    char resolved[PATH_MAX];

    /* Try to resolve the path (follows symlinks) */
    if (realpath(host_path, resolved) != NULL) {
        /* File exists - check if it's within jail */
        return (strncmp(resolved, jail->jail_root, jail->jail_root_len) == 0 &&
                (resolved[jail->jail_root_len] == '/' ||
                 resolved[jail->jail_root_len] == '\0'));
    }

    /* File doesn't exist - check parent directory */
    char *path_copy = strdup(host_path);
    if (!path_copy) {
        return false;
    }

    char *parent = dirname(path_copy);
    bool result = false;

    if (realpath(parent, resolved) != NULL) {
        result = (strncmp(resolved, jail->jail_root, jail->jail_root_len) == 0 &&
                  (resolved[jail->jail_root_len] == '/' ||
                   resolved[jail->jail_root_len] == '\0'));
    }

    free(path_copy);
    return result;
}
