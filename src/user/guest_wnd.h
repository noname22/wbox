/*
 * WBOX Guest WND Management
 * Allocates and manages WND structures in the desktop heap for user mode access
 */
#ifndef WBOX_GUEST_WND_H
#define WBOX_GUEST_WND_H

#include <stdint.h>
#include "user_window.h"

/*
 * Create a guest WND structure for a host WBOX_WND
 * Allocates memory from the desktop heap and initializes all fields
 * Returns guest virtual address of WND, or 0 on failure
 */
uint32_t guest_wnd_create(WBOX_WND *host_wnd);

/*
 * Destroy a guest WND structure
 * Note: Currently just marks it as free since we use a bump allocator
 */
void guest_wnd_destroy(uint32_t guest_va);

/*
 * Synchronize host WBOX_WND data to guest WND
 * Call this after modifying WBOX_WND fields to update the guest copy
 */
void guest_wnd_sync(WBOX_WND *host_wnd);

/*
 * Update window hierarchy pointers in guest WND
 * Call this after linking/unlinking windows
 */
void guest_wnd_update_hierarchy(WBOX_WND *host_wnd);

/*
 * Set the dialog pointer field in guest WND
 * guest_va: The guest WND address
 * dialog_info: Guest pointer to DIALOGINFO (allocated by user32.dll)
 */
void guest_wnd_set_dialog_pointer(uint32_t guest_va, uint32_t dialog_info);

/*
 * Get guest WND address for parent window pointer lookup
 * Returns 0 if host_wnd is NULL or has no guest WND
 */
uint32_t guest_wnd_get_va(WBOX_WND *host_wnd);

#endif /* WBOX_GUEST_WND_H */
