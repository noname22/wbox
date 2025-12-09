/*
 * wbox - Machine interface stub
 * Stub header replacing 86box/machine.h
 */
#ifndef WBOX_MACHINE_H
#define WBOX_MACHINE_H

#include <stdint.h>

/* Machine flags - minimal set needed for CPU code */
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

typedef struct _machine_cpu_ {
    uint32_t       package;
    const uint8_t *block;
    uint32_t       min_bus;
    uint32_t       max_bus;
    uint16_t       min_voltage;
    uint16_t       max_voltage;
    float          min_multi;
    float          max_multi;
} machine_cpu_t;

typedef struct _machine_memory_ {
    uint32_t min;
    uint32_t max;
    int      step;
} machine_memory_t;

typedef struct machine_t {
    const char            *name;
    const char            *internal_name;
    uint32_t               type;
    uintptr_t              chipset;
    int                  (*init)(const struct machine_t *);
    uint8_t              (*p1_handler)(void);
    uint32_t             (*gpio_handler)(uint8_t write, uint32_t val);
    uintptr_t              available_flag;
    uint32_t             (*gpio_acpi_handler)(uint8_t write, uint32_t val);
    const machine_cpu_t    cpu;
    uintptr_t              bus_flags;
    uintptr_t              flags;
    const machine_memory_t ram;
    int                    ram_granularity;
    int                    nvrmask;
    int                    jumpered_ecp_dma;
    int                    default_jumpered_ecp_dma;
    void                  *kbc_device;
    uintptr_t              kbc_params;
    uint32_t               kbc_p1;
    uint32_t               gpio;
    uint32_t               gpio_acpi;
    void                  *device;
    void                  *kbd_device;
    void                  *fdc_device;
    void                  *sio_device;
} machine_t;

extern const machine_t machines[];
extern int machine_at;

#endif /* WBOX_MACHINE_H */
