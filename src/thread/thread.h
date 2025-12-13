/*
 * WBOX Thread Infrastructure
 * Thread structures and management for multi-threading support
 */
#ifndef WBOX_THREAD_H
#define WBOX_THREAD_H

#include <stdint.h>
#include <stdbool.h>

/* Forward declarations */
struct vm_context;
struct wbox_scheduler;
union wbox_sync_object;

/*
 * Thread states (matching Windows KTHREAD_STATE)
 */
typedef enum {
    THREAD_STATE_INITIALIZED = 0,
    THREAD_STATE_READY,
    THREAD_STATE_RUNNING,
    THREAD_STATE_WAITING,
    THREAD_STATE_TERMINATED,
} wbox_thread_state_t;

/*
 * Wait types for NtWaitForMultipleObjects
 */
typedef enum {
    WAIT_TYPE_ALL = 0,    /* Wait until all objects signaled */
    WAIT_TYPE_ANY = 1,    /* Wait until any object signaled */
} wbox_wait_type_t;

/*
 * x86 segment descriptor (copied from cpu.h for thread context)
 */
typedef struct wbox_x86seg {
    uint32_t base;
    uint32_t limit;
    uint8_t  access;
    uint8_t  ar_high;
    uint16_t seg;
    uint32_t limit_low;
    uint32_t limit_high;
    int      checked;
} wbox_x86seg_t;

/*
 * CPU context for context switching
 * Must match cpu_state_t layout for proper save/restore
 */
typedef struct wbox_cpu_context {
    /* General purpose registers */
    uint32_t eax, ecx, edx, ebx;
    uint32_t esp, ebp, esi, edi;

    /* Instruction pointer and flags */
    uint32_t eip;
    uint16_t flags;
    uint16_t eflags;

    /* Segment registers */
    wbox_x86seg_t seg_cs;
    wbox_x86seg_t seg_ds;
    wbox_x86seg_t seg_es;
    wbox_x86seg_t seg_ss;
    wbox_x86seg_t seg_fs;
    wbox_x86seg_t seg_gs;

    /* FPU state */
    double ST[8];
    uint16_t npxs;
    uint16_t npxc;
    int TOP;
    uint8_t tag[8];
} wbox_cpu_context_t;

/*
 * Wait block - links a thread to an object it's waiting on
 */
typedef struct wbox_wait_block {
    struct wbox_thread *thread;         /* Thread that is waiting */
    void *object;                       /* Sync object being waited on */
    struct wbox_wait_block *next;       /* Next in object's wait list */
    uint32_t wait_key;                  /* Index for multi-object waits (return value) */
} wbox_wait_block_t;

/* Maximum objects for WaitForMultipleObjects */
#define THREAD_WAIT_OBJECTS 64

/* Default thread quantum (scheduler ticks) */
#define THREAD_DEFAULT_QUANTUM 6

/* Thread stack size */
#define THREAD_DEFAULT_STACK_SIZE (64 * 1024)  /* 64KB */

/* Main thread ID (matches Windows convention) */
#define WBOX_THREAD_ID 0x1004

/*
 * Thread structure
 */
typedef struct wbox_thread {
    /* Thread identification */
    uint32_t thread_id;
    uint32_t process_id;

    /* Thread state */
    wbox_thread_state_t state;

    /* CPU context (saved when not running) */
    wbox_cpu_context_t context;
    bool context_valid;

    /* Stack information */
    uint32_t stack_base;        /* Top of stack (high address) */
    uint32_t stack_limit;       /* Bottom of stack (low address) */
    uint32_t stack_size;

    /* TEB address (unique per thread) */
    uint32_t teb_addr;

    /* Wait state */
    uint32_t wait_status;                       /* NTSTATUS result of wait */
    uint64_t wait_timeout;                      /* Absolute timeout (100ns), 0 = infinite */
    wbox_wait_block_t wait_blocks[THREAD_WAIT_OBJECTS];
    int wait_count;                             /* Number of objects being waited on */
    wbox_wait_type_t wait_type;                 /* WaitAll or WaitAny */
    bool alertable;                             /* Can be alerted during wait */

    /* Scheduling */
    int8_t priority;                            /* -15 to +15 */
    int8_t base_priority;
    uint8_t quantum;                            /* Time slice remaining */
    uint8_t quantum_reset;                      /* Reset value for quantum */

    /* Exit state */
    uint32_t exit_code;
    bool terminated;

    /* Special thread flags */
    bool is_idle_thread;                    /* True if this is the system idle thread */

    /* Linked list pointers */
    struct wbox_thread *next;           /* Next in all-threads list */
    struct wbox_thread *ready_next;     /* Next in ready queue */

    /* Message queue (for GUI threads, NULL if not a GUI thread) */
    void *msg_queue;
} wbox_thread_t;

/*
 * Create a new thread
 * @param vm VM context
 * @param start_address Entry point for the thread
 * @param parameter Parameter passed to thread entry point
 * @param stack_size Stack size (0 for default)
 * @param suspended true to create in suspended state
 * @return Newly created thread or NULL on failure
 */
wbox_thread_t *thread_create(struct vm_context *vm, uint32_t start_address,
                             uint32_t parameter, uint32_t stack_size,
                             bool suspended);

/*
 * Create the initial/main thread (thread 0)
 * Uses the existing TEB at 0x7FFDF000
 * @param vm VM context
 * @return Main thread or NULL on failure
 */
wbox_thread_t *thread_create_main(struct vm_context *vm);

/*
 * Terminate a thread
 * @param thread Thread to terminate
 * @param exit_code Exit code
 */
void thread_terminate(wbox_thread_t *thread, uint32_t exit_code);

/*
 * Save CPU state to thread context
 * @param thread Thread to save context into
 */
void thread_save_context(wbox_thread_t *thread);

/*
 * Restore CPU state from thread context
 * Also updates FS segment base for TEB access
 * @param thread Thread to restore context from
 */
void thread_restore_context(wbox_thread_t *thread);

/*
 * Allocate a new TEB for a thread
 * @param vm VM context
 * @param thread_id Thread ID for the TEB
 * @return TEB virtual address or 0 on failure
 */
uint32_t thread_allocate_teb(struct vm_context *vm, uint32_t thread_id);

/*
 * Allocate stack for a thread
 * @param vm VM context
 * @param size Stack size in bytes
 * @param out_base Output: stack base (high address)
 * @param out_limit Output: stack limit (low address)
 * @return true on success
 */
bool thread_allocate_stack(struct vm_context *vm, uint32_t size,
                          uint32_t *out_base, uint32_t *out_limit);

/*
 * Get current thread ID
 * @return Current thread ID from scheduler
 */
uint32_t thread_get_current_id(void);

/*
 * Get current thread
 * @return Current thread pointer or NULL
 */
wbox_thread_t *thread_get_current(void);

/*
 * Create the system idle thread
 * This thread never executes guest code - it signals that the scheduler
 * should sleep because no other threads are ready.
 * @return Idle thread or NULL on failure
 */
wbox_thread_t *thread_create_idle(void);

#endif /* WBOX_THREAD_H */
