/*
 * wbox - SMRAM interface stub
 * Stub header replacing 86box/smram.h
 */
#ifndef WBOX_SMRAM_H
#define WBOX_SMRAM_H

#include <stdint.h>
#include "mem.h"

typedef struct _smram_ {
    struct _smram_ *prev;
    struct _smram_ *next;

    mem_mapping_t mapping;

    uint32_t host_base;
    uint32_t ram_base;
    uint32_t size;
    uint32_t old_host_base;
    uint32_t old_size;
} smram_t;

extern void smram_backup_all(void);
extern void smram_recalc_all(int ret);
extern void smram_del(smram_t *smr);
extern smram_t *smram_add(void);
extern void smram_map_ex(int bus, int smm, uint32_t addr, uint32_t size, int is_smram);
extern void smram_map(int smm, uint32_t addr, uint32_t size, int is_smram);
extern void smram_disable(smram_t *smr);
extern void smram_disable_all(void);
extern void smram_enable_ex(smram_t *smr, uint32_t host_base, uint32_t ram_base, uint32_t size,
                            int flags_normal, int flags_normal_bus, int flags_smm, int flags_smm_bus);
extern void smram_enable(smram_t *smr, uint32_t host_base, uint32_t ram_base, uint32_t size,
                         int flags_normal, int flags_smm);
extern int smram_enabled(smram_t *smr);
extern void smram_state_change(smram_t *smr, int smm, int flags);
extern void smram_set_separate_smram(uint8_t set);

#endif /* WBOX_SMRAM_H */
