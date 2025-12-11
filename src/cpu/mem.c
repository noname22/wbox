/*
 * Memory handling and MMU.
 * Ported from 86Box (https://github.com/86Box/86Box)
 *
 * Original Authors: Sarah Walker, Miran Grca, Fred N. van Kempen
 * Copyright 2008-2020 Sarah Walker.
 * Copyright 2016-2020 Miran Grca.
 * Copyright 2017-2020 Fred N. van Kempen.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

#include "cpu.h"
#include "mem.h"
#include "x86.h"
#include "x86seg_common.h"

#ifdef USE_DYNAREC
#include "codegen_public.h"
#endif

/* External: configured memory size from stubs.c */
extern uint32_t mem_size;

/* Default RAM size - 16 MB */
#define DEFAULT_RAM_SIZE (16 * 1024 * 1024)

/* RAM */
uint8_t *ram = NULL;
uint8_t *ram2 = NULL;
uint32_t rammask;
static size_t ram_size = 0;

uint8_t *rom = NULL;
uint32_t biosmask = 0;

uint8_t page_ff[4096];
static uint8_t ff_pccache[4] = { 0xff, 0xff, 0xff, 0xff };

/* Page table */
page_t *pages = NULL;
uint32_t pages_sz = 0;
uint32_t addr_space_size = 1048576;

/* Lookup tables */
int readlookup[256];
int writelookup[256];
int readlnext = 0;
int writelnext = 0;
int cachesize = 256;

page_t *page_lookup[1048576];
uintptr_t readlookup2[1048576];
uintptr_t writelookup2[1048576];
uintptr_t old_rl2 = 0;
uint8_t uncached = 0;

/* PC cache for instruction fetch */
uint32_t pccache;
uint8_t *pccache2;

/* Memory mapping arrays */
mem_mapping_t *read_mapping[MEM_MAPPINGS_NO];
mem_mapping_t *write_mapping[MEM_MAPPINGS_NO];
uint8_t *_mem_exec[MEM_MAPPINGS_NO];

/* Memory state */
uint32_t mem_logical_addr = 0;
uint32_t get_phys_virt = 0;
uint32_t get_phys_phys = 0;
uint8_t high_page = 0;
int mmuflush = 0;
int read_type = 0;

/* A20 gate */
int mem_a20_key = 0;
int mem_a20_alt = 0;
int mem_a20_state = 1;

/* Shadow BIOS */
int shadowbios = 0;
int shadowbios_write = 0;

#ifdef USE_NEW_DYNAREC
uint64_t *byte_dirty_mask = NULL;
uint64_t *byte_code_present_mask = NULL;
uint32_t purgable_page_list_head = EVICT_NOT_IN_LIST;
int purgeable_page_count = 0;
#endif

/* Memory mapping linked list */
static mem_mapping_t *base_mapping = NULL;
static mem_mapping_t *last_mapping = NULL;

/* RAM mappings */
static mem_mapping_t ram_low_mapping;
static mem_mapping_t ram_high_mapping;

/* Forward declarations */
static uint8_t mem_read_ram(uint32_t addr, void *priv);
static uint16_t mem_read_ramw(uint32_t addr, void *priv);
static uint32_t mem_read_raml(uint32_t addr, void *priv);
static void mem_write_ram(uint32_t addr, uint8_t val, void *priv);
static void mem_write_ramw(uint32_t addr, uint16_t val, void *priv);
static void mem_write_raml(uint32_t addr, uint32_t val, void *priv);

/*
 * Check if address is in RAM.
 */
int
mem_addr_is_ram(uint32_t addr)
{
    return (addr < ram_size);
}

/*
 * Reset the lookup tables.
 */
void
resetreadlookup(void)
{
    memset(page_lookup, 0x00, (1 << 20) * sizeof(page_t *));

    for (int c = 0; c < 256; c++) {
        readlookup[c] = 0xffffffff;
        writelookup[c] = 0xffffffff;
    }

    memset(readlookup2, 0xff, (1 << 20) * sizeof(uintptr_t));
    memset(writelookup2, 0xff, (1 << 20) * sizeof(uintptr_t));

    readlnext = 0;
    writelnext = 0;
    pccache = 0xffffffff;
    high_page = 0;
}

/*
 * Flush the MMU cache.
 */
void
flushmmucache(void)
{
    for (int c = 0; c < 256; c++) {
        if (readlookup[c] != (int) 0xffffffff) {
            readlookup2[readlookup[c]] = LOOKUP_INV;
            readlookup[c] = 0xffffffff;
        }
        if (writelookup[c] != (int) 0xffffffff) {
            page_lookup[writelookup[c]] = NULL;
            writelookup2[writelookup[c]] = LOOKUP_INV;
            writelookup[c] = 0xffffffff;
        }
    }
    mmuflush++;

    pccache = (uint32_t) 0xffffffff;
    pccache2 = (uint8_t *) (uintptr_t) 0xffffffff;

#ifdef USE_DYNAREC
    codegen_flush();
#endif
}

void
flushmmucache_write(void)
{
    for (int c = 0; c < 256; c++) {
        if (writelookup[c] != (int) 0xffffffff) {
            page_lookup[writelookup[c]] = NULL;
            writelookup2[writelookup[c]] = LOOKUP_INV;
            writelookup[c] = 0xffffffff;
        }
    }
    mmuflush++;
}

void
flushmmucache_pc(void)
{
    mmuflush++;

    pccache = (uint32_t) 0xffffffff;
    pccache2 = (uint8_t *) (uintptr_t) 0xffffffff;

#ifdef USE_DYNAREC
    codegen_flush();
#endif
}

void
flushmmucache_nopc(void)
{
    for (int c = 0; c < 256; c++) {
        if (readlookup[c] != (int) 0xffffffff) {
            readlookup2[readlookup[c]] = LOOKUP_INV;
            readlookup[c] = 0xffffffff;
        }
        if (writelookup[c] != (int) 0xffffffff) {
            page_lookup[writelookup[c]] = NULL;
            writelookup2[writelookup[c]] = LOOKUP_INV;
            writelookup[c] = 0xffffffff;
        }
    }
}

void
mem_flush_write_page(uint32_t addr, uint32_t virt)
{
    if (!pages)
        return;

    const page_t *page_target = &pages[addr >> 12];

    for (int c = 0; c < 256; c++) {
        if (writelookup[c] != (int) 0xffffffff) {
            uintptr_t target = (uintptr_t) &ram[(uintptr_t) (addr & ~0xfff) - (virt & ~0xfff)];
            if (writelookup2[writelookup[c]] == target || page_lookup[writelookup[c]] == page_target) {
                writelookup2[writelookup[c]] = LOOKUP_INV;
                page_lookup[writelookup[c]] = NULL;
                writelookup[c] = 0xffffffff;
            }
        }
    }
}

void
mem_invalidate_range(uint32_t start_addr, uint32_t end_addr)
{
    (void)start_addr;
    (void)end_addr;
    flushmmucache_nopc();
}

/*
 * MMU translation macros.
 */
#define mmutranslate_read(addr)  mmutranslatereal(addr, 0)
#define mmutranslate_write(addr) mmutranslatereal(addr, 1)

/* Direct RAM access for page table reads */
#define rammap(x)   ((uint32_t *) &ram[(x) & rammask])[0]
#define rammap64(x) ((uint64_t *) &ram[(x) & rammask])[0]

/*
 * Standard 32-bit paging translation.
 */
static __inline uint64_t
mmutranslatereal_normal(uint32_t addr, int rw)
{
    uint32_t temp, temp2, temp3;
    uint32_t addr2;

    if (cpu_state.abrt)
        return 0xffffffffffffffffULL;

    addr2 = ((cr3 & ~0xfff) + ((addr >> 20) & 0xffc));
    temp = temp2 = rammap(addr2);

    if (!(temp & 1)) {
        cr2 = addr;
        temp &= 1;
        if (CPL == 3)
            temp |= 4;
        if (rw)
            temp |= 2;
        cpu_state.abrt = ABRT_PF;
        abrt_error = temp;
        return 0xffffffffffffffffULL;
    }

    /* 4MB page (PSE) */
    if ((temp & 0x80) && (cr4 & CR4_PSE)) {
        if (((CPL == 3) && !(temp & 4) && !cpl_override) ||
            (rw && !cpl_override && !(temp & 2) &&
             (((CPL == 3) && !cpl_override) || (cr0 & WP_FLAG)))) {
            cr2 = addr;
            temp &= 1;
            if (CPL == 3)
                temp |= 4;
            if (rw)
                temp |= 2;
            cpu_state.abrt = ABRT_PF;
            abrt_error = temp;
            return 0xffffffffffffffffULL;
        }

        rammap(addr2) |= (rw ? 0x60 : 0x20);

        uint64_t page = temp & ~0x3fffff;
        if (cpu_features & CPU_FEATURE_PSE36)
            page |= (uint64_t) (temp & 0x1e000) << 19;
        return page + (addr & 0x3fffff);
    }

    /* Regular 4KB page */
    temp = rammap((temp & ~0xfff) + ((addr >> 10) & 0xffc));
    temp3 = temp & temp2;

    if (!(temp & 1) ||
        ((CPL == 3) && !(temp3 & 4) && !cpl_override) ||
        (rw && !cpl_override && !(temp3 & 2) &&
         (((CPL == 3) && !cpl_override) || (cr0 & WP_FLAG)))) {
        /* WBOX: Trace page fault with recent instruction history */
        static int pf_count = 0;
        if (pf_count < 5) {
            fprintf(stderr, "[PF#%d] VA=0x%08X rw=%d CPL=%d temp3=0x%08X PC=0x%08X\n",
                    pf_count, addr, rw, CPL, temp3, cpu_state.pc);
            fprintf(stderr, "  oldpc=0x%08X\n", cpu_state.oldpc);
            pf_count++;
        }
        cr2 = addr;
        temp &= 1;
        if (CPL == 3)
            temp |= 4;
        if (rw)
            temp |= 2;
        cpu_state.abrt = ABRT_PF;
        abrt_error = temp;
        return 0xffffffffffffffffULL;
    }

    rammap(addr2) |= 0x20;
    rammap((temp2 & ~0xfff) + ((addr >> 10) & 0xffc)) |= (rw ? 0x60 : 0x20);

    return (uint64_t) ((temp & ~0xfff) + (addr & 0xfff));
}

/*
 * PAE paging translation (3-level page tables).
 */
static __inline uint64_t
mmutranslatereal_pae(uint32_t addr, int rw)
{
    uint64_t temp, temp2, temp3, temp4;
    uint64_t addr2, addr3, addr4;

    if (cpu_state.abrt)
        return 0xffffffffffffffffULL;

    /* PDPT entry */
    addr2 = (cr3 & ~0x1f) + ((addr >> 27) & 0x18);
    temp = temp2 = rammap64(addr2) & 0x000000ffffffffffULL;

    if (!(temp & 1)) {
        cr2 = addr;
        temp &= 1;
        if (CPL == 3)
            temp |= 4;
        if (rw)
            temp |= 2;
        cpu_state.abrt = ABRT_PF;
        abrt_error = temp;
        return 0xffffffffffffffffULL;
    }

    /* PDE */
    addr3 = (temp & ~0xfffULL) + ((addr >> 18) & 0xff8);
    temp = temp4 = rammap64(addr3) & 0x000000ffffffffffULL;
    temp3 = temp & temp2;

    if (!(temp & 1)) {
        cr2 = addr;
        temp &= 1;
        if (CPL == 3)
            temp |= 4;
        if (rw)
            temp |= 2;
        cpu_state.abrt = ABRT_PF;
        abrt_error = temp;
        return 0xffffffffffffffffULL;
    }

    /* 2MB page */
    if (temp & 0x80) {
        if (((CPL == 3) && !(temp & 4) && !cpl_override) ||
            (rw && !cpl_override && !(temp & 2) &&
             (((CPL == 3) && !cpl_override) || (cr0 & WP_FLAG)))) {
            cr2 = addr;
            temp &= 1;
            if (CPL == 3)
                temp |= 4;
            if (rw)
                temp |= 2;
            cpu_state.abrt = ABRT_PF;
            abrt_error = temp;
            return 0xffffffffffffffffULL;
        }
        rammap64(addr3) |= (rw ? 0x60 : 0x20);
        return ((temp & ~0x1fffffULL) + (addr & 0x1fffffULL)) & 0x000000ffffffffffULL;
    }

    /* PTE */
    addr4 = (temp & ~0xfffULL) + ((addr >> 9) & 0xff8);
    temp = rammap64(addr4) & 0x000000ffffffffffULL;
    temp3 = temp & temp4;

    if (!(temp & 1) ||
        ((CPL == 3) && !(temp3 & 4) && !cpl_override) ||
        (rw && !cpl_override && !(temp3 & 2) &&
         (((CPL == 3) && !cpl_override) || (cr0 & WP_FLAG)))) {
        cr2 = addr;
        temp &= 1;
        if (CPL == 3)
            temp |= 4;
        if (rw)
            temp |= 2;
        cpu_state.abrt = ABRT_PF;
        abrt_error = temp;
        return 0xffffffffffffffffULL;
    }

    rammap64(addr3) |= 0x20;
    rammap64(addr4) |= (rw ? 0x60 : 0x20);

    return ((temp & ~0xfffULL) + ((uint64_t) (addr & 0xfff))) & 0x000000ffffffffffULL;
}

/*
 * Main MMU translation entry point.
 */
uint64_t
mmutranslatereal(uint32_t addr, int rw)
{
    if (cpu_state.abrt)
        return 0xffffffffffffffffULL;

    if (cr4 & CR4_PAE)
        return mmutranslatereal_pae(addr, rw);
    else
        return mmutranslatereal_normal(addr, rw);
}

uint32_t
mmutranslatereal32(uint32_t addr, int rw)
{
    if (cpu_state.abrt)
        return 0xffffffff;

    return (uint32_t) mmutranslatereal(addr, rw);
}

/*
 * No-abort version of MMU translation.
 */
static __inline uint64_t
mmutranslate_noabrt_normal(uint32_t addr, int rw)
{
    uint32_t temp, temp2, temp3;
    uint32_t addr2;

    if (cpu_state.abrt)
        return 0xffffffffffffffffULL;

    addr2 = ((cr3 & ~0xfff) + ((addr >> 20) & 0xffc));
    temp = temp2 = rammap(addr2);

    if (!(temp & 1))
        return 0xffffffffffffffffULL;

    if ((temp & 0x80) && (cr4 & CR4_PSE)) {
        if (((CPL == 3) && !(temp & 4) && !cpl_override) ||
            (rw && !cpl_override && !(temp & 2) && ((CPL == 3) || (cr0 & WP_FLAG))))
            return 0xffffffffffffffffULL;

        uint64_t page = temp & ~0x3fffff;
        if (cpu_features & CPU_FEATURE_PSE36)
            page |= (uint64_t) (temp & 0x1e000) << 19;
        return page + (addr & 0x3fffff);
    }

    temp = rammap((temp & ~0xfff) + ((addr >> 10) & 0xffc));
    temp3 = temp & temp2;

    if (!(temp & 1) ||
        ((CPL == 3) && !(temp3 & 4) && !cpl_override) ||
        (rw && !cpl_override && !(temp3 & 2) && ((CPL == 3) || (cr0 & WP_FLAG))))
        return 0xffffffffffffffffULL;

    return (uint64_t) ((temp & ~0xfff) + (addr & 0xfff));
}

static __inline uint64_t
mmutranslate_noabrt_pae(uint32_t addr, int rw)
{
    uint64_t temp, temp2, temp3, temp4;
    uint64_t addr2, addr3, addr4;

    if (cpu_state.abrt)
        return 0xffffffffffffffffULL;

    addr2 = (cr3 & ~0x1f) + ((addr >> 27) & 0x18);
    temp = temp2 = rammap64(addr2) & 0x000000ffffffffffULL;

    if (!(temp & 1))
        return 0xffffffffffffffffULL;

    addr3 = (temp & ~0xfffULL) + ((addr >> 18) & 0xff8);
    temp = temp4 = rammap64(addr3) & 0x000000ffffffffffULL;
    temp3 = temp & temp2;

    if (!(temp & 1))
        return 0xffffffffffffffffULL;

    if (temp & 0x80) {
        if (((CPL == 3) && !(temp & 4) && !cpl_override) ||
            (rw && !cpl_override && !(temp & 2) && ((CPL == 3) || (cr0 & WP_FLAG))))
            return 0xffffffffffffffffULL;
        return ((temp & ~0x1fffffULL) + (addr & 0x1fffff)) & 0x000000ffffffffffULL;
    }

    addr4 = (temp & ~0xfffULL) + ((addr >> 9) & 0xff8);
    temp = rammap64(addr4) & 0x000000ffffffffffULL;
    temp3 = temp & temp4;

    if (!(temp & 1) ||
        ((CPL == 3) && !(temp3 & 4) && !cpl_override) ||
        (rw && !cpl_override && !(temp3 & 2) && ((CPL == 3) || (cr0 & WP_FLAG))))
        return 0xffffffffffffffffULL;

    return ((temp & ~0xfffULL) + ((uint64_t) (addr & 0xfff))) & 0x000000ffffffffffULL;
}

uint64_t
mmutranslate_noabrt(uint32_t addr, int rw)
{
    if (cpu_state.abrt)
        return 0xffffffffffffffffULL;

    if (cr4 & CR4_PAE)
        return mmutranslate_noabrt_pae(addr, rw);
    else
        return mmutranslate_noabrt_normal(addr, rw);
}

/*
 * Add a read lookup entry.
 */
void
addreadlookup(uint32_t virt, uint32_t phys)
{
    if (virt == 0xffffffff)
        return;

    if (readlookup2[virt >> 12] != (uintptr_t) LOOKUP_INV)
        return;

    if (readlookup[readlnext] != (int) 0xffffffff) {
        if ((readlookup[readlnext] == (int)((es + DI) >> 12)) ||
            (readlookup[readlnext] == (int)((es + EDI) >> 12)))
            uncached = 1;
        readlookup2[readlookup[readlnext]] = LOOKUP_INV;
    }

    readlookup2[virt >> 12] = (uintptr_t) &ram[(uintptr_t) (phys & ~0xFFF) - (uintptr_t) (virt & ~0xfff)];

    readlookup[readlnext++] = virt >> 12;
    readlnext &= (cachesize - 1);
}

/*
 * Add a write lookup entry.
 */
void
addwritelookup(uint32_t virt, uint32_t phys)
{
    if (virt == 0xffffffff)
        return;

    if (page_lookup[virt >> 12])
        return;

    if (writelookup2[virt >> 12] != (uintptr_t) LOOKUP_INV)
        return;

    if (writelookup[writelnext] != -1) {
        page_lookup[writelookup[writelnext]] = NULL;
        writelookup2[writelookup[writelnext]] = LOOKUP_INV;
    }

#ifdef USE_NEW_DYNAREC
    if (pages && pages[phys >> 12].block) {
        page_lookup[virt >> 12] = &pages[phys >> 12];
    } else
#endif
    {
        writelookup2[virt >> 12] = (uintptr_t) &ram[(uintptr_t) (phys & ~0xFFF) - (uintptr_t) (virt & ~0xfff)];
    }

    writelookup[writelnext++] = virt >> 12;
    writelnext &= (cachesize - 1);
}

/*
 * Get executable memory pointer for instruction fetch.
 * Returns a BIASED pointer such that ptr[linear_addr] = ram[physical_addr].
 * For identity mapping (real mode): returns ram
 * For paged mapping: returns ram + (phys_page - virt_page)
 */
static int getpccache_trace_count = 0;

uint8_t *
getpccache(uint32_t a)
{
    uint64_t phys = (uint64_t) a;
    uint32_t virt = a;

    /* Trace first few code fetches */
    if (getpccache_trace_count < 10) {
        fprintf(stderr, "getpccache[%d]: VA=0x%08X\n", getpccache_trace_count, virt);
        getpccache_trace_count++;
    }

    /* Trace execution around user32.dll crash area */
    if (virt >= 0x77A4ED60 && virt <= 0x77A4EDA0) {
        fprintf(stderr, "[USER32_PC] VA=0x%08X EAX=0x%08X EDX=0x%08X\n", virt, EAX, EDX);
    }
    /* Trace syscall stub access */
    if (virt >= 0x7FFE0340 && virt <= 0x7FFE0360) {
        fprintf(stderr, "[SYSCALL_STUB_PC] VA=0x%08X EAX=0x%08X\n", virt, EAX);
    }

    /* Handle paging if enabled */
    if (cr0 >> 31) {
        phys = mmutranslate_read(virt);
        if (phys == 0xffffffffffffffffULL) {
            fprintf(stderr, "getpccache: MMU translation failed for VA 0x%08X (page=0x%05X)\n",
                    virt, virt >> 12);
            cpu_state.abrt = ABRT_PF;  /* Set page fault instead of returning invalid pointer */
            return (uint8_t *) &ff_pccache;
        }
    }
    phys &= rammask;

    /* Check if physical address is in RAM range */
    if (phys < ram_size && ram) {
        /*
         * Return a biased pointer such that ptr[virt] gives ram[phys].
         * ptr[virt] = ptr + virt = ram + phys
         * So ptr = ram + phys - virt = ram + (phys_page - virt_page)
         * For identity mapping (virt == phys), this simplifies to ram.
         */
        return (uint8_t *) ((uintptr_t) ram + (uintptr_t) (phys & ~0xfff) - (uintptr_t) (virt & ~0xfff));
    }

    /* No executable memory at this address, return error page */
    return (uint8_t *) &ff_pccache;
}

/*
 * RAM read callbacks.
 */
static uint8_t
mem_read_ram(uint32_t addr, void *priv)
{
    (void)priv;
    return ram[addr];
}

static uint16_t
mem_read_ramw(uint32_t addr, void *priv)
{
    (void)priv;
    return *(uint16_t *)&ram[addr];
}

static uint32_t
mem_read_raml(uint32_t addr, void *priv)
{
    (void)priv;
    return *(uint32_t *)&ram[addr];
}

/*
 * RAM write callbacks.
 */
static void
mem_write_ram(uint32_t addr, uint8_t val, void *priv)
{
    (void)priv;
    ram[addr] = val;
}

static void
mem_write_ramw(uint32_t addr, uint16_t val, void *priv)
{
    (void)priv;
    *(uint16_t *)&ram[addr] = val;
}

static void
mem_write_raml(uint32_t addr, uint32_t val, void *priv)
{
    (void)priv;
    *(uint32_t *)&ram[addr] = val;
}

/*
 * Page write callbacks for dynarec.
 */
void
mem_write_ramb_page(uint32_t addr, uint8_t val, page_t *page)
{
    (void)page;
    ram[addr] = val;
}

void
mem_write_ramw_page(uint32_t addr, uint16_t val, page_t *page)
{
    (void)page;
    *(uint16_t *)&ram[addr] = val;
}

void
mem_write_raml_page(uint32_t addr, uint32_t val, page_t *page)
{
    (void)page;
    *(uint32_t *)&ram[addr] = val;
}

/*
 * Logical memory read functions (with MMU translation).
 */
uint8_t
readmembl(uint32_t addr)
{
    uint64_t a;

    mem_logical_addr = addr;
    high_page = 0;

    if (cr0 >> 31) {
        a = mmutranslate_read(addr);
        if (a > 0xffffffffULL)
            return 0xff;
        addr = (uint32_t) a;
    }
    addr &= rammask;

    if (read_mapping[addr >> MEM_GRANULARITY_BITS]) {
        mem_mapping_t *map = read_mapping[addr >> MEM_GRANULARITY_BITS];
        if (map->read_b)
            return map->read_b(addr, map->priv);
    }

    return 0xff;
}

uint16_t
readmemwl(uint32_t addr)
{
    uint64_t a;

    mem_logical_addr = addr;
    high_page = 0;

    if (cr0 >> 31) {
        a = mmutranslate_read(addr);
        if (a > 0xffffffffULL)
            return 0xffff;

        if ((addr & 0xfff) > 0xffe) {
            /* Word crosses page boundary */
            uint8_t lo = readmembl(addr);
            uint8_t hi = readmembl(addr + 1);
            return lo | (hi << 8);
        }
        addr = (uint32_t) a;
    }
    addr &= rammask;

    if (read_mapping[addr >> MEM_GRANULARITY_BITS]) {
        mem_mapping_t *map = read_mapping[addr >> MEM_GRANULARITY_BITS];
        if (map->read_w)
            return map->read_w(addr, map->priv);
        if (map->read_b)
            return map->read_b(addr, map->priv) | (map->read_b(addr + 1, map->priv) << 8);
    }

    return 0xffff;
}

uint32_t
readmemll(uint32_t addr)
{
    uint64_t a;

    mem_logical_addr = addr;
    high_page = 0;

    if (cr0 >> 31) {
        a = mmutranslate_read(addr);
        if (a > 0xffffffffULL)
            return 0xffffffff;

        if ((addr & 0xfff) > 0xffc) {
            /* Dword crosses page boundary */
            uint16_t lo = readmemwl(addr);
            uint16_t hi = readmemwl(addr + 2);
            return lo | (hi << 16);
        }
        addr = (uint32_t) a;
    }
    addr &= rammask;

    if (read_mapping[addr >> MEM_GRANULARITY_BITS]) {
        mem_mapping_t *map = read_mapping[addr >> MEM_GRANULARITY_BITS];
        if (map->read_l)
            return map->read_l(addr, map->priv);
        if (map->read_w)
            return map->read_w(addr, map->priv) | (map->read_w(addr + 2, map->priv) << 16);
        if (map->read_b)
            return map->read_b(addr, map->priv) |
                   (map->read_b(addr + 1, map->priv) << 8) |
                   (map->read_b(addr + 2, map->priv) << 16) |
                   (map->read_b(addr + 3, map->priv) << 24);
    }

    return 0xffffffff;
}

uint64_t
readmemql(uint32_t addr)
{
    return (uint64_t)readmemll(addr) | ((uint64_t)readmemll(addr + 4) << 32);
}

/*
 * Logical memory write functions (with MMU translation).
 */
void
writemembl(uint32_t addr, uint8_t val)
{
    uint64_t a;

    mem_logical_addr = addr;
    high_page = 0;

    if (cr0 >> 31) {
        a = mmutranslate_write(addr);
        if (a > 0xffffffffULL)
            return;
        addr = (uint32_t) a;
    }
    addr &= rammask;

    if (write_mapping[addr >> MEM_GRANULARITY_BITS]) {
        mem_mapping_t *map = write_mapping[addr >> MEM_GRANULARITY_BITS];
        if (map->write_b)
            map->write_b(addr, val, map->priv);
    }
}

void
writememwl(uint32_t addr, uint16_t val)
{
    uint64_t a;

    mem_logical_addr = addr;
    high_page = 0;

    if (cr0 >> 31) {
        a = mmutranslate_write(addr);
        if (a > 0xffffffffULL)
            return;

        if ((addr & 0xfff) > 0xffe) {
            /* Word crosses page boundary */
            writemembl(addr, val & 0xff);
            writemembl(addr + 1, val >> 8);
            return;
        }
        addr = (uint32_t) a;
    }
    addr &= rammask;

    if (write_mapping[addr >> MEM_GRANULARITY_BITS]) {
        mem_mapping_t *map = write_mapping[addr >> MEM_GRANULARITY_BITS];
        if (map->write_w)
            map->write_w(addr, val, map->priv);
        else if (map->write_b) {
            map->write_b(addr, val & 0xff, map->priv);
            map->write_b(addr + 1, val >> 8, map->priv);
        }
    }
}

void
writememll(uint32_t addr, uint32_t val)
{
    uint64_t a;

    mem_logical_addr = addr;
    high_page = 0;

    if (cr0 >> 31) {
        a = mmutranslate_write(addr);
        if (a > 0xffffffffULL)
            return;

        if ((addr & 0xfff) > 0xffc) {
            /* Dword crosses page boundary */
            writememwl(addr, val & 0xffff);
            writememwl(addr + 2, val >> 16);
            return;
        }
        addr = (uint32_t) a;
    }
    addr &= rammask;

    if (write_mapping[addr >> MEM_GRANULARITY_BITS]) {
        mem_mapping_t *map = write_mapping[addr >> MEM_GRANULARITY_BITS];
        if (map->write_l)
            map->write_l(addr, val, map->priv);
        else if (map->write_w) {
            map->write_w(addr, val & 0xffff, map->priv);
            map->write_w(addr + 2, val >> 16, map->priv);
        } else if (map->write_b) {
            map->write_b(addr, val & 0xff, map->priv);
            map->write_b(addr + 1, (val >> 8) & 0xff, map->priv);
            map->write_b(addr + 2, (val >> 16) & 0xff, map->priv);
            map->write_b(addr + 3, val >> 24, map->priv);
        }
    }
}

void
writememql(uint32_t addr, uint64_t val)
{
    writememll(addr, val & 0xffffffff);
    writememll(addr + 4, val >> 32);
}

/*
 * No-MMU-translate variants (for split-page operations).
 */
uint8_t readmembl_no_mmut(uint32_t addr, uint32_t a64) { (void)a64; return readmembl(addr); }
void writemembl_no_mmut(uint32_t addr, uint32_t a64, uint8_t val) { (void)a64; writemembl(addr, val); }
uint16_t readmemwl_no_mmut(uint32_t addr, uint32_t *a64) { (void)a64; return readmemwl(addr); }
void writememwl_no_mmut(uint32_t addr, uint32_t *a64, uint16_t val) { (void)a64; writememwl(addr, val); }
uint32_t readmemll_no_mmut(uint32_t addr, uint32_t *a64) { (void)a64; return readmemll(addr); }
void writememll_no_mmut(uint32_t addr, uint32_t *a64, uint32_t val) { (void)a64; writememll(addr, val); }

void do_mmutranslate(uint32_t addr, uint32_t *a64, int num, int write) {
    (void)addr; (void)a64; (void)num; (void)write;
}

/*
 * 2386 variants - same as normal.
 */
uint8_t readmembl_2386(uint32_t addr) { return readmembl(addr); }
void writemembl_2386(uint32_t addr, uint8_t val) { writemembl(addr, val); }
uint16_t readmemwl_2386(uint32_t addr) { return readmemwl(addr); }
void writememwl_2386(uint32_t addr, uint16_t val) { writememwl(addr, val); }
uint32_t readmemll_2386(uint32_t addr) { return readmemll(addr); }
void writememll_2386(uint32_t addr, uint32_t val) { writememll(addr, val); }
uint64_t readmemql_2386(uint32_t addr) { return readmemql(addr); }
void writememql_2386(uint32_t addr, uint64_t val) { writememql(addr, val); }

uint8_t readmembl_no_mmut_2386(uint32_t addr, uint32_t a64) { return readmembl_no_mmut(addr, a64); }
void writemembl_no_mmut_2386(uint32_t addr, uint32_t a64, uint8_t val) { writemembl_no_mmut(addr, a64, val); }
uint16_t readmemwl_no_mmut_2386(uint32_t addr, uint32_t *a64) { return readmemwl_no_mmut(addr, a64); }
void writememwl_no_mmut_2386(uint32_t addr, uint32_t *a64, uint16_t val) { writememwl_no_mmut(addr, a64, val); }
uint32_t readmemll_no_mmut_2386(uint32_t addr, uint32_t *a64) { return readmemll_no_mmut(addr, a64); }
void writememll_no_mmut_2386(uint32_t addr, uint32_t *a64, uint32_t val) { writememll_no_mmut(addr, a64, val); }

void do_mmutranslate_2386(uint32_t addr, uint32_t *a64, int num, int write) {
    do_mmutranslate(addr, a64, num, write);
}

/*
 * Physical memory access (no translation).
 */
uint8_t mem_readb_phys(uint32_t addr) {
    if (addr < ram_size)
        return ram[addr];
    return 0xff;
}

uint16_t mem_readw_phys(uint32_t addr) {
    if (addr + 1 < ram_size)
        return *(uint16_t *)&ram[addr];
    return 0xffff;
}

uint32_t mem_readl_phys(uint32_t addr) {
    if (addr + 3 < ram_size)
        return *(uint32_t *)&ram[addr];
    return 0xffffffff;
}

void mem_writeb_phys(uint32_t addr, uint8_t val) {
    if (addr < ram_size)
        ram[addr] = val;
}

void mem_writew_phys(uint32_t addr, uint16_t val) {
    if (addr + 1 < ram_size)
        *(uint16_t *)&ram[addr] = val;
}

void mem_writel_phys(uint32_t addr, uint32_t val) {
    if (addr + 3 < ram_size)
        *(uint32_t *)&ram[addr] = val;
}

/*
 * Memory mapping functions.
 */
void
mem_mapping_add(mem_mapping_t *map,
                uint32_t base, uint32_t size,
                uint8_t (*read_b)(uint32_t addr, void *priv),
                uint16_t (*read_w)(uint32_t addr, void *priv),
                uint32_t (*read_l)(uint32_t addr, void *priv),
                void (*write_b)(uint32_t addr, uint8_t val, void *priv),
                void (*write_w)(uint32_t addr, uint16_t val, void *priv),
                void (*write_l)(uint32_t addr, uint32_t val, void *priv),
                uint8_t *exec, uint32_t flags, void *priv)
{
    map->enable = 1;
    map->base = base;
    map->size = size;
    map->read_b = read_b;
    map->read_w = read_w;
    map->read_l = read_l;
    map->write_b = write_b;
    map->write_w = write_w;
    map->write_l = write_l;
    map->exec = exec;
    map->flags = flags;
    map->priv = priv;
    map->next = NULL;

    /* Add to linked list */
    if (base_mapping == NULL) {
        map->prev = NULL;
        base_mapping = last_mapping = map;
    } else {
        map->prev = last_mapping;
        last_mapping->next = map;
        last_mapping = map;
    }

    /* Update the routing tables */
    for (uint32_t c = base; c < base + size; c += MEM_GRANULARITY_SIZE) {
        uint32_t idx = c >> MEM_GRANULARITY_BITS;
        if (idx < MEM_MAPPINGS_NO) {
            if (exec)
                _mem_exec[idx] = exec + (c - base);
            if (read_b || read_w || read_l)
                read_mapping[idx] = map;
            if (write_b || write_w || write_l)
                write_mapping[idx] = map;
        }
    }

    flushmmucache_nopc();
}

void
mem_mapping_disable(mem_mapping_t *map)
{
    map->enable = 0;

    for (uint32_t c = map->base; c < map->base + map->size; c += MEM_GRANULARITY_SIZE) {
        uint32_t idx = c >> MEM_GRANULARITY_BITS;
        if (idx < MEM_MAPPINGS_NO) {
            if (read_mapping[idx] == map)
                read_mapping[idx] = NULL;
            if (write_mapping[idx] == map)
                write_mapping[idx] = NULL;
            if (_mem_exec[idx] == map->exec + (c - map->base))
                _mem_exec[idx] = NULL;
        }
    }

    flushmmucache_nopc();
}

void
mem_mapping_enable(mem_mapping_t *map)
{
    map->enable = 1;

    for (uint32_t c = map->base; c < map->base + map->size; c += MEM_GRANULARITY_SIZE) {
        uint32_t idx = c >> MEM_GRANULARITY_BITS;
        if (idx < MEM_MAPPINGS_NO) {
            if (map->exec)
                _mem_exec[idx] = map->exec + (c - map->base);
            if (map->read_b || map->read_w || map->read_l)
                read_mapping[idx] = map;
            if (map->write_b || map->write_w || map->write_l)
                write_mapping[idx] = map;
        }
    }

    flushmmucache_nopc();
}

/*
 * A20 gate management.
 */
void
mem_a20_init(void)
{
    mem_a20_key = mem_a20_alt = 0;
    mem_a20_state = 1;
    rammask = ram_size - 1;
}

void
mem_a20_recalc(void)
{
    int state = mem_a20_key | mem_a20_alt;
    if (state && !mem_a20_state) {
        rammask = ram_size - 1;
        flushmmucache();
    } else if (!state && mem_a20_state) {
        rammask = ram_size - 1 - (1 << 20);
        flushmmucache();
    }
    mem_a20_state = state;
}

/*
 * Page eviction list management (for dynarec).
 */
void page_remove_from_evict_list(page_t *page) { (void)page; }
void page_add_to_evict_list(page_t *page) { (void)page; }

/*
 * Reset page blocks (for dynarec).
 */
void
mem_reset_page_blocks(void)
{
#ifdef USE_NEW_DYNAREC
    if (pages) {
        for (uint32_t c = 0; c < pages_sz; c++) {
            pages[c].block = 0;
            pages[c].block_2 = 0;
            pages[c].head = 0;
        }
    }
#endif
}

/*
 * Platform memory allocation.
 */
static void *
plat_mmap_local(size_t size, uint8_t executable)
{
#ifdef _WIN32
    DWORD protect = executable ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;
    return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, protect);
#else
    int prot = PROT_READ | PROT_WRITE;
    if (executable)
        prot |= PROT_EXEC;
    void *ptr = mmap(NULL, size, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED)
        return NULL;
    return ptr;
#endif
}

static void
plat_munmap_local(void *ptr, size_t size)
{
#ifdef _WIN32
    (void)size;
    VirtualFree(ptr, 0, MEM_RELEASE);
#else
    munmap(ptr, size);
#endif
}

/*
 * Initialize memory subsystem.
 */
void
mem_init(void)
{
    ram = rom = NULL;
    memset(page_ff, 0xff, sizeof(page_ff));
}

/*
 * Reset memory subsystem.
 */
void
mem_reset(void)
{
    memset(page_ff, 0xff, sizeof(page_ff));

#ifdef USE_NEW_DYNAREC
    if (byte_dirty_mask) {
        free(byte_dirty_mask);
        byte_dirty_mask = NULL;
    }
    if (byte_code_present_mask) {
        free(byte_code_present_mask);
        byte_code_present_mask = NULL;
    }
#endif

    if (pages) {
        free(pages);
        pages = NULL;
    }

    if (ram != NULL) {
        plat_munmap_local(ram, ram_size);
        ram = NULL;
        ram_size = 0;
    }

    /* Calculate RAM size */
    ram_size = mem_size * 1024;
    if (ram_size < DEFAULT_RAM_SIZE)
        ram_size = DEFAULT_RAM_SIZE;

    /* Allocate RAM (with 16 extra bytes for safety) */
    ram = (uint8_t *) plat_mmap_local(ram_size + 16, 0);
    if (ram == NULL) {
        fprintf(stderr, "Failed to allocate RAM block.\n");
        return;
    }
    memset(ram, 0x00, ram_size + 16);

    /* Set RAM mask for A20 gate */
    rammask = ram_size - 1;

    /* Allocate page table */
    addr_space_size = 1048576;  /* 4GB address space with 4KB pages */
    pages_sz = addr_space_size;
    pages = (page_t *) malloc(pages_sz * sizeof(page_t));
    if (!pages) {
        fprintf(stderr, "Failed to allocate page table.\n");
        return;
    }

    memset(page_lookup, 0x00, (1 << 20) * sizeof(page_t *));
    memset(pages, 0x00, pages_sz * sizeof(page_t));

#ifdef USE_NEW_DYNAREC
    byte_dirty_mask = malloc((ram_size / 1024) / 8);
    memset(byte_dirty_mask, 0, (ram_size / 1024) / 8);
    byte_code_present_mask = malloc((ram_size / 1024) / 8);
    memset(byte_code_present_mask, 0, (ram_size / 1024) / 8);
#endif

    /* Initialize page table entries */
    for (uint32_t c = 0; c < pages_sz; c++) {
        if ((c << 12) >= ram_size)
            pages[c].mem = page_ff;
        else
            pages[c].mem = &ram[c << 12];

        pages[c].write_b = mem_write_ramb_page;
        pages[c].write_w = mem_write_ramw_page;
        pages[c].write_l = mem_write_raml_page;

#ifdef USE_NEW_DYNAREC
        pages[c].evict_prev = EVICT_NOT_IN_LIST;
        if (c < ram_size / 4096) {
            pages[c].byte_dirty_mask = &byte_dirty_mask[c * 64];
            pages[c].byte_code_present_mask = &byte_code_present_mask[c * 64];
        }
#endif
    }

    /* Clear mapping arrays */
    memset(_mem_exec, 0x00, sizeof(_mem_exec));
    memset(write_mapping, 0x00, sizeof(write_mapping));
    memset(read_mapping, 0x00, sizeof(read_mapping));

    base_mapping = last_mapping = NULL;

    /* Add RAM mapping for entire memory space */
    mem_mapping_add(&ram_low_mapping, 0x000000, ram_size,
                    mem_read_ram, mem_read_ramw, mem_read_raml,
                    mem_write_ram, mem_write_ramw, mem_write_raml,
                    ram, MEM_MAPPING_INTERNAL, NULL);

    mem_a20_init();

    /* Initialize lookup tables */
    resetreadlookup();

#ifdef USE_NEW_DYNAREC
    purgable_page_list_head = EVICT_NOT_IN_LIST;
    purgeable_page_count = 0;
#endif
}

/*
 * Close memory subsystem.
 */
void
mem_close(void)
{
#ifdef USE_NEW_DYNAREC
    if (byte_dirty_mask) {
        free(byte_dirty_mask);
        byte_dirty_mask = NULL;
    }
    if (byte_code_present_mask) {
        free(byte_code_present_mask);
        byte_code_present_mask = NULL;
    }
#endif

    if (pages) {
        free(pages);
        pages = NULL;
    }

    if (ram) {
        plat_munmap_local(ram, ram_size);
        ram = NULL;
    }

    ram_size = 0;
}
