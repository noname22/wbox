/*
 * WBOX - Windows Box
 * A DOSBox-like emulator for 32-bit Windows 2k/XP era applications
 *
 * Phase 1: PE loader with syscall interception
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpu/cpu.h"
#include "cpu/mem.h"
#include "cpu/platform.h"
#include "cpu/codegen_public.h"
#include "vm/vm.h"
#include "vm/paging.h"
#include "process/process.h"
#include "nt/syscalls.h"

static void print_usage(const char *progname)
{
    fprintf(stderr, "WBOX - Windows Box\n");
    fprintf(stderr, "A DOSBox-like emulator for 32-bit Windows applications\n\n");
    fprintf(stderr, "Usage: %s <executable.exe>\n", progname);
    fprintf(stderr, "\nCurrently supports:\n");
    fprintf(stderr, "  - Static 32-bit PE executables\n");
    fprintf(stderr, "  - Console applications (CUI)\n");
    fprintf(stderr, "\nThe emulator will load the PE, run until the first syscall,\n");
    fprintf(stderr, "print the syscall information, and exit.\n");
}

int main(int argc, char *argv[])
{
    int ret = 0;

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *exe_path = argv[1];

    printf("=== WBOX - Windows Box ===\n");
    printf("Loading: %s\n\n", exe_path);

    /* Initialize memory system */
    printf("Initializing memory (%d MB)...\n", VM_PHYS_MEM_SIZE / (1024 * 1024));
    mem_size = VM_PHYS_MEM_SIZE;
    mem_init();
    mem_reset();

    /* Initialize CPU */
    printf("Initializing CPU (Pentium Pro)...\n");
    cpu_f = cpu_get_family("pentiumpro");
    if (!cpu_f) {
        fprintf(stderr, "Failed to find Pentium CPU family\n");
        ret = 1;
        goto cleanup;
    }
    cpu = 0;  /* Use first CPU in family */
    cpu_set();
    codegen_init();
    resetx86();

    /* Create VM context */
    vm_context_t vm;
    if (vm_init(&vm) != 0) {
        fprintf(stderr, "Failed to initialize VM\n");
        ret = 1;
        goto cleanup;
    }

    /* Load PE executable */
    printf("\nLoading PE executable...\n");
    if (vm_load_pe(&vm, exe_path) != 0) {
        fprintf(stderr, "Failed to load PE: %s\n", exe_path);
        ret = 1;
        goto cleanup;
    }

    /* Set up GDT */
    printf("\nSetting up protected mode...\n");
    if (vm_setup_gdt(&vm) != 0) {
        fprintf(stderr, "Failed to set up GDT\n");
        ret = 1;
        goto cleanup;
    }

    /* Set up IDT */
    if (vm_setup_idt(&vm) != 0) {
        fprintf(stderr, "Failed to set up IDT\n");
        ret = 1;
        goto cleanup;
    }

    /* Set up paging */
    vm_setup_paging(&vm);

    /* Set up SYSENTER MSRs */
    vm_setup_sysenter(&vm);

    /* Initialize TEB/PEB */
    printf("\nInitializing process structures...\n");
    process_init_teb(&vm);
    process_init_peb(&vm);

    /* Set up CPU state for Ring 3 entry */
    vm_setup_cpu_state(&vm);

    /* Install syscall handler */
    nt_install_syscall_handler();

    /* Dump initial state */
    printf("\n");
    vm_dump_state(&vm);

    /* Start execution */
    printf("\nStarting execution at 0x%08X...\n", vm.entry_point);
    vm_start(&vm);

    /* Print final state */
    printf("\nFinal CPU state:\n");
    printf("  EAX=%08X (return value / syscall result)\n", EAX);
    printf("  Exit code: 0x%08X\n", vm.exit_code);

cleanup:
    nt_remove_syscall_handler();
    mem_close();

    return ret;
}
