/*
 * wbox - GDB stub interface stub
 * Stub header replacing 86box/gdbstub.h
 */
#ifndef WBOX_GDBSTUB_H
#define WBOX_GDBSTUB_H

#include <stdint.h>

/* GDB stub is disabled by default */
#define GDBSTUB_ENABLED 0

extern int gdbstub_instruction(void);
extern void gdbstub_cpu_init(void);

#endif /* WBOX_GDBSTUB_H */
