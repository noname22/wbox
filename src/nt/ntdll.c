/*
 * WBOX NT Syscall Dispatcher
 * Handles SYSENTER calls from userspace, dispatches to syscall implementations
 */
#include "syscalls.h"
#include "win32k_syscalls.h"
#include "win32k_dispatcher.h"
#include "heap.h"
#include "handles.h"
#include "sync.h"
#include "../cpu/cpu.h"
#include "../cpu/mem.h"
#include "../vm/vm.h"
#include "../vm/paging.h"
#include "../user/user_callback.h"
#include "../thread/thread.h"
#include "../thread/scheduler.h"

#include <stdio.h>
#include <stdbool.h>

/*
 * Check if PC is in the loader stub region
 */
static inline int is_loader_stub_pc(uint32_t pc)
{
    return (pc >= 0x7F000000 && pc < 0x7F010000);
}

/*
 * Read syscall argument from stack, handling both normal and loader stub calls.
 *
 * Normal NT syscalls (through ntdll's NtXxx functions):
 *   Stack: [ret_syscall_stub] [ret_NtXxx] [arg0] [arg1] ...
 *   Args start at ESP+8
 *
 * Loader stub syscalls:
 *   Stack: [ret_to_caller] [arg0] [arg1] ...
 *   Args start at ESP+4
 */
static inline uint32_t nt_read_arg(int index)
{
    int base_offset = is_loader_stub_pc(cpu_state.pc) ? 4 : 8;
    return readmemll(ESP + base_offset + (index * 4));
}

/*
 * Return from syscall to user mode
 * Sets EAX to return value and returns to user code
 *
 * For normal NT syscalls (through ntdll's NtXxx functions):
 *   The return address is on the user stack (ESP), placed there by the
 *   CALL [KUSD.SystemCall] instruction. We pop it and return there.
 *
 * For loader stub syscalls (PC in 0x7F000000-0x7F010000):
 *   The stub has its own RET N instruction. We just set EAX and let
 *   execution continue at the RET, which cleans up the stack properly.
 */
static void syscall_return(ntstatus_t status)
{
    /* Set return value */
    EAX = status;

    /* Check if this is a loader stub syscall */
    if (is_loader_stub_pc(cpu_state.pc)) {
        /* Don't modify ESP or pc - let the stub's RET N execute */
        return;
    }

    /* Normal NT syscall - pop return address and jump there */
    uint32_t return_addr = readmemll(ESP);
    ESP += 4;  /* Pop the return address */
    cpu_state.pc = return_addr;

    /* Segment registers are already set for Ring 3 from VM setup */
}

/*
 * Return from win32k syscall - preserves EAX set by handler.
 * Win32k syscalls (GDI/USER) return handles/values in EAX which the
 * handler sets directly, so we must NOT overwrite it here.
 */
static void win32k_syscall_return(void)
{
    /* EAX already set by the win32k handler */

    /* Check if this is a loader stub syscall */
    if (is_loader_stub_pc(cpu_state.pc)) {
        /* Don't modify ESP or pc - let the stub's RET N execute */
        return;
    }

    /* Read return address from user stack */
    uint32_t return_addr = readmemll(ESP);
    ESP += 4;  /* Pop the return address */
    cpu_state.pc = return_addr;
}

/*
 * Return from stdcall function (cleans up parameters)
 * Used for hooked functions like RtlAllocateHeap
 */
static void stdcall_return(uint32_t result, int num_params)
{
    /* Set return value */
    EAX = result;

    /* Read return address from stack */
    uint32_t return_addr = readmemll(ESP);

    /* Pop return address + parameters (stdcall convention) */
    ESP += 4 + (num_params * 4);

    cpu_state.pc = return_addr;
}

/*
 * NT syscall handler - called when SYSENTER is executed
 * Returns 1 to skip the normal SYSENTER processing
 */
int nt_syscall_handler(void)
{
    uint32_t syscall_num = EAX;
    ntstatus_t result;

    /* Dispatch to specific syscall handler */
    switch (syscall_num) {
        case NtClose:
            result = sys_NtClose();
            syscall_return(result);
            return 1;

        case NtCreateFile:
            result = sys_NtCreateFile();
            syscall_return(result);
            return 1;

        case NtOpenFile:
            result = sys_NtOpenFile();
            syscall_return(result);
            return 1;

        case NtReadFile:
            result = sys_NtReadFile();
            syscall_return(result);
            return 1;

        case NtWriteFile:
            result = sys_NtWriteFile();
            syscall_return(result);
            return 1;

        case NtTerminateProcess:
            result = sys_NtTerminateProcess();
            /* NtTerminateProcess exits, no return to user mode */
            return 1;

        case NtQueryPerformanceCounter:
            result = sys_NtQueryPerformanceCounter();
            syscall_return(result);
            return 1;

        case NtCreateEvent: {
            /* NtCreateEvent(EventHandle, DesiredAccess, ObjectAttributes, EventType, InitialState)
             * EventType: 0 = NotificationEvent (manual-reset), 1 = SynchronizationEvent (auto-reset) */
            vm_context_t *vm = vm_get_context();
            uint32_t event_handle_ptr = nt_read_arg(0);
            uint32_t desired_access = nt_read_arg(1);
            uint32_t obj_attr_ptr = nt_read_arg(2);
            uint32_t event_type = nt_read_arg(3);
            uint32_t initial_state = nt_read_arg(4);
            (void)desired_access; (void)obj_attr_ptr;

            if (!vm || !event_handle_ptr) {
                syscall_return(STATUS_INVALID_PARAMETER);
                return 1;
            }

            /* Validate event type */
            if (event_type > 1) {
                syscall_return(STATUS_INVALID_PARAMETER);
                return 1;
            }

            /* Create event object */
            wbox_disp_type_t dtype = (event_type == 0) ?
                WBOX_DISP_EVENT_NOTIFICATION : WBOX_DISP_EVENT_SYNCHRONIZATION;
            wbox_event_t *event = sync_create_event(dtype, initial_state != 0);
            if (!event) {
                syscall_return(STATUS_INSUFFICIENT_RESOURCES);
                return 1;
            }

            /* Add to handle table */
            uint32_t handle = handles_add_object(&vm->handles, HANDLE_TYPE_EVENT, event);
            if (!handle) {
                sync_free_object(event, HANDLE_TYPE_EVENT);
                syscall_return(STATUS_INSUFFICIENT_RESOURCES);
                return 1;
            }

            /* Write handle to output */
            uint32_t phys = paging_get_phys(&vm->paging, event_handle_ptr);
            if (phys) {
                mem_writel_phys(phys, handle);
            }

            syscall_return(STATUS_SUCCESS);
            return 1;
        }

        case NtQueryDefaultLocale: {
            /* NtQueryDefaultLocale(UserProfile, DefaultLocaleId)
             * Return US English locale (LCID 0x0409) */
            vm_context_t *vm = vm_get_context();
            uint32_t locale_ptr = nt_read_arg(1);
            if (vm && locale_ptr) {
                uint32_t phys = paging_get_phys(&vm->paging, locale_ptr);
                if (phys) {
                    mem_writel_phys(phys, 0x0409);  /* en-US */
                }
            }
            syscall_return(STATUS_SUCCESS);
            return 1;
        }

        case NtWaitForSingleObject: {
            /* NtWaitForSingleObject(Handle, Alertable, Timeout) */
            vm_context_t *vm = vm_get_context();
            uint32_t handle = nt_read_arg(0);
            uint32_t alertable = nt_read_arg(1);
            uint32_t timeout_ptr = nt_read_arg(2);

            if (!vm) {
                syscall_return(STATUS_INVALID_PARAMETER);
                return 1;
            }

            /* Get object from handle */
            handle_entry_t *entry = handles_get(&vm->handles, handle);
            if (!entry) {
                syscall_return(STATUS_INVALID_HANDLE);
                return 1;
            }

            /* Check handle type */
            if (entry->type != HANDLE_TYPE_EVENT &&
                entry->type != HANDLE_TYPE_SEMAPHORE &&
                entry->type != HANDLE_TYPE_MUTEX) {
                syscall_return(STATUS_OBJECT_TYPE_MISMATCH);
                return 1;
            }

            wbox_sync_object_t *obj = (wbox_sync_object_t *)entry->object_data;
            if (!obj) {
                /* Legacy handle without object data - return success for compatibility */
                syscall_return(STATUS_SUCCESS);
                return 1;
            }

            uint32_t current_thread_id = thread_get_current_id();

            /* Check if object is signaled */
            if (sync_is_signaled(obj, current_thread_id)) {
                sync_satisfy_wait(obj, current_thread_id);
                syscall_return(STATUS_SUCCESS);
                return 1;
            }

            /* Read timeout if provided */
            int64_t timeout_100ns = 0;
            bool has_timeout = false;
            if (timeout_ptr) {
                uint32_t phys = paging_get_phys(&vm->paging, timeout_ptr);
                if (phys) {
                    timeout_100ns = (int64_t)mem_readl_phys(phys) |
                                   ((int64_t)mem_readl_phys(phys + 4) << 32);
                    has_timeout = true;
                }
            }

            /* Zero timeout = poll, return TIMEOUT immediately if not signaled */
            if (has_timeout && timeout_100ns == 0) {
                syscall_return(STATUS_TIMEOUT);
                return 1;
            }

            /* Need to block - use scheduler if available */
            wbox_scheduler_t *sched = vm->scheduler;
            if (sched) {
                /* Calculate absolute timeout */
                uint64_t abs_timeout = 0;
                if (has_timeout) {
                    if (timeout_100ns < 0) {
                        /* Negative = relative timeout */
                        abs_timeout = scheduler_get_time_100ns() + (uint64_t)(-timeout_100ns);
                    } else {
                        /* Positive = absolute time (from Jan 1, 1601) - convert to our epoch */
                        abs_timeout = (uint64_t)timeout_100ns;
                    }
                }

                void *objects[1] = { obj };
                int types[1] = { entry->type };
                uint32_t result = scheduler_block_thread(sched, objects, types, 1,
                                                        WAIT_TYPE_ANY, abs_timeout,
                                                        alertable != 0);
                syscall_return(result);
                return 1;
            }

            /* No scheduler - return timeout for non-signaled objects */
            syscall_return(STATUS_TIMEOUT);
            return 1;
        }

        case NtWaitForMultipleObjects: {
            /* NtWaitForMultipleObjects(Count, Handles, WaitType, Alertable, Timeout)
             * WaitType: 0 = WaitAll, 1 = WaitAny */
            vm_context_t *vm = vm_get_context();
            uint32_t count = nt_read_arg(0);
            uint32_t handles_ptr = nt_read_arg(1);
            uint32_t wait_type = nt_read_arg(2);
            uint32_t alertable = nt_read_arg(3);
            uint32_t timeout_ptr = nt_read_arg(4);

            if (!vm || count == 0 || count > 64 || !handles_ptr) {
                syscall_return(STATUS_INVALID_PARAMETER);
                return 1;
            }

            uint32_t current_thread_id = thread_get_current_id();
            uint32_t handles_phys = paging_get_phys(&vm->paging, handles_ptr);
            if (!handles_phys) {
                syscall_return(STATUS_INVALID_PARAMETER);
                return 1;
            }

            /* Read timeout */
            int64_t timeout_100ns = 0;
            bool has_timeout = false;
            if (timeout_ptr) {
                uint32_t phys = paging_get_phys(&vm->paging, timeout_ptr);
                if (phys) {
                    timeout_100ns = (int64_t)mem_readl_phys(phys) |
                                   ((int64_t)mem_readl_phys(phys + 4) << 32);
                    has_timeout = true;
                }
            }

            /* Build object array */
            void *objects[64];
            int types[64];
            for (uint32_t i = 0; i < count; i++) {
                uint32_t h = mem_readl_phys(handles_phys + i * 4);
                handle_entry_t *entry = handles_get(&vm->handles, h);
                if (!entry || !entry->object_data) {
                    objects[i] = NULL;
                    types[i] = 0;
                } else {
                    objects[i] = entry->object_data;
                    types[i] = entry->type;
                }
            }

            /* Check for immediate satisfaction */
            if (wait_type == 1) {
                /* WaitAny: check if any object is signaled */
                for (uint32_t i = 0; i < count; i++) {
                    if (objects[i] && sync_is_signaled((wbox_sync_object_t *)objects[i], current_thread_id)) {
                        sync_satisfy_wait((wbox_sync_object_t *)objects[i], current_thread_id);
                        syscall_return(STATUS_WAIT_0 + i);
                        return 1;
                    }
                }
            } else {
                /* WaitAll: check if all objects are signaled */
                bool all_signaled = true;
                for (uint32_t i = 0; i < count; i++) {
                    if (objects[i] && !sync_is_signaled((wbox_sync_object_t *)objects[i], current_thread_id)) {
                        all_signaled = false;
                        break;
                    }
                }
                if (all_signaled) {
                    for (uint32_t i = 0; i < count; i++) {
                        if (objects[i]) {
                            sync_satisfy_wait((wbox_sync_object_t *)objects[i], current_thread_id);
                        }
                    }
                    syscall_return(STATUS_WAIT_0);
                    return 1;
                }
            }

            /* Zero timeout = poll */
            if (has_timeout && timeout_100ns == 0) {
                syscall_return(STATUS_TIMEOUT);
                return 1;
            }

            /* Need to block */
            wbox_scheduler_t *sched = vm->scheduler;
            if (sched) {
                uint64_t abs_timeout = 0;
                if (has_timeout && timeout_100ns < 0) {
                    abs_timeout = scheduler_get_time_100ns() + (uint64_t)(-timeout_100ns);
                } else if (has_timeout) {
                    abs_timeout = (uint64_t)timeout_100ns;
                }

                uint32_t result = scheduler_block_thread(sched, objects, types, (int)count,
                                                        wait_type == 0 ? WAIT_TYPE_ALL : WAIT_TYPE_ANY,
                                                        abs_timeout, alertable != 0);
                syscall_return(result);
                return 1;
            }

            syscall_return(STATUS_TIMEOUT);
            return 1;
        }

        case NtSetEvent: {
            /* NtSetEvent(EventHandle, PreviousState) */
            vm_context_t *vm = vm_get_context();
            uint32_t event_handle = nt_read_arg(0);
            uint32_t prev_state_ptr = nt_read_arg(1);

            if (!vm) {
                syscall_return(STATUS_INVALID_PARAMETER);
                return 1;
            }

            handle_entry_t *entry = handles_get(&vm->handles, event_handle);
            if (!entry || entry->type != HANDLE_TYPE_EVENT) {
                syscall_return(STATUS_INVALID_HANDLE);
                return 1;
            }

            wbox_event_t *event = (wbox_event_t *)entry->object_data;
            if (!event) {
                /* Legacy handle - just return success */
                syscall_return(STATUS_SUCCESS);
                return 1;
            }

            int32_t prev = event->header.signal_state;
            event->header.signal_state = 1;

            /* Return previous state if requested */
            if (prev_state_ptr) {
                uint32_t phys = paging_get_phys(&vm->paging, prev_state_ptr);
                if (phys) {
                    mem_writel_phys(phys, prev);
                }
            }

            /* Wake waiting threads */
            wbox_scheduler_t *sched = vm->scheduler;
            if (sched) {
                scheduler_signal_object(sched, event, HANDLE_TYPE_EVENT);
            }

            syscall_return(STATUS_SUCCESS);
            return 1;
        }

        case NtResetEvent: {
            /* NtResetEvent(EventHandle, PreviousState) */
            vm_context_t *vm = vm_get_context();
            uint32_t event_handle = nt_read_arg(0);
            uint32_t prev_state_ptr = nt_read_arg(1);

            if (!vm) {
                syscall_return(STATUS_INVALID_PARAMETER);
                return 1;
            }

            handle_entry_t *entry = handles_get(&vm->handles, event_handle);
            if (!entry || entry->type != HANDLE_TYPE_EVENT) {
                syscall_return(STATUS_INVALID_HANDLE);
                return 1;
            }

            wbox_event_t *event = (wbox_event_t *)entry->object_data;
            if (!event) {
                syscall_return(STATUS_SUCCESS);
                return 1;
            }

            int32_t prev = event->header.signal_state;
            event->header.signal_state = 0;

            if (prev_state_ptr) {
                uint32_t phys = paging_get_phys(&vm->paging, prev_state_ptr);
                if (phys) {
                    mem_writel_phys(phys, prev);
                }
            }

            syscall_return(STATUS_SUCCESS);
            return 1;
        }

        case NtClearEvent: {
            /* NtClearEvent(EventHandle) - same as NtResetEvent without PreviousState */
            vm_context_t *vm = vm_get_context();
            uint32_t event_handle = nt_read_arg(0);

            if (!vm) {
                syscall_return(STATUS_INVALID_PARAMETER);
                return 1;
            }

            handle_entry_t *entry = handles_get(&vm->handles, event_handle);
            if (!entry || entry->type != HANDLE_TYPE_EVENT) {
                syscall_return(STATUS_INVALID_HANDLE);
                return 1;
            }

            wbox_event_t *event = (wbox_event_t *)entry->object_data;
            if (event) {
                event->header.signal_state = 0;
            }

            syscall_return(STATUS_SUCCESS);
            return 1;
        }

        case NtPulseEvent: {
            /* NtPulseEvent(EventHandle, PreviousState)
             * Momentarily signal event, wake waiters, then reset */
            vm_context_t *vm = vm_get_context();
            uint32_t event_handle = nt_read_arg(0);
            uint32_t prev_state_ptr = nt_read_arg(1);

            if (!vm) {
                syscall_return(STATUS_INVALID_PARAMETER);
                return 1;
            }

            handle_entry_t *entry = handles_get(&vm->handles, event_handle);
            if (!entry || entry->type != HANDLE_TYPE_EVENT) {
                syscall_return(STATUS_INVALID_HANDLE);
                return 1;
            }

            wbox_event_t *event = (wbox_event_t *)entry->object_data;
            if (!event) {
                syscall_return(STATUS_SUCCESS);
                return 1;
            }

            int32_t prev = event->header.signal_state;
            event->header.signal_state = 1;

            /* Wake waiting threads */
            wbox_scheduler_t *sched = vm->scheduler;
            if (sched) {
                scheduler_signal_object(sched, event, HANDLE_TYPE_EVENT);
            }

            /* Reset the event */
            event->header.signal_state = 0;

            if (prev_state_ptr) {
                uint32_t phys = paging_get_phys(&vm->paging, prev_state_ptr);
                if (phys) {
                    mem_writel_phys(phys, prev);
                }
            }

            syscall_return(STATUS_SUCCESS);
            return 1;
        }

        case NtOpenKey: {
            /* NtOpenKey(KeyHandle, DesiredAccess, ObjectAttributes)
             * Registry not supported - return error to make code use fallbacks */
            syscall_return(STATUS_OBJECT_NAME_NOT_FOUND);
            return 1;
        }

        case NtQueryValueKey: {
            /* NtQueryValueKey(KeyHandle, ValueName, KeyValueInfoClass, KeyValueInfo, Length, ResultLength)
             * Registry not supported */
            syscall_return(STATUS_OBJECT_NAME_NOT_FOUND);
            return 1;
        }

        case NtAddAtom: {
            /* NtAddAtom(AtomName, Length, Atom) - stub that returns fake atom */
            static uint16_t next_atom = 0xC000;  /* Start above reserved atoms */
            vm_context_t *vm = vm_get_context();
            if (vm) {
                /* Read Atom output pointer from stack (3rd param) */
                uint32_t atom_ptr = nt_read_arg(2);
                if (atom_ptr != 0) {
                    uint32_t phys = paging_get_phys(&vm->paging, atom_ptr);
                    if (phys) {
                        mem_writew_phys(phys, next_atom++);
                    }
                }
            }
            syscall_return(STATUS_SUCCESS);
            return 1;
        }

        case WBOX_SYSCALL_DLL_INIT_DONE: {
            /* DLL entry point returned - signal completion */
            vm_context_t *vm = vm_get_context();
            if (vm) {
                vm->dll_init_done = 1;
            }
            cpu_exit_requested = 1;  /* Stop execution */
            return 1;
        }

        case WBOX_SYSCALL_WNDPROC_RETURN: {
            /* WndProc callback returned - ECX contains the result
             * (saved by the return stub before loading syscall number into EAX) */
            user_callback_return(ECX);
            return 1;
        }

        /* Heap function hooks */
        case WBOX_SYSCALL_HEAP_ALLOC: {
            /* RtlAllocateHeap(HeapHandle, Flags, Size) - stdcall, 3 params */
            vm_context_t *vm = vm_get_context();
            uint32_t heap_handle = readmemll(ESP + 4);
            uint32_t flags = readmemll(ESP + 8);
            uint32_t size = readmemll(ESP + 12);
            uint32_t res = 0;
            if (vm && vm->heap) {
                res = heap_alloc(vm->heap, vm, heap_handle, flags, size);
            }
            if (res == 0 && size > 0) {
                fprintf(stderr, "HEAP: Alloc FAILED heap=0x%X flags=0x%X size=%u\n",
                        heap_handle, flags, size);
            }
            stdcall_return(res, 3);
            return 1;
        }

        case WBOX_SYSCALL_HEAP_FREE: {
            /* RtlFreeHeap(HeapHandle, Flags, Ptr) - stdcall, 3 params */
            vm_context_t *vm = vm_get_context();
            uint32_t heap_handle = readmemll(ESP + 4);
            uint32_t flags = readmemll(ESP + 8);
            uint32_t ptr = readmemll(ESP + 12);
            uint32_t res = 0;
            if (vm && vm->heap) {
                res = heap_free(vm->heap, vm, heap_handle, flags, ptr) ? 1 : 0;
            }
            stdcall_return(res, 3);
            return 1;
        }

        case WBOX_SYSCALL_HEAP_REALLOC: {
            /* RtlReAllocateHeap(HeapHandle, Flags, Ptr, Size) - stdcall, 4 params */
            vm_context_t *vm = vm_get_context();
            uint32_t heap_handle = readmemll(ESP + 4);
            uint32_t flags = readmemll(ESP + 8);
            uint32_t ptr = readmemll(ESP + 12);
            uint32_t size = readmemll(ESP + 16);
            uint32_t res = 0;
            if (vm && vm->heap) {
                res = heap_realloc(vm->heap, vm, heap_handle, flags, ptr, size);
            }
            stdcall_return(res, 4);
            return 1;
        }

        case WBOX_SYSCALL_HEAP_SIZE: {
            /* RtlSizeHeap(HeapHandle, Flags, Ptr) - stdcall, 3 params */
            vm_context_t *vm = vm_get_context();
            uint32_t heap_handle = readmemll(ESP + 4);
            uint32_t flags = readmemll(ESP + 8);
            uint32_t ptr = readmemll(ESP + 12);
            uint32_t res = (uint32_t)-1;
            if (vm && vm->heap) {
                res = heap_size(vm->heap, vm, heap_handle, flags, ptr);
            }
            stdcall_return(res, 3);
            return 1;
        }

        /* String conversion syscalls */
        case WBOX_SYSCALL_MBSTR_TO_UNICODE: {
            /* RtlMultiByteToUnicodeN(UnicodeString, UnicodeSize, ResultSize, MbString, MbSize)
             * stdcall, 5 params */
            uint32_t unicode_str = readmemll(ESP + 4);   /* OUT PWCHAR */
            uint32_t unicode_size = readmemll(ESP + 8);  /* IN ULONG */
            uint32_t result_size_ptr = readmemll(ESP + 12); /* OUT PULONG (optional) */
            uint32_t mb_str = readmemll(ESP + 16);       /* IN PCSTR */
            uint32_t mb_size = readmemll(ESP + 20);      /* IN ULONG */

            vm_context_t *vm = vm_get_context();
            uint32_t chars_written = 0;

            if (vm) {
                /* Simple ASCII to Unicode conversion */
                uint32_t max_chars = unicode_size / 2;
                uint32_t chars_to_convert = mb_size < max_chars ? mb_size : max_chars;

                uint32_t mb_phys = paging_get_phys(&vm->paging, mb_str);
                uint32_t uni_phys = paging_get_phys(&vm->paging, unicode_str);

                if (mb_phys && uni_phys) {
                    for (uint32_t i = 0; i < chars_to_convert; i++) {
                        uint8_t ch = mem_readb_phys(mb_phys + i);
                        /* Write as 16-bit Unicode (simple ASCII extension) */
                        mem_writew_phys(uni_phys + i * 2, (uint16_t)ch);
                        chars_written++;
                    }
                }

                /* Write result size if pointer provided */
                if (result_size_ptr != 0) {
                    uint32_t result_phys = paging_get_phys(&vm->paging, result_size_ptr);
                    if (result_phys) {
                        mem_writel_phys(result_phys, chars_written * 2);
                    }
                }
            }

            stdcall_return(STATUS_SUCCESS, 5);
            return 1;
        }

        case WBOX_SYSCALL_UNICODE_TO_MBSTR: {
            /* RtlUnicodeToMultiByteN(MbString, MbSize, ResultSize, UnicodeString, UnicodeSize)
             * stdcall, 5 params */
            uint32_t mb_str = readmemll(ESP + 4);        /* OUT PCHAR */
            uint32_t mb_size = readmemll(ESP + 8);       /* IN ULONG */
            uint32_t result_size_ptr = readmemll(ESP + 12); /* OUT PULONG (optional) */
            uint32_t unicode_str = readmemll(ESP + 16);  /* IN PCWSTR */
            uint32_t unicode_size = readmemll(ESP + 20); /* IN ULONG (bytes) */

            vm_context_t *vm = vm_get_context();
            uint32_t bytes_written = 0;

            if (vm) {
                /* Simple Unicode to ASCII conversion */
                uint32_t unicode_chars = unicode_size / 2;
                uint32_t chars_to_convert = unicode_chars < mb_size ? unicode_chars : mb_size;

                uint32_t uni_phys = paging_get_phys(&vm->paging, unicode_str);
                uint32_t mb_phys = paging_get_phys(&vm->paging, mb_str);

                if (uni_phys && mb_phys) {
                    for (uint32_t i = 0; i < chars_to_convert; i++) {
                        uint16_t wch = mem_readw_phys(uni_phys + i * 2);
                        /* Truncate to ASCII (simple conversion) */
                        uint8_t ch = (wch < 256) ? (uint8_t)wch : '?';
                        mem_writeb_phys(mb_phys + i, ch);
                        bytes_written++;
                    }
                }

                /* Write result size if pointer provided */
                if (result_size_ptr != 0) {
                    uint32_t result_phys = paging_get_phys(&vm->paging, result_size_ptr);
                    if (result_phys) {
                        mem_writel_phys(result_phys, bytes_written);
                    }
                }
            }

            stdcall_return(STATUS_SUCCESS, 5);
            return 1;
        }

        case WBOX_SYSCALL_MBSTR_SIZE: {
            /* RtlMultiByteToUnicodeSize(UnicodeSize, MbString, MbSize)
             * stdcall, 3 params */
            uint32_t unicode_size_ptr = readmemll(ESP + 4); /* OUT PULONG */
            uint32_t mb_str = readmemll(ESP + 8);           /* IN PCSTR (unused) */
            uint32_t mb_size = readmemll(ESP + 12);         /* IN ULONG */

            (void)mb_str;  /* Unused for simple ASCII conversion */

            vm_context_t *vm = vm_get_context();
            if (vm && unicode_size_ptr != 0) {
                /* For ASCII, each byte becomes one 16-bit Unicode char */
                uint32_t result_phys = paging_get_phys(&vm->paging, unicode_size_ptr);
                if (result_phys) {
                    mem_writel_phys(result_phys, mb_size * 2);
                }
            }

            stdcall_return(STATUS_SUCCESS, 3);
            return 1;
        }

        case WBOX_SYSCALL_UNICODE_SIZE: {
            /* RtlUnicodeToMultiByteSize(MbSize, UnicodeString, UnicodeSize)
             * stdcall, 3 params */
            uint32_t mb_size_ptr = readmemll(ESP + 4);     /* OUT PULONG */
            uint32_t unicode_str = readmemll(ESP + 8);     /* IN PCWSTR (unused) */
            uint32_t unicode_size = readmemll(ESP + 12);   /* IN ULONG (bytes) */

            (void)unicode_str;  /* Unused for simple conversion */

            vm_context_t *vm = vm_get_context();
            if (vm && mb_size_ptr != 0) {
                /* For simple conversion, each 16-bit char becomes one byte */
                uint32_t result_phys = paging_get_phys(&vm->paging, mb_size_ptr);
                if (result_phys) {
                    mem_writel_phys(result_phys, unicode_size / 2);
                }
            }

            stdcall_return(STATUS_SUCCESS, 3);
            return 1;
        }

        case WBOX_SYSCALL_OEM_TO_UNICODE: {
            /* RtlOemToUnicodeN(UnicodeString, UnicodeSize, ResultSize, OemString, OemSize)
             * stdcall, 5 params - same signature as RtlMultiByteToUnicodeN
             * OEM and ANSI are the same for ASCII range (0-127), so we can use the same logic */
            uint32_t unicode_str = readmemll(ESP + 4);     /* OUT PWSTR */
            uint32_t unicode_size = readmemll(ESP + 8);    /* IN ULONG (max bytes) */
            uint32_t result_size_ptr = readmemll(ESP + 12);/* OUT PULONG (optional) */
            uint32_t oem_str = readmemll(ESP + 16);        /* IN PCSTR */
            uint32_t oem_size = readmemll(ESP + 20);       /* IN ULONG (bytes) */

            vm_context_t *vm = vm_get_context();
            uint32_t chars_converted = 0;

            if (vm && unicode_str != 0 && oem_str != 0) {
                /* Convert OEM to Unicode - for ASCII, just zero-extend each byte */
                uint32_t max_chars = unicode_size / 2;
                uint32_t oem_phys = paging_get_phys(&vm->paging, oem_str);
                uint32_t unicode_phys = paging_get_phys(&vm->paging, unicode_str);

                if (oem_phys && unicode_phys) {
                    for (uint32_t i = 0; i < oem_size && chars_converted < max_chars; i++) {
                        uint8_t ch = mem_readb_phys(oem_phys + i);
                        /* Write 16-bit Unicode character (little-endian) */
                        mem_writeb_phys(unicode_phys + chars_converted * 2, ch);
                        mem_writeb_phys(unicode_phys + chars_converted * 2 + 1, 0);
                        chars_converted++;
                    }
                }
            }

            /* Store result size if requested */
            if (vm && result_size_ptr != 0) {
                uint32_t result_phys = paging_get_phys(&vm->paging, result_size_ptr);
                if (result_phys) {
                    mem_writel_phys(result_phys, chars_converted * 2);
                }
            }

            stdcall_return(STATUS_SUCCESS, 5);
            return 1;
        }

        case WBOX_SYSCALL_UNICODE_TO_OEM: {
            /* RtlUnicodeToOemN(OemString, OemSize, ResultSize, UnicodeString, UnicodeSize)
             * stdcall, 5 params - same signature as RtlUnicodeToMultiByteN */
            uint32_t oem_str = readmemll(ESP + 4);         /* OUT PCHAR */
            uint32_t oem_size = readmemll(ESP + 8);        /* IN ULONG (max bytes) */
            uint32_t result_size_ptr = readmemll(ESP + 12);/* OUT PULONG (optional) */
            uint32_t unicode_str = readmemll(ESP + 16);    /* IN PCWSTR */
            uint32_t unicode_size = readmemll(ESP + 20);   /* IN ULONG (bytes) */

            vm_context_t *vm = vm_get_context();
            uint32_t bytes_written = 0;

            if (vm && oem_str != 0 && unicode_str != 0) {
                /* Convert Unicode to OEM - for ASCII range, just truncate to 8-bit */
                uint32_t chars_to_convert = unicode_size / 2;
                uint32_t oem_phys = paging_get_phys(&vm->paging, oem_str);
                uint32_t unicode_phys = paging_get_phys(&vm->paging, unicode_str);

                if (oem_phys && unicode_phys) {
                    for (uint32_t i = 0; i < chars_to_convert && bytes_written < oem_size; i++) {
                        uint16_t wch = mem_readb_phys(unicode_phys + i * 2) |
                                       (mem_readb_phys(unicode_phys + i * 2 + 1) << 8);
                        /* Map non-ASCII to '?' like Windows does */
                        uint8_t ch = (wch <= 0x7F) ? (uint8_t)wch : '?';
                        mem_writeb_phys(oem_phys + bytes_written, ch);
                        bytes_written++;
                    }
                }
            }

            /* Store result size if requested */
            if (vm && result_size_ptr != 0) {
                uint32_t result_phys = paging_get_phys(&vm->paging, result_size_ptr);
                if (result_phys) {
                    mem_writel_phys(result_phys, bytes_written);
                }
            }

            stdcall_return(STATUS_SUCCESS, 5);
            return 1;
        }

        case WBOX_SYSCALL_GET_CMD_LINE_A: {
            /* GetCommandLineA() - stdcall, 0 params
             * Returns pointer to ANSI command line string.
             * We use a static buffer allocated in guest memory. */
            vm_context_t *vm = vm_get_context();
            static uint32_t cmdline_a_addr = 0;

            if (vm && cmdline_a_addr == 0) {
                /* Allocate guest memory for ANSI command line - use loader heap area */
                /* Put it after TlsBitmap (0x7FFDE840) */
                cmdline_a_addr = 0x7FFDE860;
                /* Read the command line from ProcessParameters and convert to ANSI */
                uint32_t peb = vm->peb_addr;
                uint32_t peb_phys = paging_get_phys(&vm->paging, peb);
                if (peb_phys) {
                    uint32_t params = mem_readl_phys(peb_phys + 0x10);  /* PEB_PROCESS_PARAMETERS */
                    uint32_t params_phys = paging_get_phys(&vm->paging, params);
                    if (params_phys) {
                        /* CommandLine UNICODE_STRING at offset 0x40 */
                        uint32_t cmd_buffer = mem_readl_phys(params_phys + 0x40 + 4);  /* Buffer pointer */
                        uint16_t cmd_len = mem_readw_phys(params_phys + 0x40);  /* Length in bytes */
                        uint32_t cmd_phys = paging_get_phys(&vm->paging, cmd_buffer);
                        uint32_t dest_phys = paging_get_phys(&vm->paging, cmdline_a_addr);
                        if (cmd_phys && dest_phys && cmd_len > 0) {
                            /* Convert wide string to ANSI */
                            for (uint32_t i = 0; i < cmd_len / 2 && i < 255; i++) {
                                uint16_t wch = mem_readw_phys(cmd_phys + i * 2);
                                uint8_t ch = (wch <= 0x7F) ? (uint8_t)wch : '?';
                                mem_writeb_phys(dest_phys + i, ch);
                            }
                            mem_writeb_phys(dest_phys + cmd_len / 2, 0);  /* Null terminate */
                        } else if (dest_phys) {
                            /* Empty command line */
                            mem_writeb_phys(dest_phys, 0);
                        }
                    }
                }
            }

            /* Return pointer to static buffer */
            EAX = cmdline_a_addr;
            /* Pop return address and return (stdcall, 0 params) */
            uint32_t ret_addr = readmemll(ESP);
            ESP += 4;
            cpu_state.pc = ret_addr;
            return 1;
        }

        case WBOX_SYSCALL_GET_CMD_LINE_W: {
            /* GetCommandLineW() - stdcall, 0 params
             * Returns pointer to wide command line string.
             * The command line is already stored in ProcessParameters. */
            vm_context_t *vm = vm_get_context();
            uint32_t cmdline_w_addr = 0;

            if (vm) {
                /* Get command line from ProcessParameters */
                uint32_t peb = vm->peb_addr;
                uint32_t peb_phys = paging_get_phys(&vm->paging, peb);
                if (peb_phys) {
                    uint32_t params = mem_readl_phys(peb_phys + 0x10);  /* PEB_PROCESS_PARAMETERS */
                    uint32_t params_phys = paging_get_phys(&vm->paging, params);
                    if (params_phys) {
                        /* CommandLine UNICODE_STRING at offset 0x40 */
                        cmdline_w_addr = mem_readl_phys(params_phys + 0x40 + 4);  /* Buffer pointer */
                    }
                }
            }

            /* Return pointer to command line buffer in ProcessParameters */
            EAX = cmdline_w_addr;
            /* Pop return address and return (stdcall, 0 params) */
            uint32_t ret_addr = readmemll(ESP);
            ESP += 4;
            cpu_state.pc = ret_addr;
            return 1;
        }

        case NtCreateSection: {
            /* NtCreateSection(SectionHandle, DesiredAccess, ObjectAttributes, MaxSize, PageProtection, AllocationAttributes, FileHandle)
             * syscall, 7 params on stack
             *
             * This is used for file mapping and NLS data. We don't fully support sections,
             * so return STATUS_ACCESS_DENIED to let the caller handle it gracefully.
             * Many paths have fallback code when section creation fails. */
            syscall_return(STATUS_ACCESS_DENIED);
            return 1;
        }

        case NtOpenThreadToken:
        case NtOpenThreadTokenEx:
        case NtOpenProcessToken:
        case NtOpenProcessTokenEx: {
            /* NtOpenThreadToken[Ex] / NtOpenProcessToken[Ex]
             * Security token operations - we don't have a security subsystem
             * Return error so caller uses fallback path */
            syscall_return(STATUS_NO_TOKEN);
            return 1;
        }

        case NtQueryInformationProcess: {
            /* NtQueryInformationProcess(ProcessHandle, InfoClass, ProcessInfo, InfoLength, ReturnLength)
             * syscall, 5 params on stack */
            uint32_t process_handle = readmemll(ESP + 4);
            uint32_t info_class = readmemll(ESP + 8);
            uint32_t process_info = readmemll(ESP + 12);
            uint32_t info_length = readmemll(ESP + 16);
            uint32_t return_length_ptr = readmemll(ESP + 20);

            (void)process_handle;

            vm_context_t *vm = vm_get_context();

            /* Handle common information classes */
            switch (info_class) {
                case 0:  /* ProcessBasicInformation */
                    if (vm && process_info && info_length >= 24) {
                        /* PROCESS_BASIC_INFORMATION:
                         *   ExitStatus (4), PebBaseAddress (4), AffinityMask (4),
                         *   BasePriority (4), UniqueProcessId (4), InheritedFromUniqueProcessId (4) */
                        uint32_t phys = paging_get_phys(&vm->paging, process_info);
                        if (phys) {
                            mem_writel_phys(phys + 0, 0);           /* ExitStatus = still running */
                            mem_writel_phys(phys + 4, vm->peb_addr); /* PebBaseAddress */
                            mem_writel_phys(phys + 8, 1);           /* AffinityMask */
                            mem_writel_phys(phys + 12, 8);          /* BasePriority */
                            mem_writel_phys(phys + 16, 4096);       /* UniqueProcessId */
                            mem_writel_phys(phys + 20, 0);          /* InheritedFromUniqueProcessId */
                        }
                        if (return_length_ptr) {
                            uint32_t rlen_phys = paging_get_phys(&vm->paging, return_length_ptr);
                            if (rlen_phys) {
                                mem_writel_phys(rlen_phys, 24);
                            }
                        }
                    }
                    break;

                case 7:  /* ProcessDebugPort */
                    if (vm && process_info && info_length >= 4) {
                        uint32_t phys = paging_get_phys(&vm->paging, process_info);
                        if (phys) {
                            mem_writel_phys(phys, 0);  /* No debugger attached */
                        }
                        if (return_length_ptr) {
                            uint32_t rlen_phys = paging_get_phys(&vm->paging, return_length_ptr);
                            if (rlen_phys) {
                                mem_writel_phys(rlen_phys, 4);
                            }
                        }
                    }
                    break;

                case 31: /* ProcessDebugFlags */
                    if (vm && process_info && info_length >= 4) {
                        uint32_t phys = paging_get_phys(&vm->paging, process_info);
                        if (phys) {
                            mem_writel_phys(phys, 1);  /* PROCESS_DEBUG_INHERIT = not debugged */
                        }
                        if (return_length_ptr) {
                            uint32_t rlen_phys = paging_get_phys(&vm->paging, return_length_ptr);
                            if (rlen_phys) {
                                mem_writel_phys(rlen_phys, 4);
                            }
                        }
                    }
                    break;

                default:
                    /* Unknown info class - return success with zeroed data */
                    break;
            }

            syscall_return(STATUS_SUCCESS);
            return 1;
        }

        case NtQueryAttributesFile: {
            /* NtQueryAttributesFile(ObjectAttributes, FileInformation)
             * syscall, 2 params on stack
             * Used to check if a file exists and get basic info.
             * Return STATUS_OBJECT_NAME_NOT_FOUND - caller will handle gracefully. */
            syscall_return(STATUS_OBJECT_NAME_NOT_FOUND);
            return 1;
        }

        case NtQueryFullAttributesFile: {
            /* NtQueryFullAttributesFile(ObjectAttributes, FileInformation)
             * syscall, 2 params on stack
             * Similar to NtQueryAttributesFile but returns more info.
             * Return STATUS_OBJECT_NAME_NOT_FOUND - caller will handle gracefully. */
            syscall_return(STATUS_OBJECT_NAME_NOT_FOUND);
            return 1;
        }

        case NtCreateSemaphore: {
            /* NtCreateSemaphore(SemaphoreHandle, DesiredAccess, ObjectAttributes, InitialCount, MaximumCount)
             * syscall, 5 params */
            uint32_t handle_ptr = nt_read_arg(0);      /* OUT PHANDLE */
            uint32_t desired_access = nt_read_arg(1);  /* IN ACCESS_MASK */
            uint32_t obj_attrs = nt_read_arg(2);       /* IN POBJECT_ATTRIBUTES (optional) */
            int32_t initial_count = (int32_t)nt_read_arg(3);  /* IN LONG */
            int32_t max_count = (int32_t)nt_read_arg(4);      /* IN LONG */

            (void)desired_access;
            (void)obj_attrs;

            fprintf(stderr, "SYSCALL: NtCreateSemaphore(handle_ptr=0x%X, initial=%d, max=%d)\n",
                    handle_ptr, initial_count, max_count);

            /* Validate parameters */
            if (max_count <= 0 || initial_count < 0 || initial_count > max_count) {
                syscall_return(STATUS_INVALID_PARAMETER);
                return 1;
            }

            /* Create semaphore object */
            wbox_semaphore_t *sem = sync_create_semaphore(initial_count, max_count);
            if (!sem) {
                syscall_return(STATUS_INSUFFICIENT_RESOURCES);
                return 1;
            }

            /* Add to handle table */
            vm_context_t *vm = vm_get_context();
            if (!vm) {
                sync_free_object((wbox_sync_object_t *)sem, HANDLE_TYPE_SEMAPHORE);
                syscall_return(STATUS_INTERNAL_ERROR);
                return 1;
            }

            uint32_t handle = handles_add_object(&vm->handles, HANDLE_TYPE_SEMAPHORE, sem);
            if (handle == INVALID_HANDLE_VALUE) {
                sync_free_object((wbox_sync_object_t *)sem, HANDLE_TYPE_SEMAPHORE);
                syscall_return(STATUS_INSUFFICIENT_RESOURCES);
                return 1;
            }

            /* Write handle to caller */
            if (handle_ptr) {
                uint32_t phys = paging_get_phys(&vm->paging, handle_ptr);
                if (phys) {
                    mem_writel_phys(phys, handle);
                }
            }

            fprintf(stderr, "  -> Created semaphore handle 0x%X (initial=%d, max=%d)\n",
                    handle, initial_count, max_count);
            syscall_return(STATUS_SUCCESS);
            return 1;
        }

        case NtReleaseSemaphore: {
            /* NtReleaseSemaphore(SemaphoreHandle, ReleaseCount, PreviousCount)
             * syscall, 3 params */
            uint32_t handle = nt_read_arg(0);                /* IN HANDLE */
            int32_t release_count = (int32_t)nt_read_arg(1); /* IN LONG */
            uint32_t prev_count_ptr = nt_read_arg(2);        /* OUT PLONG (optional) */

            fprintf(stderr, "SYSCALL: NtReleaseSemaphore(handle=0x%X, count=%d)\n",
                    handle, release_count);

            if (release_count <= 0) {
                syscall_return(STATUS_INVALID_PARAMETER);
                return 1;
            }

            vm_context_t *vm = vm_get_context();
            if (!vm) {
                syscall_return(STATUS_INTERNAL_ERROR);
                return 1;
            }

            /* Get semaphore from handle */
            handle_entry_t *entry = handles_get(&vm->handles, handle);
            if (!entry || entry->type != HANDLE_TYPE_SEMAPHORE || !entry->object_data) {
                syscall_return(STATUS_INVALID_HANDLE);
                return 1;
            }

            wbox_semaphore_t *sem = (wbox_semaphore_t *)entry->object_data;
            int32_t previous = sem->header.signal_state;

            /* Check for limit overflow */
            if (sem->header.signal_state + release_count > sem->limit) {
                syscall_return(STATUS_SEMAPHORE_LIMIT_EXCEEDED);
                return 1;
            }

            /* Write previous count if requested */
            if (prev_count_ptr) {
                uint32_t phys = paging_get_phys(&vm->paging, prev_count_ptr);
                if (phys) {
                    mem_writel_phys(phys, (uint32_t)previous);
                }
            }

            /* Increment semaphore count */
            sem->header.signal_state += release_count;

            /* Wake waiting threads */
            wbox_scheduler_t *sched = scheduler_get_instance();
            if (sched) {
                scheduler_signal_object(sched, sem, HANDLE_TYPE_SEMAPHORE);
            }

            fprintf(stderr, "  -> Released, prev=%d, new=%d\n", previous, sem->header.signal_state);
            syscall_return(STATUS_SUCCESS);
            return 1;
        }

        case NtCreateMutant: {
            /* NtCreateMutant(MutantHandle, DesiredAccess, ObjectAttributes, InitialOwner)
             * syscall, 4 params */
            uint32_t handle_ptr = nt_read_arg(0);      /* OUT PHANDLE */
            uint32_t desired_access = nt_read_arg(1);  /* IN ACCESS_MASK */
            uint32_t obj_attrs = nt_read_arg(2);       /* IN POBJECT_ATTRIBUTES (optional) */
            uint32_t initial_owner = nt_read_arg(3);   /* IN BOOLEAN */

            (void)desired_access;
            (void)obj_attrs;

            fprintf(stderr, "SYSCALL: NtCreateMutant(handle_ptr=0x%X, initial_owner=%d)\n",
                    handle_ptr, initial_owner);

            vm_context_t *vm = vm_get_context();
            if (!vm) {
                syscall_return(STATUS_INTERNAL_ERROR);
                return 1;
            }

            /* Get current thread ID if initial owner requested */
            uint32_t owner_id = 0;
            if (initial_owner) {
                wbox_scheduler_t *sched = scheduler_get_instance();
                if (sched && sched->current_thread) {
                    owner_id = sched->current_thread->thread_id;
                }
            }

            /* Create mutant object */
            wbox_mutant_t *mutant = sync_create_mutant(initial_owner != 0, owner_id);
            if (!mutant) {
                syscall_return(STATUS_INSUFFICIENT_RESOURCES);
                return 1;
            }

            /* Add to handle table */
            uint32_t handle = handles_add_object(&vm->handles, HANDLE_TYPE_MUTANT, mutant);
            if (handle == INVALID_HANDLE_VALUE) {
                sync_free_object((wbox_sync_object_t *)mutant, HANDLE_TYPE_MUTANT);
                syscall_return(STATUS_INSUFFICIENT_RESOURCES);
                return 1;
            }

            /* Write handle to caller */
            if (handle_ptr) {
                uint32_t phys = paging_get_phys(&vm->paging, handle_ptr);
                if (phys) {
                    mem_writel_phys(phys, handle);
                }
            }

            fprintf(stderr, "  -> Created mutant handle 0x%X (owner=%u)\n", handle, owner_id);
            syscall_return(STATUS_SUCCESS);
            return 1;
        }

        case NtReleaseMutant: {
            /* NtReleaseMutant(MutantHandle, PreviousCount)
             * syscall, 2 params */
            uint32_t handle = nt_read_arg(0);           /* IN HANDLE */
            uint32_t prev_count_ptr = nt_read_arg(1);   /* OUT PLONG (optional) */

            fprintf(stderr, "SYSCALL: NtReleaseMutant(handle=0x%X)\n", handle);

            vm_context_t *vm = vm_get_context();
            if (!vm) {
                syscall_return(STATUS_INTERNAL_ERROR);
                return 1;
            }

            /* Get mutant from handle */
            handle_entry_t *entry = handles_get(&vm->handles, handle);
            if (!entry || entry->type != HANDLE_TYPE_MUTANT || !entry->object_data) {
                syscall_return(STATUS_INVALID_HANDLE);
                return 1;
            }

            wbox_mutant_t *mutant = (wbox_mutant_t *)entry->object_data;

            /* Verify current thread owns the mutant */
            uint32_t current_thread_id = 0;
            wbox_scheduler_t *sched = scheduler_get_instance();
            if (sched && sched->current_thread) {
                current_thread_id = sched->current_thread->thread_id;
            }

            if (mutant->owner_thread_id != current_thread_id) {
                syscall_return(STATUS_MUTANT_NOT_OWNED);
                return 1;
            }

            /* Get previous state (recursion count, negated for acquired state) */
            int32_t previous = mutant->recursion_count > 0 ? -(int32_t)mutant->recursion_count : 1;

            /* Write previous count if requested */
            if (prev_count_ptr) {
                uint32_t phys = paging_get_phys(&vm->paging, prev_count_ptr);
                if (phys) {
                    mem_writel_phys(phys, (uint32_t)previous);
                }
            }

            /* Decrement recursion count */
            mutant->recursion_count--;

            if (mutant->recursion_count == 0) {
                /* Release the mutant */
                mutant->owner_thread_id = 0;
                mutant->header.signal_state = 1;  /* Now signaled */

                /* Wake waiting threads */
                if (sched) {
                    scheduler_signal_object(sched, mutant, HANDLE_TYPE_MUTANT);
                }
            }

            fprintf(stderr, "  -> Released, recursion_count=%d\n", mutant->recursion_count);
            syscall_return(STATUS_SUCCESS);
            return 1;
        }

        case NtDelayExecution: {
            /* NtDelayExecution(Alertable, DelayInterval)
             * syscall, 2 params
             * DelayInterval is a pointer to LARGE_INTEGER (negative = relative, positive = absolute) */
            uint32_t alertable = nt_read_arg(0);        /* IN BOOLEAN */
            uint32_t interval_ptr = nt_read_arg(1);     /* IN PLARGE_INTEGER */

            fprintf(stderr, "SYSCALL: NtDelayExecution(alertable=%d, interval_ptr=0x%X)\n",
                    alertable, interval_ptr);

            vm_context_t *vm = vm_get_context();
            if (!vm) {
                syscall_return(STATUS_INTERNAL_ERROR);
                return 1;
            }

            /* Read the delay interval (100ns units) */
            int64_t delay_100ns = 0;
            if (interval_ptr) {
                uint32_t phys = paging_get_phys(&vm->paging, interval_ptr);
                if (phys) {
                    uint32_t lo = mem_readl_phys(phys);
                    int32_t hi = (int32_t)mem_readl_phys(phys + 4);
                    delay_100ns = ((int64_t)hi << 32) | lo;
                }
            }

            /* Calculate absolute timeout */
            uint64_t timeout;
            if (delay_100ns == 0) {
                /* Zero delay - just yield */
                syscall_return(STATUS_SUCCESS);
                return 1;
            } else if (delay_100ns < 0) {
                /* Negative = relative time */
                uint64_t now = scheduler_get_time_100ns();
                timeout = now + (uint64_t)(-delay_100ns);
            } else {
                /* Positive = absolute time (we don't support this well, treat as relative) */
                uint64_t now = scheduler_get_time_100ns();
                timeout = now + (uint64_t)delay_100ns;
            }

            /* Block the thread with no objects, just a timeout */
            wbox_scheduler_t *sched = scheduler_get_instance();
            if (sched) {
                uint32_t status = scheduler_block_thread(sched, NULL, NULL, 0,
                                                         WAIT_TYPE_ANY, timeout, alertable != 0);
                syscall_return(status);
            } else {
                /* No scheduler - just return */
                syscall_return(STATUS_SUCCESS);
            }
            return 1;
        }

        case NtQuerySystemInformation: {
            /* NtQuerySystemInformation(SystemInformationClass, SystemInformation, Length, ReturnLength)
             * Return basic info or error depending on class */
            uint32_t info_class = nt_read_arg(0);
            uint32_t info_ptr = nt_read_arg(1);
            uint32_t length = nt_read_arg(2);
            uint32_t ret_len_ptr = nt_read_arg(3);
            (void)info_ptr; (void)length;

            /* Write 0 to return length if provided */
            if (ret_len_ptr) {
                vm_context_t *vm = vm_get_context();
                if (vm) {
                    uint32_t phys = paging_get_phys(&vm->paging, ret_len_ptr);
                    if (phys) {
                        mem_writel_phys(phys, 0);
                    }
                }
            }

            /* Return STATUS_INFO_LENGTH_MISMATCH for most queries */
            (void)info_class;
            syscall_return(0xC0000004);  /* STATUS_INFO_LENGTH_MISMATCH */
            return 1;
        }

        case NtQueryVolumeInformationFile: {
            /* NtQueryVolumeInformationFile(FileHandle, IoStatusBlock, FsInformation, Length, FsInfoClass)
             * Return error - volume info not supported */
            syscall_return(STATUS_NOT_IMPLEMENTED);
            return 1;
        }

        case NtCallbackReturn: {
            /* NtCallbackReturn(Result, ResultLength, Status)
             * Called by user32 when returning from a kernel callback (e.g. WndProc)
             * We extract the result and signal the callback as complete */
            uint32_t result_ptr    = nt_read_arg(0);  /* Pointer to result data */
            uint32_t result_length = nt_read_arg(1);  /* Size of result data */
            uint32_t status        = nt_read_arg(2);  /* NTSTATUS */

            fprintf(stderr, "SYSCALL: NtCallbackReturn(result=0x%X, len=%d, status=0x%X)\n",
                    result_ptr, result_length, status);

            /* The result value is in the WINDOWPROC_CALLBACK_ARGUMENTS.Result field
             * which user32 filled in before calling NtCallbackReturn.
             * We read it from result_ptr if provided, otherwise the callback mechanism
             * should have stored it already.
             *
             * For simplicity, get the result from Result field in arguments structure.
             * result_ptr points to the callback arguments block that was passed to
             * User32CallWindowProcFromKernel.
             */
            uint32_t result_value = 0;
            if (result_ptr != 0 && result_length >= 32) {
                /* Result is at offset 28 (WPCB_RESULT) in WINDOWPROC_CALLBACK_ARGUMENTS */
                result_value = readmemll(result_ptr + 28);
                fprintf(stderr, "SYSCALL: NtCallbackReturn read result=0x%X from args\n", result_value);
            }

            /* Mark the callback as completed */
            user_callback_return(result_value);

            /* Return to caller - but the callback loop will detect completion and restore state */
            syscall_return(status);
            return 1;
        }

        case NtCreateKeyedEvent: {
            /* NtCreateKeyedEvent(KeyedEventHandle, DesiredAccess, ObjectAttributes, Flags)
             * Create a keyed event object - return a fake handle */
            static uint32_t next_keyed_handle = 0x200;
            uint32_t handle_ptr = nt_read_arg(0);
            if (handle_ptr) {
                vm_context_t *vm = vm_get_context();
                if (vm) {
                    uint32_t phys = paging_get_phys(&vm->paging, handle_ptr);
                    if (phys) {
                        mem_writel_phys(phys, next_keyed_handle++);
                    }
                }
            }
            syscall_return(STATUS_SUCCESS);
            return 1;
        }

        case NtCreateThread: {
            /* NtCreateThread(ThreadHandle, DesiredAccess, ObjectAttributes, ProcessHandle,
             *                ClientId, ThreadContext, InitialTeb, CreateSuspended)
             * syscall, 8 params
             * ThreadContext points to CONTEXT structure with initial register state
             * InitialTeb points to INITIAL_TEB structure with stack info
             */
            uint32_t handle_ptr = nt_read_arg(0);       /* OUT PHANDLE */
            uint32_t desired_access = nt_read_arg(1);   /* IN ACCESS_MASK */
            uint32_t obj_attrs = nt_read_arg(2);        /* IN POBJECT_ATTRIBUTES (optional) */
            uint32_t process_handle = nt_read_arg(3);   /* IN HANDLE */
            uint32_t client_id_ptr = nt_read_arg(4);    /* OUT PCLIENT_ID */
            uint32_t context_ptr = nt_read_arg(5);      /* IN PCONTEXT */
            uint32_t initial_teb_ptr = nt_read_arg(6);  /* IN PINITIAL_TEB */
            uint32_t create_suspended = nt_read_arg(7); /* IN BOOLEAN */

            (void)desired_access;
            (void)obj_attrs;
            (void)process_handle;  /* We only have one process */
            (void)initial_teb_ptr; /* We allocate our own stack */

            fprintf(stderr, "SYSCALL: NtCreateThread(context=0x%X, suspended=%d)\n",
                    context_ptr, create_suspended);

            vm_context_t *vm = vm_get_context();
            if (!vm) {
                syscall_return(STATUS_INTERNAL_ERROR);
                return 1;
            }

            /* Read start address (EIP) and parameter (EAX) from CONTEXT */
            uint32_t start_address = 0;
            uint32_t parameter = 0;
            if (context_ptr) {
                uint32_t ctx_phys = paging_get_phys(&vm->paging, context_ptr);
                if (ctx_phys) {
                    /* CONTEXT offsets (x86):
                     * +0xB0: Esp
                     * +0xB4: Ebp
                     * +0xB8: Eip
                     * +0xBC: SegCs
                     * +0xC0: EFlags
                     * +0xA4: Eax (parameter typically passed in Eax)
                     */
                    start_address = mem_readl_phys(ctx_phys + 0xB8);  /* Eip */
                    parameter = mem_readl_phys(ctx_phys + 0xA4);      /* Eax */
                }
            }

            if (start_address == 0) {
                fprintf(stderr, "  -> ERROR: No start address in context\n");
                syscall_return(STATUS_INVALID_PARAMETER);
                return 1;
            }

            /* Create the thread */
            wbox_thread_t *thread = thread_create(vm, start_address, parameter, 0,
                                                  create_suspended != 0);
            if (!thread) {
                fprintf(stderr, "  -> ERROR: thread_create failed\n");
                syscall_return(STATUS_INSUFFICIENT_RESOURCES);
                return 1;
            }

            /* Add to scheduler */
            wbox_scheduler_t *sched = scheduler_get_instance();
            if (sched) {
                scheduler_add_thread(sched, thread);
            }

            /* Create handle for the thread */
            uint32_t handle = handles_add_object(&vm->handles, HANDLE_TYPE_THREAD, thread);
            if (handle == INVALID_HANDLE_VALUE) {
                /* Thread was already added to scheduler, can't really undo cleanly */
                syscall_return(STATUS_INSUFFICIENT_RESOURCES);
                return 1;
            }

            /* Write handle to caller */
            if (handle_ptr) {
                uint32_t phys = paging_get_phys(&vm->paging, handle_ptr);
                if (phys) {
                    mem_writel_phys(phys, handle);
                }
            }

            /* Write CLIENT_ID (ProcessId, ThreadId) */
            if (client_id_ptr) {
                uint32_t phys = paging_get_phys(&vm->paging, client_id_ptr);
                if (phys) {
                    mem_writel_phys(phys, thread->process_id);      /* UniqueProcess */
                    mem_writel_phys(phys + 4, thread->thread_id);   /* UniqueThread */
                }
            }

            fprintf(stderr, "  -> Created thread %u, handle 0x%X, start=0x%X\n",
                    thread->thread_id, handle, start_address);
            syscall_return(STATUS_SUCCESS);
            return 1;
        }

        case NtTerminateThread: {
            /* NtTerminateThread(ThreadHandle, ExitStatus)
             * syscall, 2 params */
            uint32_t handle = nt_read_arg(0);       /* IN HANDLE (NULL = current thread) */
            uint32_t exit_status = nt_read_arg(1);  /* IN NTSTATUS */

            fprintf(stderr, "SYSCALL: NtTerminateThread(handle=0x%X, status=0x%X)\n",
                    handle, exit_status);

            vm_context_t *vm = vm_get_context();
            if (!vm) {
                syscall_return(STATUS_INTERNAL_ERROR);
                return 1;
            }

            wbox_scheduler_t *sched = scheduler_get_instance();
            wbox_thread_t *thread = NULL;

            if (handle == 0 || handle == 0xFFFFFFFF) {
                /* NULL or -1 means current thread */
                if (sched) {
                    thread = sched->current_thread;
                }
            } else {
                /* Get thread from handle */
                handle_entry_t *entry = handles_get(&vm->handles, handle);
                if (entry && entry->type == HANDLE_TYPE_THREAD) {
                    thread = (wbox_thread_t *)entry->object_data;
                }
            }

            if (!thread) {
                syscall_return(STATUS_INVALID_HANDLE);
                return 1;
            }

            /* Terminate the thread */
            thread_terminate(thread, exit_status);

            /* Remove from scheduler */
            if (sched) {
                scheduler_remove_thread(sched, thread);

                /* If we terminated the current thread, need to switch */
                if (thread == sched->current_thread) {
                    scheduler_switch(sched);
                }

                /* If no threads left, request VM exit */
                if (sched->all_threads == NULL) {
                    vm_request_exit(vm, exit_status);
                }
            }

            fprintf(stderr, "  -> Terminated thread %u\n", thread->thread_id);
            syscall_return(STATUS_SUCCESS);
            return 1;
        }

        case NtQueryInformationThread: {
            /* NtQueryInformationThread(ThreadHandle, InfoClass, Info, Length, ReturnLength)
             * Return basic thread info */
            uint32_t handle = nt_read_arg(0);
            uint32_t info_class = nt_read_arg(1);
            uint32_t info_ptr = nt_read_arg(2);
            uint32_t length = nt_read_arg(3);
            uint32_t ret_len_ptr = nt_read_arg(4);

            fprintf(stderr, "SYSCALL: NtQueryInformationThread(handle=0x%X, class=%d)\n",
                    handle, info_class);

            vm_context_t *vm = vm_get_context();
            if (!vm) {
                syscall_return(STATUS_INTERNAL_ERROR);
                return 1;
            }

            wbox_thread_t *thread = NULL;
            if (handle == 0xFFFFFFFE) {  /* NtCurrentThread() pseudo-handle */
                wbox_scheduler_t *sched = scheduler_get_instance();
                if (sched) {
                    thread = sched->current_thread;
                }
            } else {
                handle_entry_t *entry = handles_get(&vm->handles, handle);
                if (entry && entry->type == HANDLE_TYPE_THREAD) {
                    thread = (wbox_thread_t *)entry->object_data;
                }
            }

            if (!thread) {
                /* Return success with empty data for compatibility */
                if (ret_len_ptr) {
                    uint32_t phys = paging_get_phys(&vm->paging, ret_len_ptr);
                    if (phys) mem_writel_phys(phys, 0);
                }
                syscall_return(STATUS_SUCCESS);
                return 1;
            }

            /* ThreadBasicInformation (class 0) */
            if (info_class == 0 && info_ptr && length >= 28) {
                uint32_t phys = paging_get_phys(&vm->paging, info_ptr);
                if (phys) {
                    mem_writel_phys(phys + 0, STATUS_SUCCESS);     /* ExitStatus */
                    mem_writel_phys(phys + 4, thread->teb_addr);   /* TebBaseAddress */
                    mem_writel_phys(phys + 8, thread->process_id); /* ClientId.UniqueProcess */
                    mem_writel_phys(phys + 12, thread->thread_id); /* ClientId.UniqueThread */
                    mem_writel_phys(phys + 16, 0);                 /* AffinityMask */
                    mem_writel_phys(phys + 20, thread->priority);  /* Priority */
                    mem_writel_phys(phys + 24, thread->base_priority); /* BasePriority */
                }
                if (ret_len_ptr) {
                    uint32_t rphys = paging_get_phys(&vm->paging, ret_len_ptr);
                    if (rphys) mem_writel_phys(rphys, 28);
                }
            }

            syscall_return(STATUS_SUCCESS);
            return 1;
        }

        case NtResumeThread: {
            /* NtResumeThread(ThreadHandle, SuspendCount)
             * Resume a suspended thread */
            uint32_t handle = nt_read_arg(0);
            uint32_t suspend_count_ptr = nt_read_arg(1);

            fprintf(stderr, "SYSCALL: NtResumeThread(handle=0x%X)\n", handle);

            vm_context_t *vm = vm_get_context();
            if (!vm) {
                syscall_return(STATUS_INTERNAL_ERROR);
                return 1;
            }

            wbox_thread_t *thread = NULL;
            handle_entry_t *entry = handles_get(&vm->handles, handle);
            if (entry && entry->type == HANDLE_TYPE_THREAD) {
                thread = (wbox_thread_t *)entry->object_data;
            }

            if (!thread) {
                syscall_return(STATUS_INVALID_HANDLE);
                return 1;
            }

            /* Return previous suspend count (we don't track this properly, assume 1) */
            if (suspend_count_ptr) {
                uint32_t phys = paging_get_phys(&vm->paging, suspend_count_ptr);
                if (phys) mem_writel_phys(phys, 1);
            }

            /* If thread is in INITIALIZED state, make it ready */
            if (thread->state == THREAD_STATE_INITIALIZED) {
                thread->state = THREAD_STATE_READY;
                wbox_scheduler_t *sched = scheduler_get_instance();
                if (sched) {
                    scheduler_add_ready(sched, thread);
                }
            }

            fprintf(stderr, "  -> Resumed thread %u\n", thread->thread_id);
            syscall_return(STATUS_SUCCESS);
            return 1;
        }

        case NtSuspendThread: {
            /* NtSuspendThread(ThreadHandle, PreviousSuspendCount)
             * Suspend a thread */
            uint32_t handle = nt_read_arg(0);
            uint32_t prev_count_ptr = nt_read_arg(1);

            fprintf(stderr, "SYSCALL: NtSuspendThread(handle=0x%X)\n", handle);

            vm_context_t *vm = vm_get_context();
            if (!vm) {
                syscall_return(STATUS_INTERNAL_ERROR);
                return 1;
            }

            wbox_thread_t *thread = NULL;
            handle_entry_t *entry = handles_get(&vm->handles, handle);
            if (entry && entry->type == HANDLE_TYPE_THREAD) {
                thread = (wbox_thread_t *)entry->object_data;
            }

            if (!thread) {
                syscall_return(STATUS_INVALID_HANDLE);
                return 1;
            }

            /* Return previous suspend count (we don't track this properly, assume 0) */
            if (prev_count_ptr) {
                uint32_t phys = paging_get_phys(&vm->paging, prev_count_ptr);
                if (phys) mem_writel_phys(phys, 0);
            }

            /* Remove from ready queue if ready */
            wbox_scheduler_t *sched = scheduler_get_instance();
            if (thread->state == THREAD_STATE_READY && sched) {
                scheduler_remove_ready(sched, thread);
            }

            /* Set to initialized (suspended) state */
            if (thread->state == THREAD_STATE_READY ||
                thread->state == THREAD_STATE_RUNNING) {
                thread->state = THREAD_STATE_INITIALIZED;
            }

            /* If suspending current thread, switch away */
            if (sched && thread == sched->current_thread) {
                scheduler_switch(sched);
            }

            fprintf(stderr, "  -> Suspended thread %u\n", thread->thread_id);
            syscall_return(STATUS_SUCCESS);
            return 1;
        }

        default:
            /* Check if this is a win32k syscall */
            if (syscall_num >= WIN32K_SYSCALL_BASE) {
                /* Win32k syscall - dispatch to GDI/USER handler.
                 * These handlers set EAX themselves with handles/values,
                 * so we use win32k_syscall_return() which preserves EAX. */
                fprintf(stderr, "SYSCALL: win32k 0x%X (%s)\n", syscall_num, syscall_get_name(syscall_num));
                win32k_syscall_dispatch(syscall_num);
                fprintf(stderr, "SYSCALL: win32k 0x%X returned 0x%X\n", syscall_num, EAX);
                win32k_syscall_return();
                return 1;
            }

            /* Unimplemented NT syscall - print info and exit */
            printf("\n=== UNIMPLEMENTED SYSCALL ===\n");
            printf("Number: 0x%03X (%d)\n", syscall_num, syscall_num);
            printf("Name:   %s\n", syscall_get_name(syscall_num));
            printf("\nRegisters:\n");
            printf("  EAX=%08X (syscall number)\n", EAX);
            printf("  EDX=%08X (args pointer)\n", EDX);
            printf("  ESP=%08X EBP=%08X\n", ESP, EBP);

            /* Exit with STATUS_NOT_IMPLEMENTED */
            EAX = STATUS_NOT_IMPLEMENTED;
            vm_context_t *vm = vm_get_context();
            if (vm) {
                vm_request_exit(vm, STATUS_NOT_IMPLEMENTED);
            }
            cpu_exit_requested = 1;
            return 1;
    }
}

/*
 * Software interrupt handler
 * Handles INT 0x03 (breakpoint) and INT 0x2D (debug service)
 * Returns 1 if handled, 0 to process normally
 */
static int nt_softint_handler(int num)
{
    switch (num) {
        case 0x03:
            /* INT 3 (breakpoint) - just continue execution
             * This is used by debug code and should be a no-op when no debugger is attached */
            return 1;

        case 0x2D:
            /* INT 0x2D (debug service) - Windows kernel debugger interface
             * When no debugger is attached, return STATUS_BREAKPOINT (0x80000003) in EAX
             * The caller expects this status when not being debugged */
            EAX = 0x80000003;  /* STATUS_BREAKPOINT */
            return 1;

        default:
            /* Let other interrupts be processed normally */
            return 0;
    }
}

/*
 * Install the syscall handler
 * This sets up the SYSENTER callback in the CPU emulator
 */
void nt_install_syscall_handler(void)
{
    printf("Installing NT syscall handler\n");
    sysenter_callback = nt_syscall_handler;
    softint_callback = nt_softint_handler;
}

/*
 * Remove the syscall handler
 */
void nt_remove_syscall_handler(void)
{
    sysenter_callback = NULL;
    softint_callback = NULL;
}
