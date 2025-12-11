/*
 * WBOX USER Handle Table
 * Manages HWND, HMENU, HCURSOR and other USER handles
 */
#ifndef WBOX_USER_HANDLE_TABLE_H
#define WBOX_USER_HANDLE_TABLE_H

#include <stdint.h>
#include <stdbool.h>

/* Maximum number of USER handles */
#define USER_MAX_HANDLES    4096

/* Handle types */
typedef enum _USER_HANDLE_TYPE {
    USER_TYPE_FREE = 0,
    USER_TYPE_WINDOW = 1,
    USER_TYPE_MENU = 2,
    USER_TYPE_CURSOR = 3,
    USER_TYPE_SETWINDOWPOS = 4,
    USER_TYPE_HOOK = 5,
    USER_TYPE_CLIPDATA = 6,
    USER_TYPE_CALLPROC = 7,
    USER_TYPE_ACCEL = 8,
    USER_TYPE_DDEACCESS = 9,
    USER_TYPE_DDECONV = 10,
    USER_TYPE_DDEXACT = 11,
    USER_TYPE_MONITOR = 12,
    USER_TYPE_KBDLAYOUT = 13,
    USER_TYPE_KBDFILE = 14,
    USER_TYPE_WINEVENTHOOK = 15,
    USER_TYPE_TIMER = 16,
    USER_TYPE_INPUTCONTEXT = 17,
    USER_TYPE_HIDDATA = 18,
    USER_TYPE_DEVICEINFO = 19,
    USER_TYPE_TOUCHINPUT = 20,
    USER_TYPE_GESTUREINFO = 21,
    USER_TYPE_MAX
} USER_HANDLE_TYPE;

/* Handle entry in the table */
typedef struct _USER_HANDLE_ENTRY {
    void *ptr;                  /* Pointer to the object */
    void *owner;                /* Owner (thread or process) */
    uint8_t type;               /* Handle type */
    uint8_t flags;              /* Flags */
    uint16_t generation;        /* Generation counter for validation */
} USER_HANDLE_ENTRY;

/* Handle table structure */
typedef struct _USER_HANDLE_TABLE {
    USER_HANDLE_ENTRY entries[USER_MAX_HANDLES];
    int first_free;             /* Index of first free entry */
    int handle_count;           /* Number of allocated handles */
} USER_HANDLE_TABLE;

/*
 * Handle encoding:
 *   Bits 0-15:  Index into handle table
 *   Bits 16-31: Generation counter (for handle validation)
 */
#define USER_MAKE_HANDLE(index, gen)    (((uint32_t)(gen) << 16) | ((index) & 0xFFFF))
#define USER_HANDLE_INDEX(h)            ((h) & 0xFFFF)
#define USER_HANDLE_GEN(h)              (((h) >> 16) & 0xFFFF)

/* Special handles */
#define HWND_DESKTOP        ((uint32_t)0)
#define HWND_BROADCAST      ((uint32_t)0xFFFF)
#define HWND_TOP            ((uint32_t)0)
#define HWND_BOTTOM         ((uint32_t)1)
#define HWND_TOPMOST        ((uint32_t)-1)
#define HWND_NOTOPMOST      ((uint32_t)-2)
#define HWND_MESSAGE        ((uint32_t)-3)

/*
 * Initialize the USER handle table
 */
int user_handle_table_init(USER_HANDLE_TABLE *table);

/*
 * Shutdown the handle table
 */
void user_handle_table_shutdown(USER_HANDLE_TABLE *table);

/*
 * Allocate a handle for an object
 * Returns the handle value, or 0 on failure
 */
uint32_t user_handle_alloc(USER_HANDLE_TABLE *table, void *ptr,
                           USER_HANDLE_TYPE type, void *owner);

/*
 * Free a handle
 * Returns true on success
 */
bool user_handle_free(USER_HANDLE_TABLE *table, uint32_t handle);

/*
 * Get the object pointer for a handle
 * Returns NULL if handle is invalid
 */
void *user_handle_get(USER_HANDLE_TABLE *table, uint32_t handle);

/*
 * Get the object pointer with type checking
 * Returns NULL if handle is invalid or wrong type
 */
void *user_handle_get_typed(USER_HANDLE_TABLE *table, uint32_t handle,
                            USER_HANDLE_TYPE expected_type);

/*
 * Get handle type
 */
USER_HANDLE_TYPE user_handle_get_type(USER_HANDLE_TABLE *table, uint32_t handle);

/*
 * Get handle owner
 */
void *user_handle_get_owner(USER_HANDLE_TABLE *table, uint32_t handle);

/*
 * Check if handle is valid
 */
bool user_handle_is_valid(USER_HANDLE_TABLE *table, uint32_t handle);

/*
 * Get the global USER handle table
 */
USER_HANDLE_TABLE *user_get_handle_table(void);

/*
 * Initialize the global USER handle table
 */
int user_handle_table_global_init(void);

/*
 * Shutdown the global handle table
 */
void user_handle_table_global_shutdown(void);

#endif /* WBOX_USER_HANDLE_TABLE_H */
