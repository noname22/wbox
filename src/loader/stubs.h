/*
 * WBOX Stub Code Generator
 * Generates x86 stub code in guest memory for intercepted functions
 */
#ifndef WBOX_STUBS_H
#define WBOX_STUBS_H

#include <stdint.h>
#include <stdbool.h>
#include "../vm/vm.h"

/* Stub types */
typedef enum {
    STUB_TYPE_SYSCALL,      /* Redirect to syscall via SYSENTER */
    STUB_TYPE_RETURN_ZERO,  /* Return 0 immediately */
    STUB_TYPE_RETURN_ERROR, /* Return error code */
} stub_type_t;

/* Stub definition */
typedef struct {
    const char  *name;          /* Function name */
    stub_type_t  type;          /* Stub type */
    uint32_t     syscall_num;   /* For SYSCALL type: syscall number */
    uint32_t     return_value;  /* For RETURN types: value to return */
    int          num_args;      /* Number of stack arguments (for stdcall cleanup) */
} stub_def_t;

/* Stub registry entry */
typedef struct {
    char        *name;          /* Function name (owned) */
    uint32_t     stub_va;       /* VA of generated stub */
} stub_entry_t;

/* Stub manager */
typedef struct stub_manager {
    uint32_t    stub_region_va;     /* Base VA for stub code region */
    uint32_t    stub_region_phys;   /* Physical address */
    uint32_t    stub_region_size;   /* Total size */
    uint32_t    stub_alloc_ptr;     /* Current allocation offset */

    /* Stub registry */
    stub_entry_t *registry;
    uint32_t    registry_count;
    uint32_t    registry_capacity;
} stub_manager_t;

/* Size of each stub (padded to 16 bytes for alignment) */
#define STUB_CODE_SIZE 16

/*
 * Initialize stub manager, allocate stub code region in guest
 * Returns 0 on success, -1 on error
 */
int stubs_init(stub_manager_t *mgr, vm_context_t *vm);

/*
 * Generate a stub for a function
 * Returns VA of generated stub, or 0 on error
 */
uint32_t stubs_generate(stub_manager_t *mgr, vm_context_t *vm,
                        const stub_def_t *def);

/*
 * Lookup existing stub by name
 * Returns VA of stub, or 0 if not found
 */
uint32_t stubs_lookup(stub_manager_t *mgr, const char *name);

/*
 * Get or create stub for a function
 * Returns VA of stub (existing or newly generated), or 0 on error
 */
uint32_t stubs_get_or_create(stub_manager_t *mgr, vm_context_t *vm,
                             const stub_def_t *def);

/*
 * Free stub manager resources
 */
void stubs_free(stub_manager_t *mgr);

#endif /* WBOX_STUBS_H */
