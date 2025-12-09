/*
 * wbox - Device interface stub
 * Stub header replacing 86box/device.h
 */
#ifndef WBOX_DEVICE_H
#define WBOX_DEVICE_H

#include <stdint.h>

/* Device flags - not used but needed for compilation */
enum {
    DEVICE_ISA       = 4,
    DEVICE_ISA16     = 0x20,
    DEVICE_MCA       = 0x80,
    DEVICE_VLB       = 0x8000,
    DEVICE_PCI       = 0x10000,
    DEVICE_AGP       = 0x80000,
    DEVICE_ALL       = 0xFFFFFFFF,
    DEVICE_KBC       = 0x100000,
    DEVICE_SOFTRESET = 0x200000
};

/* Minimal device_t stub */
typedef struct device_t {
    const char *name;
    const char *internal_name;
    uint32_t    flags;
    uint32_t    local;
    void       *(*init)(const struct device_t *);
    void        (*close)(void *priv);
    void        (*reset)(void *priv);
    int         (*available)(void);
    void        (*speed_changed)(void *priv);
    void        (*force_redraw)(void *priv);
    void       *config;
} device_t;

extern void *device_add(const device_t *d);
extern void  device_close_all(void);
extern void  device_reset_all(uint32_t flags);

#endif /* WBOX_DEVICE_H */
