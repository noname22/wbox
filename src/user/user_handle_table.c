/*
 * WBOX USER Handle Table Implementation
 */
#include "user_handle_table.h"
#include "../cpu/mem.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Guest handle entries address (from user_shared.c) */
#define GUEST_HANDLE_ENTRIES_VA  0x7F031000
#define GUEST_HANDLE_ENTRY_SIZE  12

/* Global handle table */
static USER_HANDLE_TABLE *g_user_handles = NULL;

int user_handle_table_init(USER_HANDLE_TABLE *table)
{
    if (!table) {
        return -1;
    }

    memset(table, 0, sizeof(USER_HANDLE_TABLE));

    /* Initialize free list - link all entries */
    for (int i = 0; i < USER_MAX_HANDLES - 1; i++) {
        table->entries[i].type = USER_TYPE_FREE;
        table->entries[i].generation = 1;  /* Start at 1 so handle 0 is invalid */
        /* Use ptr as next free index (ugly but works) */
        table->entries[i].ptr = (void *)(uintptr_t)(i + 1);
    }
    /* Last entry points to -1 (end of list) */
    table->entries[USER_MAX_HANDLES - 1].type = USER_TYPE_FREE;
    table->entries[USER_MAX_HANDLES - 1].generation = 1;
    table->entries[USER_MAX_HANDLES - 1].ptr = (void *)(uintptr_t)(-1);

    /* Reserve index 0 for HWND_DESKTOP (NULL handle) */
    table->entries[0].type = USER_TYPE_WINDOW;
    table->entries[0].ptr = NULL;  /* Desktop window - special case */
    table->first_free = 1;
    table->handle_count = 1;

    return 0;
}

void user_handle_table_shutdown(USER_HANDLE_TABLE *table)
{
    if (!table) {
        return;
    }

    /* Free all allocated objects */
    for (int i = 0; i < USER_MAX_HANDLES; i++) {
        if (table->entries[i].type != USER_TYPE_FREE && table->entries[i].ptr) {
            /* Note: actual object cleanup should be done elsewhere */
            table->entries[i].ptr = NULL;
            table->entries[i].type = USER_TYPE_FREE;
        }
    }

    table->handle_count = 0;
}

uint32_t user_handle_alloc(USER_HANDLE_TABLE *table, void *ptr,
                           USER_HANDLE_TYPE type, void *owner)
{
    if (!table || type == USER_TYPE_FREE) {
        return 0;
    }

    /* Get next free index */
    int index = table->first_free;
    if (index < 0 || index >= USER_MAX_HANDLES) {
        printf("USER: Handle table full!\n");
        return 0;
    }

    USER_HANDLE_ENTRY *entry = &table->entries[index];

    /* Update free list */
    table->first_free = (int)(uintptr_t)entry->ptr;

    /* Fill entry */
    entry->ptr = ptr;
    entry->owner = owner;
    entry->type = type;
    entry->flags = 0;
    /* Generation stays the same (incremented on free) */

    table->handle_count++;

    return USER_MAKE_HANDLE(index, entry->generation);
}

bool user_handle_free(USER_HANDLE_TABLE *table, uint32_t handle)
{
    if (!table || handle == 0) {
        return false;
    }

    int index = USER_HANDLE_INDEX(handle);
    uint16_t gen = USER_HANDLE_GEN(handle);

    if (index < 0 || index >= USER_MAX_HANDLES) {
        return false;
    }

    USER_HANDLE_ENTRY *entry = &table->entries[index];

    /* Validate generation */
    if (entry->generation != gen) {
        return false;
    }

    /* Already free? */
    if (entry->type == USER_TYPE_FREE) {
        return false;
    }

    /* Clear entry */
    entry->ptr = (void *)(uintptr_t)table->first_free;
    entry->owner = NULL;
    entry->type = USER_TYPE_FREE;
    entry->flags = 0;
    entry->generation++;  /* Increment generation to invalidate old handles */

    /* Add to free list */
    table->first_free = index;
    table->handle_count--;

    return true;
}

void *user_handle_get(USER_HANDLE_TABLE *table, uint32_t handle)
{
    if (!table) {
        return NULL;
    }

    /* Special case: HWND_DESKTOP */
    if (handle == HWND_DESKTOP) {
        return table->entries[0].ptr;
    }

    int index = USER_HANDLE_INDEX(handle);
    uint16_t gen = USER_HANDLE_GEN(handle);

    if (index < 0 || index >= USER_MAX_HANDLES) {
        return NULL;
    }

    USER_HANDLE_ENTRY *entry = &table->entries[index];

    /* Validate generation */
    if (entry->generation != gen) {
        return NULL;
    }

    /* Check not free */
    if (entry->type == USER_TYPE_FREE) {
        return NULL;
    }

    return entry->ptr;
}

void *user_handle_get_typed(USER_HANDLE_TABLE *table, uint32_t handle,
                            USER_HANDLE_TYPE expected_type)
{
    if (!table) {
        return NULL;
    }

    /* Special case: HWND_DESKTOP */
    if (handle == HWND_DESKTOP && expected_type == USER_TYPE_WINDOW) {
        return table->entries[0].ptr;
    }

    int index = USER_HANDLE_INDEX(handle);
    uint16_t gen = USER_HANDLE_GEN(handle);

    if (index < 0 || index >= USER_MAX_HANDLES) {
        return NULL;
    }

    USER_HANDLE_ENTRY *entry = &table->entries[index];

    /* Validate generation */
    if (entry->generation != gen) {
        return NULL;
    }

    /* Check type */
    if (entry->type != expected_type) {
        return NULL;
    }

    return entry->ptr;
}

USER_HANDLE_TYPE user_handle_get_type(USER_HANDLE_TABLE *table, uint32_t handle)
{
    if (!table || handle == 0) {
        return USER_TYPE_FREE;
    }

    int index = USER_HANDLE_INDEX(handle);
    uint16_t gen = USER_HANDLE_GEN(handle);

    if (index < 0 || index >= USER_MAX_HANDLES) {
        return USER_TYPE_FREE;
    }

    USER_HANDLE_ENTRY *entry = &table->entries[index];

    if (entry->generation != gen) {
        return USER_TYPE_FREE;
    }

    return entry->type;
}

void *user_handle_get_owner(USER_HANDLE_TABLE *table, uint32_t handle)
{
    if (!table || handle == 0) {
        return NULL;
    }

    int index = USER_HANDLE_INDEX(handle);
    uint16_t gen = USER_HANDLE_GEN(handle);

    if (index < 0 || index >= USER_MAX_HANDLES) {
        return NULL;
    }

    USER_HANDLE_ENTRY *entry = &table->entries[index];

    if (entry->generation != gen || entry->type == USER_TYPE_FREE) {
        return NULL;
    }

    return entry->owner;
}

bool user_handle_is_valid(USER_HANDLE_TABLE *table, uint32_t handle)
{
    if (!table) {
        return false;
    }

    /* HWND_DESKTOP is always valid */
    if (handle == HWND_DESKTOP) {
        return true;
    }

    int index = USER_HANDLE_INDEX(handle);
    uint16_t gen = USER_HANDLE_GEN(handle);

    if (index < 0 || index >= USER_MAX_HANDLES) {
        return false;
    }

    USER_HANDLE_ENTRY *entry = &table->entries[index];

    return (entry->generation == gen && entry->type != USER_TYPE_FREE);
}

/* Global handle table functions */

USER_HANDLE_TABLE *user_get_handle_table(void)
{
    return g_user_handles;
}

int user_handle_table_global_init(void)
{
    if (g_user_handles) {
        return 0;  /* Already initialized */
    }

    g_user_handles = calloc(1, sizeof(USER_HANDLE_TABLE));
    if (!g_user_handles) {
        return -1;
    }

    if (user_handle_table_init(g_user_handles) < 0) {
        free(g_user_handles);
        g_user_handles = NULL;
        return -1;
    }

    printf("USER: Handle table initialized (%d entries)\n", USER_MAX_HANDLES);
    return 0;
}

void user_handle_table_global_shutdown(void)
{
    if (g_user_handles) {
        user_handle_table_shutdown(g_user_handles);
        free(g_user_handles);
        g_user_handles = NULL;
    }
}

void user_handle_set_guest_ptr(uint32_t handle, uint32_t guest_ptr)
{
    if (!g_user_handles || handle == 0) {
        return;
    }

    int index = USER_HANDLE_INDEX(handle);
    uint16_t gen = USER_HANDLE_GEN(handle);

    if (index < 0 || index >= USER_MAX_HANDLES) {
        return;
    }

    USER_HANDLE_ENTRY *entry = &g_user_handles->entries[index];

    /* Validate this is a valid entry */
    if (entry->generation != gen || entry->type == USER_TYPE_FREE) {
        return;
    }

    /* Calculate guest entry address */
    uint32_t entry_va = GUEST_HANDLE_ENTRIES_VA + (index * GUEST_HANDLE_ENTRY_SIZE);

    /* Write to guest handle entry:
     * +0: ptr (guest pointer to object)
     * +4: pOwner
     * +8: type (1 byte) + flags (1 byte) + generation (2 bytes)
     */
    writememll(entry_va + 0, guest_ptr);
    writememll(entry_va + 4, 0);  /* pOwner - we don't track this in guest */
    writememwl(entry_va + 8, (entry->flags << 8) | entry->type);
    writememwl(entry_va + 10, gen);

    printf("USER: Set guest handle entry index=%d ptr=0x%08X gen=%d\n",
           index, guest_ptr, gen);
}
