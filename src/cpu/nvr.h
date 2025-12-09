/*
 * wbox - NVR interface stub
 * Stub header replacing 86box/nvr.h
 */
#ifndef WBOX_NVR_H
#define WBOX_NVR_H

#include <stdint.h>

typedef struct nvr_t {
    uint8_t regs[256];
} nvr_t;

extern nvr_t *nvr;

#endif /* WBOX_NVR_H */
