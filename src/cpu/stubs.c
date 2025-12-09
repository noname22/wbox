/*
 * wbox - Stub implementations
 * Placeholder implementations for external dependencies
 * These will be replaced with real implementations as the emulator is developed
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

#include "platform.h"
#include "mem.h"
#include "io.h"
#include "pic.h"
#include "timer.h"
#include "nmi.h"

/* Default RAM size - 16 MB */
#define DEFAULT_RAM_SIZE (16 * 1024 * 1024)

/* Global CPU configuration */
/* Note: cpu_override is defined in cpu.c */
int cpu = 0;
int fpu_type = 0;
int fpu_softfloat = 1;
int machine = 0;

#include "cpu.h"

/* Note: cpu_families is defined in cpu_table.c */

/* Machine stub */
#include "machine.h"

const machine_t machines[] = {
    {
        .name = "Generic Pentium II",
        .internal_name = "p2",
        .bus_flags = MACHINE_PCI,
        .cpu = { 0, NULL, 0, 0, 0, 0, 1.5f, 10.0f },
    },
    { NULL }  /* Terminator */
};

int cpu_use_dynarec = 1;
int pci_burst_time = 1;
int pci_nonburst_time = 4;
int agp_burst_time = 1;
int agp_nonburst_time = 4;

uint32_t mem_size = DEFAULT_RAM_SIZE;
int force_10ms = 0;
uint8_t page_ff[4096] = {0xFF};

uint32_t get_phys(uint32_t addr) {
    return addr & rammask;
}

uint32_t get_phys_noabrt(uint32_t addr) {
    return addr & rammask;
}

void pc_speed_changed(void) {}

void io_handler(int set, uint16_t base, int size,
                uint8_t (*inb_func)(uint16_t addr, void *priv),
                uint16_t (*inw_func)(uint16_t addr, void *priv),
                uint32_t (*inl_func)(uint16_t addr, void *priv),
                void (*outb_func)(uint16_t addr, uint8_t val, void *priv),
                void (*outw_func)(uint16_t addr, uint16_t val, void *priv),
                void (*outl_func)(uint16_t addr, uint32_t val, void *priv),
                void *priv) {
    (void)set; (void)base; (void)size;
    (void)inb_func; (void)inw_func; (void)inl_func;
    (void)outb_func; (void)outw_func; (void)outl_func;
    (void)priv;
}

/* RAM */
uint8_t *ram = NULL;
uint8_t *ram2 = NULL;
uint32_t rammask = DEFAULT_RAM_SIZE - 1;
uint8_t *rom = NULL;
uint32_t biosmask = 0;

/* Lookup tables */
int readlookup[256] = {0};
uintptr_t old_rl2 = 0;
uint8_t uncached = 0;
int readlnext = 0;
int writelookup[256] = {0};
int writelnext = 0;

page_t *pages = NULL;
page_t *page_lookup[1048576] = {NULL};
uintptr_t readlookup2[1048576] = {0};
uintptr_t writelookup2[1048576] = {0};

uint32_t get_phys_virt = 0;
uint32_t get_phys_phys = 0;

int read_type = 0;
uint8_t high_page = 0;

int shadowbios = 0;
int shadowbios_write = 0;

/* PC cache for fast instruction fetch */
uint32_t pccache = 0;
uint8_t *pccache2 = NULL;

int mem_a20_state = 1;
int mem_a20_alt = 0;
int mem_a20_key = 0;

uint32_t mem_logical_addr = 0;

uint64_t *byte_dirty_mask = NULL;
uint64_t *byte_code_present_mask = NULL;

uint32_t purgable_page_list_head = EVICT_NOT_IN_LIST;

uint8_t *_mem_exec[MEM_MAPPINGS_NO] = {NULL};

/* Timer globals */
/* Note: tsc is defined in cpu.c */
uint32_t timer_target = 0;
uint64_t TIMER_USEC = 1000;

/* PIC globals */
pic_t pic = {0};
pic_t pic2 = {0};

/* NMI globals */
int nmi_mask = 0;
/* Note: nmi and nmi_auto_clear are defined in x86.c */

/* PIT globals */
double isa_timing = 1.0;
double bus_timing = 1.0;
double pci_timing = 1.0;
double agp_timing = 1.0;
uint64_t cpu_clock_multi = 1;

/* Machine globals */
int machine_at = 1;

/* Memory read/write stubs */
uint8_t readmembl(uint32_t addr) {
    if (ram && (addr & rammask) < DEFAULT_RAM_SIZE)
        return ram[addr & rammask];
    return 0xFF;
}

void writemembl(uint32_t addr, uint8_t val) {
    if (ram && (addr & rammask) < DEFAULT_RAM_SIZE)
        ram[addr & rammask] = val;
}

uint16_t readmemwl(uint32_t addr) {
    return readmembl(addr) | (readmembl(addr + 1) << 8);
}

void writememwl(uint32_t addr, uint16_t val) {
    writemembl(addr, val & 0xFF);
    writemembl(addr + 1, (val >> 8) & 0xFF);
}

uint32_t readmemll(uint32_t addr) {
    return readmemwl(addr) | (readmemwl(addr + 2) << 16);
}

void writememll(uint32_t addr, uint32_t val) {
    writememwl(addr, val & 0xFFFF);
    writememwl(addr + 2, (val >> 16) & 0xFFFF);
}

uint64_t readmemql(uint32_t addr) {
    return (uint64_t)readmemll(addr) | ((uint64_t)readmemll(addr + 4) << 32);
}

void writememql(uint32_t addr, uint64_t val) {
    writememll(addr, val & 0xFFFFFFFF);
    writememll(addr + 4, (val >> 32) & 0xFFFFFFFF);
}

/* No-MMU variants */
uint8_t readmembl_no_mmut(uint32_t addr, uint32_t a64) {
    (void)a64;
    return readmembl(addr);
}

void writemembl_no_mmut(uint32_t addr, uint32_t a64, uint8_t val) {
    (void)a64;
    writemembl(addr, val);
}

uint16_t readmemwl_no_mmut(uint32_t addr, uint32_t *a64) {
    (void)a64;
    return readmemwl(addr);
}

void writememwl_no_mmut(uint32_t addr, uint32_t *a64, uint16_t val) {
    (void)a64;
    writememwl(addr, val);
}

uint32_t readmemll_no_mmut(uint32_t addr, uint32_t *a64) {
    (void)a64;
    return readmemll(addr);
}

void writememll_no_mmut(uint32_t addr, uint32_t *a64, uint32_t val) {
    (void)a64;
    writememll(addr, val);
}

void do_mmutranslate(uint32_t addr, uint32_t *a64, int num, int write) {
    (void)addr; (void)a64; (void)num; (void)write;
}

/* 2386 variants - same as normal for now */
uint8_t readmembl_2386(uint32_t addr) { return readmembl(addr); }
void writemembl_2386(uint32_t addr, uint8_t val) { writemembl(addr, val); }
uint16_t readmemwl_2386(uint32_t addr) { return readmemwl(addr); }
void writememwl_2386(uint32_t addr, uint16_t val) { writememwl(addr, val); }
uint32_t readmemll_2386(uint32_t addr) { return readmemll(addr); }
void writememll_2386(uint32_t addr, uint32_t val) { writememll(addr, val); }
uint64_t readmemql_2386(uint32_t addr) { return readmemql(addr); }
void writememql_2386(uint32_t addr, uint64_t val) { writememql(addr, val); }

uint8_t readmembl_no_mmut_2386(uint32_t addr, uint32_t a64) {
    return readmembl_no_mmut(addr, a64);
}
void writemembl_no_mmut_2386(uint32_t addr, uint32_t a64, uint8_t val) {
    writemembl_no_mmut(addr, a64, val);
}
uint16_t readmemwl_no_mmut_2386(uint32_t addr, uint32_t *a64) {
    return readmemwl_no_mmut(addr, a64);
}
void writememwl_no_mmut_2386(uint32_t addr, uint32_t *a64, uint16_t val) {
    writememwl_no_mmut(addr, a64, val);
}
uint32_t readmemll_no_mmut_2386(uint32_t addr, uint32_t *a64) {
    return readmemll_no_mmut(addr, a64);
}
void writememll_no_mmut_2386(uint32_t addr, uint32_t *a64, uint32_t val) {
    writememll_no_mmut(addr, a64, val);
}

void do_mmutranslate_2386(uint32_t addr, uint32_t *a64, int num, int write) {
    do_mmutranslate(addr, a64, num, write);
}

/* Physical memory access */
uint8_t mem_readb_phys(uint32_t addr) { return readmembl(addr); }
uint16_t mem_readw_phys(uint32_t addr) { return readmemwl(addr); }
uint32_t mem_readl_phys(uint32_t addr) { return readmemll(addr); }
void mem_writeb_phys(uint32_t addr, uint8_t val) { writemembl(addr, val); }
void mem_writew_phys(uint32_t addr, uint16_t val) { writememwl(addr, val); }
void mem_writel_phys(uint32_t addr, uint32_t val) { writememll(addr, val); }

/* Page write functions for dynarec */
void mem_write_ramb_page(uint32_t addr, uint8_t val, page_t *page) {
    (void)page;
    writemembl(addr, val);
}

void mem_write_ramw_page(uint32_t addr, uint16_t val, page_t *page) {
    (void)page;
    writememwl(addr, val);
}

void mem_write_raml_page(uint32_t addr, uint32_t val, page_t *page) {
    (void)page;
    writememll(addr, val);
}

void mem_flush_write_page(uint32_t addr, uint32_t virt) {
    (void)addr; (void)virt;
}

/* Cache functions */
uint8_t *getpccache(uint32_t a) {
    if (ram)
        return &ram[a & rammask];
    return NULL;
}

uint64_t mmutranslatereal(uint32_t addr, int rw) {
    (void)rw;
    return addr & rammask;
}

uint32_t mmutranslatereal32(uint32_t addr, int rw) {
    (void)rw;
    return addr & rammask;
}

uint64_t mmutranslate_noabrt(uint32_t addr, int rw) {
    (void)rw;
    return addr & rammask;
}

void addreadlookup(uint32_t virt, uint32_t phys) {
    (void)virt; (void)phys;
}

void addwritelookup(uint32_t virt, uint32_t phys) {
    (void)virt; (void)phys;
}

/* MMU cache flush */
void flushmmucache(void) {}
void flushmmucache_write(void) {}
void flushmmucache_pc(void) {}
void flushmmucache_nopc(void) {}

void mem_invalidate_range(uint32_t start_addr, uint32_t end_addr) {
    (void)start_addr; (void)end_addr;
}

void mem_reset_page_blocks(void) {}

int mem_addr_is_ram(uint32_t addr) {
    return (addr & rammask) < DEFAULT_RAM_SIZE;
}

/* Memory mapping functions */
void mem_mapping_add(mem_mapping_t *map,
                     uint32_t base, uint32_t size,
                     uint8_t (*read_b)(uint32_t addr, void *priv),
                     uint16_t (*read_w)(uint32_t addr, void *priv),
                     uint32_t (*read_l)(uint32_t addr, void *priv),
                     void (*write_b)(uint32_t addr, uint8_t val, void *priv),
                     void (*write_w)(uint32_t addr, uint16_t val, void *priv),
                     void (*write_l)(uint32_t addr, uint32_t val, void *priv),
                     uint8_t *exec, uint32_t flags, void *priv) {
    (void)map; (void)base; (void)size;
    (void)read_b; (void)read_w; (void)read_l;
    (void)write_b; (void)write_w; (void)write_l;
    (void)exec; (void)flags; (void)priv;
}

void mem_mapping_disable(mem_mapping_t *map) { (void)map; }
void mem_mapping_enable(mem_mapping_t *map) { (void)map; }

/* A20 gate */
void mem_a20_init(void) {}
void mem_a20_recalc(void) {}

/* Memory init/reset */
void mem_init(void) {
    if (!ram) {
        ram = calloc(1, DEFAULT_RAM_SIZE);
    }
}

void mem_close(void) {
    if (ram) {
        free(ram);
        ram = NULL;
    }
}

void mem_reset(void) {
    if (ram) {
        memset(ram, 0, DEFAULT_RAM_SIZE);
    }
}

/* Page eviction list */
void page_remove_from_evict_list(page_t *page) { (void)page; }
void page_add_to_evict_list(page_t *page) { (void)page; }

/* I/O stubs */
void io_init(void) {}

void io_sethandler(uint16_t base, int size,
                   uint8_t (*inb)(uint16_t addr, void *priv),
                   uint16_t (*inw)(uint16_t addr, void *priv),
                   uint32_t (*inl)(uint16_t addr, void *priv),
                   void (*outb)(uint16_t addr, uint8_t val, void *priv),
                   void (*outw)(uint16_t addr, uint16_t val, void *priv),
                   void (*outl)(uint16_t addr, uint32_t val, void *priv),
                   void *priv) {
    (void)base; (void)size;
    (void)inb; (void)inw; (void)inl;
    (void)outb; (void)outw; (void)outl;
    (void)priv;
}

void io_removehandler(uint16_t base, int size,
                      uint8_t (*inb)(uint16_t addr, void *priv),
                      uint16_t (*inw)(uint16_t addr, void *priv),
                      uint32_t (*inl)(uint16_t addr, void *priv),
                      void (*outb)(uint16_t addr, uint8_t val, void *priv),
                      void (*outw)(uint16_t addr, uint16_t val, void *priv),
                      void (*outl)(uint16_t addr, uint32_t val, void *priv),
                      void *priv) {
    (void)base; (void)size;
    (void)inb; (void)inw; (void)inl;
    (void)outb; (void)outw; (void)outl;
    (void)priv;
}

uint8_t inb(uint16_t port) { (void)port; return 0xFF; }
void outb(uint16_t port, uint8_t val) { (void)port; (void)val; }
uint16_t inw(uint16_t port) { (void)port; return 0xFFFF; }
void outw(uint16_t port, uint16_t val) { (void)port; (void)val; }
uint32_t inl(uint16_t port) { (void)port; return 0xFFFFFFFF; }
void outl(uint16_t port, uint32_t val) { (void)port; (void)val; }

/* PIC stubs */
void pic_init(void) {}
void pic2_init(void) {}
void pic_reset(void) {}

void picint_common(uint16_t num, int level, int set, uint8_t *irq_state) {
    (void)num; (void)level; (void)set; (void)irq_state;
}

int picinterrupt(void) {
    return -1;  /* No interrupt pending */
}

uint8_t pic_irq_ack(void) {
    return 0;
}

/* Timer stubs */
void timer_enable(pc_timer_t *timer) { (void)timer; }
void timer_disable(pc_timer_t *timer) { (void)timer; }
void timer_process(void) {}
void timer_close(void) {}
void timer_init(void) {}
void timer_add(pc_timer_t *timer, void (*callback)(void *priv), void *priv, int start_timer) {
    (void)timer; (void)callback; (void)priv; (void)start_timer;
}
void timer_stop(pc_timer_t *timer) { (void)timer; }
void timer_on_auto(pc_timer_t *timer, double period) { (void)timer; (void)period; }
void timer_set_new_tsc(uint64_t new_tsc) { (void)new_tsc; }
int timer_target_elapsed(void) { return 0; }
void cycles_reset(void) {}

/* NMI stubs */
void nmi_init(void) {}
void nmi_write(uint16_t port, uint8_t val, void *priv) {
    (void)port; (void)val; (void)priv;
}

/* SMRAM stubs */
void smram_backup_all(void) {}
void smram_recalc_all(int ret) { (void)ret; }

/* GDB stub */
int gdbstub_instruction(void) { return 0; }
void gdbstub_cpu_init(void) {}

/* DMA stubs */
void dma_init(void) {}
void dma_reset(void) {}
void dma_set_at(int is286) { (void)is286; }

/* Platform executable memory allocation for dynarec */
void *plat_mmap(size_t size, uint8_t executable) {
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

void plat_munmap(void *ptr, size_t size) {
#ifdef _WIN32
    (void)size;
    VirtualFree(ptr, 0, MEM_RELEASE);
#else
    munmap(ptr, size);
#endif
}

/* 8086 stubs - we only support 386+ but references exist */
int is8086 = 0;
void execx86(int cycs) { (void)cycs; }

/* Device reset */
void device_reset_all(uint32_t flags) { (void)flags; }

/* 8086 reset stub */
void reset_808x(int hard) { (void)hard; }

/* Memory lookup reset */
void resetreadlookup(void) {
    memset(readlookup, 0xFF, sizeof(readlookup));
    memset(writelookup, 0xFF, sizeof(writelookup));
    memset(readlookup2, 0xFF, sizeof(readlookup2));
    memset(writelookup2, 0xFF, sizeof(writelookup2));
}
