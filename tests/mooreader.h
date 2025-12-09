/*
    MIT License

    Copyright (c) 2025 Angela McEgo
    Copyright (c) 2025 Daniel Balsom

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/
#ifndef MOOREADER_H
#define MOOREADER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MOO_REG16_AX = 0,
    MOO_REG16_BX = 1,
    MOO_REG16_CX = 2,
    MOO_REG16_DX = 3,
    MOO_REG16_CS = 4,
    MOO_REG16_SS = 5,
    MOO_REG16_DS = 6,
    MOO_REG16_ES = 7,
    MOO_REG16_SP = 8,
    MOO_REG16_BP = 9,
    MOO_REG16_SI = 10,
    MOO_REG16_DI = 11,
    MOO_REG16_IP = 12,
    MOO_REG16_FLAGS = 13,
    MOO_REG16_COUNT = 14
} moo_reg16_t;

typedef enum {
    MOO_REG32_CR0 = 0,
    MOO_REG32_CR3 = 1,
    MOO_REG32_EAX = 2,
    MOO_REG32_EBX = 3,
    MOO_REG32_ECX = 4,
    MOO_REG32_EDX = 5,
    MOO_REG32_ESI = 6,
    MOO_REG32_EDI = 7,
    MOO_REG32_EBP = 8,
    MOO_REG32_ESP = 9,
    MOO_REG32_CS = 10,
    MOO_REG32_DS = 11,
    MOO_REG32_ES = 12,
    MOO_REG32_FS = 13,
    MOO_REG32_GS = 14,
    MOO_REG32_SS = 15,
    MOO_REG32_EIP = 16,
    MOO_REG32_EFLAGS = 17,
    MOO_REG32_DR6 = 18,
    MOO_REG32_DR7 = 19,
    MOO_REG32_COUNT = 20
} moo_reg32_t;

typedef enum {
    MOO_CPU_8088,
    MOO_CPU_8086,
    MOO_CPU_V20,
    MOO_CPU_V30,
    MOO_CPU_286,
    MOO_CPU_386E,
    MOO_CPU_COUNT
} moo_cpu_type_t;

typedef enum {
    MOO_REG_TYPE_16,
    MOO_REG_TYPE_32
} moo_reg_type_t;

typedef enum {
    MOO_OK = 0,
    MOO_ERR_FILE_OPEN,
    MOO_ERR_FILE_READ,
    MOO_ERR_OUT_OF_MEMORY,
    MOO_ERR_INVALID_FORMAT,
    MOO_ERR_UNSUPPORTED_VERSION,
    MOO_ERR_UNSUPPORTED_CPU,
    MOO_ERR_READ_PAST_END
} moo_error_t;

typedef struct {
    uint32_t bitmask;
    uint32_t *values;
    size_t values_count;
    moo_reg_type_t type;
    bool is_populated;
} moo_register_state_t;

typedef struct {
    uint32_t address;
    uint8_t value;
} moo_ram_entry_t;

typedef struct {
    uint8_t *bytes;
    size_t count;
} moo_queue_data_t;

typedef struct {
    moo_register_state_t regs;
    moo_register_state_t masks;
    moo_ram_entry_t *ram;
    size_t ram_count;
    moo_queue_data_t queue;
    bool has_queue;
} moo_cpu_state_t;

typedef struct {
    uint8_t ale : 1;
    uint8_t bhe : 1;
    uint8_t ready : 1;
    uint8_t lock : 1;
    uint8_t reserved : 4;
} moo_cycle_bitfield0_t;

typedef struct {
    uint8_t bhe : 1;
    uint8_t reserved : 7;
} moo_cycle_bitfield1_t;

typedef struct {
    moo_cycle_bitfield0_t pin_bitfield0;
    uint32_t address_latch;
    uint8_t segment_status;
    uint8_t memory_status;
    uint8_t io_status;
    moo_cycle_bitfield1_t pin_bitfield1;
    uint16_t data_bus;
    uint8_t bus_status;
    uint8_t t_state;
    uint8_t queue_op_status;
    uint8_t queue_byte_read;
} moo_cycle_t;

typedef struct {
    uint8_t number;
    uint32_t flag_addr;
} moo_exception_t;

typedef struct {
    uint32_t index;
    char *name;
    uint8_t *bytes;
    size_t bytes_count;
    moo_cpu_state_t init_state;
    moo_cpu_state_t final_state;
    moo_cycle_t *cycles;
    size_t cycles_count;
    bool has_exception;
    moo_exception_t exception;
    bool has_hash;
    uint8_t hash[20];
} moo_test_t;

typedef struct {
    uint8_t version_major;
    uint8_t version_minor;
    uint8_t reserved[2];
    uint32_t test_count;
    char cpu_name[9];
    moo_cpu_type_t cpu_type;
} moo_header_t;

typedef struct {
    moo_header_t header;
    moo_test_t *tests;
    size_t tests_count;
    uint8_t *data;
    size_t data_size;
    size_t offset;
    moo_error_t last_error;
} moo_reader_t;

const char *moo_reg16_name(moo_reg16_t reg);
const char *moo_reg32_name(moo_reg32_t reg);

moo_reader_t *moo_reader_create(void);
void moo_reader_destroy(moo_reader_t *reader);

moo_error_t moo_reader_load_file(moo_reader_t *reader, const char *filename);
moo_error_t moo_reader_get_last_error(const moo_reader_t *reader);
const char *moo_error_string(moo_error_t error);

const moo_header_t *moo_reader_get_header(const moo_reader_t *reader);
size_t moo_reader_get_test_count(const moo_reader_t *reader);
const moo_test_t *moo_reader_get_test(const moo_reader_t *reader, size_t index);

const char *moo_get_register_name(const moo_reader_t *reader, int bit_position);
const char *moo_get_bus_status_name(const moo_reader_t *reader, uint8_t status);
const char *moo_get_t_state_name(const moo_reader_t *reader, uint8_t t_state);
const char *moo_get_queue_op_name(uint8_t queue_op);

bool moo_register_state_has_reg16(const moo_register_state_t *state, moo_reg16_t reg);
uint16_t moo_register_state_get_reg16(const moo_register_state_t *state, moo_reg16_t reg);
bool moo_register_state_has_reg32(const moo_register_state_t *state, moo_reg32_t reg);
uint32_t moo_register_state_get_reg32(const moo_register_state_t *state, moo_reg32_t reg);

uint16_t moo_test_get_initial_reg16(const moo_test_t *test, moo_reg16_t reg);
uint16_t moo_test_get_final_reg16(const moo_test_t *test, moo_reg16_t reg, bool masked);
uint32_t moo_test_get_initial_reg32(const moo_test_t *test, moo_reg32_t reg);
uint32_t moo_test_get_final_reg32(const moo_test_t *test, moo_reg32_t reg, bool masked);

#ifdef __cplusplus
}
#endif

#endif /* MOOREADER_H */
