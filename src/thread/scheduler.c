/*
 * WBOX Thread Scheduler Implementation
 */
#include "scheduler.h"
#include "thread.h"
#include "../vm/vm.h"
#include "../nt/sync.h"
#include "../nt/handles.h"
#include "../nt/syscalls.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* Global scheduler instance */
static wbox_scheduler_t *g_scheduler = NULL;

wbox_scheduler_t *scheduler_get_instance(void)
{
    return g_scheduler;
}

void scheduler_set_instance(wbox_scheduler_t *sched)
{
    g_scheduler = sched;
}

uint64_t scheduler_get_time_100ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 10000000ULL + (uint64_t)ts.tv_nsec / 100ULL;
}

int scheduler_init(wbox_scheduler_t *sched, struct vm_context *vm)
{
    if (!sched || !vm) {
        return -1;
    }

    memset(sched, 0, sizeof(*sched));
    sched->vm = vm;
    sched->next_thread_id = WBOX_THREAD_ID + 4;

    /* Create main thread from current CPU state */
    wbox_thread_t *main_thread = thread_create_main(vm);
    if (!main_thread) {
        fprintf(stderr, "scheduler_init: Failed to create main thread\n");
        return -1;
    }

    /* Add to thread list */
    sched->all_threads = main_thread;
    sched->current_thread = main_thread;

    /* Main thread is running, not in ready queue */
    sched->ready_head = NULL;
    sched->ready_tail = NULL;

    sched->idle = false;

    /* Set global instance */
    scheduler_set_instance(sched);

    printf("Scheduler initialized with main thread %u\n", main_thread->thread_id);
    return 0;
}

void scheduler_cleanup(wbox_scheduler_t *sched)
{
    if (!sched) {
        return;
    }

    /* Free all threads */
    wbox_thread_t *thread = sched->all_threads;
    while (thread) {
        wbox_thread_t *next = thread->next;
        free(thread);
        thread = next;
    }

    sched->all_threads = NULL;
    sched->current_thread = NULL;
    sched->ready_head = NULL;
    sched->ready_tail = NULL;

    if (g_scheduler == sched) {
        g_scheduler = NULL;
    }
}

void scheduler_add_ready(wbox_scheduler_t *sched, wbox_thread_t *thread)
{
    if (!sched || !thread) {
        return;
    }

    thread->ready_next = NULL;

    if (sched->ready_tail) {
        sched->ready_tail->ready_next = thread;
        sched->ready_tail = thread;
    } else {
        sched->ready_head = thread;
        sched->ready_tail = thread;
    }

    sched->idle = false;
}

void scheduler_remove_ready(wbox_scheduler_t *sched, wbox_thread_t *thread)
{
    if (!sched || !thread) {
        return;
    }

    wbox_thread_t *prev = NULL;
    wbox_thread_t *curr = sched->ready_head;

    while (curr) {
        if (curr == thread) {
            if (prev) {
                prev->ready_next = curr->ready_next;
            } else {
                sched->ready_head = curr->ready_next;
            }

            if (curr == sched->ready_tail) {
                sched->ready_tail = prev;
            }

            thread->ready_next = NULL;
            return;
        }
        prev = curr;
        curr = curr->ready_next;
    }
}

void scheduler_tick(wbox_scheduler_t *sched)
{
    if (!sched || !sched->current_thread) {
        return;
    }

    sched->tick_count++;

    /* Decrement quantum */
    if (sched->current_thread->quantum > 0) {
        sched->current_thread->quantum--;
    }

    /* Check for preemption */
    if (sched->current_thread->quantum == 0) {
        /* Reset quantum */
        sched->current_thread->quantum = sched->current_thread->quantum_reset;

        /* If there are other ready threads, preempt */
        if (sched->ready_head != NULL) {
            sched->preemption_pending = true;

            /* Add current thread to ready queue */
            sched->current_thread->state = THREAD_STATE_READY;
            scheduler_add_ready(sched, sched->current_thread);

            /* Switch to next thread */
            scheduler_switch(sched);
        }
    }
}

void scheduler_switch(wbox_scheduler_t *sched)
{
    if (!sched) {
        return;
    }

    wbox_thread_t *old_thread = sched->current_thread;
    wbox_thread_t *new_thread = NULL;

    /* Get next ready thread */
    if (sched->ready_head) {
        new_thread = sched->ready_head;
        sched->ready_head = new_thread->ready_next;
        if (sched->ready_head == NULL) {
            sched->ready_tail = NULL;
        }
        new_thread->ready_next = NULL;
    }

    if (!new_thread) {
        /* No ready threads - go idle */
        sched->idle = true;
        sched->current_thread = NULL;
        return;
    }

    /* Save old thread context if it was running */
    if (old_thread && old_thread->state == THREAD_STATE_RUNNING) {
        thread_save_context(old_thread);
    }

    /* Switch to new thread */
    sched->current_thread = new_thread;
    new_thread->state = THREAD_STATE_RUNNING;

    /* Restore new thread context */
    thread_restore_context(new_thread);

    sched->context_switches++;
    sched->idle = false;
    sched->preemption_pending = false;
}

void scheduler_check_timeouts(wbox_scheduler_t *sched)
{
    if (!sched) {
        return;
    }

    uint64_t now = scheduler_get_time_100ns();

    for (wbox_thread_t *thread = sched->all_threads; thread; thread = thread->next) {
        if (thread->state == THREAD_STATE_WAITING && thread->wait_timeout != 0) {
            if (now >= thread->wait_timeout) {
                /* Timeout expired */
                thread->wait_status = STATUS_TIMEOUT;

                /* Remove from wait lists */
                for (int i = 0; i < thread->wait_count; i++) {
                    wbox_wait_block_t *wb = &thread->wait_blocks[i];
                    if (wb->object) {
                        wbox_dispatcher_header_t *header = (wbox_dispatcher_header_t *)wb->object;

                        /* Remove from object's wait list */
                        wbox_wait_block_t **pp = (wbox_wait_block_t **)&header->wait_list;
                        while (*pp) {
                            if (*pp == wb) {
                                *pp = wb->next;
                                break;
                            }
                            pp = &(*pp)->next;
                        }
                    }
                }
                thread->wait_count = 0;
                thread->wait_timeout = 0;

                /* Add to ready queue */
                thread->state = THREAD_STATE_READY;
                scheduler_add_ready(sched, thread);
            }
        }
    }
}

void scheduler_add_thread(wbox_scheduler_t *sched, wbox_thread_t *thread)
{
    if (!sched || !thread) {
        return;
    }

    /* Add to all_threads list */
    thread->next = sched->all_threads;
    sched->all_threads = thread;

    /* If thread is ready, add to ready queue */
    if (thread->state == THREAD_STATE_READY) {
        scheduler_add_ready(sched, thread);
    }
}

void scheduler_remove_thread(wbox_scheduler_t *sched, wbox_thread_t *thread)
{
    if (!sched || !thread) {
        return;
    }

    /* Remove from all_threads list */
    wbox_thread_t **pp = &sched->all_threads;
    while (*pp) {
        if (*pp == thread) {
            *pp = thread->next;
            break;
        }
        pp = &(*pp)->next;
    }

    /* Remove from ready queue if present */
    scheduler_remove_ready(sched, thread);

    /* If this was the current thread, switch away */
    if (sched->current_thread == thread) {
        sched->current_thread = NULL;
        scheduler_switch(sched);
    }
}

uint32_t scheduler_block_thread(wbox_scheduler_t *sched,
                                void **objects, int *types, int count,
                                wbox_wait_type_t wait_type, uint64_t timeout,
                                bool alertable)
{
    if (!sched || !sched->current_thread) {
        return STATUS_INVALID_PARAMETER;
    }

    if (count > THREAD_WAIT_OBJECTS) {
        return STATUS_INVALID_PARAMETER;
    }

    wbox_thread_t *thread = sched->current_thread;
    uint32_t current_thread_id = thread->thread_id;

    /* First check if wait can be satisfied immediately */
    if (wait_type == WAIT_TYPE_ANY) {
        /* WaitAny: check if any object is signaled */
        for (int i = 0; i < count; i++) {
            if (objects[i] && sync_is_signaled((wbox_sync_object_t *)objects[i], current_thread_id)) {
                sync_satisfy_wait((wbox_sync_object_t *)objects[i], current_thread_id);
                return STATUS_WAIT_0 + i;
            }
        }
    } else {
        /* WaitAll: check if all objects are signaled */
        bool all_signaled = true;
        for (int i = 0; i < count; i++) {
            if (objects[i] && !sync_is_signaled((wbox_sync_object_t *)objects[i], current_thread_id)) {
                all_signaled = false;
                break;
            }
        }

        if (all_signaled) {
            /* Satisfy all waits */
            for (int i = 0; i < count; i++) {
                if (objects[i]) {
                    sync_satisfy_wait((wbox_sync_object_t *)objects[i], current_thread_id);
                }
            }
            return STATUS_WAIT_0;
        }
    }

    /* Wait cannot be satisfied immediately - must block */

    /* If timeout is already expired (0 means poll), return timeout */
    if (timeout == 0) {
        return STATUS_TIMEOUT;
    }

    /* Set up wait blocks */
    thread->wait_count = count;
    thread->wait_type = wait_type;
    thread->wait_timeout = timeout;
    thread->alertable = alertable;

    for (int i = 0; i < count; i++) {
        wbox_wait_block_t *wb = &thread->wait_blocks[i];
        wb->thread = thread;
        wb->object = objects[i];
        wb->wait_key = i;
        wb->next = NULL;

        /* Add to object's wait list */
        if (objects[i]) {
            wbox_dispatcher_header_t *header = (wbox_dispatcher_header_t *)objects[i];
            wb->next = (wbox_wait_block_t *)header->wait_list;
            header->wait_list = (struct wbox_wait_block *)wb;
        }
    }

    /* Change thread state to waiting */
    thread->state = THREAD_STATE_WAITING;

    /* Context switch to another thread */
    scheduler_switch(sched);

    /* When we return here, the wait has been satisfied or timed out */
    return thread->wait_status;
}

void scheduler_signal_object(wbox_scheduler_t *sched, void *object, int type)
{
    if (!sched || !object) {
        return;
    }

    wbox_dispatcher_header_t *header = sync_get_header(object, type);
    if (!header) {
        return;
    }

    /* Walk the wait list and wake appropriate threads */
    wbox_wait_block_t *wb = (wbox_wait_block_t *)header->wait_list;
    wbox_wait_block_t *prev = NULL;

    while (wb) {
        wbox_wait_block_t *next = wb->next;
        wbox_thread_t *thread = wb->thread;

        if (!thread || thread->state != THREAD_STATE_WAITING) {
            prev = wb;
            wb = next;
            continue;
        }

        bool wake_thread = false;
        uint32_t wait_status = STATUS_WAIT_0;

        if (thread->wait_type == WAIT_TYPE_ANY) {
            /* WaitAny: wake if this object is signaled */
            if (sync_is_signaled((wbox_sync_object_t *)object, thread->thread_id)) {
                wake_thread = true;
                wait_status = STATUS_WAIT_0 + wb->wait_key;
            }
        } else {
            /* WaitAll: wake only if ALL objects are signaled */
            bool all_signaled = true;
            for (int i = 0; i < thread->wait_count; i++) {
                if (thread->wait_blocks[i].object) {
                    if (!sync_is_signaled((wbox_sync_object_t *)thread->wait_blocks[i].object,
                                          thread->thread_id)) {
                        all_signaled = false;
                        break;
                    }
                }
            }

            if (all_signaled) {
                wake_thread = true;
                wait_status = STATUS_WAIT_0;
            }
        }

        if (wake_thread) {
            /* Satisfy the wait(s) */
            if (thread->wait_type == WAIT_TYPE_ANY) {
                sync_satisfy_wait((wbox_sync_object_t *)object, thread->thread_id);
            } else {
                /* WaitAll: satisfy all objects */
                for (int i = 0; i < thread->wait_count; i++) {
                    if (thread->wait_blocks[i].object) {
                        sync_satisfy_wait((wbox_sync_object_t *)thread->wait_blocks[i].object,
                                         thread->thread_id);
                    }
                }
            }

            /* Remove thread from all wait lists */
            for (int i = 0; i < thread->wait_count; i++) {
                wbox_wait_block_t *twb = &thread->wait_blocks[i];
                if (twb->object) {
                    wbox_dispatcher_header_t *h = (wbox_dispatcher_header_t *)twb->object;
                    wbox_wait_block_t **pp = (wbox_wait_block_t **)&h->wait_list;
                    while (*pp) {
                        if (*pp == twb) {
                            *pp = twb->next;
                            break;
                        }
                        pp = &(*pp)->next;
                    }
                }
            }

            /* Wake the thread */
            thread->wait_status = wait_status;
            thread->wait_count = 0;
            thread->wait_timeout = 0;
            thread->state = THREAD_STATE_READY;
            scheduler_add_ready(sched, thread);

            /* For auto-reset events and semaphores, may need to stop after one wake */
            wbox_sync_object_t *sync_obj = (wbox_sync_object_t *)object;
            if (sync_obj->header.type == WBOX_DISP_EVENT_SYNCHRONIZATION ||
                sync_obj->header.type == WBOX_DISP_SEMAPHORE) {
                /* Check if still signaled for more wakeups */
                if (!sync_is_signaled(sync_obj, 0)) {
                    break;  /* Object no longer signaled */
                }
            }

            /* Don't update prev since we removed this entry */
            wb = next;
            continue;
        }

        prev = wb;
        wb = next;
    }
}
