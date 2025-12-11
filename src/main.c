/*
 * WBOX - Windows Box
 * A DOSBox-like emulator for 32-bit Windows 2k/XP era applications
 *
 * Phase 1: PE loader with syscall interception
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "cpu/cpu.h"
#include "cpu/mem.h"
#include "cpu/platform.h"
#include "cpu/codegen_public.h"
#include "vm/vm.h"
#include "vm/paging.h"
#include "process/process.h"
#include "nt/syscalls.h"
#include "nt/vfs_jail.h"

static void print_usage(const char *progname)
{
    fprintf(stderr, "WBOX - Windows Box\n");
    fprintf(stderr, "A DOSBox-like emulator for 32-bit Windows applications\n\n");
    fprintf(stderr, "Usage: %s [options] <executable.exe>\n\n", progname);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -C: <path>    Map C: drive to host directory\n");
    fprintf(stderr, "  -D: <path>    Map D: drive to host directory (etc. for A-Z)\n");
    fprintf(stderr, "  --jail <path> Legacy: Map C: drive to host directory\n");
    fprintf(stderr, "\nExamples:\n");
    fprintf(stderr, "  %s -C: ~/winxp ./tests/pe/hello.exe\n", progname);
    fprintf(stderr, "  %s -C: ~/winxp -D: ./tests/pe ./tests/pe/import_test.exe\n", progname);
    fprintf(stderr, "\nDLL resolution:\n");
    fprintf(stderr, "  ntdll.dll is automatically loaded from C:\\WINDOWS\\system32\n");
    fprintf(stderr, "\nCurrently supports:\n");
    fprintf(stderr, "  - Static 32-bit PE executables\n");
    fprintf(stderr, "  - Console applications (CUI)\n");
    fprintf(stderr, "  - DLL imports from ntdll.dll (requires C: drive mapping)\n");
}

/* Check if argument is a drive letter option like "-C:" */
static int is_drive_option(const char *arg)
{
    if (arg[0] == '-' &&
        ((arg[1] >= 'A' && arg[1] <= 'Z') || (arg[1] >= 'a' && arg[1] <= 'z')) &&
        arg[2] == ':' &&
        arg[3] == '\0') {
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    int ret = 0;
    const char *exe_path = NULL;
    char drive_mappings[26][4096] = {{0}};  /* A-Z drive paths */
    int num_drives = 0;

    /* Parse command line options */
    for (int i = 1; i < argc; i++) {
        if (is_drive_option(argv[i])) {
            /* -C: <path>, -D: <path>, etc. */
            char drive = toupper(argv[i][1]);
            int index = drive - 'A';
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -%c: requires a path argument\n", drive);
                return 1;
            }
            strncpy(drive_mappings[index], argv[++i], sizeof(drive_mappings[index]) - 1);
            num_drives++;
        } else if (strcmp(argv[i], "--jail") == 0) {
            /* Legacy --jail option maps to C: */
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --jail requires a path argument\n");
                return 1;
            }
            strncpy(drive_mappings[2], argv[++i], sizeof(drive_mappings[2]) - 1);  /* C: = index 2 */
            num_drives++;
        } else if (strcmp(argv[i], "--ntdll") == 0) {
            /* Legacy --ntdll option - now deprecated, ignore */
            fprintf(stderr, "Warning: --ntdll is deprecated, ntdll.dll is now loaded from VFS\n");
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                i++;  /* Skip the path argument */
            }
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        } else {
            exe_path = argv[i];
        }
    }

    if (!exe_path) {
        print_usage(argv[0]);
        return 1;
    }

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

    /* Initialize VFS with drive mappings */
    vfs_init(&vm.vfs_jail);
    for (int i = 0; i < 26; i++) {
        if (drive_mappings[i][0] != '\0') {
            char drive = 'A' + i;
            printf("Mapping drive %c: -> %s\n", drive, drive_mappings[i]);
            if (vfs_map_drive(&vm.vfs_jail, drive, drive_mappings[i]) != 0) {
                fprintf(stderr, "Failed to map drive %c: to '%s'\n", drive, drive_mappings[i]);
                ret = 1;
                goto cleanup;
            }
        }
    }

    /* Check if ntdll.dll can be found (only if any drive is mapped) */
    char ntdll_path[VFS_MAX_PATH] = {0};
    bool has_ntdll = false;
    if (num_drives > 0) {
        if (vfs_find_dll(&vm.vfs_jail, "ntdll.dll", ntdll_path) == 0) {
            printf("Found ntdll.dll at: %s\n", ntdll_path);
            has_ntdll = true;
        }
    }

    /* Load PE executable */
    printf("\nLoading PE executable...\n");
    if (has_ntdll) {
        /* Use new loader with DLL support */
        printf("Using DLL loader (ntdll: %s)\n", ntdll_path);
        if (vm_load_pe_with_dlls(&vm, exe_path, ntdll_path) != 0) {
            fprintf(stderr, "Failed to load PE with DLLs: %s\n", exe_path);
            ret = 1;
            goto cleanup;
        }
    } else {
        /* Use legacy loader (no DLL imports) */
        if (vm_load_pe(&vm, exe_path) != 0) {
            fprintf(stderr, "Failed to load PE: %s\n", exe_path);
            ret = 1;
            goto cleanup;
        }
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
