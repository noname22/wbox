/*
 * wbox - FDD interface stub
 * Stub header replacing 86box/fdd.h
 */
#ifndef WBOX_FDD_H
#define WBOX_FDD_H

#include <stdint.h>

extern void fdd_set_type(int drive, int type);
extern int  fdd_get_type(int drive);

#endif /* WBOX_FDD_H */
