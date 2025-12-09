/*
 * wbox - PIC interface stub
 * Stub header replacing 86box/pic.h
 */
#ifndef WBOX_PIC_H
#define WBOX_PIC_H

#include <stdint.h>

typedef struct pic {
    uint8_t     icw1;
    uint8_t     icw2;
    uint8_t     icw3;
    uint8_t     icw4;
    uint8_t     imr;
    uint8_t     isr;
    uint8_t     irr;
    uint8_t     ocw2;
    uint8_t     ocw3;
    uint8_t     int_pending;
    uint8_t     is_master;
    uint8_t     elcr;
    uint8_t     state;
    uint8_t     ack_bytes;
    uint8_t     priority;
    uint8_t     special_mask_mode;
    uint8_t     auto_eoi_rotate;
    uint8_t     interrupt;
    uint8_t     data_bus;
    uint8_t     irq_latch;
    uint8_t     has_slaves;
    uint8_t     flags;
    uint8_t     edge_lines;
    uint8_t     pad;
    uint32_t    lines[8];
    uint32_t    at;
    struct pic *slaves[8];
} pic_t;

extern pic_t pic;
extern pic_t pic2;

#define PIC_IRQ_EDGE   0
#define PIC_IRQ_LEVEL  1

extern void pic_init(void);
extern void pic2_init(void);
extern void pic_reset(void);

extern void picint_common(uint16_t num, int level, int set, uint8_t *irq_state);
extern int  picinterrupt(void);

#define picint(num)                     picint_common(num, PIC_IRQ_EDGE,  1, NULL)
#define picintlevel(num, irq_state)     picint_common(num, PIC_IRQ_LEVEL, 1, irq_state)
#define picintc(num)                    picint_common(num, PIC_IRQ_EDGE,  0, NULL)
#define picintclevel(num, irq_state)    picint_common(num, PIC_IRQ_LEVEL, 0, irq_state)

extern uint8_t pic_irq_ack(void);

#endif /* WBOX_PIC_H */
