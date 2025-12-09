/*
 * wbox - Device interface stub
 * Minimal header for device reset functionality
 */
#ifndef WBOX_DEVICE_H
#define WBOX_DEVICE_H

#include <stdint.h>

/* Device reset flags */
enum {
    DEVICE_ALL       = 0xFFFFFFFF,
    DEVICE_KBC       = 0x100000,
    DEVICE_SOFTRESET = 0x200000
};

extern void device_reset_all(uint32_t flags);

#endif /* WBOX_DEVICE_H */
