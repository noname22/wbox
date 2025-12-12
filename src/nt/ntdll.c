/*
 * WBOX NT Syscall Dispatcher
 * Handles SYSENTER calls from userspace, dispatches to syscall implementations
 */
#include "syscalls.h"
#include "win32k_syscalls.h"
#include "win32k_dispatcher.h"
#include "heap.h"
#include "../cpu/cpu.h"
#include "../cpu/mem.h"
#include "../vm/vm.h"
#include "../vm/paging.h"
#include "../user/user_callback.h"

#include <stdio.h>

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

    /* Debug: show all syscalls */
    static int syscall_count = 0;
    syscall_count++;
    if (syscall_num >= 0x1000 && syscall_num < 0x1400) {
        fprintf(stderr, "DEBUG[%d]: win32k syscall 0x%04X at PC=0x%08X\n",
               syscall_count, syscall_num, cpu_state.pc);
    }

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
             * Create a fake event handle - just for basic DLL init compatibility */
            vm_context_t *vm = vm_get_context();
            uint32_t event_handle_ptr = nt_read_arg(0);
            if (vm && event_handle_ptr) {
                uint32_t handle = handles_add(&vm->handles, HANDLE_TYPE_EVENT, -1);
                if (handle) {
                    uint32_t phys = paging_get_phys(&vm->paging, event_handle_ptr);
                    if (phys) {
                        mem_writel_phys(phys, handle);
                    }
                    syscall_return(STATUS_SUCCESS);
                    return 1;
                }
            }
            syscall_return(STATUS_INSUFFICIENT_RESOURCES);
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
            /* NtWaitForSingleObject(Handle, Alertable, Timeout)
             * For basic compatibility, return immediately with success */
            syscall_return(STATUS_SUCCESS);
            return 1;
        }

        case NtSetEvent: {
            /* NtSetEvent(EventHandle, PreviousState)
             * Set event to signaled state - stub just returns success */
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
            /* WndProc callback returned - EAX contains the result */
            user_callback_return(EAX);
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
