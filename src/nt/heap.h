/*
 * WBOX Heap Manager
 * Provides process heap functionality by intercepting RtlAllocateHeap/RtlFreeHeap
 */
#ifndef WBOX_HEAP_H
#define WBOX_HEAP_H

#include <stdint.h>
#include <stdbool.h>

/* Forward declaration to avoid circular include with vm.h */
typedef struct vm_context vm_context_t;

/* Heap region in guest address space
 * Located at 0x10000000 to avoid overlap with:
 *   - PE images (typically at 0x00400000+)
 *   - Stack (0x04000000-0x08000000, 64MB grows down)
 */
#define HEAP_REGION_VA      0x10000000  /* 256MB mark */
#define HEAP_REGION_SIZE    (16 * 1024 * 1024)  /* 16MB initial heap */

/* Magic heap handle value - should be within the heap region */
#define WBOX_PROCESS_HEAP_HANDLE  0x10000000

/* Heap allocation header (stored before each allocation) */
typedef struct {
    uint32_t magic;     /* HEAP_ALLOC_MAGIC */
    uint32_t size;      /* Allocation size (not including header) */
    uint32_t flags;     /* Allocation flags */
} heap_alloc_header_t;

#define HEAP_ALLOC_MAGIC  0xABCD1234
#define HEAP_FREE_MAGIC   0xDEAD5678

/* Heap state */
typedef struct heap_state {
    uint32_t base_va;           /* Base virtual address of heap region */
    uint32_t base_phys;         /* Physical address of heap region */
    uint32_t size;              /* Total size of heap region */
    uint32_t alloc_ptr;         /* Current allocation offset (bump allocator) */

    /* Statistics */
    uint32_t total_allocated;
    uint32_t total_freed;
    uint32_t num_allocations;
} heap_state_t;

/* Initialize heap subsystem */
int heap_init(heap_state_t *heap, vm_context_t *vm);

/* Allocate from heap - returns guest VA or 0 on failure */
uint32_t heap_alloc(heap_state_t *heap, vm_context_t *vm,
                    uint32_t heap_handle, uint32_t flags, uint32_t size);

/* Free heap allocation - returns true on success */
bool heap_free(heap_state_t *heap, vm_context_t *vm,
               uint32_t heap_handle, uint32_t flags, uint32_t ptr);

/* Realloc heap allocation - returns new VA or 0 on failure */
uint32_t heap_realloc(heap_state_t *heap, vm_context_t *vm,
                      uint32_t heap_handle, uint32_t flags,
                      uint32_t ptr, uint32_t size);

/* Get size of allocation */
uint32_t heap_size(heap_state_t *heap, vm_context_t *vm,
                   uint32_t heap_handle, uint32_t flags, uint32_t ptr);

/* Install function hooks in ntdll.dll for heap functions */
int heap_install_hooks(heap_state_t *heap, vm_context_t *vm);

/* Install function hooks in kernel32.dll */
struct loaded_module;  /* Forward declaration */
int heap_install_kernel32_hooks(vm_context_t *vm, struct loaded_module *kernel32);

/* Check if an address is one of our hooked functions */
bool heap_is_hooked_addr(heap_state_t *heap, uint32_t addr);

/* Handle a heap function call (returns true if handled) */
bool heap_handle_call(heap_state_t *heap, vm_context_t *vm, uint32_t addr);

#endif /* WBOX_HEAP_H */
