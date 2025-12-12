/*
 * WBOX Thread Management Implementation
 */
#include "thread.h"
#include "scheduler.h"
#include "../vm/vm.h"
#include "../vm/paging.h"
#include "../cpu/cpu.h"
#include "../cpu/mem.h"
#include "../process/process.h"
#include "../nt/sync.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* TEB base address for main thread */
#define MAIN_THREAD_TEB_ADDR    0x7FFDF000

/* TEB allocation goes downward from main thread TEB */
#define TEB_ALLOCATION_STEP     0x3000  /* 12KB spacing (TEB + guard pages) */

/* Stack allocation region */
#define STACK_REGION_BASE       0x04000000
#define STACK_REGION_END        0x08000000

/* Track next stack allocation address */
static uint32_t next_stack_addr = STACK_REGION_END;

/* Track next TEB address */
static uint32_t next_teb_addr = MAIN_THREAD_TEB_ADDR - TEB_ALLOCATION_STEP;

/* Next thread ID (starts after main thread) */
static uint32_t next_thread_id = WBOX_THREAD_ID + 4;

wbox_thread_t *thread_create_main(struct vm_context *vm)
{
    wbox_thread_t *thread = calloc(1, sizeof(wbox_thread_t));
    if (!thread) {
        return NULL;
    }

    thread->thread_id = WBOX_THREAD_ID;
    thread->process_id = WBOX_PROCESS_ID;
    thread->state = THREAD_STATE_RUNNING;
    thread->context_valid = false;  /* Context is in CPU, not saved */

    /* Use existing TEB */
    thread->teb_addr = MAIN_THREAD_TEB_ADDR;

    /* Stack info from TEB (already set up by process_init_teb) */
    thread->stack_base = VM_STACK_TOP;
    thread->stack_limit = VM_STACK_BASE;
    thread->stack_size = VM_STACK_TOP - VM_STACK_BASE;

    /* Scheduling defaults */
    thread->priority = 0;
    thread->base_priority = 0;
    thread->quantum = THREAD_DEFAULT_QUANTUM;
    thread->quantum_reset = THREAD_DEFAULT_QUANTUM;

    /* Wait state */
    thread->wait_status = 0;
    thread->wait_timeout = 0;
    thread->wait_count = 0;
    thread->alertable = false;

    /* Not terminated */
    thread->exit_code = 0;
    thread->terminated = false;

    /* No message queue initially */
    thread->msg_queue = NULL;

    /* Linked list */
    thread->next = NULL;
    thread->ready_next = NULL;

    return thread;
}

wbox_thread_t *thread_create(struct vm_context *vm, uint32_t start_address,
                             uint32_t parameter, uint32_t stack_size,
                             bool suspended)
{
    if (!vm) {
        return NULL;
    }

    if (stack_size == 0) {
        stack_size = THREAD_DEFAULT_STACK_SIZE;
    }

    wbox_thread_t *thread = calloc(1, sizeof(wbox_thread_t));
    if (!thread) {
        return NULL;
    }

    /* Assign thread ID */
    thread->thread_id = next_thread_id;
    next_thread_id += 4;
    thread->process_id = WBOX_PROCESS_ID;

    /* Allocate TEB */
    thread->teb_addr = thread_allocate_teb(vm, thread->thread_id);
    if (thread->teb_addr == 0) {
        free(thread);
        return NULL;
    }

    /* Allocate stack */
    if (!thread_allocate_stack(vm, stack_size, &thread->stack_base, &thread->stack_limit)) {
        /* TODO: deallocate TEB */
        free(thread);
        return NULL;
    }
    thread->stack_size = stack_size;

    /* Initialize TEB fields */
    uint32_t teb_phys = paging_get_phys(&vm->paging, thread->teb_addr);
    if (teb_phys) {
        mem_writel_phys(teb_phys + TEB_SELF, thread->teb_addr);
        mem_writel_phys(teb_phys + TEB_STACK_BASE, thread->stack_base);
        mem_writel_phys(teb_phys + TEB_STACK_LIMIT, thread->stack_limit);
        mem_writel_phys(teb_phys + TEB_PROCESS_ID, WBOX_PROCESS_ID);
        mem_writel_phys(teb_phys + TEB_THREAD_ID, thread->thread_id);
        mem_writel_phys(teb_phys + TEB_PEB_POINTER, VM_PEB_ADDR);
    }

    /* Initialize CPU context */
    thread->context_valid = true;

    /* Set up initial register state */
    memset(&thread->context, 0, sizeof(thread->context));

    /* Entry point */
    thread->context.eip = start_address;

    /* Stack: set up initial frame with parameter and return address */
    uint32_t esp = thread->stack_base - 8;  /* Room for param and return addr */

    /* Write parameter on stack */
    uint32_t stack_phys = paging_get_phys(&vm->paging, esp + 4);
    if (stack_phys) {
        mem_writel_phys(stack_phys, parameter);
    }

    /* Write fake return address (0 - will cause crash if thread returns) */
    stack_phys = paging_get_phys(&vm->paging, esp);
    if (stack_phys) {
        mem_writel_phys(stack_phys, 0);
    }

    thread->context.esp = esp;
    thread->context.ebp = 0;

    /* Copy segment state from current CPU (assume same for all threads) */
    thread->context.seg_cs.base = cpu_state.seg_cs.base;
    thread->context.seg_cs.limit = cpu_state.seg_cs.limit;
    thread->context.seg_cs.access = cpu_state.seg_cs.access;
    thread->context.seg_cs.ar_high = cpu_state.seg_cs.ar_high;
    thread->context.seg_cs.seg = cpu_state.seg_cs.seg;
    thread->context.seg_cs.limit_low = cpu_state.seg_cs.limit_low;
    thread->context.seg_cs.limit_high = cpu_state.seg_cs.limit_high;
    thread->context.seg_cs.checked = cpu_state.seg_cs.checked;

    thread->context.seg_ds = thread->context.seg_cs;
    thread->context.seg_ds.seg = cpu_state.seg_ds.seg;
    thread->context.seg_ds.base = cpu_state.seg_ds.base;

    thread->context.seg_es = thread->context.seg_ds;
    thread->context.seg_es.seg = cpu_state.seg_es.seg;

    thread->context.seg_ss = thread->context.seg_ds;
    thread->context.seg_ss.seg = cpu_state.seg_ss.seg;

    /* FS points to TEB - will be updated on context restore */
    thread->context.seg_fs = thread->context.seg_ds;
    thread->context.seg_fs.seg = cpu_state.seg_fs.seg;
    thread->context.seg_fs.base = thread->teb_addr;

    thread->context.seg_gs = thread->context.seg_ds;
    thread->context.seg_gs.seg = cpu_state.seg_gs.seg;

    /* Flags: interrupts enabled */
    thread->context.flags = I_FLAG;
    thread->context.eflags = 0;

    /* Scheduling */
    thread->priority = 0;
    thread->base_priority = 0;
    thread->quantum = THREAD_DEFAULT_QUANTUM;
    thread->quantum_reset = THREAD_DEFAULT_QUANTUM;

    /* Set initial state */
    thread->state = suspended ? THREAD_STATE_INITIALIZED : THREAD_STATE_READY;

    thread->exit_code = 0;
    thread->terminated = false;
    thread->msg_queue = NULL;
    thread->next = NULL;
    thread->ready_next = NULL;

    return thread;
}

void thread_terminate(wbox_thread_t *thread, uint32_t exit_code)
{
    if (!thread) {
        return;
    }

    thread->exit_code = exit_code;
    thread->terminated = true;
    thread->state = THREAD_STATE_TERMINATED;

    /* TODO: Signal thread handle to wake any waiters */
    /* TODO: Remove from wait lists if waiting */

    printf("Thread %u terminated with exit code %u\n",
           thread->thread_id, exit_code);
}

void thread_save_context(wbox_thread_t *thread)
{
    if (!thread) {
        return;
    }

    /* Save general purpose registers */
    thread->context.eax = EAX;
    thread->context.ecx = ECX;
    thread->context.edx = EDX;
    thread->context.ebx = EBX;
    thread->context.esp = ESP;
    thread->context.ebp = EBP;
    thread->context.esi = ESI;
    thread->context.edi = EDI;

    /* Save instruction pointer and flags */
    thread->context.eip = cpu_state.pc;
    thread->context.flags = cpu_state.flags;
    thread->context.eflags = cpu_state.eflags;

    /* Save segment registers */
    thread->context.seg_cs.base = cpu_state.seg_cs.base;
    thread->context.seg_cs.limit = cpu_state.seg_cs.limit;
    thread->context.seg_cs.access = cpu_state.seg_cs.access;
    thread->context.seg_cs.ar_high = cpu_state.seg_cs.ar_high;
    thread->context.seg_cs.seg = cpu_state.seg_cs.seg;
    thread->context.seg_cs.limit_low = cpu_state.seg_cs.limit_low;
    thread->context.seg_cs.limit_high = cpu_state.seg_cs.limit_high;
    thread->context.seg_cs.checked = cpu_state.seg_cs.checked;

    thread->context.seg_ds.base = cpu_state.seg_ds.base;
    thread->context.seg_ds.limit = cpu_state.seg_ds.limit;
    thread->context.seg_ds.access = cpu_state.seg_ds.access;
    thread->context.seg_ds.ar_high = cpu_state.seg_ds.ar_high;
    thread->context.seg_ds.seg = cpu_state.seg_ds.seg;
    thread->context.seg_ds.limit_low = cpu_state.seg_ds.limit_low;
    thread->context.seg_ds.limit_high = cpu_state.seg_ds.limit_high;
    thread->context.seg_ds.checked = cpu_state.seg_ds.checked;

    thread->context.seg_es.base = cpu_state.seg_es.base;
    thread->context.seg_es.limit = cpu_state.seg_es.limit;
    thread->context.seg_es.access = cpu_state.seg_es.access;
    thread->context.seg_es.ar_high = cpu_state.seg_es.ar_high;
    thread->context.seg_es.seg = cpu_state.seg_es.seg;
    thread->context.seg_es.limit_low = cpu_state.seg_es.limit_low;
    thread->context.seg_es.limit_high = cpu_state.seg_es.limit_high;
    thread->context.seg_es.checked = cpu_state.seg_es.checked;

    thread->context.seg_ss.base = cpu_state.seg_ss.base;
    thread->context.seg_ss.limit = cpu_state.seg_ss.limit;
    thread->context.seg_ss.access = cpu_state.seg_ss.access;
    thread->context.seg_ss.ar_high = cpu_state.seg_ss.ar_high;
    thread->context.seg_ss.seg = cpu_state.seg_ss.seg;
    thread->context.seg_ss.limit_low = cpu_state.seg_ss.limit_low;
    thread->context.seg_ss.limit_high = cpu_state.seg_ss.limit_high;
    thread->context.seg_ss.checked = cpu_state.seg_ss.checked;

    thread->context.seg_fs.base = cpu_state.seg_fs.base;
    thread->context.seg_fs.limit = cpu_state.seg_fs.limit;
    thread->context.seg_fs.access = cpu_state.seg_fs.access;
    thread->context.seg_fs.ar_high = cpu_state.seg_fs.ar_high;
    thread->context.seg_fs.seg = cpu_state.seg_fs.seg;
    thread->context.seg_fs.limit_low = cpu_state.seg_fs.limit_low;
    thread->context.seg_fs.limit_high = cpu_state.seg_fs.limit_high;
    thread->context.seg_fs.checked = cpu_state.seg_fs.checked;

    thread->context.seg_gs.base = cpu_state.seg_gs.base;
    thread->context.seg_gs.limit = cpu_state.seg_gs.limit;
    thread->context.seg_gs.access = cpu_state.seg_gs.access;
    thread->context.seg_gs.ar_high = cpu_state.seg_gs.ar_high;
    thread->context.seg_gs.seg = cpu_state.seg_gs.seg;
    thread->context.seg_gs.limit_low = cpu_state.seg_gs.limit_low;
    thread->context.seg_gs.limit_high = cpu_state.seg_gs.limit_high;
    thread->context.seg_gs.checked = cpu_state.seg_gs.checked;

    /* Save FPU state */
    for (int i = 0; i < 8; i++) {
        thread->context.ST[i] = cpu_state.ST[i];
        thread->context.tag[i] = cpu_state.tag[i];
    }
    thread->context.npxs = cpu_state.npxs;
    thread->context.npxc = cpu_state.npxc;
    thread->context.TOP = cpu_state.TOP;

    thread->context_valid = true;
}

void thread_restore_context(wbox_thread_t *thread)
{
    if (!thread || !thread->context_valid) {
        return;
    }

    /* Restore general purpose registers */
    EAX = thread->context.eax;
    ECX = thread->context.ecx;
    EDX = thread->context.edx;
    EBX = thread->context.ebx;
    ESP = thread->context.esp;
    EBP = thread->context.ebp;
    ESI = thread->context.esi;
    EDI = thread->context.edi;

    /* Restore instruction pointer and flags */
    cpu_state.pc = thread->context.eip;
    cpu_state.flags = thread->context.flags;
    cpu_state.eflags = thread->context.eflags;

    /* Restore segment registers */
    cpu_state.seg_cs.base = thread->context.seg_cs.base;
    cpu_state.seg_cs.limit = thread->context.seg_cs.limit;
    cpu_state.seg_cs.access = thread->context.seg_cs.access;
    cpu_state.seg_cs.ar_high = thread->context.seg_cs.ar_high;
    cpu_state.seg_cs.seg = thread->context.seg_cs.seg;
    cpu_state.seg_cs.limit_low = thread->context.seg_cs.limit_low;
    cpu_state.seg_cs.limit_high = thread->context.seg_cs.limit_high;
    cpu_state.seg_cs.checked = thread->context.seg_cs.checked;

    cpu_state.seg_ds.base = thread->context.seg_ds.base;
    cpu_state.seg_ds.limit = thread->context.seg_ds.limit;
    cpu_state.seg_ds.access = thread->context.seg_ds.access;
    cpu_state.seg_ds.ar_high = thread->context.seg_ds.ar_high;
    cpu_state.seg_ds.seg = thread->context.seg_ds.seg;
    cpu_state.seg_ds.limit_low = thread->context.seg_ds.limit_low;
    cpu_state.seg_ds.limit_high = thread->context.seg_ds.limit_high;
    cpu_state.seg_ds.checked = thread->context.seg_ds.checked;

    cpu_state.seg_es.base = thread->context.seg_es.base;
    cpu_state.seg_es.limit = thread->context.seg_es.limit;
    cpu_state.seg_es.access = thread->context.seg_es.access;
    cpu_state.seg_es.ar_high = thread->context.seg_es.ar_high;
    cpu_state.seg_es.seg = thread->context.seg_es.seg;
    cpu_state.seg_es.limit_low = thread->context.seg_es.limit_low;
    cpu_state.seg_es.limit_high = thread->context.seg_es.limit_high;
    cpu_state.seg_es.checked = thread->context.seg_es.checked;

    cpu_state.seg_ss.base = thread->context.seg_ss.base;
    cpu_state.seg_ss.limit = thread->context.seg_ss.limit;
    cpu_state.seg_ss.access = thread->context.seg_ss.access;
    cpu_state.seg_ss.ar_high = thread->context.seg_ss.ar_high;
    cpu_state.seg_ss.seg = thread->context.seg_ss.seg;
    cpu_state.seg_ss.limit_low = thread->context.seg_ss.limit_low;
    cpu_state.seg_ss.limit_high = thread->context.seg_ss.limit_high;
    cpu_state.seg_ss.checked = thread->context.seg_ss.checked;

    /* Restore FS with thread's TEB address */
    cpu_state.seg_fs.base = thread->teb_addr;  /* Key: TEB address */
    cpu_state.seg_fs.limit = thread->context.seg_fs.limit;
    cpu_state.seg_fs.access = thread->context.seg_fs.access;
    cpu_state.seg_fs.ar_high = thread->context.seg_fs.ar_high;
    cpu_state.seg_fs.seg = thread->context.seg_fs.seg;
    cpu_state.seg_fs.limit_low = thread->context.seg_fs.limit_low;
    cpu_state.seg_fs.limit_high = thread->context.seg_fs.limit_high;
    cpu_state.seg_fs.checked = thread->context.seg_fs.checked;

    cpu_state.seg_gs.base = thread->context.seg_gs.base;
    cpu_state.seg_gs.limit = thread->context.seg_gs.limit;
    cpu_state.seg_gs.access = thread->context.seg_gs.access;
    cpu_state.seg_gs.ar_high = thread->context.seg_gs.ar_high;
    cpu_state.seg_gs.seg = thread->context.seg_gs.seg;
    cpu_state.seg_gs.limit_low = thread->context.seg_gs.limit_low;
    cpu_state.seg_gs.limit_high = thread->context.seg_gs.limit_high;
    cpu_state.seg_gs.checked = thread->context.seg_gs.checked;

    /* Restore FPU state */
    for (int i = 0; i < 8; i++) {
        cpu_state.ST[i] = thread->context.ST[i];
        cpu_state.tag[i] = thread->context.tag[i];
    }
    cpu_state.npxs = thread->context.npxs;
    cpu_state.npxc = thread->context.npxc;
    cpu_state.TOP = thread->context.TOP;
}

uint32_t thread_allocate_teb(struct vm_context *vm, uint32_t thread_id)
{
    if (!vm) {
        return 0;
    }

    /* Get next TEB address */
    uint32_t teb_addr = next_teb_addr;
    next_teb_addr -= TEB_ALLOCATION_STEP;

    /* Check bounds */
    if (teb_addr < 0x7FF00000) {
        fprintf(stderr, "thread_allocate_teb: Out of TEB address space\n");
        return 0;
    }

    /* Allocate physical memory for TEB */
    uint32_t teb_phys = paging_alloc_phys(&vm->paging, PAGE_SIZE);
    if (teb_phys == 0) {
        fprintf(stderr, "thread_allocate_teb: Failed to allocate physical memory for TEB\n");
        return 0;
    }

    /* Map TEB page */
    if (paging_map_page(&vm->paging, teb_addr, teb_phys, PTE_USER | PTE_WRITABLE) != 0) {
        fprintf(stderr, "thread_allocate_teb: Failed to map TEB at 0x%08X\n", teb_addr);
        return 0;
    }

    /* Zero the TEB page */
    if (teb_phys) {
        for (uint32_t i = 0; i < 0x1000; i += 4) {
            mem_writel_phys(teb_phys + i, 0);
        }
    }

    printf("Allocated TEB for thread %u at 0x%08X\n", thread_id, teb_addr);
    return teb_addr;
}

bool thread_allocate_stack(struct vm_context *vm, uint32_t size,
                          uint32_t *out_base, uint32_t *out_limit)
{
    if (!vm || !out_base || !out_limit) {
        return false;
    }

    /* Round size up to page boundary */
    size = (size + 0xFFF) & ~0xFFF;

    /* Add guard page */
    uint32_t total_size = size + 0x1000;

    /* Check if we have room */
    if (next_stack_addr - total_size < STACK_REGION_BASE) {
        fprintf(stderr, "thread_allocate_stack: Out of stack address space\n");
        return false;
    }

    /* Allocate from top down */
    uint32_t stack_top = next_stack_addr;
    uint32_t stack_base = stack_top - size;
    uint32_t guard_page = stack_base - 0x1000;

    next_stack_addr = guard_page;

    /* Map stack pages */
    for (uint32_t addr = stack_base; addr < stack_top; addr += 0x1000) {
        /* Allocate physical page */
        uint32_t phys = paging_alloc_phys(&vm->paging, PAGE_SIZE);
        if (phys == 0) {
            fprintf(stderr, "thread_allocate_stack: Failed to allocate physical memory at 0x%08X\n", addr);
            return false;
        }
        if (paging_map_page(&vm->paging, addr, phys, PTE_USER | PTE_WRITABLE) != 0) {
            fprintf(stderr, "thread_allocate_stack: Failed to map page at 0x%08X\n", addr);
            return false;
        }
    }

    /* Guard page is not mapped (will cause fault on stack overflow) */

    *out_base = stack_top;
    *out_limit = stack_base;

    printf("Allocated stack: base=0x%08X, limit=0x%08X, size=%u\n",
           stack_top, stack_base, size);
    return true;
}

uint32_t thread_get_current_id(void)
{
    wbox_scheduler_t *sched = scheduler_get_instance();
    if (sched && sched->current_thread) {
        return sched->current_thread->thread_id;
    }
    return WBOX_THREAD_ID;  /* Default to main thread ID */
}

wbox_thread_t *thread_get_current(void)
{
    wbox_scheduler_t *sched = scheduler_get_instance();
    if (sched) {
        return sched->current_thread;
    }
    return NULL;
}
