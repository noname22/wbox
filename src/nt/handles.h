/*
 * WBOX NT Handle Table
 * Manages file and object handles for the emulated process
 */
#ifndef WBOX_HANDLES_H
#define WBOX_HANDLES_H

#include <stdint.h>

/* Handle types */
typedef enum {
    HANDLE_TYPE_NONE = 0,
    HANDLE_TYPE_FILE,
    HANDLE_TYPE_CONSOLE_IN,
    HANDLE_TYPE_CONSOLE_OUT,
    HANDLE_TYPE_CONSOLE_ERR,
    HANDLE_TYPE_EVENT,
    HANDLE_TYPE_SEMAPHORE,
    HANDLE_TYPE_MUTEX,
    HANDLE_TYPE_MUTANT,       /* Mutex (NT naming) */
    HANDLE_TYPE_SECTION,
    HANDLE_TYPE_KEY,          /* Registry key */
    HANDLE_TYPE_THREAD,       /* Thread handle */
} handle_type_t;

/* Handle entry */
typedef struct {
    handle_type_t type;
    int host_fd;           /* Host file descriptor (-1 if not applicable) */
    uint32_t access_mask;  /* Requested access flags (GENERIC_READ, etc.) */
    uint64_t file_offset;  /* Current file position for seekable files */
    void *object_data;     /* Pointer to sync object (events, semaphores, mutexes) */
} handle_entry_t;

/* Maximum number of handles per process */
#define MAX_HANDLES 256

/* Handle table */
typedef struct {
    handle_entry_t entries[MAX_HANDLES];
    uint32_t next_handle;
} handle_table_t;

/* Windows standard handle pseudo-values */
#define STD_INPUT_HANDLE  ((uint32_t)-10)  /* 0xFFFFFFF6 */
#define STD_OUTPUT_HANDLE ((uint32_t)-11)  /* 0xFFFFFFF5 */
#define STD_ERROR_HANDLE  ((uint32_t)-12)  /* 0xFFFFFFF4 */

/*
 * Initialize handle table
 * Pre-populates stdin/stdout/stderr handles
 */
void handles_init(handle_table_t *ht);

/*
 * Add a new handle to the table
 * Returns the handle value, or 0 on failure
 */
uint32_t handles_add(handle_table_t *ht, handle_type_t type, int host_fd);

/*
 * Add a new handle with an associated object (for sync objects)
 * Returns the handle value, or 0 on failure
 */
uint32_t handles_add_object(handle_table_t *ht, handle_type_t type, void *object_data);

/*
 * Get handle entry by handle value
 * Returns NULL if handle is invalid
 */
handle_entry_t *handles_get(handle_table_t *ht, uint32_t handle);

/*
 * Remove a handle from the table
 */
void handles_remove(handle_table_t *ht, uint32_t handle);

/*
 * Resolve a handle, including standard pseudo-handles
 * This is the main entry point for syscall handlers
 * Returns NULL if handle is invalid
 */
handle_entry_t *handles_resolve(handle_table_t *ht, uint32_t handle);

#endif /* WBOX_HANDLES_H */
