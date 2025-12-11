/*
 * WBOX Paging System
 * Builds x86 page tables for protected mode execution
 */
#ifndef WBOX_PAGING_H
#define WBOX_PAGING_H

#include <stdint.h>
#include <stdbool.h>

/* Page table entry flags */
#define PTE_PRESENT    0x001   /* Page is present in memory */
#define PTE_WRITABLE   0x002   /* Page is writable */
#define PTE_USER       0x004   /* Page accessible from Ring 3 */
#define PTE_ACCESSED   0x020   /* Page has been accessed */
#define PTE_DIRTY      0x040   /* Page has been written to */
#define PTE_LARGE      0x080   /* 4MB page (in PDE with PSE) */

/* Page sizes */
#define PAGE_SIZE      4096
#define PAGE_SHIFT     12
#define PAGE_MASK      0xFFFFF000

/* Page directory/table entry counts */
#define PDE_COUNT      1024    /* 4GB / 4MB = 1024 PDEs */
#define PTE_COUNT      1024    /* 4MB / 4KB = 1024 PTEs per PT */

/* Virtual address breakdown */
#define VA_PDE_INDEX(va)  (((va) >> 22) & 0x3FF)
#define VA_PTE_INDEX(va)  (((va) >> 12) & 0x3FF)
#define VA_OFFSET(va)     ((va) & 0xFFF)

/* Physical address layout for page structures */
#define PAGING_PHYS_BASE     0x00100000  /* 1MB - page directory starts here */
#define PAGE_DIRECTORY_PHYS  PAGING_PHYS_BASE

/* Paging context - tracks page table allocation */
typedef struct {
    uint32_t cr3;              /* CR3 value (page directory physical address) */
    uint32_t pd_phys;          /* Page directory physical address */
    uint32_t next_pt_phys;     /* Next available page table physical address */
    uint32_t phys_alloc_base;  /* Base of physical memory for allocations */
    uint32_t phys_alloc_ptr;   /* Next physical address to allocate */
    uint32_t phys_mem_size;    /* Total physical memory size */
} paging_context_t;

/*
 * Initialize paging context
 * phys_base: base physical address for page structures
 * phys_size: total physical memory available
 */
void paging_init(paging_context_t *ctx, uint32_t phys_base, uint32_t phys_size);

/*
 * Map a single 4KB page
 * virt: virtual address (will be page-aligned)
 * phys: physical address (will be page-aligned)
 * flags: PTE_* flags (PTE_PRESENT is always set)
 * Returns 0 on success, -1 on failure
 */
int paging_map_page(paging_context_t *ctx, uint32_t virt, uint32_t phys, uint32_t flags);

/*
 * Map a range of pages
 * virt: starting virtual address
 * phys: starting physical address
 * size: size in bytes (will be rounded up to page size)
 * flags: PTE_* flags
 * Returns 0 on success, -1 on failure
 */
int paging_map_range(paging_context_t *ctx, uint32_t virt, uint32_t phys,
                     uint32_t size, uint32_t flags);

/*
 * Unmap a single page
 * virt: virtual address to unmap
 */
void paging_unmap_page(paging_context_t *ctx, uint32_t virt);

/*
 * Allocate physical memory from the paging pool
 * size: bytes to allocate (rounded up to page size)
 * Returns physical address, or 0 on failure
 */
uint32_t paging_alloc_phys(paging_context_t *ctx, uint32_t size);

/*
 * Check if a virtual address is mapped
 */
bool paging_is_mapped(paging_context_t *ctx, uint32_t virt);

/*
 * Get physical address for a virtual address
 * Returns 0 if not mapped
 */
uint32_t paging_get_phys(paging_context_t *ctx, uint32_t virt);

/*
 * Debug: dump page directory and page tables
 */
void paging_dump(paging_context_t *ctx);

#endif /* WBOX_PAGING_H */
