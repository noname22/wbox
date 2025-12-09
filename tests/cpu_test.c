#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "mooreader.h"
#include "cpu.h"
#include "mem.h"
#include "codegen_public.h"

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
        break;
    case MOO_REG32_DS:
        cpu_state.seg_ds.seg = value & 0xFFFF;
        cpu_state.seg_ds.base = (value & 0xFFFF) << 4;
        break;
    case MOO_REG32_ES:
        cpu_state.seg_es.seg = value & 0xFFFF;
        cpu_state.seg_es.base = (value & 0xFFFF) << 4;
        break;
    case MOO_REG32_FS:
        cpu_state.seg_fs.seg = value & 0xFFFF;
        cpu_state.seg_fs.base = (value & 0xFFFF) << 4;
        break;
    case MOO_REG32_GS:
        cpu_state.seg_gs.seg = value & 0xFFFF;
        cpu_state.seg_gs.base = (value & 0xFFFF) << 4;
        break;
    case MOO_REG32_SS:
        cpu_state.seg_ss.seg = value & 0xFFFF;
        cpu_state.seg_ss.base = (value & 0xFFFF) << 4;
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

static void setup_cpu_state(const moo_cpu_state_t *state)
{
    mem_reset();

    for (size_t i = 0; i < state->ram_count; i++) {
        if (ram && state->ram[i].address < 16 * 1024 * 1024) {
            ram[state->ram[i].address] = state->ram[i].value;
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

static int compare_cpu_state(const moo_test_t *test)
{
    const moo_cpu_state_t *final = &test->final_state;
    int mismatches = 0;

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
                    if (mismatches == 0) {
                        printf("\n");
                    }
                    printf("    %s: expected 0x%08X, got 0x%08X\n",
                           moo_reg32_name(i), expected, actual);
                    mismatches++;
                }
            }
        }
    }

    for (size_t i = 0; i < final->ram_count; i++) {
        uint32_t addr = final->ram[i].address;
        uint8_t expected = final->ram[i].value;
        uint8_t actual = (ram && addr < 16 * 1024 * 1024) ? ram[addr] : 0;

        if (expected != actual) {
            if (mismatches == 0) {
                printf("\n");
            }
            printf("    RAM[0x%08X]: expected 0x%02X, got 0x%02X\n",
                   addr, expected, actual);
            mismatches++;
        }
    }

    return mismatches;
}

static int run_single_test(const moo_test_t *test)
{
    setup_cpu_state(&test->init_state);

    cpu_state.abrt = 0;
    cpu_state.pc = moo_test_get_initial_reg32(test, MOO_REG32_EIP);
    cpu_state.oldpc = cpu_state.pc;

    cycles = 1000;
    cpu_exec(1);

    return compare_cpu_state(test);
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
        printf("Failed to load %s: %s\n", filename, moo_error_string(err));
        moo_reader_destroy(reader);
        return;
    }

    const moo_header_t *header = moo_reader_get_header(reader);
    printf("Loaded %s: %s CPU, %u tests\n",
           filename, header->cpu_name, header->test_count);

    size_t count = moo_reader_get_test_count(reader);
    if (max_tests > 0 && (size_t)max_tests < count) {
        count = max_tests;
    }

    for (size_t i = 0; i < count; i++) {
        const moo_test_t *test = moo_reader_get_test(reader, i);
        if (!test)
            continue;

        tests_run++;
        printf("Test %zu: %s... ", i, test->name ? test->name : "(unnamed)");
        fflush(stdout);

        int mismatches = run_single_test(test);

        if (mismatches == 0) {
            printf("PASSED\n");
            tests_passed++;
        } else {
            printf("FAILED (%d mismatches)\n", mismatches);
            tests_failed++;
        }
    }

    moo_reader_destroy(reader);
}

int main(int argc, char *argv[])
{
    const char *test_file = NULL;
    int max_tests = 10;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            max_tests = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-a") == 0) {
            max_tests = -1;
        } else {
            test_file = argv[i];
        }
    }

    printf("CPU Test Suite\n");
    printf("==============\n\n");

    cpu_test_init();

    if (test_file) {
        run_moo_tests(test_file, max_tests);
    } else {
        printf("No test file specified.\n");
        printf("Usage: %s [-n count | -a] <test.moo.gz>\n", argv[0]);
        printf("  -n count  Run only first 'count' tests (default: 10)\n");
        printf("  -a        Run all tests\n");
    }

    cpu_test_cleanup();

    printf("\n%d/%d tests passed", tests_passed, tests_run);
    if (tests_failed > 0) {
        printf(", %d failed", tests_failed);
    }
    printf("\n");

    return tests_failed > 0 ? 1 : 0;
}
