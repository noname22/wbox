/*
 * WBOX Paging System
 * Builds x86 page tables for protected mode execution
 */
#include "paging.h"
#include "mem.h"

#include <stdio.h>
#include <string.h>

void paging_init(paging_context_t *ctx, uint32_t phys_base, uint32_t phys_size)
{
    memset(ctx, 0, sizeof(*ctx));

    /* Align base to page boundary */
    phys_base = (phys_base + PAGE_SIZE - 1) & PAGE_MASK;

    ctx->pd_phys = phys_base;
    ctx->cr3 = phys_base;
    ctx->next_pt_phys = phys_base + PAGE_SIZE;  /* First PT is right after PD */
    ctx->phys_alloc_base = phys_base + (256 * PAGE_SIZE);  /* Reserve 1MB for page tables */
    ctx->phys_alloc_ptr = ctx->phys_alloc_base;
    ctx->phys_mem_size = phys_size;

    /* Clear the page directory */
    for (int i = 0; i < PDE_COUNT; i++) {
        mem_writel_phys(ctx->pd_phys + i * 4, 0);
    }
}

/* Allocate a page table from our reserved area */
static uint32_t alloc_page_table(paging_context_t *ctx)
{
    uint32_t pt_phys = ctx->next_pt_phys;
    ctx->next_pt_phys += PAGE_SIZE;

    /* Clear the new page table */
    for (int i = 0; i < PTE_COUNT; i++) {
        mem_writel_phys(pt_phys + i * 4, 0);
    }

    return pt_phys;
}

int paging_map_page(paging_context_t *ctx, uint32_t virt, uint32_t phys, uint32_t flags)
{
    virt &= PAGE_MASK;
    phys &= PAGE_MASK;

    uint32_t pde_index = VA_PDE_INDEX(virt);
    uint32_t pte_index = VA_PTE_INDEX(virt);

    /* Read PDE */
    uint32_t pde_addr = ctx->pd_phys + pde_index * 4;
    uint32_t pde = mem_readl_phys(pde_addr);

    uint32_t pt_phys;
    if (!(pde & PTE_PRESENT)) {
        /* Page table not present, allocate one */
        pt_phys = alloc_page_table(ctx);
        /* Create PDE: present + writable + user (allow all access at PDE level) */
        pde = pt_phys | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
        mem_writel_phys(pde_addr, pde);
    } else {
        pt_phys = pde & PAGE_MASK;
    }

    /* Create PTE */
    uint32_t pte_addr = pt_phys + pte_index * 4;
    uint32_t pte = phys | PTE_PRESENT | (flags & (PTE_WRITABLE | PTE_USER));
    mem_writel_phys(pte_addr, pte);

    return 0;
}

int paging_map_range(paging_context_t *ctx, uint32_t virt, uint32_t phys,
                     uint32_t size, uint32_t flags)
{
    /* Round size up to page boundary */
    size = (size + PAGE_SIZE - 1) & PAGE_MASK;

    uint32_t end = virt + size;
    while (virt < end) {
        if (paging_map_page(ctx, virt, phys, flags) != 0) {
            return -1;
        }
        virt += PAGE_SIZE;
        phys += PAGE_SIZE;
    }

    return 0;
}

void paging_unmap_page(paging_context_t *ctx, uint32_t virt)
{
    virt &= PAGE_MASK;

    uint32_t pde_index = VA_PDE_INDEX(virt);
    uint32_t pte_index = VA_PTE_INDEX(virt);

    /* Read PDE */
    uint32_t pde_addr = ctx->pd_phys + pde_index * 4;
    uint32_t pde = mem_readl_phys(pde_addr);

    if (!(pde & PTE_PRESENT)) {
        return;  /* Page table not present, nothing to unmap */
    }

    uint32_t pt_phys = pde & PAGE_MASK;
    uint32_t pte_addr = pt_phys + pte_index * 4;

    /* Clear PTE */
    mem_writel_phys(pte_addr, 0);
}

uint32_t paging_alloc_phys(paging_context_t *ctx, uint32_t size)
{
    /* Round size up to page boundary */
    size = (size + PAGE_SIZE - 1) & PAGE_MASK;

    if (ctx->phys_alloc_ptr + size > ctx->phys_mem_size) {
        fprintf(stderr, "paging_alloc_phys: out of physical memory\n");
        return 0;
    }

    uint32_t addr = ctx->phys_alloc_ptr;
    ctx->phys_alloc_ptr += size;

    /* Zero the allocated memory */
    for (uint32_t i = 0; i < size; i++) {
        mem_writeb_phys(addr + i, 0);
    }

    return addr;
}

bool paging_is_mapped(paging_context_t *ctx, uint32_t virt)
{
    virt &= PAGE_MASK;

    uint32_t pde_index = VA_PDE_INDEX(virt);
    uint32_t pde = mem_readl_phys(ctx->pd_phys + pde_index * 4);

    if (!(pde & PTE_PRESENT)) {
        return false;
    }

    uint32_t pt_phys = pde & PAGE_MASK;
    uint32_t pte_index = VA_PTE_INDEX(virt);
    uint32_t pte = mem_readl_phys(pt_phys + pte_index * 4);

    return (pte & PTE_PRESENT) != 0;
}

uint32_t paging_get_phys(paging_context_t *ctx, uint32_t virt)
{
    uint32_t offset = VA_OFFSET(virt);
    virt &= PAGE_MASK;

    uint32_t pde_index = VA_PDE_INDEX(virt);
    uint32_t pde = mem_readl_phys(ctx->pd_phys + pde_index * 4);

    if (!(pde & PTE_PRESENT)) {
        return 0;
    }

    uint32_t pt_phys = pde & PAGE_MASK;
    uint32_t pte_index = VA_PTE_INDEX(virt);
    uint32_t pte = mem_readl_phys(pt_phys + pte_index * 4);

    if (!(pte & PTE_PRESENT)) {
        return 0;
    }

    return (pte & PAGE_MASK) | offset;
}

void paging_dump(paging_context_t *ctx)
{
    printf("Paging Context:\n");
    printf("  CR3 (PD phys): 0x%08X\n", ctx->cr3);
    printf("  Next PT phys:  0x%08X\n", ctx->next_pt_phys);
    printf("  Alloc ptr:     0x%08X\n", ctx->phys_alloc_ptr);
    printf("\n");

    printf("Page Directory (non-empty entries):\n");
    for (int i = 0; i < PDE_COUNT; i++) {
        uint32_t pde = mem_readl_phys(ctx->pd_phys + i * 4);
        if (pde & PTE_PRESENT) {
            uint32_t va_start = i << 22;
            printf("  PDE[%3d]: VA 0x%08X-0x%08X -> PT 0x%08X  flags=0x%03X\n",
                   i, va_start, va_start + 0x3FFFFF, pde & PAGE_MASK, pde & 0xFFF);

            /* Dump page table entries */
            uint32_t pt_phys = pde & PAGE_MASK;
            int first_mapped = -1;
            int last_mapped = -1;
            for (int j = 0; j < PTE_COUNT; j++) {
                uint32_t pte = mem_readl_phys(pt_phys + j * 4);
                if (pte & PTE_PRESENT) {
                    if (first_mapped < 0) first_mapped = j;
                    last_mapped = j;
                }
            }
            if (first_mapped >= 0) {
                printf("           PT has entries from [%d] to [%d]\n", first_mapped, last_mapped);
            }
        }
    }
}
