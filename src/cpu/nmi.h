/*
 * wbox - NMI interface stub
 * Stub header replacing 86box/nmi.h
 */
#ifndef WBOX_NMI_H
#define WBOX_NMI_H

#include <stdint.h>

extern int nmi_mask;
extern int nmi;
extern int nmi_auto_clear;

extern void nmi_init(void);
extern void nmi_write(uint16_t port, uint8_t val, void *priv);

#endif /* WBOX_NMI_H */
