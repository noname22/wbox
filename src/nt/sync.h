/*
 * WBOX NT Synchronization Objects
 * Events, semaphores, and mutexes following ReactOS patterns
 */
#ifndef WBOX_SYNC_H
#define WBOX_SYNC_H

#include <stdint.h>
#include <stdbool.h>

/* Forward declarations */
struct wbox_thread;
struct wbox_wait_block;

/*
 * Dispatcher object types (matching ReactOS KOBJECTS)
 */
typedef enum {
    WBOX_DISP_EVENT_NOTIFICATION = 0,   /* Manual-reset event */
    WBOX_DISP_EVENT_SYNCHRONIZATION,    /* Auto-reset event */
    WBOX_DISP_SEMAPHORE,
    WBOX_DISP_MUTANT,
    WBOX_DISP_TIMER,
    WBOX_DISP_THREAD,                   /* Thread object (waitable) */
} wbox_disp_type_t;

/*
 * Dispatcher header - base for all synchronization objects
 * Matches ReactOS DISPATCHER_HEADER concept
 */
typedef struct wbox_dispatcher_header {
    wbox_disp_type_t type;              /* Object type */
    int32_t signal_state;               /* >0 = signaled */
    struct wbox_wait_block *wait_list;  /* List of threads waiting on this object */
} wbox_dispatcher_header_t;

/*
 * Event object
 * signal_state: 0 = not signaled, 1 = signaled
 * Type NOTIFICATION = manual-reset (stays signaled until explicit reset)
 * Type SYNCHRONIZATION = auto-reset (clears after satisfying one wait)
 */
typedef struct wbox_event {
    wbox_dispatcher_header_t header;
} wbox_event_t;

/*
 * Semaphore object
 * signal_state: current count (0 to limit)
 * Signaled when signal_state > 0
 */
typedef struct wbox_semaphore {
    wbox_dispatcher_header_t header;
    int32_t limit;                      /* Maximum count */
} wbox_semaphore_t;

/*
 * Mutant (mutex) object
 * signal_state: 1 = available (signaled), <=0 = owned (not signaled)
 * Negative values track recursive acquisition depth
 */
typedef struct wbox_mutant {
    wbox_dispatcher_header_t header;
    uint32_t owner_thread_id;           /* Thread ID of owner (0 if none) */
    int recursion_count;                /* Number of recursive acquisitions */
    bool abandoned;                     /* Set if owner terminated without releasing */
} wbox_mutant_t;

/*
 * Timer object (for future use)
 */
typedef struct wbox_timer {
    wbox_dispatcher_header_t header;
    uint64_t due_time;                  /* When timer fires (100ns units) */
    uint32_t period;                    /* Period for periodic timers (ms), 0 = one-shot */
} wbox_timer_t;

/*
 * Union for generic sync object access
 */
typedef union wbox_sync_object {
    wbox_dispatcher_header_t header;
    wbox_event_t event;
    wbox_semaphore_t semaphore;
    wbox_mutant_t mutant;
    wbox_timer_t timer;
} wbox_sync_object_t;

/*
 * Create an event object
 * @param type WBOX_DISP_EVENT_NOTIFICATION or WBOX_DISP_EVENT_SYNCHRONIZATION
 * @param initial_state true = signaled, false = not signaled
 * @return Allocated event object or NULL on failure
 */
wbox_event_t *sync_create_event(wbox_disp_type_t type, bool initial_state);

/*
 * Create a semaphore object
 * @param initial_count Initial count (must be <= max_count)
 * @param max_count Maximum count (must be > 0)
 * @return Allocated semaphore object or NULL on failure
 */
wbox_semaphore_t *sync_create_semaphore(int32_t initial_count, int32_t max_count);

/*
 * Create a mutant (mutex) object
 * @param initial_owner true = calling thread owns initially
 * @param owner_thread_id Thread ID of initial owner (if initial_owner is true)
 * @return Allocated mutant object or NULL on failure
 */
wbox_mutant_t *sync_create_mutant(bool initial_owner, uint32_t owner_thread_id);

/*
 * Free a synchronization object
 * @param object Pointer to sync object
 * @param type Handle type (HANDLE_TYPE_EVENT, etc.)
 */
void sync_free_object(void *object, int type);

/*
 * Check if an object is signaled
 * @param obj Sync object to check
 * @param thread_id Current thread ID (for mutant ownership check)
 * @return true if the object is in signaled state
 */
bool sync_is_signaled(wbox_sync_object_t *obj, uint32_t thread_id);

/*
 * Satisfy a wait on an object (modify signal state as appropriate)
 * Called when a wait is being satisfied.
 * @param obj Sync object
 * @param thread_id Thread ID acquiring the object (for mutants)
 */
void sync_satisfy_wait(wbox_sync_object_t *obj, uint32_t thread_id);

/*
 * Get the dispatcher header from a sync object
 * @param object Pointer to sync object
 * @param type Handle type
 * @return Pointer to dispatcher header or NULL
 */
wbox_dispatcher_header_t *sync_get_header(void *object, int type);

#endif /* WBOX_SYNC_H */
