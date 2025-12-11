/*
 * WBOX Virtual Machine Manager
 * Sets up protected mode execution environment for PE binaries
 */
#ifndef WBOX_VM_H
#define WBOX_VM_H

#include <stdint.h>
#include <stdbool.h>
#include "paging.h"
#include "../pe/pe_loader.h"
#include "../nt/handles.h"
#include "../nt/vfs_jail.h"
#include "../gdi/display.h"

/* Forward declaration for loader */
struct loader_context;

/* Forward declaration for heap */
struct heap_state;

/* Memory layout constants */
#define VM_PHYS_MEM_SIZE       (256 * 1024 * 1024)  /* 256MB physical memory */
#define VM_KERNEL_BASE         0x80000000           /* Kernel space starts at 2GB */
#define VM_USER_STACK_TOP      0x7FFEFFF0           /* Top of user stack */
#define VM_USER_STACK_SIZE     (64 * 1024)          /* 64KB stack */
#define VM_TEB_ADDR            0x7FFDF000           /* Thread Environment Block */
#define VM_PEB_ADDR            0x7FFDE000           /* Process Environment Block */
#define VM_KUSD_ADDR           0x7FFE0000           /* KUSER_SHARED_DATA */
#define VM_DEFAULT_IMAGE_BASE  0x00400000           /* Default PE load address */

/* GDT selector values */
#define VM_SEL_NULL            0x00
#define VM_SEL_KERNEL_CODE     0x08
#define VM_SEL_KERNEL_DATA     0x10
#define VM_SEL_USER_CODE       0x1B  /* 0x18 | RPL=3 */
#define VM_SEL_USER_DATA       0x23  /* 0x20 | RPL=3 */
#define VM_SEL_TEB             0x3B  /* 0x38 | RPL=3 (FS for TEB) */

/* GDT entry count */
#define VM_GDT_ENTRIES         8

/* Initial EFLAGS value */
#define VM_INITIAL_EFLAGS      0x00000202  /* IF=1 */

/* Virtual machine context */
typedef struct vm_context {
    /* Paging context */
    paging_context_t paging;

    /* Memory layout info */
    uint32_t stack_top;         /* User stack top (grows down) */
    uint32_t stack_base;        /* User stack base */
    uint32_t teb_addr;          /* TEB virtual address */
    uint32_t peb_addr;          /* PEB virtual address */

    /* Loaded PE info */
    uint32_t image_base;        /* Loaded image base */
    uint32_t entry_point;       /* Entry point virtual address */
    uint32_t size_of_image;     /* PE image size */

    /* GDT physical address */
    uint32_t gdt_phys;
    uint32_t gdt_virt;
    uint16_t gdt_limit;

    /* IDT physical address */
    uint32_t idt_phys;
    uint32_t idt_virt;
    uint16_t idt_limit;

    /* Exit flag */
    volatile int exit_requested;
    uint32_t exit_code;

    /* DLL initialization state */
    volatile int dll_init_done;      /* Set by special syscall when DllMain returns */
    uint32_t dll_init_stub_addr;     /* Address of DLL init return stub */

    /* Handle table for files, etc. */
    handle_table_t handles;

    /* Virtual filesystem jail */
    vfs_jail_t vfs_jail;

    /* Loader context (optional, for DLL loading) */
    struct loader_context *loader;

    /* Heap context (for RtlAllocateHeap interception) */
    struct heap_state *heap;

    /* Display context (for GUI applications) */
    display_context_t display;
    bool gui_mode;
} vm_context_t;

/*
 * Initialize VM context
 * Sets up physical memory and paging structures
 */
int vm_init(vm_context_t *vm);

/*
 * Load a PE file into the VM
 * Parses PE, maps sections, applies relocations
 * Returns 0 on success, -1 on failure
 */
int vm_load_pe(vm_context_t *vm, const char *path);

/*
 * Set up GDT for protected mode
 * Creates flat 4GB segments for Ring 0/3 + TEB segment
 */
int vm_setup_gdt(vm_context_t *vm);

/*
 * Set up IDT for interrupt handling
 * Minimal IDT with INT 2E handler for syscalls
 */
int vm_setup_idt(vm_context_t *vm);

/*
 * Enable paging and set CR3
 */
void vm_setup_paging(vm_context_t *vm);

/*
 * Configure SYSENTER MSRs
 */
void vm_setup_sysenter(vm_context_t *vm);

/*
 * Set up CPU registers for Ring 3 entry
 * Sets CS/DS/ES/SS/FS, EIP, ESP, EFLAGS
 */
void vm_setup_cpu_state(vm_context_t *vm);

/*
 * Start VM execution
 * Enters Ring 3 and runs until syscall or exit
 */
void vm_start(vm_context_t *vm);

/*
 * Request VM exit (called from syscall handler)
 */
void vm_request_exit(vm_context_t *vm, uint32_t code);

/*
 * Call a DLL entry point (DllMain)
 * entry_point: VA of DLL's entry point
 * base_va: DLL base address (passed as hModule)
 * reason: DLL_PROCESS_ATTACH=1, DLL_PROCESS_DETACH=0
 * Returns: TRUE if DllMain returned TRUE, FALSE otherwise
 */
int vm_call_dll_entry(vm_context_t *vm, uint32_t entry_point, uint32_t base_va, uint32_t reason);

/*
 * Initialize all loaded DLLs by calling their entry points
 * Must be called after vm_load_pe_with_dlls and vm_setup_cpu_state
 * Returns 0 on success, -1 on failure
 */
int vm_init_dlls(vm_context_t *vm);

/*
 * Get the global VM context (for syscall handler)
 */
vm_context_t *vm_get_context(void);

/*
 * Debug: dump VM state
 */
void vm_dump_state(vm_context_t *vm);

/*
 * Load PE with DLL support using the loader subsystem
 * This allocates and initializes the loader context internally
 * ntdll_path may be NULL if no ntdll.dll imports are expected
 * Returns 0 on success, -1 on failure
 */
int vm_load_pe_with_dlls(vm_context_t *vm, const char *exe_path,
                         const char *ntdll_path);

/*
 * Translate virtual address to physical address
 * Returns 0 if translation fails
 */
uint32_t vm_va_to_phys(vm_context_t *vm, uint32_t va);

#endif /* WBOX_VM_H */
