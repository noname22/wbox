/*
 * WBOX Desktop Heap Implementation
 */
#include "desktop_heap.h"
#include "../vm/paging.h"
#include "../cpu/mem.h"
#include <stdio.h>
#include <string.h>

/* Global desktop heap context */
static desktop_heap_t g_desktop_heap = {0};

/* Cached VM context for memory writes */
static vm_context_t *g_vm = NULL;

int desktop_heap_init(vm_context_t *vm)
{
    if (g_desktop_heap.initialized) {
        return 0;  /* Already initialized */
    }

    if (!vm) {
        fprintf(stderr, "desktop_heap_init: NULL VM context\n");
        return -1;
    }

    g_vm = vm;

    /* Allocate physical memory for the desktop heap */
    uint32_t phys = paging_alloc_phys(&vm->paging, DESKTOP_HEAP_SIZE);
    if (phys == 0) {
        fprintf(stderr, "desktop_heap_init: Failed to allocate %u bytes\n",
                DESKTOP_HEAP_SIZE);
        return -1;
    }

    /* Map to guest virtual address with user-accessible permissions
     * Note: We use PTE_WRITABLE so we can write from host side,
     * but guest user mode only needs read access for ValidateHwnd */
    if (paging_map_range(&vm->paging, DESKTOP_HEAP_BASE_VA, phys,
                         DESKTOP_HEAP_SIZE,
                         PTE_PRESENT | PTE_USER | PTE_WRITABLE) < 0) {
        fprintf(stderr, "desktop_heap_init: Failed to map desktop heap\n");
        return -1;
    }

    /* Zero out the heap */
    for (uint32_t i = 0; i < DESKTOP_HEAP_SIZE; i++) {
        mem_writeb_phys(phys + i, 0);
    }

    /* Initialize the context */
    g_desktop_heap.base_va = DESKTOP_HEAP_BASE_VA;
    g_desktop_heap.limit_va = DESKTOP_HEAP_LIMIT_VA;
    g_desktop_heap.phys_base = phys;
    g_desktop_heap.alloc_offset = 0;
    g_desktop_heap.initialized = true;

    printf("USER: Desktop heap initialized at VA 0x%08X-0x%08X (phys 0x%08X)\n",
           DESKTOP_HEAP_BASE_VA, DESKTOP_HEAP_LIMIT_VA, phys);

    return 0;
}

void desktop_heap_shutdown(void)
{
    /* Just reset state - physical memory is managed by paging system */
    memset(&g_desktop_heap, 0, sizeof(g_desktop_heap));
    g_vm = NULL;
}

desktop_heap_t *desktop_heap_get(void)
{
    if (!g_desktop_heap.initialized) {
        return NULL;
    }
    return &g_desktop_heap;
}

uint32_t desktop_heap_alloc(uint32_t size)
{
    if (!g_desktop_heap.initialized) {
        fprintf(stderr, "desktop_heap_alloc: heap not initialized\n");
        return 0;
    }

    if (size == 0) {
        return 0;
    }

    /* Align size to 4 bytes */
    size = (size + 3) & ~3;

    /* Check if we have enough space */
    if (g_desktop_heap.alloc_offset + size > DESKTOP_HEAP_SIZE) {
        fprintf(stderr, "desktop_heap_alloc: out of space (need %u, have %u)\n",
                size, DESKTOP_HEAP_SIZE - g_desktop_heap.alloc_offset);
        return 0;
    }

    /* Calculate virtual address */
    uint32_t va = g_desktop_heap.base_va + g_desktop_heap.alloc_offset;

    /* Advance allocation pointer */
    g_desktop_heap.alloc_offset += size;

    return va;
}

void desktop_heap_write(uint32_t va, const void *data, uint32_t size)
{
    if (!g_desktop_heap.initialized || !data || size == 0) {
        return;
    }

    /* Validate address is within heap */
    if (va < g_desktop_heap.base_va || va + size > g_desktop_heap.limit_va) {
        fprintf(stderr, "desktop_heap_write: address 0x%08X out of range\n", va);
        return;
    }

    /* Calculate physical address */
    uint32_t offset = va - g_desktop_heap.base_va;
    uint32_t phys = g_desktop_heap.phys_base + offset;

    /* Write data via physical memory */
    const uint8_t *src = (const uint8_t *)data;
    for (uint32_t i = 0; i < size; i++) {
        mem_writeb_phys(phys + i, src[i]);
    }
}

void desktop_heap_write32(uint32_t va, uint32_t value)
{
    if (!g_desktop_heap.initialized) {
        return;
    }

    if (va < g_desktop_heap.base_va || va + 4 > g_desktop_heap.limit_va) {
        fprintf(stderr, "desktop_heap_write32: address 0x%08X out of range\n", va);
        return;
    }

    uint32_t offset = va - g_desktop_heap.base_va;
    uint32_t phys = g_desktop_heap.phys_base + offset;

    /* Write as little-endian */
    mem_writeb_phys(phys + 0, (value >> 0) & 0xFF);
    mem_writeb_phys(phys + 1, (value >> 8) & 0xFF);
    mem_writeb_phys(phys + 2, (value >> 16) & 0xFF);
    mem_writeb_phys(phys + 3, (value >> 24) & 0xFF);
}

void desktop_heap_write16(uint32_t va, uint16_t value)
{
    if (!g_desktop_heap.initialized) {
        return;
    }

    if (va < g_desktop_heap.base_va || va + 2 > g_desktop_heap.limit_va) {
        fprintf(stderr, "desktop_heap_write16: address 0x%08X out of range\n", va);
        return;
    }

    uint32_t offset = va - g_desktop_heap.base_va;
    uint32_t phys = g_desktop_heap.phys_base + offset;

    mem_writeb_phys(phys + 0, (value >> 0) & 0xFF);
    mem_writeb_phys(phys + 1, (value >> 8) & 0xFF);
}

void desktop_heap_write8(uint32_t va, uint8_t value)
{
    if (!g_desktop_heap.initialized) {
        return;
    }

    if (va < g_desktop_heap.base_va || va + 1 > g_desktop_heap.limit_va) {
        fprintf(stderr, "desktop_heap_write8: address 0x%08X out of range\n", va);
        return;
    }

    uint32_t offset = va - g_desktop_heap.base_va;
    uint32_t phys = g_desktop_heap.phys_base + offset;

    mem_writeb_phys(phys, value);
}

bool desktop_heap_contains(uint32_t va)
{
    if (!g_desktop_heap.initialized) {
        return false;
    }

    return va >= g_desktop_heap.base_va && va < g_desktop_heap.limit_va;
}
