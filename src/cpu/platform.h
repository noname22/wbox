/*
 * wbox - Platform definitions for CPU emulation
 * Stub header replacing 86box/86box.h and related platform headers
 */
#ifndef WBOX_PLATFORM_H
#define WBOX_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#ifdef _MSC_VER
#    define UNUSED(arg) arg
#else
#    define UNUSED(arg) __attribute__((unused)) arg
#endif

#ifdef _MSC_VER
#    define fallthrough do {} while (0)
#else
#    if __has_attribute(fallthrough)
#        define fallthrough __attribute__((fallthrough))
#    else
#        define fallthrough do {} while (0)
#    endif
#endif

#ifndef MIN
#    define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#    define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef ABS
#    define ABS(x) ((x) > 0 ? (x) : -(x))
#endif

#define BCD8(x)   ((((x) / 10) << 4) | ((x) % 10))
#define BCD16(x)  ((((x) / 1000) << 12) | (((x) / 100) << 8) | BCD8(x))
#define BCD32(x)  ((((x) / 10000000) << 28) | (((x) / 1000000) << 24) | (((x) / 100000) << 20) | (((x) / 10000) << 16) | BCD16(x))

#define AS_U8(x)     (*((uint8_t *) &(x)))
#define AS_U16(x)    (*((uint16_t *) &(x)))
#define AS_U32(x)    (*((uint32_t *) &(x)))
#define AS_U64(x)    (*((uint64_t *) &(x)))
#define AS_I8(x)     (*((int8_t *) &(x)))
#define AS_I16(x)    (*((int16_t *) &(x)))
#define AS_I32(x)    (*((int32_t *) &(x)))
#define AS_I64(x)    (*((int64_t *) &(x)))
#define AS_FLOAT(x)  (*((float *) &(x)))
#define AS_DOUBLE(x) (*((double *) &(x)))

#if defined(__GNUC__) || defined(__clang__)
#    define UNLIKELY(x) __builtin_expect((x), 0)
#    define LIKELY(x)   __builtin_expect((x), 1)
#else
#    define UNLIKELY(x) (x)
#    define LIKELY(x)   (x)
#endif

/* Platform-specific atomic handling */
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#    define ATOMIC_INT volatile int
#    define ATOMIC_UINT volatile uint32_t
#    define ATOMIC_DOUBLE volatile double
#    define ATOMIC_LOAD(var) (var)
#    define ATOMIC_STORE(var, val) ((var) = (val))
#    define ATOMIC_INC(var) (++(var))
#    define ATOMIC_DEC(var) (--(var))
#    define ATOMIC_ADD(var, val) ((var) += (val))
#    define ATOMIC_SUB(var, val) ((var) -= (val))
#    define ATOMIC_DOUBLE_ADD(var, val) ((var) += (val))
#else
#    include <stdatomic.h>
#    define ATOMIC_INT atomic_int
#    define ATOMIC_UINT atomic_uint
#    define ATOMIC_DOUBLE _Atomic double
#    define ATOMIC_LOAD(var) atomic_load(&(var))
#    define ATOMIC_STORE(var, val) atomic_store(&(var), (val))
#    define ATOMIC_INC(var) atomic_fetch_add(&(var), 1)
#    define ATOMIC_DEC(var) atomic_fetch_sub(&(var), 1)
#    define ATOMIC_ADD(var, val) atomic_fetch_add(&(var), val)
#    define ATOMIC_SUB(var, val) atomic_fetch_sub(&(var), val)
#    define ATOMIC_DOUBLE_ADD(var, val) atomic_double_add(&(var), val)
#endif

/* Logging */
#define pclog(...) fprintf(stderr, __VA_ARGS__)
#define fatal(...) do { fprintf(stderr, __VA_ARGS__); abort(); } while(0)

/* Configuration stubs */
extern int cpu_override;
extern int cpu;
extern int fpu_type;
extern int fpu_softfloat;
extern int machine;

/* Function stubs */
extern void pc_speed_changed(void);
extern void io_handler(int set, uint16_t base, int size,
                       uint8_t (*inb)(uint16_t addr, void *priv),
                       uint16_t (*inw)(uint16_t addr, void *priv),
                       uint32_t (*inl)(uint16_t addr, void *priv),
                       void (*outb)(uint16_t addr, uint8_t val, void *priv),
                       void (*outw)(uint16_t addr, uint16_t val, void *priv),
                       void (*outl)(uint16_t addr, uint32_t val, void *priv),
                       void *priv);

extern void *machine_at_nupro592_init;

/* CPU dynarec flag */
extern int cpu_use_dynarec;

/* PCI/AGP timing */
extern int pci_burst_time;
extern int pci_nonburst_time;
extern int agp_burst_time;
extern int agp_nonburst_time;

/* Memory size */
extern uint32_t mem_size;

/* Force timing */
extern int force_10ms;

#endif /* WBOX_PLATFORM_H */
