/*
 * WBOX NT Synchronization Objects Implementation
 */
#include "sync.h"
#include "handles.h"
#include <stdlib.h>
#include <string.h>

wbox_event_t *sync_create_event(wbox_disp_type_t type, bool initial_state)
{
    if (type != WBOX_DISP_EVENT_NOTIFICATION &&
        type != WBOX_DISP_EVENT_SYNCHRONIZATION) {
        return NULL;
    }

    wbox_event_t *event = calloc(1, sizeof(wbox_event_t));
    if (!event) {
        return NULL;
    }

    event->header.type = type;
    event->header.signal_state = initial_state ? 1 : 0;
    event->header.wait_list = NULL;

    return event;
}

wbox_semaphore_t *sync_create_semaphore(int32_t initial_count, int32_t max_count)
{
    if (max_count <= 0 || initial_count < 0 || initial_count > max_count) {
        return NULL;
    }

    wbox_semaphore_t *sem = calloc(1, sizeof(wbox_semaphore_t));
    if (!sem) {
        return NULL;
    }

    sem->header.type = WBOX_DISP_SEMAPHORE;
    sem->header.signal_state = initial_count;
    sem->header.wait_list = NULL;
    sem->limit = max_count;

    return sem;
}

wbox_mutant_t *sync_create_mutant(bool initial_owner, uint32_t owner_thread_id)
{
    wbox_mutant_t *mutant = calloc(1, sizeof(wbox_mutant_t));
    if (!mutant) {
        return NULL;
    }

    mutant->header.type = WBOX_DISP_MUTANT;
    mutant->header.wait_list = NULL;
    mutant->abandoned = false;

    if (initial_owner) {
        /* Owned by caller: signal_state = -1 (not signaled, recursion 1) */
        mutant->header.signal_state = -1;
        mutant->owner_thread_id = owner_thread_id;
        mutant->recursion_count = 1;
    } else {
        /* Not owned: signal_state = 1 (signaled/available) */
        mutant->header.signal_state = 1;
        mutant->owner_thread_id = 0;
        mutant->recursion_count = 0;
    }

    return mutant;
}

void sync_free_object(void *object, int type)
{
    if (!object) {
        return;
    }

    /* Just free the object - wait list should be empty if properly closed */
    free(object);
}

bool sync_is_signaled(wbox_sync_object_t *obj, uint32_t thread_id)
{
    if (!obj) {
        return false;
    }

    switch (obj->header.type) {
        case WBOX_DISP_EVENT_NOTIFICATION:
        case WBOX_DISP_EVENT_SYNCHRONIZATION:
            /* Events: signaled when signal_state > 0 */
            return obj->header.signal_state > 0;

        case WBOX_DISP_SEMAPHORE:
            /* Semaphores: signaled when count > 0 */
            return obj->header.signal_state > 0;

        case WBOX_DISP_MUTANT:
            /* Mutants: signaled when available (signal_state > 0)
             * OR when current thread already owns it (can acquire recursively) */
            if (obj->header.signal_state > 0) {
                return true;
            }
            /* Check if current thread owns it */
            if (obj->mutant.owner_thread_id == thread_id && thread_id != 0) {
                return true;
            }
            return false;

        case WBOX_DISP_TIMER:
            /* Timers: signaled when signal_state > 0 */
            return obj->header.signal_state > 0;

        case WBOX_DISP_THREAD:
            /* Threads: signaled when terminated */
            return obj->header.signal_state > 0;

        default:
            return false;
    }
}

void sync_satisfy_wait(wbox_sync_object_t *obj, uint32_t thread_id)
{
    if (!obj) {
        return;
    }

    switch (obj->header.type) {
        case WBOX_DISP_EVENT_NOTIFICATION:
            /* Manual-reset event: stays signaled */
            break;

        case WBOX_DISP_EVENT_SYNCHRONIZATION:
            /* Auto-reset event: becomes non-signaled after satisfying wait */
            obj->header.signal_state = 0;
            break;

        case WBOX_DISP_SEMAPHORE:
            /* Semaphore: decrement count */
            if (obj->header.signal_state > 0) {
                obj->header.signal_state--;
            }
            break;

        case WBOX_DISP_MUTANT:
            /* Mutant: acquire ownership */
            if (obj->header.signal_state > 0) {
                /* First acquisition */
                obj->header.signal_state = -1;
                obj->mutant.owner_thread_id = thread_id;
                obj->mutant.recursion_count = 1;
            } else if (obj->mutant.owner_thread_id == thread_id) {
                /* Recursive acquisition by same thread */
                obj->header.signal_state--;
                obj->mutant.recursion_count++;
            }
            /* If neither, the wait shouldn't have been satisfied */
            break;

        case WBOX_DISP_TIMER:
            /* Timers: typically auto-reset for synchronization timers */
            /* For now, treat as manual-reset */
            break;

        case WBOX_DISP_THREAD:
            /* Thread objects stay signaled once terminated */
            break;

        default:
            break;
    }
}

wbox_dispatcher_header_t *sync_get_header(void *object, int type)
{
    if (!object) {
        return NULL;
    }

    switch (type) {
        case HANDLE_TYPE_EVENT:
            return &((wbox_event_t *)object)->header;

        case HANDLE_TYPE_SEMAPHORE:
            return &((wbox_semaphore_t *)object)->header;

        case HANDLE_TYPE_MUTEX:
            return &((wbox_mutant_t *)object)->header;

        default:
            return NULL;
    }
}
