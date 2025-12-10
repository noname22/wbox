#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>

#include "mooreader.h"
#include "cpu.h"
#include "mem.h"
#include "codegen_public.h"

#ifndef TEST_DATA_DIR
#define TEST_DATA_DIR "ext/80386/v1_ex_real_mode"
#endif

extern uint8_t *ram;
extern uint32_t cr2, cr3, cr4;
extern uint32_t dr[8];
extern cpu_family_t *cpu_f;
extern int cpu;

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

static void cpu_test_init(void)
{
    mem_init();

    cpu_f = cpu_get_family("i386dx");
    if (!cpu_f) {
        fprintf(stderr, "Failed to find i386dx CPU family\n");
        exit(1);
    }
    cpu = 0;

    cpu_set();
    codegen_init();
    resetx86();
}

static void cpu_test_cleanup(void)
{
    mem_close();
}

static void set_reg32(moo_reg32_t reg, uint32_t value)
{
    switch (reg) {
    case MOO_REG32_CR0:
        cr0 = value;
        break;
    case MOO_REG32_CR3:
        cr3 = value;
        break;
    case MOO_REG32_EAX:
        EAX = value;
        break;
    case MOO_REG32_EBX:
        EBX = value;
        break;
    case MOO_REG32_ECX:
        ECX = value;
        break;
    case MOO_REG32_EDX:
        EDX = value;
        break;
    case MOO_REG32_ESI:
        ESI = value;
        break;
    case MOO_REG32_EDI:
        EDI = value;
        break;
    case MOO_REG32_EBP:
        EBP = value;
        break;
    case MOO_REG32_ESP:
        ESP = value;
        break;
    case MOO_REG32_CS:
        cpu_state.seg_cs.seg = value & 0xFFFF;
        cpu_state.seg_cs.base = (value & 0xFFFF) << 4;
        cpu_state.seg_cs.limit_low = 0;
        cpu_state.seg_cs.limit_high = 0xFFFF;
        cpu_state.seg_cs.access = 0x82;  /* Present, readable code segment */
        break;
    case MOO_REG32_DS:
        cpu_state.seg_ds.seg = value & 0xFFFF;
        cpu_state.seg_ds.base = (value & 0xFFFF) << 4;
        cpu_state.seg_ds.limit_low = 0;
        cpu_state.seg_ds.limit_high = 0xFFFF;
        cpu_state.seg_ds.access = 0x82;  /* Present, readable data segment */
        break;
    case MOO_REG32_ES:
        cpu_state.seg_es.seg = value & 0xFFFF;
        cpu_state.seg_es.base = (value & 0xFFFF) << 4;
        cpu_state.seg_es.limit_low = 0;
        cpu_state.seg_es.limit_high = 0xFFFF;
        cpu_state.seg_es.access = 0x82;
        break;
    case MOO_REG32_FS:
        cpu_state.seg_fs.seg = value & 0xFFFF;
        cpu_state.seg_fs.base = (value & 0xFFFF) << 4;
        cpu_state.seg_fs.limit_low = 0;
        cpu_state.seg_fs.limit_high = 0xFFFF;
        cpu_state.seg_fs.access = 0x82;
        break;
    case MOO_REG32_GS:
        cpu_state.seg_gs.seg = value & 0xFFFF;
        cpu_state.seg_gs.base = (value & 0xFFFF) << 4;
        cpu_state.seg_gs.limit_low = 0;
        cpu_state.seg_gs.limit_high = 0xFFFF;
        cpu_state.seg_gs.access = 0x82;
        break;
    case MOO_REG32_SS:
        cpu_state.seg_ss.seg = value & 0xFFFF;
        cpu_state.seg_ss.base = (value & 0xFFFF) << 4;
        cpu_state.seg_ss.limit_low = 0;
        cpu_state.seg_ss.limit_high = 0xFFFF;
        cpu_state.seg_ss.access = 0x82;
        break;
    case MOO_REG32_EIP:
        cpu_state.pc = value;
        break;
    case MOO_REG32_EFLAGS:
        cpu_state.flags = value & 0xFFFF;
        cpu_state.eflags = (value >> 16) & 0xFFFF;
        break;
    case MOO_REG32_DR6:
        dr[6] = value;
        break;
    case MOO_REG32_DR7:
        dr[7] = value;
        break;
    default:
        break;
    }
}

static uint32_t get_reg32(moo_reg32_t reg)
{
    switch (reg) {
    case MOO_REG32_CR0:
        return cr0;
    case MOO_REG32_CR3:
        return cr3;
    case MOO_REG32_EAX:
        return EAX;
    case MOO_REG32_EBX:
        return EBX;
    case MOO_REG32_ECX:
        return ECX;
    case MOO_REG32_EDX:
        return EDX;
    case MOO_REG32_ESI:
        return ESI;
    case MOO_REG32_EDI:
        return EDI;
    case MOO_REG32_EBP:
        return EBP;
    case MOO_REG32_ESP:
        return ESP;
    case MOO_REG32_CS:
        return cpu_state.seg_cs.seg;
    case MOO_REG32_DS:
        return cpu_state.seg_ds.seg;
    case MOO_REG32_ES:
        return cpu_state.seg_es.seg;
    case MOO_REG32_FS:
        return cpu_state.seg_fs.seg;
    case MOO_REG32_GS:
        return cpu_state.seg_gs.seg;
    case MOO_REG32_SS:
        return cpu_state.seg_ss.seg;
    case MOO_REG32_EIP:
        return cpu_state.pc;
    case MOO_REG32_EFLAGS:
        return cpu_state.flags | ((uint32_t)cpu_state.eflags << 16);
    case MOO_REG32_DR6:
        return dr[6];
    case MOO_REG32_DR7:
        return dr[7];
    default:
        return 0;
    }
}

/* Track RAM addresses modified by tests for quick cleanup */
static uint32_t dirty_addrs[4096];
static size_t dirty_count = 0;
static int mem_initialized = 0;

static void setup_cpu_state(const moo_cpu_state_t *state)
{
    /* Initialize memory once per test file */
    if (!mem_initialized) {
        mem_reset();
        mem_initialized = 1;
    }

    /* Clear only the RAM locations dirtied by previous test */
    for (size_t i = 0; i < dirty_count; i++) {
        if (ram && dirty_addrs[i] < 16 * 1024 * 1024) {
            ram[dirty_addrs[i]] = 0;
        }
    }
    dirty_count = 0;

    /* Set initial RAM state and track addresses for cleanup */
    for (size_t i = 0; i < state->ram_count; i++) {
        if (ram && state->ram[i].address < 16 * 1024 * 1024) {
            ram[state->ram[i].address] = state->ram[i].value;
            if (dirty_count < 4096) {
                dirty_addrs[dirty_count++] = state->ram[i].address;
            }
        }
    }

    if (state->regs.is_populated && state->regs.type == MOO_REG_TYPE_32) {
        for (int i = 0; i < MOO_REG32_COUNT; i++) {
            if (moo_register_state_has_reg32(&state->regs, i)) {
                set_reg32(i, moo_register_state_get_reg32(&state->regs, i));
            }
        }
    }
}

/* Flags are lazily computed in 86Box. Call flags_rebuild() to materialize them. */
extern void cpu_386_flags_rebuild(void);

static int compare_cpu_state(const moo_test_t *test, int verbose)
{
    const moo_cpu_state_t *final = &test->final_state;
    int mismatches = 0;

    /* Rebuild flags from lazy state before comparison */
    cpu_386_flags_rebuild();

    if (final->regs.is_populated && final->regs.type == MOO_REG_TYPE_32) {
        for (int i = 0; i < MOO_REG32_COUNT; i++) {
            if (moo_register_state_has_reg32(&final->regs, i)) {
                uint32_t expected = moo_test_get_final_reg32(test, i, true);
                uint32_t actual = get_reg32(i);

                if (final->masks.is_populated && moo_register_state_has_reg32(&final->masks, i)) {
                    uint32_t mask = moo_register_state_get_reg32(&final->masks, i);
                    actual &= mask;
                }

                if (expected != actual) {
                    if (verbose) {
                        if (mismatches == 0)
                            printf("\n");
                        printf("    %s: expected 0x%08X, got 0x%08X\n",
                               moo_reg32_name(i), expected, actual);
                    }
                    mismatches++;
                }
            }
        }
    }

    for (size_t i = 0; i < final->ram_count; i++) {
        uint32_t addr = final->ram[i].address;
        uint8_t expected = final->ram[i].value;
        uint8_t actual = (ram && addr < 16 * 1024 * 1024) ? ram[addr] : 0;

        /* Track this address for cleanup before next test */
        if (dirty_count < 4096) {
            dirty_addrs[dirty_count++] = addr;
        }

        if (expected != actual) {
            if (verbose) {
                if (mismatches == 0)
                    printf("\n");
                printf("    RAM[0x%08X]: expected 0x%02X, got 0x%02X\n",
                       addr, expected, actual);
            }
            mismatches++;
        }
    }

    return mismatches;
}

extern uint32_t timer_target;
extern uint64_t tsc;
extern int32_t cycles_main;

static int run_single_test(const moo_test_t *test, int verbose)
{
    setup_cpu_state(&test->init_state);

    cpu_state.abrt = 0;
    cpu_state.pc = moo_test_get_initial_reg32(test, MOO_REG32_EIP);
    cpu_state.oldpc = cpu_state.pc;

    /* Set timer_target high so we have enough execution cycles */
    tsc = 0;
    timer_target = 0xFFFFFFFF;
    cycles_main = 0;  /* Reset cycles_main for each test */

    /* Ensure CR0 is in real mode state */
    cr0 = 0x60000010;  /* CD=1, ET=1, all else 0 - standard real mode */

    /* HLT consumes 100 cycles per iteration, so with low cycles count
       it will exhaust cycles after the test instruction + one HLT. */

    if (verbose) {
        uint32_t linear = cpu_state.seg_cs.base + cpu_state.pc;
        uint32_t eip = cpu_state.pc;
        extern uint32_t use32;
        extern const int (*x86_opcodes)[1];  /* Pointer to function pointer array */
        printf("\n  Initial: CS=%04X base=%08X EIP=%08X linear=%08X use32=%d\n",
               cpu_state.seg_cs.seg, cpu_state.seg_cs.base, eip, linear, use32);
        printf("  RAM at linear 0x%X: %02X %02X %02X %02X %02X %02X\n",
               linear, ram[linear], ram[linear+1], ram[linear+2], ram[linear+3],
               ram[linear+4], ram[linear+5]);
        printf("  x86_opcodes=%p\n", (void*)x86_opcodes);
    }

    cycles = 110;  /* Enough for instruction + one HLT iteration (100 cycles) */
    cpu_exec(1);

    /* HLT instruction decrements PC to stay at HLT for interrupt waiting.
       But tests expect EIP to be past HLT. Check if we're at HLT and adjust. */
    {
        uint32_t linear = cpu_state.seg_cs.base + cpu_state.pc;
        if (linear < 16 * 1024 * 1024 && ram && ram[linear] == 0xF4) {
            cpu_state.pc++;  /* Move past HLT for test comparison */
        }
    }

    if (verbose) {
        printf("  Final: EIP=%08X abrt=%d\n", cpu_state.pc, cpu_state.abrt);
    }

    return compare_cpu_state(test, verbose);
}

static void run_moo_tests(const char *filename, int max_tests)
{
    moo_reader_t *reader = moo_reader_create();
    if (!reader) {
        printf("Failed to create MOO reader\n");
        return;
    }

    moo_error_t err = moo_reader_load_file(reader, filename);
    if (err != MOO_OK) {
        printf("%s: LOAD ERROR (%s)\n", filename, moo_error_string(err));
        moo_reader_destroy(reader);
        return;
    }

    /* Extract just the filename for display */
    const char *basename = strrchr(filename, '/');
    basename = basename ? basename + 1 : filename;

    printf("%s: ", basename);
    fflush(stdout);

    size_t count = moo_reader_get_test_count(reader);
    if (max_tests > 0 && (size_t)max_tests < count) {
        count = max_tests;
    }

    int file_passed = 0;
    int file_failed = 0;

    for (size_t i = 0; i < count; i++) {
        const moo_test_t *test = moo_reader_get_test(reader, i);
        if (!test)
            continue;

        tests_run++;
        int mismatches = run_single_test(test, 0);

        if (mismatches == 0) {
            tests_passed++;
            file_passed++;
        } else {
            tests_failed++;
            file_failed++;
        }
    }

    printf("  %d/%zu passed", file_passed, count);
    if (file_failed > 0)
        printf(", %d failed", file_failed);
    printf("\n");

    moo_reader_destroy(reader);

    /* Reset memory state for next file */
    mem_initialized = 0;
    dirty_count = 0;
}

static int compare_strings(const void *a, const void *b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}

static void run_all_tests_in_dir(const char *dir_path)
{
    DIR *dir = opendir(dir_path);
    if (!dir) {
        printf("Failed to open test directory: %s\n", dir_path);
        return;
    }

    /* Collect all .MOO.gz files */
    char **files = NULL;
    size_t file_count = 0;
    size_t file_capacity = 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (len > 7 && strcmp(entry->d_name + len - 7, ".MOO.gz") == 0) {
            if (file_count >= file_capacity) {
                file_capacity = file_capacity ? file_capacity * 2 : 64;
                files = realloc(files, file_capacity * sizeof(char *));
            }
            /* Build full path */
            char *path = malloc(strlen(dir_path) + 1 + len + 1);
            sprintf(path, "%s/%s", dir_path, entry->d_name);
            files[file_count++] = path;
        }
    }
    closedir(dir);

    /* Sort files alphabetically for consistent ordering */
    qsort(files, file_count, sizeof(char *), compare_strings);

    printf("Found %zu test files in %s\n\n", file_count, dir_path);

    /* Run all test files */
    for (size_t i = 0; i < file_count; i++) {
        run_moo_tests(files[i], -1);  /* -1 = run all tests in file */
        free(files[i]);
    }
    free(files);
}

int main(int argc, char *argv[])
{
    const char *test_file = NULL;
    int max_tests = -1;  /* Default: run all tests */

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            max_tests = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            test_file = argv[++i];
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [-n count] [-f file.MOO.gz]\n", argv[0]);
            printf("  -n count  Run only first 'count' tests per file\n");
            printf("  -f file   Run tests from specific file only\n");
            printf("\nBy default, runs all tests from %s\n", TEST_DATA_DIR);
            return 0;
        }
    }

    printf("CPU Test Suite\n");
    printf("==============\n\n");

    cpu_test_init();

    if (test_file) {
        run_moo_tests(test_file, max_tests);
    } else {
        run_all_tests_in_dir(TEST_DATA_DIR);
    }

    cpu_test_cleanup();

    printf("\n%d/%d tests passed", tests_passed, tests_run);
    if (tests_failed > 0) {
        printf(", %d failed", tests_failed);
    }
    printf("\n");

    return tests_failed > 0 ? 1 : 0;
}
