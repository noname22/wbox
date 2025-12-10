/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          CPU type handler.
 *
 * Authors: Sarah Walker, <https://pcem-emulator.co.uk/>
 *          leilei,
 *          Miran Grca, <mgrca8@gmail.com>
 *          Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *          Copyright 2008-2020 Sarah Walker.
 *          Copyright 2016-2018 leilei.
 *          Copyright 2016-2020 Miran Grca.
 *          Copyright 2018-2021 Fred N. van Kempen.
 */
#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>

#define HAVE_STDARG_H
#include "platform.h"
#include "cpu.h"
#include "x86.h"
#include "x87_sf.h"
#include "device.h"
#include "machine.h"
#include "io.h"
#include "x86_ops.h"
#include "x86seg_common.h"
#include "mem.h"
#include "nmi.h"
#include "pic.h"
#include "smram.h"
#include "timer.h"
#include "gdbstub.h"
#include "platform.h"
#include "platform.h"

#ifdef USE_DYNAREC
#    include "codegen.h"
#endif /* USE_DYNAREC */
#include "x87_timings.h"

enum {
    CPUID_FPU       = (1 << 0),  /* On-chip Floating Point Unit */
    CPUID_VME       = (1 << 1),  /* Virtual 8086 mode extensions */
    CPUID_DE        = (1 << 2),  /* Debugging extensions */
    CPUID_PSE       = (1 << 3),  /* Page Size Extension */
    CPUID_TSC       = (1 << 4),  /* Time Stamp Counter */
    CPUID_MSR       = (1 << 5),  /* Model-specific registers */
    CPUID_PAE       = (1 << 6),  /* Physical Address Extension */
    CPUID_MCE       = (1 << 7),  /* Machine Check Exception */
    CPUID_CMPXCHG8B = (1 << 8),  /* CMPXCHG8B instruction */
    CPUID_APIC      = (1 << 9),  /* On-chip APIC */
    CPUID_AMDPGE    = (1 << 9),  /* Global Page Enable (AMD K5 Model 0 only) */
    CPUID_AMDSEP    = (1 << 10), /* SYSCALL and SYSRET instructions (AMD K6 only) */
    CPUID_SEP       = (1 << 11), /* SYSENTER and SYSEXIT instructions (SYSCALL and SYSRET if EAX=80000001h) */
    CPUID_MTRR      = (1 << 12), /* Memory type range registers */
    CPUID_PGE       = (1 << 13), /* Page Global Enable */
    CPUID_MCA       = (1 << 14), /* Machine Check Architecture */
    CPUID_CMOV      = (1 << 15), /* Conditional move instructions */
    CPUID_PAT       = (1 << 16), /* Page Attribute Table */
    CPUID_PSE36     = (1 << 17), /* 36-bit Page Size Extension */
    CPUID_MMX       = (1 << 23), /* MMX technology */
    CPUID_FXSR      = (1 << 24)  /* FXSAVE and FXRSTOR instructions */
};

/* Additional flags returned by CPUID function 0x80000001 */
#define CPUID_3DNOWE (1UL << 30UL) /* Extended 3DNow! instructions */
#define CPUID_3DNOW  (1UL << 31UL) /* 3DNow! instructions */

/* Remove the Debugging Extensions CPUID flag if not compiled
   with debug register support for 486 and later CPUs. */
#ifndef USE_DEBUG_REGS_486
#    define CPUID_DE 0
#endif

/* Make sure this is as low as possible. */
cpu_state_t cpu_state;
fpu_state_t fpu_state;

/* Place this immediately after. */
uint32_t abrt_error;

/* Callback for illegal instruction events (NULL = silent) */
cpu_illegal_instr_callback_t cpu_illegal_instr_callback = NULL;

#ifdef USE_DYNAREC
const OpFn *x86_dynarec_opcodes;
const OpFn *x86_dynarec_opcodes_0f;
const OpFn *x86_dynarec_opcodes_d8_a16;
const OpFn *x86_dynarec_opcodes_d8_a32;
const OpFn *x86_dynarec_opcodes_d9_a16;
const OpFn *x86_dynarec_opcodes_d9_a32;
const OpFn *x86_dynarec_opcodes_da_a16;
const OpFn *x86_dynarec_opcodes_da_a32;
const OpFn *x86_dynarec_opcodes_db_a16;
const OpFn *x86_dynarec_opcodes_db_a32;
const OpFn *x86_dynarec_opcodes_dc_a16;
const OpFn *x86_dynarec_opcodes_dc_a32;
const OpFn *x86_dynarec_opcodes_dd_a16;
const OpFn *x86_dynarec_opcodes_dd_a32;
const OpFn *x86_dynarec_opcodes_de_a16;
const OpFn *x86_dynarec_opcodes_de_a32;
const OpFn *x86_dynarec_opcodes_df_a16;
const OpFn *x86_dynarec_opcodes_df_a32;
const OpFn *x86_dynarec_opcodes_REPE;
const OpFn *x86_dynarec_opcodes_REPNE;
const OpFn *x86_dynarec_opcodes_3DNOW;
#endif /* USE_DYNAREC */

const OpFn *x86_opcodes;
const OpFn *x86_opcodes_0f;
const OpFn *x86_opcodes_d8_a16;
const OpFn *x86_opcodes_d8_a32;
const OpFn *x86_opcodes_d9_a16;
const OpFn *x86_opcodes_d9_a32;
const OpFn *x86_opcodes_da_a16;
const OpFn *x86_opcodes_da_a32;
const OpFn *x86_opcodes_db_a16;
const OpFn *x86_opcodes_db_a32;
const OpFn *x86_opcodes_dc_a16;
const OpFn *x86_opcodes_dc_a32;
const OpFn *x86_opcodes_dd_a16;
const OpFn *x86_opcodes_dd_a32;
const OpFn *x86_opcodes_de_a16;
const OpFn *x86_opcodes_de_a32;
const OpFn *x86_opcodes_df_a16;
const OpFn *x86_opcodes_df_a32;
const OpFn *x86_opcodes_REPE;
const OpFn *x86_opcodes_REPNE;
const OpFn *x86_opcodes_3DNOW;

const OpFn *x86_2386_opcodes;
const OpFn *x86_2386_opcodes_0f;
const OpFn *x86_2386_opcodes_d8_a16;
const OpFn *x86_2386_opcodes_d8_a32;
const OpFn *x86_2386_opcodes_d9_a16;
const OpFn *x86_2386_opcodes_d9_a32;
const OpFn *x86_2386_opcodes_da_a16;
const OpFn *x86_2386_opcodes_da_a32;
const OpFn *x86_2386_opcodes_db_a16;
const OpFn *x86_2386_opcodes_db_a32;
const OpFn *x86_2386_opcodes_dc_a16;
const OpFn *x86_2386_opcodes_dc_a32;
const OpFn *x86_2386_opcodes_dd_a16;
const OpFn *x86_2386_opcodes_dd_a32;
const OpFn *x86_2386_opcodes_de_a16;
const OpFn *x86_2386_opcodes_de_a32;
const OpFn *x86_2386_opcodes_df_a16;
const OpFn *x86_2386_opcodes_df_a32;
const OpFn *x86_2386_opcodes_REPE;
const OpFn *x86_2386_opcodes_REPNE;

uint16_t cpu_fast_off_count;
uint16_t cpu_fast_off_val;
uint16_t temp_seg_data[4] = { 0, 0, 0, 0 };

int isa_cycles;
int cpu_inited;

int cpu_cycles_read;
int cpu_cycles_read_l;
int cpu_cycles_write;
int cpu_cycles_write_l;
int cpu_prefetch_cycles;
int cpu_prefetch_width;
int cpu_mem_prefetch_cycles;
int cpu_rom_prefetch_cycles;
int cpu_waitstates;
int cpu_cache_int_enabled;
int cpu_cache_ext_enabled;
int cpu_flush_pending;
int cpu_old_paging;
int cpu_isa_speed;
int cpu_pci_speed;
int cpu_isa_pci_div;
int cpu_agp_speed;
int cpu_alt_reset;

int cpu_override;
int cpu_effective;
int cpu_multi;
int cpu_cyrix_alignment;
int cpu_cpurst_on_sr;
int cpu_use_exec = 0;
int cpu_override_interpreter;
int CPUID;


int is_vpc;

int timing_rr;
int timing_mr;
int timing_mrl;
int timing_rm;
int timing_rml;
int timing_mm;
int timing_mml;
int timing_bt;
int timing_bnt;
int timing_int;
int timing_int_rm;
int timing_int_v86;
int timing_int_pm;
int timing_int_pm_outer;
int timing_iret_rm;
int timing_iret_v86;
int timing_iret_pm;
int timing_iret_pm_outer;
int timing_call_rm;
int timing_call_pm;
int timing_call_pm_gate;
int timing_call_pm_gate_inner;
int timing_retf_rm;
int timing_retf_pm;
int timing_retf_pm_outer;
int timing_jmp_rm;
int timing_jmp_pm;
int timing_jmp_pm_gate;
int timing_misaligned;

uint32_t cpu_features;
uint32_t cpu_fast_off_flags;

uint32_t _tr[8]      = { 0, 0, 0, 0, 0, 0, 0, 0 };
uint32_t cache_index = 0;
uint8_t  _cache[2048];

uint64_t cpu_CR4_mask;
uint64_t tsc = 0;

double cpu_dmulti;
double cpu_busspeed;

msr_t msr;

cyrix_t cyrix;

cpu_family_t *cpu_f;
CPU          *cpu_s;

uint8_t do_translate  = 0;
uint8_t do_translate2 = 0;

void (*cpu_exec)(int32_t cycs);

uint8_t ccr0;
uint8_t ccr1;
uint8_t ccr2;
uint8_t ccr3;
uint8_t ccr4;
uint8_t ccr5;
uint8_t ccr6;
uint8_t ccr7;

uint8_t reg_30 = 0x00;
uint8_t arr[24] = { 0 };
uint8_t rcr[8] = { 0 };

/* Table for FXTRACT. */
double exp_pow_table[0x800];

static int cyrix_addr;

static void    cpu_write(uint16_t addr, uint8_t val, void *priv);
static uint8_t cpu_read(uint16_t addr, void *priv);

#ifdef ENABLE_CPU_LOG
int cpu_do_log = ENABLE_CPU_LOG;

void
cpu_log(const char *fmt, ...)
{
    va_list ap;

    if (cpu_do_log) {
        va_start(ap, fmt);
        pclog_ex(fmt, ap);
        va_end(ap);
    }
}
#else
#    define cpu_log(fmt, ...)
#endif

int
cpu_has_feature(int feature)
{
    return cpu_features & feature;
}

void
cpu_dynamic_switch(int new_cpu)
{
    int c;

    if (cpu_effective == new_cpu)
        return;

    c   = cpu;
    cpu = new_cpu;
    cpu_set();
    pc_speed_changed();
    cpu = c;
}

void
cpu_set_edx(void)
{
    EDX = cpu_s->edx_reset;
    if (fpu_softfloat)
        SF_FPU_reset();
}

cpu_family_t *
cpu_get_family(const char *internal_name)
{
    int c = 0;

    while (cpu_families[c].package) {
        if (!strcmp(internal_name, cpu_families[c].internal_name))
            return (cpu_family_t *) &cpu_families[c];
        c++;
    }

    return NULL;
}

uint8_t
cpu_is_eligible(const cpu_family_t *cpu_family, int cpu, int machine)
{
    const machine_t *machine_s = &machines[machine];
    const CPU       *cpu_s     = &cpu_family->cpus[cpu];
    uint32_t         packages;
    uint32_t         bus_speed;
    uint8_t          i;
    double           multi;

    /* Full override. */
    if (cpu_override > 1)
        return 1;

    /* Add implicit CPU package compatibility. */
    packages = machine_s->cpu.package;
    if (packages & CPU_PKG_SOCKET3)
        packages |= CPU_PKG_SOCKET1;
    else if (packages & CPU_PKG_SLOT1)
        packages |= CPU_PKG_SOCKET370 | CPU_PKG_SOCKET8;

    /* Package type. */
    if (!(cpu_family->package & packages))
        return 0;

    /* Partial override. */
    if (cpu_override)
        return 1;

    /* Check CPU blocklist. */
    if (machine_s->cpu.block) {
        i = 0;

        while (machine_s->cpu.block[i]) {
            if (machine_s->cpu.block[i++] == cpu_s->cpu_type)
                return 0;
        }
    }

    bus_speed = cpu_s->rspeed / cpu_s->multi;

    /* Minimum bus speed with ~0.84 MHz (for 8086) tolerance. */
    if (machine_s->cpu.min_bus && (bus_speed < (machine_s->cpu.min_bus - 840907)))
        return 0;

    /* Maximum bus speed with ~0.84 MHz (for 8086) tolerance. */
    if (machine_s->cpu.max_bus && (bus_speed > (machine_s->cpu.max_bus + 840907)))
        return 0;

    /* Minimum voltage with 0.1V tolerance. */
    if (machine_s->cpu.min_voltage && (cpu_s->voltage < (machine_s->cpu.min_voltage - 100)))
        return 0;

    /* Maximum voltage with 0.1V tolerance. */
    if (machine_s->cpu.max_voltage && (cpu_s->voltage > (machine_s->cpu.max_voltage + 100)))
        return 0;

    /* Account for CPUs which use a different internal multiplier than specified by jumpers. */
    multi = cpu_s->multi;

    /* Don't care about multiplier compatibility on fixed multiplier CPUs. */
    if (cpu_s->cpu_flags & CPU_FIXED_MULTIPLIER)
        return 1;

    /* Minimum multiplier, */
    if (multi < machine_s->cpu.min_multi)
        return 0;

    /* Maximum multiplier. */
    if (machine_s->cpu.max_multi && (multi > machine_s->cpu.max_multi))
        return 0;

    return 1;
}

uint8_t
cpu_family_is_eligible(const cpu_family_t *cpu_family, int machine)
{
    int c = 0;

    while (cpu_family->cpus[c].cpu_type) {
        if (cpu_is_eligible(cpu_family, c, machine))
            return 1;
        c++;
    }

    return 0;
}

void
SF_FPU_reset(void)
{
    if (fpu_type != FPU_NONE) {
        fpu_state.cwd = 0x0040;
        fpu_state.swd = 0;
        fpu_state.tos = 0;
        fpu_state.tag = 0x5555;
        fpu_state.foo = 0;
        fpu_state.fip = 0;
        fpu_state.fcs = 0;
        fpu_state.fds = 0;
        fpu_state.fdp = 0;
        memset(fpu_state.st_space, 0, sizeof(floatx80) * 8);
    }
}

void
cpu_set(void)
{
    cpu_inited = 1;

    cpu_effective = cpu;
    cpu_s         = (CPU *) &cpu_f->cpus[cpu_effective];

#ifdef USE_ACYCS
    acycs = 0;
#endif /* USE_ACYCS */

    soft_reset_pci    = 0;
    cpu_init          = 0;

    cpu_alt_reset     = 0;
    unmask_a20_in_smm = 0;

    CPUID       = cpu_s->cpuid_model;
    /* CPU capability flags are now compile-time constants for Pentium II (see cpu.h) */

    if (cpu_s->multi)
        cpu_busspeed = cpu_s->rspeed / cpu_s->multi;
    else
        cpu_busspeed = cpu_s->rspeed;
    cpu_multi  = (int) ceil(cpu_s->multi);
    cpu_dmulti = cpu_s->multi;
    ccr0 = ccr1 = ccr2 = ccr3 = ccr4 = ccr5 = ccr6 = ccr7 = 0;
    ccr4 = 0x85;

    cpu_update_waitstates();

    isa_cycles = cpu_s->atclk_div;

    if (cpu_s->rspeed <= 8000000)
        cpu_rom_prefetch_cycles = cpu_mem_prefetch_cycles;
    else
        cpu_rom_prefetch_cycles = cpu_s->rspeed / 1000000;

    cpu_set_isa_pci_div(0);
    cpu_set_pci_speed(0);
    cpu_set_agp_speed(0);

    io_handler(0, 0x0022, 0x0002, cpu_read, NULL, NULL, cpu_write, NULL, NULL, NULL);

    io_handler(1, 0x00f0, 0x000f, cpu_read, NULL, NULL, cpu_write, NULL, NULL, NULL);
    io_handler(1, 0xf007, 0x0001, cpu_read, NULL, NULL, cpu_write, NULL, NULL, NULL);

#ifdef USE_DYNAREC
    x86_setopcodes(ops_386, ops_386_0f, dynarec_ops_386, dynarec_ops_386_0f);
#else
    x86_setopcodes(ops_386, ops_386_0f);
#endif /* USE_DYNAREC */
    x86_setopcodes_2386(ops_2386_386, ops_2386_386_0f);
    x86_opcodes_REPE       = ops_REPE;
    x86_opcodes_REPNE      = ops_REPNE;
    x86_2386_opcodes_REPE  = ops_2386_REPE;
    x86_2386_opcodes_REPNE = ops_2386_REPNE;
    x86_opcodes_3DNOW      = ops_3DNOW;
#ifdef USE_DYNAREC
    x86_dynarec_opcodes_REPE  = dynarec_ops_REPE;
    x86_dynarec_opcodes_REPNE = dynarec_ops_REPNE;
    x86_dynarec_opcodes_3DNOW = dynarec_ops_3DNOW;
#endif /* USE_DYNAREC */

#ifdef USE_DYNAREC
    if (fpu_softfloat) {
        x86_dynarec_opcodes_d8_a16 = dynarec_ops_sf_fpu_d8_a16;
        x86_dynarec_opcodes_d8_a32 = dynarec_ops_sf_fpu_d8_a32;
        x86_dynarec_opcodes_d9_a16 = dynarec_ops_sf_fpu_d9_a16;
        x86_dynarec_opcodes_d9_a32 = dynarec_ops_sf_fpu_d9_a32;
        x86_dynarec_opcodes_da_a16 = dynarec_ops_sf_fpu_da_a16;
        x86_dynarec_opcodes_da_a32 = dynarec_ops_sf_fpu_da_a32;
        x86_dynarec_opcodes_db_a16 = dynarec_ops_sf_fpu_db_a16;
        x86_dynarec_opcodes_db_a32 = dynarec_ops_sf_fpu_db_a32;
        x86_dynarec_opcodes_dc_a16 = dynarec_ops_sf_fpu_dc_a16;
        x86_dynarec_opcodes_dc_a32 = dynarec_ops_sf_fpu_dc_a32;
        x86_dynarec_opcodes_dd_a16 = dynarec_ops_sf_fpu_dd_a16;
        x86_dynarec_opcodes_dd_a32 = dynarec_ops_sf_fpu_dd_a32;
        x86_dynarec_opcodes_de_a16 = dynarec_ops_sf_fpu_de_a16;
        x86_dynarec_opcodes_de_a32 = dynarec_ops_sf_fpu_de_a32;
        x86_dynarec_opcodes_df_a16 = dynarec_ops_sf_fpu_df_a16;
        x86_dynarec_opcodes_df_a32 = dynarec_ops_sf_fpu_df_a32;
    } else {
        x86_dynarec_opcodes_d8_a16 = dynarec_ops_fpu_d8_a16;
        x86_dynarec_opcodes_d8_a32 = dynarec_ops_fpu_d8_a32;
        x86_dynarec_opcodes_d9_a16 = dynarec_ops_fpu_d9_a16;
        x86_dynarec_opcodes_d9_a32 = dynarec_ops_fpu_d9_a32;
        x86_dynarec_opcodes_da_a16 = dynarec_ops_fpu_da_a16;
        x86_dynarec_opcodes_da_a32 = dynarec_ops_fpu_da_a32;
        x86_dynarec_opcodes_db_a16 = dynarec_ops_fpu_db_a16;
        x86_dynarec_opcodes_db_a32 = dynarec_ops_fpu_db_a32;
        x86_dynarec_opcodes_dc_a16 = dynarec_ops_fpu_dc_a16;
        x86_dynarec_opcodes_dc_a32 = dynarec_ops_fpu_dc_a32;
        x86_dynarec_opcodes_dd_a16 = dynarec_ops_fpu_dd_a16;
        x86_dynarec_opcodes_dd_a32 = dynarec_ops_fpu_dd_a32;
        x86_dynarec_opcodes_de_a16 = dynarec_ops_fpu_de_a16;
        x86_dynarec_opcodes_de_a32 = dynarec_ops_fpu_de_a32;
        x86_dynarec_opcodes_df_a16 = dynarec_ops_fpu_df_a16;
        x86_dynarec_opcodes_df_a32 = dynarec_ops_fpu_df_a32;
    }
#endif /* USE_DYNAREC */
    if (fpu_softfloat) {
        x86_opcodes_d8_a16 = ops_sf_fpu_d8_a16;
        x86_opcodes_d8_a32 = ops_sf_fpu_d8_a32;
        x86_opcodes_d9_a16 = ops_sf_fpu_d9_a16;
        x86_opcodes_d9_a32 = ops_sf_fpu_d9_a32;
        x86_opcodes_da_a16 = ops_sf_fpu_da_a16;
        x86_opcodes_da_a32 = ops_sf_fpu_da_a32;
        x86_opcodes_db_a16 = ops_sf_fpu_db_a16;
        x86_opcodes_db_a32 = ops_sf_fpu_db_a32;
        x86_opcodes_dc_a16 = ops_sf_fpu_dc_a16;
        x86_opcodes_dc_a32 = ops_sf_fpu_dc_a32;
        x86_opcodes_dd_a16 = ops_sf_fpu_dd_a16;
        x86_opcodes_dd_a32 = ops_sf_fpu_dd_a32;
        x86_opcodes_de_a16 = ops_sf_fpu_de_a16;
        x86_opcodes_de_a32 = ops_sf_fpu_de_a32;
        x86_opcodes_df_a16 = ops_sf_fpu_df_a16;
        x86_opcodes_df_a32 = ops_sf_fpu_df_a32;

        x86_2386_opcodes_d8_a16 = ops_2386_sf_fpu_d8_a16;
        x86_2386_opcodes_d8_a32 = ops_2386_sf_fpu_d8_a32;
        x86_2386_opcodes_d9_a16 = ops_2386_sf_fpu_d9_a16;
        x86_2386_opcodes_d9_a32 = ops_2386_sf_fpu_d9_a32;
        x86_2386_opcodes_da_a16 = ops_2386_sf_fpu_da_a16;
        x86_2386_opcodes_da_a32 = ops_2386_sf_fpu_da_a32;
        x86_2386_opcodes_db_a16 = ops_2386_sf_fpu_db_a16;
        x86_2386_opcodes_db_a32 = ops_2386_sf_fpu_db_a32;
        x86_2386_opcodes_dc_a16 = ops_2386_sf_fpu_dc_a16;
        x86_2386_opcodes_dc_a32 = ops_2386_sf_fpu_dc_a32;
        x86_2386_opcodes_dd_a16 = ops_2386_sf_fpu_dd_a16;
        x86_2386_opcodes_dd_a32 = ops_2386_sf_fpu_dd_a32;
        x86_2386_opcodes_de_a16 = ops_2386_sf_fpu_de_a16;
        x86_2386_opcodes_de_a32 = ops_2386_sf_fpu_de_a32;
        x86_2386_opcodes_df_a16 = ops_2386_sf_fpu_df_a16;
        x86_2386_opcodes_df_a32 = ops_2386_sf_fpu_df_a32;
    } else {
        x86_opcodes_d8_a16 = ops_fpu_d8_a16;
        x86_opcodes_d8_a32 = ops_fpu_d8_a32;
        x86_opcodes_d9_a16 = ops_fpu_d9_a16;
        x86_opcodes_d9_a32 = ops_fpu_d9_a32;
        x86_opcodes_da_a16 = ops_fpu_da_a16;
        x86_opcodes_da_a32 = ops_fpu_da_a32;
        x86_opcodes_db_a16 = ops_fpu_db_a16;
        x86_opcodes_db_a32 = ops_fpu_db_a32;
        x86_opcodes_dc_a16 = ops_fpu_dc_a16;
        x86_opcodes_dc_a32 = ops_fpu_dc_a32;
        x86_opcodes_dd_a16 = ops_fpu_dd_a16;
        x86_opcodes_dd_a32 = ops_fpu_dd_a32;
        x86_opcodes_de_a16 = ops_fpu_de_a16;
        x86_opcodes_de_a32 = ops_fpu_de_a32;
        x86_opcodes_df_a16 = ops_fpu_df_a16;
        x86_opcodes_df_a32 = ops_fpu_df_a32;

        x86_2386_opcodes_d8_a16 = ops_2386_fpu_d8_a16;
        x86_2386_opcodes_d8_a32 = ops_2386_fpu_d8_a32;
        x86_2386_opcodes_d9_a16 = ops_2386_fpu_d9_a16;
        x86_2386_opcodes_d9_a32 = ops_2386_fpu_d9_a32;
        x86_2386_opcodes_da_a16 = ops_2386_fpu_da_a16;
        x86_2386_opcodes_da_a32 = ops_2386_fpu_da_a32;
        x86_2386_opcodes_db_a16 = ops_2386_fpu_db_a16;
        x86_2386_opcodes_db_a32 = ops_2386_fpu_db_a32;
        x86_2386_opcodes_dc_a16 = ops_2386_fpu_dc_a16;
        x86_2386_opcodes_dc_a32 = ops_2386_fpu_dc_a32;
        x86_2386_opcodes_dd_a16 = ops_2386_fpu_dd_a16;
        x86_2386_opcodes_dd_a32 = ops_2386_fpu_dd_a32;
        x86_2386_opcodes_de_a16 = ops_2386_fpu_de_a16;
        x86_2386_opcodes_de_a32 = ops_2386_fpu_de_a32;
        x86_2386_opcodes_df_a16 = ops_2386_fpu_df_a16;
        x86_2386_opcodes_df_a32 = ops_2386_fpu_df_a32;
    }

#ifdef USE_DYNAREC
    codegen_timing_set(&codegen_timing_486);
#endif /* USE_DYNAREC */

    memset(&msr, 0, sizeof(msr));

    timing_misaligned   = 0;
    cpu_cyrix_alignment = 0;
    cpu_cpurst_on_sr    = 0;
    cpu_CR4_mask        = 0;

    /* Pentium II CPU initialization */
#ifdef USE_DYNAREC
    x86_setopcodes(ops_386, ops_pentium2_0f, dynarec_ops_386, dynarec_ops_pentium2_0f);
    if (fpu_softfloat) {
        x86_dynarec_opcodes_da_a16 = dynarec_ops_sf_fpu_686_da_a16;
        x86_dynarec_opcodes_da_a32 = dynarec_ops_sf_fpu_686_da_a32;
        x86_dynarec_opcodes_db_a16 = dynarec_ops_sf_fpu_686_db_a16;
        x86_dynarec_opcodes_db_a32 = dynarec_ops_sf_fpu_686_db_a32;
        x86_dynarec_opcodes_df_a16 = dynarec_ops_sf_fpu_686_df_a16;
        x86_dynarec_opcodes_df_a32 = dynarec_ops_sf_fpu_686_df_a32;
    } else {
        x86_dynarec_opcodes_da_a16 = dynarec_ops_fpu_686_da_a16;
        x86_dynarec_opcodes_da_a32 = dynarec_ops_fpu_686_da_a32;
        x86_dynarec_opcodes_db_a16 = dynarec_ops_fpu_686_db_a16;
        x86_dynarec_opcodes_db_a32 = dynarec_ops_fpu_686_db_a32;
        x86_dynarec_opcodes_df_a16 = dynarec_ops_fpu_686_df_a16;
        x86_dynarec_opcodes_df_a32 = dynarec_ops_fpu_686_df_a32;
    }
#else
    x86_setopcodes(ops_386, ops_pentium2_0f);
#endif /* USE_DYNAREC */
    if (fpu_softfloat) {
        x86_opcodes_da_a16 = ops_sf_fpu_686_da_a16;
        x86_opcodes_da_a32 = ops_sf_fpu_686_da_a32;
        x86_opcodes_db_a16 = ops_sf_fpu_686_db_a16;
        x86_opcodes_db_a32 = ops_sf_fpu_686_db_a32;
        x86_opcodes_df_a16 = ops_sf_fpu_686_df_a16;
        x86_opcodes_df_a32 = ops_sf_fpu_686_df_a32;
    } else {
        x86_opcodes_da_a16 = ops_fpu_686_da_a16;
        x86_opcodes_da_a32 = ops_fpu_686_da_a32;
        x86_opcodes_db_a16 = ops_fpu_686_db_a16;
        x86_opcodes_db_a32 = ops_fpu_686_db_a32;
        x86_opcodes_df_a16 = ops_fpu_686_df_a16;
        x86_opcodes_df_a32 = ops_fpu_686_df_a32;
    }

    timing_rr  = 1; /* register dest - register src */
    timing_rm  = 2; /* register dest - memory src */
    timing_mr  = 3; /* memory dest   - register src */
    timing_mm  = 3;
    timing_rml = 2; /* register dest - memory src long */
    timing_mrl = 3; /* memory dest   - register src long */
    timing_mml = 3;
    timing_bt  = 0; /* branch taken */
    timing_bnt = 1; /* branch not taken */

    timing_int                = 6;
    timing_int_rm             = 11;
    timing_int_v86            = 54;
    timing_int_pm             = 25;
    timing_int_pm_outer       = 42;
    timing_iret_rm            = 7;
    timing_iret_v86           = 27; /* unknown */
    timing_iret_pm            = 10;
    timing_iret_pm_outer      = 27;
    timing_call_rm            = 4;
    timing_call_pm            = 4;
    timing_call_pm_gate       = 22;
    timing_call_pm_gate_inner = 44;
    timing_retf_rm            = 4;
    timing_retf_pm            = 4;
    timing_retf_pm_outer      = 23;
    timing_jmp_rm             = 3;
    timing_jmp_pm             = 3;
    timing_jmp_pm_gate        = 18;

    timing_misaligned = 3;

    cpu_features = CPU_FEATURE_RDTSC | CPU_FEATURE_MSR | CPU_FEATURE_CR4 | CPU_FEATURE_VME | CPU_FEATURE_MMX;
    cpu_CR4_mask = CR4_VME | CR4_PVI | CR4_TSD | CR4_DE | CR4_PSE | CR4_MCE | CR4_PAE | CR4_PCE | CR4_PGE;

#ifdef USE_DYNAREC
    codegen_timing_set(&codegen_timing_p6);
#endif /* USE_DYNAREC */

    /* Pentium II has internal FPU - use 486+ timings */
    x87_timings     = x87_timings_486;
    x87_concurrency = x87_concurrency_486;

    cpu_use_exec = 1;
#if defined(USE_DYNAREC) && !defined(USE_GDBSTUB)
    if (cpu_use_dynarec)
        cpu_exec = exec386_dynarec;
    else
#endif /* defined(USE_DYNAREC) && !defined(USE_GDBSTUB) */
        cpu_exec = exec386;
    mmx_init();
    gdbstub_cpu_init();
}

void
cpu_close(void)
{
    cpu_inited = 0;
}

void
cpu_set_isa_speed(int speed)
{
    if (speed) {
        cpu_isa_speed = speed;
    } else if (cpu_busspeed >= 8000000)
        cpu_isa_speed = 8000000;
    else
        cpu_isa_speed = cpu_busspeed;

    pc_speed_changed();

    cpu_log("cpu_set_isa_speed(%d) = %d\n", speed, cpu_isa_speed);
}

void
cpu_set_pci_speed(int speed)
{
    if (speed)
        cpu_pci_speed = speed;
    else if (cpu_busspeed < 42500000)
        cpu_pci_speed = cpu_busspeed;
    else if (cpu_busspeed < 84000000)
        cpu_pci_speed = cpu_busspeed / 2;
    else if (cpu_busspeed < 120000000)
        cpu_pci_speed = cpu_busspeed / 3;
    else
        cpu_pci_speed = cpu_busspeed / 4;

    if (cpu_isa_pci_div)
        cpu_set_isa_pci_div(cpu_isa_pci_div);
    else if (speed)
        pc_speed_changed();

    pci_burst_time    = cpu_s->rspeed / cpu_pci_speed;
    pci_nonburst_time = 4 * pci_burst_time;

    cpu_log("cpu_set_pci_speed(%d) = %d\n", speed, cpu_pci_speed);
}

void
cpu_set_isa_pci_div(int div)
{
    cpu_isa_pci_div = div;

    cpu_log("cpu_set_isa_pci_div(%d)\n", cpu_isa_pci_div);

    if (cpu_isa_pci_div)
        cpu_set_isa_speed(cpu_pci_speed / cpu_isa_pci_div);
    else
        cpu_set_isa_speed(0);
}

void
cpu_set_agp_speed(int speed)
{
    if (speed) {
        cpu_agp_speed = speed;
        pc_speed_changed();
    } else if (cpu_busspeed < 84000000)
        cpu_agp_speed = cpu_busspeed;
    else if (cpu_busspeed < 120000000)
        cpu_agp_speed = cpu_busspeed / 1.5;
    else
        cpu_agp_speed = cpu_busspeed / 2;

    agp_burst_time    = cpu_s->rspeed / cpu_agp_speed;
    agp_nonburst_time = 4 * agp_burst_time;

    cpu_log("cpu_set_agp_speed(%d) = %d\n", speed, cpu_agp_speed);
}

char *
cpu_current_pc(char *bufp)
{
    static char buff[10];

    if (bufp == NULL)
        bufp = buff;

    sprintf(bufp, "%04X:%04X", CS, cpu_state.pc);

    return bufp;
}

void
cpu_CPUID(void)
{
    /* Pentium II CPUID */
    if (!EAX) {
        EAX = 0x00000002;
        EBX = 0x756e6547; /* GenuineIntel */
        EDX = 0x49656e69;
        ECX = 0x6c65746e;
    } else if (EAX == 1) {
        EAX = CPUID;
        EBX = ECX = 0;
        EDX       = CPUID_FPU | CPUID_VME | CPUID_DE | CPUID_PSE | CPUID_TSC | CPUID_MSR | CPUID_PAE | CPUID_MCE | CPUID_CMPXCHG8B | CPUID_MMX | CPUID_MTRR | CPUID_PGE | CPUID_MCA | CPUID_SEP | CPUID_CMOV;
        /*
           Return anything non-zero in bits 32-63 of the BIOS signature MSR
           to indicate there has been an update.
         */
        msr.bbl_cr_dx[3] = 0xffffffff00000000ULL;
    } else if (EAX == 2) {
        EAX = 0x03020101; /* Instruction TLB: 4 KB pages, 4-way set associative, 32 entries
                             Instruction TLB: 4 MB pages, fully associative, 2 entries
                             Data TLB: 4 KB pages, 4-way set associative, 64 entries */
        EBX = ECX = 0;
        EDX       = 0x0c040843; /* 2nd-level cache: 512 KB, 4-way set associative, 32-byte line size
                                   1st-level data cache: 16 KB, 4-way set associative, 32-byte line size
                                   Data TLB: 4 MB pages, 4-way set associative, 8 entries
                                   1st-level instruction cache: 16 KB, 4-way set associative, 32-byte line size */
    } else
        EAX = EBX = ECX = EDX = 0;
}

void
cpu_ven_reset(void)
{
    memset(&msr, 0, sizeof(msr));

    /* Pentium II MSR initialization */
    msr.mtrr_cap = 0x00000508ULL;
}

void
cpu_RDMSR(void)
{
    if ((CPL || (cpu_state.eflags & VM_FLAG)) && (cr0 & 1)) {
        x86gpf(NULL, 0);
        return;
    }

    /* Pentium II RDMSR */
    EAX = EDX = 0;
    /* Per RichardG's probing of a real Deschutes using my RDMSR tool,
       we have discovered that the top 18 bits are filtered out. */
    switch (ECX & 0x00003fff) {
        /* Machine Check Exception Address */
        case 0x00:
        /* Machine Check Exception Type */
        case 0x01:
            break;
        /* Time Stamp Counter */
        case 0x10:
            EAX = tsc & 0xffffffff;
            EDX = tsc >> 32;
            break;
        /* Unknown */
        case 0x18:
            break;
        /* IA32_APIC_BASE - APIC Base Address */
        case 0x1B:
            EAX = msr.apic_base & 0xffffffff;
            EDX = msr.apic_base >> 32;
            cpu_log("APIC_BASE read : %08X%08X\n", EDX, EAX);
            break;
        /* Unknown (undocumented?) MSR used by the Hyper-V BIOS */
        case 0x20:
            EAX = msr.ecx20 & 0xffffffff;
            EDX = msr.ecx20 >> 32;
            break;
        /* Unknown */
        case 0x21:
            break;
        /* EBL_CR_POWERON - Processor Hard Power-On Configuration */
        case 0x2a:
            EAX = 0xc4000000;
            EDX = 0;
            if (cpu_dmulti == 2.5)
                EAX |= ((0 << 25) | (1 << 24) | (1 << 23) | (1 << 22));
            else if (cpu_dmulti == 3)
                EAX |= ((0 << 25) | (0 << 24) | (0 << 23) | (1 << 22));
            else if (cpu_dmulti == 3.5)
                EAX |= ((0 << 25) | (1 << 24) | (0 << 23) | (1 << 22));
            else if (cpu_dmulti == 4)
                EAX |= ((0 << 25) | (0 << 24) | (1 << 23) | (0 << 22));
            else if (cpu_dmulti == 4.5)
                EAX |= ((0 << 25) | (1 << 24) | (1 << 23) | (0 << 22));
            else if (cpu_dmulti == 5)
                EAX |= 0;
            else if (cpu_dmulti == 5.5)
                EAX |= ((0 << 25) | (1 << 24) | (0 << 23) | (0 << 22));
            else if (cpu_dmulti == 6)
                EAX |= ((1 << 25) | (0 << 24) | (1 << 23) | (1 << 22));
            else if (cpu_dmulti == 6.5)
                EAX |= ((1 << 25) | (1 << 24) | (1 << 23) | (1 << 22));
            else if (cpu_dmulti == 7)
                EAX |= ((1 << 25) | (0 << 24) | (0 << 23) | (1 << 22));
            else if (cpu_dmulti == 7.5)
                EAX |= ((1 << 25) | (1 << 24) | (0 << 23) | (1 << 22));
            else if (cpu_dmulti == 8)
                EAX |= ((1 << 25) | (0 << 24) | (1 << 23) | (0 << 22));
            else
                EAX |= ((0 << 25) | (1 << 24) | (1 << 23) | (1 << 22));
            if (cpu_busspeed >= 84000000)
                EAX |= (1 << 19);
            break;
        /* Unknown */
        case 0x32:
            break;
        /* TEST_CTL - Test Control Register */
        case 0x33:
            EAX = msr.test_ctl;
            break;
        /* Unknown */
        case 0x34:
        case 0x3a:
        case 0x3b:
        case 0x50 ... 0x54:
            break;
        /* BIOS_UPDT_TRIG - BIOS Update Trigger */
        case 0x79:
            EAX = msr.bios_updt & 0xffffffff;
            EDX = msr.bios_updt >> 32;
            break;
        /* BBL_CR_D0 ... BBL_CR_D3 - Chunk 0..3 Data Register
           8Bh: BIOS_SIGN - BIOS Update Signature */
        case 0x88 ... 0x8b:
            EAX = msr.bbl_cr_dx[ECX - 0x88] & 0xffffffff;
            EDX = msr.bbl_cr_dx[ECX - 0x88] >> 32;
            break;
        /* Unknown */
        case 0xae:
            break;
        /* PERFCTR0 - Performance Counter Register 0 */
        case 0xc1:
        /* PERFCTR1 - Performance Counter Register 1 */
        case 0xc2:
            EAX = msr.perfctr[ECX - 0xC1] & 0xffffffff;
            EDX = msr.perfctr[ECX - 0xC1] >> 32;
            break;
        /* MTRRcap */
        case 0xfe:
            EAX = msr.mtrr_cap & 0xffffffff;
            EDX = msr.mtrr_cap >> 32;
            break;
        /* BBL_CR_ADDR - L2 Cache Address Register */
        case 0x116:
            EAX = msr.bbl_cr_addr & 0xffffffff;
            EDX = msr.bbl_cr_addr >> 32;
            break;
        /* BBL_CR_DECC - L2 Cache Date ECC Register */
        case 0x118:
            EAX = msr.bbl_cr_decc & 0xffffffff;
            EDX = msr.bbl_cr_decc >> 32;
            break;
        /* BBL_CR_CTL - L2 Cache Control Register */
        case 0x119:
            EAX = msr.bbl_cr_ctl & 0xffffffff;
            EDX = msr.bbl_cr_ctl >> 32;
            break;
        /* BBL_CR_TRIG - L2 Cache Trigger Register */
        case 0x11a:
            EAX = msr.bbl_cr_trig & 0xffffffff;
            EDX = msr.bbl_cr_trig >> 32;
            break;
        /* BBL_CR_BUSY - L2 Cache Busy Register */
        case 0x11b:
            EAX = msr.bbl_cr_busy & 0xffffffff;
            EDX = msr.bbl_cr_busy >> 32;
            break;
        /* BBL_CR_CTL3 - L2 Cache Control Register 3 */
        case 0x11e:
            EAX = msr.bbl_cr_ctl3 & 0xffffffff;
            EDX = msr.bbl_cr_ctl3 >> 32;
            break;
        /* Unknown */
        case 0x131:
        case 0x14e ... 0x151:
        case 0x154:
        case 0x15b:
        case 0x15f:
            break;
        /* SYSENTER_CS - SYSENTER target CS */
        case 0x174:
            EAX &= 0xffff0000;
            EAX |= msr.sysenter_cs;
            EDX = 0x00000000;
            break;
        /* SYSENTER_ESP - SYSENTER target ESP */
        case 0x175:
            EAX = msr.sysenter_esp;
            EDX = 0x00000000;
            break;
        /* SYSENTER_EIP - SYSENTER target EIP */
        case 0x176:
            EAX = msr.sysenter_eip;
            EDX = 0x00000000;
            break;
        /* MCG_CAP - Machine Check Global Capability */
        case 0x179:
            EAX = 0x00000105;
            EDX = 0x00000000;
            break;
        /* MCG_STATUS - Machine Check Global Status */
        case 0x17a:
            break;
        /* MCG_CTL - Machine Check Global Control */
        case 0x17b:
            EAX = msr.mcg_ctl & 0xffffffff;
            EDX = msr.mcg_ctl >> 32;
            break;
        /* EVNTSEL0 - Performance Counter Event Select 0 */
        case 0x186:
        /* EVNTSEL1 - Performance Counter Event Select 1 */
        case 0x187:
            EAX = msr.evntsel[ECX - 0x186] & 0xffffffff;
            EDX = msr.evntsel[ECX - 0x186] >> 32;
            break;
        /* Unknown */
        case 0x1d3:
            break;
        /* DEBUGCTLMSR - Debugging Control Register */
        case 0x1d9:
            EAX = msr.debug_ctl;
            break;
        /* LASTBRANCHFROMIP - address from which a branch was last taken */
        case 0x1db:
        /* LASTBRANCHTOIP - destination address of the last taken branch instruction */
        case 0x1dc:
        /* LASTINTFROMIP - address at which an interrupt last occurred */
        case 0x1dd:
        /* LASTINTTOIP - address to which the last interrupt caused a branch */
        case 0x1de:
            break;
        /* ROB_CR_BKUPTMPDR6 */
        case 0x1e0:
            EAX = msr.rob_cr_bkuptmpdr6;
            break;
        /* ECX & 0: MTRRphysBase0 ... MTRRphysBase7
           ECX & 1: MTRRphysMask0 ... MTRRphysMask7 */
        case 0x200 ... 0x20f:
            if (ECX & 1) {
                EAX = msr.mtrr_physmask[(ECX - 0x200) >> 1] & 0xffffffff;
                EDX = msr.mtrr_physmask[(ECX - 0x200) >> 1] >> 32;
            } else {
                EAX = msr.mtrr_physbase[(ECX - 0x200) >> 1] & 0xffffffff;
                EDX = msr.mtrr_physbase[(ECX - 0x200) >> 1] >> 32;
            }
            break;
        /* MTRRfix64K_00000 */
        case 0x250:
            EAX = msr.mtrr_fix64k_8000 & 0xffffffff;
            EDX = msr.mtrr_fix64k_8000 >> 32;
            break;
        /* MTRRfix16K_80000 */
        case 0x258:
            EAX = msr.mtrr_fix16k_8000 & 0xffffffff;
            EDX = msr.mtrr_fix16k_8000 >> 32;
            break;
        /* MTRRfix16K_A0000 */
        case 0x259:
            EAX = msr.mtrr_fix16k_a000 & 0xffffffff;
            EDX = msr.mtrr_fix16k_a000 >> 32;
            break;
        /* MTRRfix4K_C0000 ... MTRRfix4K_F8000 */
        case 0x268 ... 0x26f:
            EAX = msr.mtrr_fix4k[ECX - 0x268] & 0xffffffff;
            EDX = msr.mtrr_fix4k[ECX - 0x268] >> 32;
            break;
        /* Unknown */
        case 0x280:
            break;
        /* MTRRdefType */
        case 0x2ff:
            EAX = msr.mtrr_deftype & 0xffffffff;
            EDX = msr.mtrr_deftype >> 32;
            break;
        /* MC0_CTL - Machine Check 0 Control */
        case 0x400:
        /* MC1_CTL - Machine Check 1 Control */
        case 0x404:
        /* MC2_CTL - Machine Check 2 Control */
        case 0x408:
        /* MC4_CTL - Machine Check 4 Control */
        case 0x40c:
        /* MC3_CTL - Machine Check 3 Control */
        case 0x410:
            EAX = msr.mca_ctl[(ECX - 0x400) >> 2] & 0xffffffff;
            EDX = msr.mca_ctl[(ECX - 0x400) >> 2] >> 32;
            break;
        /* MC0_STATUS - Machine Check 0 Status */
        case 0x401:
        /* MC0_ADDR - Machine Check 0 Address */
        case 0x402:
        /* MC1_STATUS - Machine Check 1 Status */
        case 0x405:
        /* MC1_ADDR - Machine Check 1 Address */
        case 0x406:
        /* MC2_STATUS - Machine Check 2 Status */
        case 0x409:
        /* MC2_ADDR - Machine Check 2 Address */
        case 0x40a:
        /* MC4_STATUS - Machine Check 4 Status */
        case 0x40d:
        /* MC4_ADDR - Machine Check 4 Address */
        case 0x40e:
        /* MC3_STATUS - Machine Check 3 Status */
        case 0x411:
        /* MC3_ADDR - Machine Check 3 Address */
        case 0x412:
            break;
        /* Unknown */
        case 0x570:
            EAX = msr.ecx570 & 0xffffffff;
            EDX = msr.ecx570 >> 32;
            break;
        /* Unknown, possibly debug registers? */
        case 0x1000 ... 0x1007:
        /* Unknown, possibly control registers? */
        case 0x2000:
        case 0x2002 ... 0x2004:
            break;
        default:
            cpu_log("RDMSR: Invalid MSR: %08X\n", ECX);
            x86gpf(NULL, 0);
            break;
    }

    cpu_log("RDMSR %08X %08X%08X\n", ECX, EDX, EAX);
}

void
cpu_WRMSR(void)
{
    cpu_log("WRMSR %08X %08X%08X\n", ECX, EDX, EAX);

    if ((CPL || (cpu_state.eflags & VM_FLAG)) && (cr0 & 1)) {
        x86gpf(NULL, 0);
        return;
    }

    /* Pentium II WRMSR */
    /* Per RichardG's probing of a real Deschutes using my RDMSR tool,
       we have discovered that the top 18 bits are filtered out. */
    switch (ECX & 0x00003fff) {
        /* Machine Check Exception Address */
        case 0x00:
        /* Machine Check Exception Type */
        case 0x01:
            if (EAX || EDX)
                x86gpf(NULL, 0);
            break;
        /* Time Stamp Counter */
        case 0x10:
            timer_set_new_tsc(EAX | ((uint64_t) EDX << 32));
            break;
        /* Unknown */
        case 0x18:
            break;
        /* IA32_APIC_BASE - APIC Base Address */
        case 0x1b:
            cpu_log("APIC_BASE write: %08X%08X\n", EDX, EAX);
            break;
        /* Unknown (undocumented?) MSR used by the Hyper-V BIOS */
        case 0x20:
            msr.ecx20 = EAX | ((uint64_t) EDX << 32);
            break;
        /* Unknown */
        case 0x21:
            break;
        /* EBL_CR_POWERON - Processor Hard Power-On Configuration */
        case 0x2a:
            break;
        /* Unknown */
        case 0x32:
            break;
        /* TEST_CTL - Test Control Register */
        case 0x33:
            msr.test_ctl = EAX;
            break;
        /* Unknown */
        case 0x34:
        case 0x3a:
        case 0x3b:
        case 0x50 ... 0x54:
            break;
        /* BIOS_UPDT_TRIG - BIOS Update Trigger */
        case 0x79:
            msr.bios_updt = EAX | ((uint64_t) EDX << 32);
            break;
        /* BBL_CR_D0 ... BBL_CR_D3 - Chunk 0..3 Data Register
           8Bh: BIOS_SIGN - BIOS Update Signature */
        case 0x88 ... 0x8b:
            msr.bbl_cr_dx[ECX - 0x88] = EAX | ((uint64_t) EDX << 32);
            break;
        /* Unknown */
        case 0xae:
            break;
        /* PERFCTR0 - Performance Counter Register 0 */
        case 0xc1:
        /* PERFCTR1 - Performance Counter Register 1 */
        case 0xc2:
            msr.perfctr[ECX - 0xC1] = EAX | ((uint64_t) EDX << 32);
            break;
        /* MTRRcap */
        case 0xfe:
            msr.mtrr_cap = EAX | ((uint64_t) EDX << 32);
            break;
        /* BBL_CR_ADDR - L2 Cache Address Register */
        case 0x116:
            msr.bbl_cr_addr = EAX | ((uint64_t) EDX << 32);
            break;
        /* BBL_CR_DECC - L2 Cache Date ECC Register */
        case 0x118:
            msr.bbl_cr_decc = EAX | ((uint64_t) EDX << 32);
            break;
        /* BBL_CR_CTL - L2 Cache Control Register */
        case 0x119:
            msr.bbl_cr_ctl = EAX | ((uint64_t) EDX << 32);
            break;
        /* BBL_CR_TRIG - L2 Cache Trigger Register */
        case 0x11a:
            msr.bbl_cr_trig = EAX | ((uint64_t) EDX << 32);
            break;
        /* BBL_CR_BUSY - L2 Cache Busy Register */
        case 0x11b:
            msr.bbl_cr_busy = EAX | ((uint64_t) EDX << 32);
            break;
        /* BBL_CR_CTL3 - L2 Cache Control Register 3 */
        case 0x11e:
            msr.bbl_cr_ctl3 = (msr.bbl_cr_ctl3 & 0x02f00000) | (EAX & ~0x02f00000) | ((uint64_t) EDX << 32);
            break;
        /* Unknown */
        case 0x131:
        case 0x14e ... 0x151:
        case 0x154:
        case 0x15b:
        case 0x15f:
            break;
        /* SYSENTER_CS - SYSENTER target CS */
        case 0x174:
            msr.sysenter_cs = EAX & 0xFFFF;
            break;
        /* SYSENTER_ESP - SYSENTER target ESP */
        case 0x175:
            msr.sysenter_esp = EAX;
            break;
        /* SYSENTER_EIP - SYSENTER target EIP */
        case 0x176:
            msr.sysenter_eip = EAX;
            break;
        /* MCG_CAP - Machine Check Global Capability */
        case 0x179:
            break;
        /* MCG_STATUS - Machine Check Global Status */
        case 0x17a:
            if (EAX || EDX)
                x86gpf(NULL, 0);
            break;
        /* MCG_CTL - Machine Check Global Control */
        case 0x17b:
            msr.mcg_ctl = EAX | ((uint64_t) EDX << 32);
            break;
        /* EVNTSEL0 - Performance Counter Event Select 0 */
        case 0x186:
        /* EVNTSEL1 - Performance Counter Event Select 1 */
        case 0x187:
            msr.evntsel[ECX - 0x186] = EAX | ((uint64_t) EDX << 32);
            break;
        case 0x1d3:
            break;
        /* DEBUGCTLMSR - Debugging Control Register */
        case 0x1d9:
            msr.debug_ctl = EAX;
            break;
        /* ROB_CR_BKUPTMPDR6 */
        case 0x1e0:
            msr.rob_cr_bkuptmpdr6 = EAX;
            break;
        /* ECX & 0: MTRRphysBase0 ... MTRRphysBase7
           ECX & 1: MTRRphysMask0 ... MTRRphysMask7 */
        case 0x200 ... 0x20f:
            if (ECX & 1)
                msr.mtrr_physmask[(ECX - 0x200) >> 1] = EAX | ((uint64_t) EDX << 32);
            else
                msr.mtrr_physbase[(ECX - 0x200) >> 1] = EAX | ((uint64_t) EDX << 32);
            break;
        /* MTRRfix64K_00000 */
        case 0x250:
            msr.mtrr_fix64k_8000 = EAX | ((uint64_t) EDX << 32);
            break;
        /* MTRRfix16K_80000 */
        case 0x258:
            msr.mtrr_fix16k_8000 = EAX | ((uint64_t) EDX << 32);
            break;
        /* MTRRfix16K_A0000 */
        case 0x259:
            msr.mtrr_fix16k_a000 = EAX | ((uint64_t) EDX << 32);
            break;
        /* MTRRfix4K_C0000 ... MTRRfix4K_F8000 */
        case 0x268 ... 0x26f:
            msr.mtrr_fix4k[ECX - 0x268] = EAX | ((uint64_t) EDX << 32);
            break;
        /* Unknown */
        case 0x280:
            break;
        /* MTRRdefType */
        case 0x2ff:
            msr.mtrr_deftype = EAX | ((uint64_t) EDX << 32);
            break;
        /* MC0_CTL - Machine Check 0 Control */
        case 0x400:
        /* MC1_CTL - Machine Check 1 Control */
        case 0x404:
        /* MC2_CTL - Machine Check 2 Control */
        case 0x408:
        /* MC4_CTL - Machine Check 4 Control */
        case 0x40c:
        /* MC3_CTL - Machine Check 3 Control */
        case 0x410:
            msr.mca_ctl[(ECX - 0x400) >> 2] = EAX | ((uint64_t) EDX << 32);
            break;
        /* MC0_STATUS - Machine Check 0 Status */
        case 0x401:
        /* MC0_ADDR - Machine Check 0 Address */
        case 0x402:
        /* MC1_STATUS - Machine Check 1 Status */
        case 0x405:
        /* MC1_ADDR - Machine Check 1 Address */
        case 0x406:
        /* MC2_STATUS - Machine Check 2 Status */
        case 0x409:
        /* MC2_ADDR - Machine Check 2 Address */
        case 0x40a:
        /* MC4_STATUS - Machine Check 4 Status */
        case 0x40d:
        /* MC4_ADDR - Machine Check 4 Address */
        case 0x40e:
        /* MC3_STATUS - Machine Check 3 Status */
        case 0x411:
        /* MC3_ADDR - Machine Check 3 Address */
        case 0x412:
            if (EAX || EDX)
                x86gpf(NULL, 0);
            break;
        /* Unknown */
        case 0x570:
            msr.ecx570 = EAX | ((uint64_t) EDX << 32);
            break;
        /* Unknown, possibly debug registers? */
        case 0x1000 ... 0x1007:
        /* Unknown, possibly control registers? */
        case 0x2000:
        case 0x2002 ... 0x2004:
            break;
        default:
            cpu_log("WRMSR: Invalid MSR: %08X\n", ECX);
            x86gpf(NULL, 0);
            break;
    }
}

static void
cpu_write(uint16_t addr, uint8_t val, UNUSED(void *priv))
{
    if (addr == 0xf0) {
        /* Writes to F0 clear FPU error and deassert the interrupt. */
        picintc(1 << 13);
    } else if ((addr < 0xf1) && !(addr & 1))
        cyrix_addr = val;
    else if (addr < 0xf1)  switch (cyrix_addr) {
        default:
            if ((cyrix_addr >= 0xc0) && (cyrix_addr != 0xff))
                fatal("Writing unimplemented Cyrix register %02X\n", cyrix_addr);
            break;

        case 0x30: /* ???? */
            reg_30 = val;
            break;

        case 0xc0: /* CCR0 */
            ccr0 = val;
            break;
        case 0xc1: { /* CCR1 */
            uint8_t old = ccr1;
            if ((ccr3 & CCR3_SMI_LOCK) && !in_smm)
                val = (val & ~(CCR1_USE_SMI | CCR1_SMAC | CCR1_SM3)) | (ccr1 & (CCR1_USE_SMI | CCR1_SMAC | CCR1_SM3));
            ccr1 = val;
            if ((old ^ ccr1) & (CCR1_SMAC)) {
                if (ccr1 & CCR1_SMAC)
                    smram_backup_all();
                smram_recalc_all(!(ccr1 & CCR1_SMAC));
            }
            break;
        } case 0xc2: /* CCR2 */
            ccr2 = val;
            break;
        case 0xc3: /* CCR3 */
            if ((ccr3 & CCR3_SMI_LOCK) && !in_smm)
                val = (val & ~(CCR3_NMI_EN)) | (ccr3 & CCR3_NMI_EN) | CCR3_SMI_LOCK;
            ccr3 = val;
            break;

        case 0xc4 ... 0xcc:
            if (ccr5 & 0x20)
                arr[cyrix_addr - 0xc4] = val;
            break;
        case 0xcd:
            if ((ccr5 & 0x20) || (!(ccr3 & CCR3_SMI_LOCK) || in_smm)) {
                arr[cyrix_addr - 0xc4] = val;
                cyrix.arr[3].base = (cyrix.arr[3].base & ~0xff000000) | (val << 24);
                cyrix.smhr &= ~SMHR_VALID;
            }
            break;
        case 0xce:
            if ((ccr5 & 0x20) || (!(ccr3 & CCR3_SMI_LOCK) || in_smm)) {
                arr[cyrix_addr - 0xc4] = val;
                cyrix.arr[3].base = (cyrix.arr[3].base & ~0x00ff0000) | (val << 16);
                cyrix.smhr &= ~SMHR_VALID;
            }
            break;
        case 0xcf:
            if ((ccr5 & 0x20) || (!(ccr3 & CCR3_SMI_LOCK) || in_smm)) {
                arr[cyrix_addr - 0xc4] = val;
                cyrix.arr[3].base = (cyrix.arr[3].base & ~0x0000f000) | ((val & 0xf0) << 8);
                if ((val & 0xf) == 0xf)
                    cyrix.arr[3].size = 1ULL << 32; /* 4 GB */
                else if (val & 0xf)
                    cyrix.arr[3].size = 2048 << (val & 0xf);
                else
                    cyrix.arr[3].size = 0; /* Disabled */
                cyrix.smhr &= ~SMHR_VALID;
            }
            break;
        case 0xd0 ... 0xdb:
            if (((ccr3 & 0xf0) == 0x10) && (ccr5 & 0x20))
                arr[cyrix_addr - 0xc4] = val;
            break;

        case 0xdc ... 0xe3:
            if ((ccr3 & 0xf0) == 0x10)
                rcr[cyrix_addr - 0xdc] = val;
            break;

        case 0xe8: /* CCR4 */
            if ((ccr3 & 0xf0) == 0x10) {
                ccr4 = val;
                if (cpu_s->cpu_type >= CPU_Cx6x86) {
                    if (val & 0x80)
                        CPUID = cpu_s->cpuid_model;
                    else
                        CPUID = 0;
                }
            }
            break;
        case 0xe9: /* CCR5 */
            if ((ccr3 & 0xf0) == 0x10)
                ccr5 = val;
            break;
        case 0xea: /* CCR6 */
            if ((ccr3 & 0xf0) == 0x10)
                ccr6 = val;
            break;
        case 0xeb: /* CCR7 */
            ccr7 = val & 5;
            break;
    }
}

static uint8_t
cpu_read(uint16_t addr, UNUSED(void *priv))
{
    uint8_t ret = 0xff;

    if (addr == 0xf007)
        ret = 0x7f;
    else if ((addr < 0xf0) && (addr & 1))  switch (cyrix_addr) {
        default:
            if (cyrix_addr >= 0xc0)
                fatal("Reading unimplemented Cyrix register %02X\n", cyrix_addr);
            break;

        case 0x30: /* ???? */
            ret = reg_30;
            break;

        case 0xc0:
            ret = ccr0;
            break;
        case 0xc1:
            ret = ccr1;
            break;
        case 0xc2:
            ret = ccr2;
            break;
        case 0xc3:
            ret = ccr3;
            break;

        case 0xc4 ... 0xcc:
            if (ccr5 & 0x20)
                ret = arr[cyrix_addr - 0xc4];
            break;
        case 0xcd ... 0xcf:
            if ((ccr5 & 0x20) || (!(ccr3 & CCR3_SMI_LOCK) || in_smm))
                ret = arr[cyrix_addr - 0xc4];
            break;
        case 0xd0 ... 0xdb:
            if (((ccr3 & 0xf0) == 0x10) && (ccr5 & 0x20))
                ret = arr[cyrix_addr - 0xc4];
            break;

        case 0xdc ... 0xe3:
            if ((ccr3 & 0xf0) == 0x10)
                ret = rcr[cyrix_addr - 0xdc];
            break;

        case 0xe8:
            if ((ccr3 & 0xf0) == 0x10)
                ret = ccr4;
            break;
        case 0xe9:
            if ((ccr3 & 0xf0) == 0x10)
                ret = ccr5;
            break;
        case 0xea:
            if ((ccr3 & 0xf0) == 0x10)
                ret = ccr6;
            break;
        case 0xeb:
            ret = ccr7;
            break;
        case 0xfe:
            ret = cpu_s->cyrix_id & 0xff;
            break;
        case 0xff:
            ret = cpu_s->cyrix_id >> 8;
            break;
    }

    return ret;
}

void
#ifdef USE_DYNAREC
x86_setopcodes(const OpFn *opcodes, const OpFn *opcodes_0f,
               const OpFn *dynarec_opcodes, const OpFn *dynarec_opcodes_0f)
{
    x86_opcodes            = opcodes;
    x86_opcodes_0f         = opcodes_0f;
    x86_dynarec_opcodes    = dynarec_opcodes;
    x86_dynarec_opcodes_0f = dynarec_opcodes_0f;
}
#else
x86_setopcodes(const OpFn *opcodes, const OpFn *opcodes_0f)
{
    x86_opcodes    = opcodes;
    x86_opcodes_0f = opcodes_0f;
}
#endif /* USE_DYNAREC */

void
x86_setopcodes_2386(const OpFn *opcodes, const OpFn *opcodes_0f)
{
    x86_2386_opcodes    = opcodes;
    x86_2386_opcodes_0f = opcodes_0f;
}

void
cpu_update_waitstates(void)
{
    cpu_s = (CPU *) &cpu_f->cpus[cpu_effective];

    cpu_prefetch_width = 16;

    if (cpu_cache_int_enabled) {
        /* Disable prefetch emulation */
        cpu_prefetch_cycles = 0;
    } else if (cpu_cache_ext_enabled) {
        /* Use cache timings */
        cpu_prefetch_cycles = cpu_s->cache_read_cycles;
        cpu_cycles_read     = cpu_s->cache_read_cycles;
        cpu_cycles_read_l   =  cpu_s->cache_read_cycles;
        cpu_cycles_write    = cpu_s->cache_write_cycles;
        cpu_cycles_write_l  =  cpu_s->cache_write_cycles;
    } else if (cpu_waitstates && (cpu_s->cpu_type >= CPU_286 && cpu_s->cpu_type <= CPU_386DX)) {
        /* Waitstates override */
        cpu_prefetch_cycles = cpu_waitstates + 1;
        cpu_cycles_read     = cpu_waitstates + 1;
        cpu_cycles_read_l   =  (cpu_waitstates + 1);
        cpu_cycles_write    = cpu_waitstates + 1;
        cpu_cycles_write_l  =  (cpu_waitstates + 1);
    } else {
        /* Use memory timings */
        cpu_prefetch_cycles = cpu_s->mem_read_cycles;
        cpu_cycles_read     = cpu_s->mem_read_cycles;
        cpu_cycles_read_l   =  cpu_s->mem_read_cycles;
        cpu_cycles_write    = cpu_s->mem_write_cycles;
        cpu_cycles_write_l  =  cpu_s->mem_write_cycles;
    }

    cpu_prefetch_cycles = (cpu_prefetch_cycles * 11) / 16;

    cpu_mem_prefetch_cycles = cpu_prefetch_cycles;

    if (cpu_s->rspeed <= 8000000)
        cpu_rom_prefetch_cycles = cpu_mem_prefetch_cycles;
}
