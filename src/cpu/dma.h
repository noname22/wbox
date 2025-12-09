/*
 * wbox - DMA interface stub
 * Stub header replacing 86box/dma.h
 */
#ifndef WBOX_DMA_H
#define WBOX_DMA_H

#include <stdint.h>

extern void dma_init(void);
extern void dma_reset(void);
extern void dma_set_at(int is286);

#endif /* WBOX_DMA_H */
