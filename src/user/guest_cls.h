/*
 * WBOX Guest CLS Management
 * Allocates and manages CLS (class) structures in the desktop heap for user mode access
 */
#ifndef WBOX_GUEST_CLS_H
#define WBOX_GUEST_CLS_H

#include <stdint.h>
#include "user_class.h"

/*
 * Create a guest CLS structure for a host WBOX_CLS
 * Allocates memory from the desktop heap and initializes all fields
 * Returns guest virtual address of CLS, or 0 on failure
 */
uint32_t guest_cls_create(WBOX_CLS *host_cls);

/*
 * Destroy a guest CLS structure
 */
void guest_cls_destroy(uint32_t guest_va);

/*
 * Synchronize host WBOX_CLS data to guest CLS
 * Call this after modifying WBOX_CLS fields to update the guest copy
 */
void guest_cls_sync(WBOX_CLS *host_cls);

/*
 * Get guest CLS address
 * Returns 0 if host_cls is NULL or has no guest CLS
 */
uint32_t guest_cls_get_va(WBOX_CLS *host_cls);

#endif /* WBOX_GUEST_CLS_H */
