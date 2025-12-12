/*
 * WBOX Thread Scheduler
 * Manages thread scheduling, blocking, and context switching
 */
#ifndef WBOX_SCHEDULER_H
#define WBOX_SCHEDULER_H

#include <stdint.h>
#include <stdbool.h>
#include "thread.h"

/* Forward declarations */
struct vm_context;
struct wbox_dispatcher_header;

/*
 * Scheduler structure
 */
typedef struct wbox_scheduler {
    /* Thread lists */
    wbox_thread_t *all_threads;         /* Linked list of all threads */
    wbox_thread_t *current_thread;      /* Currently running thread */

    /* Ready queue (FIFO within priority) */
    wbox_thread_t *ready_head;
    wbox_thread_t *ready_tail;

    /* Thread ID allocation */
    uint32_t next_thread_id;

    /* Scheduling state */
    uint64_t tick_count;                /* Total scheduler ticks */
    uint32_t context_switches;          /* Total context switches */
    bool idle;                          /* No runnable threads */
    bool preemption_pending;            /* Thread switch needed */

    /* VM reference */
    struct vm_context *vm;
} wbox_scheduler_t;

/*
 * Initialize the scheduler
 * Creates the main thread (thread 0) from existing CPU state
 * @param sched Scheduler to initialize
 * @param vm VM context
 * @return 0 on success, -1 on failure
 */
int scheduler_init(wbox_scheduler_t *sched, struct vm_context *vm);

/*
 * Cleanup scheduler resources
 * @param sched Scheduler to cleanup
 */
void scheduler_cleanup(wbox_scheduler_t *sched);

/*
 * Called every N CPU cycles from main loop
 * Decrements quantum, may trigger preemption
 * @param sched Scheduler
 */
void scheduler_tick(wbox_scheduler_t *sched);

/*
 * Switch to the next ready thread
 * Saves current context, restores next thread's context
 * @param sched Scheduler
 */
void scheduler_switch(wbox_scheduler_t *sched);

/*
 * Check for timeout expiry on waiting threads
 * Wakes threads whose timeout has expired
 * @param sched Scheduler
 */
void scheduler_check_timeouts(wbox_scheduler_t *sched);

/*
 * Add a thread to the ready queue
 * @param sched Scheduler
 * @param thread Thread to add
 */
void scheduler_add_ready(wbox_scheduler_t *sched, wbox_thread_t *thread);

/*
 * Remove a thread from the ready queue
 * @param sched Scheduler
 * @param thread Thread to remove
 */
void scheduler_remove_ready(wbox_scheduler_t *sched, wbox_thread_t *thread);

/*
 * Block the current thread waiting on sync objects
 * @param sched Scheduler
 * @param objects Array of sync object pointers
 * @param types Array of handle types for each object
 * @param count Number of objects
 * @param wait_type WAIT_TYPE_ALL or WAIT_TYPE_ANY
 * @param timeout Absolute timeout in 100ns units (0 = infinite)
 * @param alertable Whether wait can be alerted
 * @return NTSTATUS (STATUS_SUCCESS, STATUS_TIMEOUT, STATUS_WAIT_0+n, etc.)
 */
uint32_t scheduler_block_thread(wbox_scheduler_t *sched,
                                void **objects, int *types, int count,
                                wbox_wait_type_t wait_type, uint64_t timeout,
                                bool alertable);

/*
 * Signal that an object has become signaled
 * Wakes threads waiting on this object as appropriate
 * @param sched Scheduler
 * @param object Sync object that was signaled
 * @param type Handle type of the object
 */
void scheduler_signal_object(wbox_scheduler_t *sched, void *object, int type);

/*
 * Add a new thread to the scheduler
 * @param sched Scheduler
 * @param thread Thread to add
 */
void scheduler_add_thread(wbox_scheduler_t *sched, wbox_thread_t *thread);

/*
 * Remove a thread from the scheduler (on termination)
 * @param sched Scheduler
 * @param thread Thread to remove
 */
void scheduler_remove_thread(wbox_scheduler_t *sched, wbox_thread_t *thread);

/*
 * Get current time in 100-nanosecond units
 * Used for timeout calculations
 * @return Current time
 */
uint64_t scheduler_get_time_100ns(void);

/*
 * Get the global scheduler instance
 * @return Scheduler pointer or NULL
 */
wbox_scheduler_t *scheduler_get_instance(void);

/*
 * Set the global scheduler instance
 * @param sched Scheduler to set as global
 */
void scheduler_set_instance(wbox_scheduler_t *sched);

#endif /* WBOX_SCHEDULER_H */
