/*
 * wbox - Machine interface stub
 * Minimal header for CPU code machine configuration
 */
#ifndef WBOX_MACHINE_H
#define WBOX_MACHINE_H

#include <stdint.h>

/* Machine bus flags - minimal set needed for CPU code */
#define MACHINE_AT          0x00000001
#define MACHINE_PS2         0x00000002
#define MACHINE_BUS_ISA     0x00000004
#define MACHINE_BUS_CBUS    0x00000008
#define MACHINE_BUS_EISA    0x00001000
#define MACHINE_BUS_VLB     0x00008000
#define MACHINE_BUS_MCA     0x00000080
#define MACHINE_BUS_PCI     0x00010000
#define MACHINE_BUS_AGP     0x00080000

/* Combined flags */
#define MACHINE_PC          (MACHINE_BUS_ISA)
#define MACHINE_VLB         (MACHINE_BUS_VLB | MACHINE_AT)
#define MACHINE_PCI         (MACHINE_BUS_PCI | MACHINE_AT)
#define MACHINE_AGP         (MACHINE_BUS_AGP | MACHINE_PCI)

typedef struct machine_t {
    const char *name;
    const char *internal_name;
    uint32_t    bus_flags;
    struct {
        uint32_t       package;
        const uint8_t *block;
        uint32_t       min_bus;
        uint32_t       max_bus;
        uint16_t       min_voltage;
        uint16_t       max_voltage;
        float          min_multi;
        float          max_multi;
    } cpu;
} machine_t;

extern const machine_t machines[];
extern int machine_at;

#endif /* WBOX_MACHINE_H */
