/*
 * wbox - Timer interface stub
 * Stub header replacing 86box/timer.h
 */
#ifndef WBOX_TIMER_H
#define WBOX_TIMER_H

#include <stdint.h>

extern uint64_t tsc;

#define MAX_USEC64    1000000ULL
#define MAX_USEC      1000000.0

#define TIMER_PROCESS 4
#define TIMER_SPLIT   2
#define TIMER_ENABLED 1

#pragma pack(push, 1)
typedef struct ts_struct_t {
    uint32_t frac;
    uint32_t integer;
} ts_struct_t;
#pragma pack(pop)

typedef union ts_t {
    uint64_t    ts64;
    ts_struct_t ts32;
} ts_t;

typedef struct pc_timer_t {
    ts_t ts;
    int  flags;
    int  in_callback;
    double period;

    void (*callback)(void *priv);
    void *priv;

    struct pc_timer_t *prev;
    struct pc_timer_t *next;
} pc_timer_t;

extern uint32_t timer_target;

extern void timer_enable(pc_timer_t *timer);
extern void timer_disable(pc_timer_t *timer);
extern void timer_process(void);
extern void timer_close(void);
extern void timer_init(void);
extern void timer_add(pc_timer_t *timer, void (*callback)(void *priv), void *priv, int start_timer);

extern uint64_t TIMER_USEC;

#define TIMER_LESS_THAN(a, b) ((int64_t) ((a)->ts.ts64 - (b)->ts.ts64) <= 0)
#define TIMER_LESS_THAN_VAL(a, b) ((int32_t) ((a)->ts.ts32.integer - (b)) <= 0)
#define TIMER_VAL_LESS_THAN_VAL(a, b) ((int32_t) ((a) - (b)) <= 0)

static inline void timer_advance_u64(pc_timer_t *timer, uint64_t delay)
{
    timer->ts.ts64 += delay;
    timer_enable(timer);
}

static inline void timer_set_delay_u64(pc_timer_t *timer, uint64_t delay)
{
    timer->ts.ts64         = 0ULL;
    timer->ts.ts32.integer = tsc;
    timer->ts.ts64 += delay;
    timer_enable(timer);
}

static inline int timer_is_enabled(pc_timer_t *timer)
{
    return !!(timer->flags & TIMER_ENABLED);
}

static inline int timer_is_on(pc_timer_t *timer)
{
    return ((timer->flags & TIMER_SPLIT) && (timer->flags & TIMER_ENABLED));
}

static inline uint32_t timer_get_ts_int(pc_timer_t *timer)
{
    return timer->ts.ts32.integer;
}

static inline uint32_t timer_get_remaining_us(pc_timer_t *timer)
{
    int64_t remaining;

    if (timer->flags & TIMER_ENABLED) {
        remaining = (int64_t) (timer->ts.ts64 - (uint64_t) (tsc << 32));
        if (remaining < 0)
            return 0;
        return remaining / TIMER_USEC;
    }
    return 0;
}

static inline uint64_t timer_get_remaining_u64(pc_timer_t *timer)
{
    int64_t remaining;

    if (timer->flags & TIMER_ENABLED) {
        remaining = (int64_t) (timer->ts.ts64 - (uint64_t) (tsc << 32));
        if (remaining < 0)
            return 0;
        return remaining;
    }
    return 0;
}

static inline void timer_set_callback(pc_timer_t *timer, void (*callback)(void *priv))
{
    timer->callback = callback;
}

static inline void timer_set_p(pc_timer_t *timer, void *priv)
{
    timer->priv = priv;
}

extern void timer_stop(pc_timer_t *timer);
extern void timer_on_auto(pc_timer_t *timer, double period);
extern void timer_set_new_tsc(uint64_t new_tsc);

#endif /* WBOX_TIMER_H */
