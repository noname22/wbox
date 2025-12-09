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
void dma_set_at(int at_mode) { (void)at_mode; }

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
void execx86(int cycs) { (void)cycs; }

/* Device reset */
void device_reset_all(uint32_t flags) { (void)flags; }

/* 8086 reset stub */
void reset_808x(int hard) { (void)hard; }
