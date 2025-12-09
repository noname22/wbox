/*
 * wbox - PIT interface stub
 * Stub header replacing 86box/pit.h
 */
#ifndef WBOX_PIT_H
#define WBOX_PIT_H

#include <stdint.h>

typedef struct pit_t {
    uint32_t l[3];
    int      m[3];
} pit_t;

extern pit_t *pit;
extern pit_t *pit2;

extern double  isa_timing;
extern double  bus_timing;
extern double  pci_timing;
extern double  agp_timing;
extern uint64_t cpu_clock_multi;

#endif /* WBOX_PIT_H */
