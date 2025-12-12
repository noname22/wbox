/*
 * WBOX NT Handle Table Implementation
 */
#include "handles.h"
#include "sync.h"
#include <string.h>
#include <unistd.h>

/* Static entries for standard handles (used for pseudo-handle resolution) */
static handle_entry_t std_in_entry  = { HANDLE_TYPE_CONSOLE_IN,  STDIN_FILENO, 0, 0, NULL };
static handle_entry_t std_out_entry = { HANDLE_TYPE_CONSOLE_OUT, STDOUT_FILENO, 0, 0, NULL };
static handle_entry_t std_err_entry = { HANDLE_TYPE_CONSOLE_ERR, STDERR_FILENO, 0, 0, NULL };

void handles_init(handle_table_t *ht)
{
    memset(ht, 0, sizeof(*ht));

    /* Reserve handle 0 as invalid (like Windows NULL handle) */
    ht->entries[0].type = HANDLE_TYPE_NONE;
    ht->entries[0].host_fd = -1;

    /* Pre-populate standard handles at indices 1, 2, 3
     * These correspond to handles 4, 8, 12 (Windows uses multiples of 4)
     * But for simplicity, we'll use simple sequential handles */
    ht->entries[1].type = HANDLE_TYPE_CONSOLE_IN;
    ht->entries[1].host_fd = STDIN_FILENO;

    ht->entries[2].type = HANDLE_TYPE_CONSOLE_OUT;
    ht->entries[2].host_fd = STDOUT_FILENO;

    ht->entries[3].type = HANDLE_TYPE_CONSOLE_ERR;
    ht->entries[3].host_fd = STDERR_FILENO;

    /* Next available handle */
    ht->next_handle = 4;
}

uint32_t handles_add(handle_table_t *ht, handle_type_t type, int host_fd)
{
    /* Find next free slot */
    for (uint32_t i = ht->next_handle; i < MAX_HANDLES; i++) {
        if (ht->entries[i].type == HANDLE_TYPE_NONE) {
            ht->entries[i].type = type;
            ht->entries[i].host_fd = host_fd;
            ht->entries[i].access_mask = 0;
            ht->entries[i].file_offset = 0;
            ht->entries[i].object_data = NULL;
            ht->next_handle = i + 1;
            return i;
        }
    }

    /* Wrap around and search from beginning */
    for (uint32_t i = 1; i < ht->next_handle; i++) {
        if (ht->entries[i].type == HANDLE_TYPE_NONE) {
            ht->entries[i].type = type;
            ht->entries[i].host_fd = host_fd;
            ht->entries[i].access_mask = 0;
            ht->entries[i].file_offset = 0;
            ht->entries[i].object_data = NULL;
            ht->next_handle = i + 1;
            return i;
        }
    }

    /* No free slots */
    return 0;
}

uint32_t handles_add_object(handle_table_t *ht, handle_type_t type, void *object_data)
{
    /* Find next free slot */
    for (uint32_t i = ht->next_handle; i < MAX_HANDLES; i++) {
        if (ht->entries[i].type == HANDLE_TYPE_NONE) {
            ht->entries[i].type = type;
            ht->entries[i].host_fd = -1;
            ht->entries[i].access_mask = 0;
            ht->entries[i].file_offset = 0;
            ht->entries[i].object_data = object_data;
            ht->next_handle = i + 1;
            return i;
        }
    }

    /* Wrap around and search from beginning */
    for (uint32_t i = 1; i < ht->next_handle; i++) {
        if (ht->entries[i].type == HANDLE_TYPE_NONE) {
            ht->entries[i].type = type;
            ht->entries[i].host_fd = -1;
            ht->entries[i].access_mask = 0;
            ht->entries[i].file_offset = 0;
            ht->entries[i].object_data = object_data;
            ht->next_handle = i + 1;
            return i;
        }
    }

    /* No free slots */
    return 0;
}

handle_entry_t *handles_get(handle_table_t *ht, uint32_t handle)
{
    if (handle == 0 || handle >= MAX_HANDLES) {
        return NULL;
    }

    if (ht->entries[handle].type == HANDLE_TYPE_NONE) {
        return NULL;
    }

    return &ht->entries[handle];
}

void handles_remove(handle_table_t *ht, uint32_t handle)
{
    if (handle > 0 && handle < MAX_HANDLES) {
        handle_entry_t *entry = &ht->entries[handle];

        /* Free sync object if present */
        if (entry->object_data != NULL) {
            sync_free_object(entry->object_data, entry->type);
        }

        entry->type = HANDLE_TYPE_NONE;
        entry->host_fd = -1;
        entry->access_mask = 0;
        entry->file_offset = 0;
        entry->object_data = NULL;
    }
}

handle_entry_t *handles_resolve(handle_table_t *ht, uint32_t handle)
{
    /* Check for Windows standard handle pseudo-values */
    switch (handle) {
        case STD_INPUT_HANDLE:
            return &std_in_entry;
        case STD_OUTPUT_HANDLE:
            return &std_out_entry;
        case STD_ERROR_HANDLE:
            return &std_err_entry;
    }

    /* Also accept small integers 0, 1, 2 as stdin/stdout/stderr
     * (common convention for console apps) */
    if (handle == 0) {
        return &std_in_entry;
    }
    if (handle == 1) {
        return &std_out_entry;
    }
    if (handle == 2) {
        return &std_err_entry;
    }

    /* Regular handle lookup */
    return handles_get(ht, handle);
}
