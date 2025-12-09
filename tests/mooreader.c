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

#include "mooreader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef MOO_USE_ZLIB
#include <zlib.h>
#endif

static const char *reg16_names[] = {
    "ax", "bx", "cx", "dx", "cs", "ss", "ds", "es",
    "sp", "bp", "si", "di", "ip", "flags"
};

static const char *reg32_names[] = {
    "cr0", "cr3", "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp",
    "cs", "ds", "es", "fs", "gs", "ss", "eip", "eflags", "dr6", "dr7"
};

const char *moo_reg16_name(moo_reg16_t reg)
{
    if (reg < MOO_REG16_COUNT)
        return reg16_names[reg];
    return "unknown";
}

const char *moo_reg32_name(moo_reg32_t reg)
{
    if (reg < MOO_REG32_COUNT)
        return reg32_names[reg];
    return "unknown";
}

const char *moo_error_string(moo_error_t error)
{
    switch (error) {
    case MOO_OK:
        return "No error";
    case MOO_ERR_FILE_OPEN:
        return "Failed to open file";
    case MOO_ERR_FILE_READ:
        return "Failed to read file";
    case MOO_ERR_OUT_OF_MEMORY:
        return "Out of memory";
    case MOO_ERR_INVALID_FORMAT:
        return "Invalid MOO file format";
    case MOO_ERR_UNSUPPORTED_VERSION:
        return "Unsupported MOO version";
    case MOO_ERR_UNSUPPORTED_CPU:
        return "Unsupported CPU type";
    case MOO_ERR_READ_PAST_END:
        return "Read past end of data";
    default:
        return "Unknown error";
    }
}

moo_reader_t *moo_reader_create(void)
{
    moo_reader_t *reader = calloc(1, sizeof(moo_reader_t));
    return reader;
}

static void free_register_state(moo_register_state_t *state)
{
    free(state->values);
    state->values = NULL;
    state->values_count = 0;
}

static void free_cpu_state(moo_cpu_state_t *state)
{
    free_register_state(&state->regs);
    free_register_state(&state->masks);
    free(state->ram);
    state->ram = NULL;
    state->ram_count = 0;
    free(state->queue.bytes);
    state->queue.bytes = NULL;
    state->queue.count = 0;
}

static void free_test(moo_test_t *test)
{
    free(test->name);
    test->name = NULL;
    free(test->bytes);
    test->bytes = NULL;
    free_cpu_state(&test->init_state);
    free_cpu_state(&test->final_state);
    free(test->cycles);
    test->cycles = NULL;
}

void moo_reader_destroy(moo_reader_t *reader)
{
    if (!reader)
        return;

    for (size_t i = 0; i < reader->tests_count; i++) {
        free_test(&reader->tests[i]);
    }
    free(reader->tests);
    free(reader->data);
    free(reader);
}

static bool read_u8(moo_reader_t *reader, uint8_t *out)
{
    if (reader->offset + 1 > reader->data_size) {
        reader->last_error = MOO_ERR_READ_PAST_END;
        return false;
    }
    *out = reader->data[reader->offset++];
    return true;
}

static bool read_u16(moo_reader_t *reader, uint16_t *out)
{
    if (reader->offset + 2 > reader->data_size) {
        reader->last_error = MOO_ERR_READ_PAST_END;
        return false;
    }
    *out = (uint16_t)reader->data[reader->offset] |
           ((uint16_t)reader->data[reader->offset + 1] << 8);
    reader->offset += 2;
    return true;
}

static bool read_u32(moo_reader_t *reader, uint32_t *out)
{
    if (reader->offset + 4 > reader->data_size) {
        reader->last_error = MOO_ERR_READ_PAST_END;
        return false;
    }
    *out = (uint32_t)reader->data[reader->offset] |
           ((uint32_t)reader->data[reader->offset + 1] << 8) |
           ((uint32_t)reader->data[reader->offset + 2] << 16) |
           ((uint32_t)reader->data[reader->offset + 3] << 24);
    reader->offset += 4;
    return true;
}

static bool read_bytes(moo_reader_t *reader, void *dest, size_t count)
{
    if (reader->offset + count > reader->data_size) {
        reader->last_error = MOO_ERR_READ_PAST_END;
        return false;
    }
    memcpy(dest, &reader->data[reader->offset], count);
    reader->offset += count;
    return true;
}

typedef struct {
    char type[5];
    uint32_t length;
    size_t data_start;
    size_t data_end;
} chunk_header_t;

static bool read_chunk_header(moo_reader_t *reader, chunk_header_t *header)
{
    if (!read_bytes(reader, header->type, 4))
        return false;
    header->type[4] = '\0';
    if (!read_u32(reader, &header->length))
        return false;
    header->data_start = reader->offset;
    header->data_end = reader->offset + header->length;
    return true;
}

static bool read_registers16(moo_reader_t *reader, moo_register_state_t *regs)
{
    uint16_t bitmask;
    if (!read_u16(reader, &bitmask))
        return false;

    regs->bitmask = bitmask;
    regs->type = MOO_REG_TYPE_16;
    regs->is_populated = true;
    regs->values_count = MOO_REG16_COUNT;
    regs->values = calloc(MOO_REG16_COUNT, sizeof(uint32_t));
    if (!regs->values) {
        reader->last_error = MOO_ERR_OUT_OF_MEMORY;
        return false;
    }

    for (int i = 0; i < 16; i++) {
        if (bitmask & (1 << i)) {
            uint16_t val;
            if (!read_u16(reader, &val))
                return false;
            regs->values[i] = val;
        }
    }
    return true;
}

static bool read_registers32(moo_reader_t *reader, moo_register_state_t *regs)
{
    uint32_t bitmask;
    if (!read_u32(reader, &bitmask))
        return false;

    regs->bitmask = bitmask;
    regs->type = MOO_REG_TYPE_32;
    regs->is_populated = true;
    regs->values_count = MOO_REG32_COUNT;
    regs->values = calloc(MOO_REG32_COUNT, sizeof(uint32_t));
    if (!regs->values) {
        reader->last_error = MOO_ERR_OUT_OF_MEMORY;
        return false;
    }

    for (int i = 0; i < 32; i++) {
        if (bitmask & (1 << i)) {
            uint32_t val;
            if (!read_u32(reader, &val))
                return false;
            regs->values[i] = val;
        }
    }
    return true;
}

static bool read_ram(moo_reader_t *reader, moo_ram_entry_t **ram, size_t *count)
{
    uint32_t n;
    if (!read_u32(reader, &n))
        return false;

    *ram = calloc(n, sizeof(moo_ram_entry_t));
    if (!*ram && n > 0) {
        reader->last_error = MOO_ERR_OUT_OF_MEMORY;
        return false;
    }
    *count = n;

    for (uint32_t i = 0; i < n; i++) {
        if (!read_u32(reader, &(*ram)[i].address))
            return false;
        if (!read_u8(reader, &(*ram)[i].value))
            return false;
    }
    return true;
}

static bool read_queue(moo_reader_t *reader, moo_queue_data_t *queue)
{
    uint32_t length;
    if (!read_u32(reader, &length))
        return false;

    queue->bytes = calloc(length, 1);
    if (!queue->bytes && length > 0) {
        reader->last_error = MOO_ERR_OUT_OF_MEMORY;
        return false;
    }
    queue->count = length;

    if (!read_bytes(reader, queue->bytes, length))
        return false;
    return true;
}

static bool read_cpu_state(moo_reader_t *reader, moo_cpu_state_t *state, size_t end_offset)
{
    memset(state, 0, sizeof(*state));

    while (reader->offset < end_offset) {
        chunk_header_t chunk;
        if (!read_chunk_header(reader, &chunk))
            return false;

        if (strcmp(chunk.type, "REGS") == 0) {
            if (!read_registers16(reader, &state->regs))
                return false;
        }
        else if (strcmp(chunk.type, "RG32") == 0) {
            if (!read_registers32(reader, &state->regs))
                return false;
        }
        else if (strcmp(chunk.type, "RMSK") == 0) {
            if (!read_registers16(reader, &state->masks))
                return false;
        }
        else if (strcmp(chunk.type, "RM32") == 0) {
            if (!read_registers32(reader, &state->masks))
                return false;
        }
        else if (strcmp(chunk.type, "RAM ") == 0) {
            if (!read_ram(reader, &state->ram, &state->ram_count))
                return false;
        }
        else if (strcmp(chunk.type, "QUEU") == 0) {
            if (!read_queue(reader, &state->queue))
                return false;
            state->has_queue = true;
        }

        reader->offset = chunk.data_end;
    }
    return true;
}

static bool read_cycles(moo_reader_t *reader, moo_cycle_t **cycles, size_t *count)
{
    uint32_t n;
    if (!read_u32(reader, &n))
        return false;

    *cycles = calloc(n, sizeof(moo_cycle_t));
    if (!*cycles && n > 0) {
        reader->last_error = MOO_ERR_OUT_OF_MEMORY;
        return false;
    }
    *count = n;

    for (uint32_t i = 0; i < n; i++) {
        moo_cycle_t *c = &(*cycles)[i];
        uint8_t bf0, bf1;

        if (!read_u8(reader, &bf0))
            return false;
        c->pin_bitfield0.ale = bf0 & 1;
        c->pin_bitfield0.bhe = (bf0 >> 1) & 1;
        c->pin_bitfield0.ready = (bf0 >> 2) & 1;
        c->pin_bitfield0.lock = (bf0 >> 3) & 1;

        if (!read_u32(reader, &c->address_latch))
            return false;
        if (!read_u8(reader, &c->segment_status))
            return false;
        if (!read_u8(reader, &c->memory_status))
            return false;
        if (!read_u8(reader, &c->io_status))
            return false;

        if (!read_u8(reader, &bf1))
            return false;
        c->pin_bitfield1.bhe = bf1 & 1;

        if (!read_u16(reader, &c->data_bus))
            return false;
        if (!read_u8(reader, &c->bus_status))
            return false;
        if (!read_u8(reader, &c->t_state))
            return false;
        if (!read_u8(reader, &c->queue_op_status))
            return false;
        if (!read_u8(reader, &c->queue_byte_read))
            return false;
    }
    return true;
}

static bool read_test(moo_reader_t *reader, moo_test_t *test)
{
    chunk_header_t test_header;
    if (!read_chunk_header(reader, &test_header))
        return false;

    while (strcmp(test_header.type, "TEST") != 0) {
        reader->offset = test_header.data_end;
        if (!read_chunk_header(reader, &test_header))
            return false;
    }

    memset(test, 0, sizeof(*test));
    if (!read_u32(reader, &test->index))
        return false;

    while (reader->offset < test_header.data_end) {
        chunk_header_t chunk;
        if (!read_chunk_header(reader, &chunk))
            return false;

        if (strcmp(chunk.type, "NAME") == 0) {
            uint32_t name_len;
            if (!read_u32(reader, &name_len))
                return false;
            test->name = calloc(name_len + 1, 1);
            if (!test->name) {
                reader->last_error = MOO_ERR_OUT_OF_MEMORY;
                return false;
            }
            if (!read_bytes(reader, test->name, name_len))
                return false;
        }
        else if (strcmp(chunk.type, "BYTS") == 0) {
            uint32_t byte_count;
            if (!read_u32(reader, &byte_count))
                return false;
            test->bytes = calloc(byte_count, 1);
            if (!test->bytes && byte_count > 0) {
                reader->last_error = MOO_ERR_OUT_OF_MEMORY;
                return false;
            }
            test->bytes_count = byte_count;
            if (!read_bytes(reader, test->bytes, byte_count))
                return false;
        }
        else if (strcmp(chunk.type, "INIT") == 0) {
            if (!read_cpu_state(reader, &test->init_state, chunk.data_end))
                return false;
        }
        else if (strcmp(chunk.type, "FINA") == 0) {
            if (!read_cpu_state(reader, &test->final_state, chunk.data_end))
                return false;
        }
        else if (strcmp(chunk.type, "CYCL") == 0) {
            if (!read_cycles(reader, &test->cycles, &test->cycles_count))
                return false;
        }
        else if (strcmp(chunk.type, "EXCP") == 0) {
            if (!read_u8(reader, &test->exception.number))
                return false;
            if (!read_u32(reader, &test->exception.flag_addr))
                return false;
            test->has_exception = true;
        }
        else if (strcmp(chunk.type, "HASH") == 0) {
            if (!read_bytes(reader, test->hash, 20))
                return false;
            test->has_hash = true;
        }

        reader->offset = chunk.data_end;
    }

    reader->offset = test_header.data_end;
    return true;
}

static bool read_moo_header(moo_reader_t *reader)
{
    if (!read_u8(reader, &reader->header.version_major))
        return false;
    if (!read_u8(reader, &reader->header.version_minor))
        return false;
    if (!read_bytes(reader, reader->header.reserved, 2))
        return false;
    if (!read_u32(reader, &reader->header.test_count))
        return false;

    memset(reader->header.cpu_name, ' ', 8);
    reader->header.cpu_name[8] = '\0';

    uint16_t version = ((uint16_t)reader->header.version_major << 8) | reader->header.version_minor;
    if (version == 0x0100 || version == 0x0101) {
        if (!read_bytes(reader, reader->header.cpu_name, 4))
            return false;
    }
    else {
        reader->last_error = MOO_ERR_UNSUPPORTED_VERSION;
        return false;
    }

    if (strncmp(reader->header.cpu_name, "8088", 4) == 0 ||
        strncmp(reader->header.cpu_name, "88  ", 4) == 0) {
        reader->header.cpu_type = MOO_CPU_8088;
    }
    else if (strncmp(reader->header.cpu_name, "8086", 4) == 0) {
        reader->header.cpu_type = MOO_CPU_8086;
    }
    else if (strncmp(reader->header.cpu_name, "V20 ", 4) == 0) {
        reader->header.cpu_type = MOO_CPU_V20;
    }
    else if (strncmp(reader->header.cpu_name, "V30 ", 4) == 0) {
        reader->header.cpu_type = MOO_CPU_V30;
    }
    else if (strncmp(reader->header.cpu_name, "286 ", 4) == 0 ||
             strncmp(reader->header.cpu_name, "C286", 4) == 0) {
        reader->header.cpu_type = MOO_CPU_286;
    }
    else if (strncmp(reader->header.cpu_name, "386E", 4) == 0) {
        reader->header.cpu_type = MOO_CPU_386E;
    }
    else {
        reader->last_error = MOO_ERR_UNSUPPORTED_CPU;
        return false;
    }

    return true;
}

static bool analyze(moo_reader_t *reader)
{
    reader->offset = 0;

    chunk_header_t first_chunk;
    if (!read_chunk_header(reader, &first_chunk))
        return false;

    if (strcmp(first_chunk.type, "MOO ") != 0) {
        reader->last_error = MOO_ERR_INVALID_FORMAT;
        return false;
    }

    if (!read_moo_header(reader))
        return false;

    reader->offset = first_chunk.data_end;

    reader->tests = calloc(reader->header.test_count, sizeof(moo_test_t));
    if (!reader->tests && reader->header.test_count > 0) {
        reader->last_error = MOO_ERR_OUT_OF_MEMORY;
        return false;
    }
    reader->tests_count = reader->header.test_count;

    for (uint32_t i = 0; i < reader->header.test_count; i++) {
        if (!read_test(reader, &reader->tests[i]))
            return false;
    }

    return true;
}

static bool is_gzip_file(const char *filename)
{
    FILE *f = fopen(filename, "rb");
    if (!f)
        return false;

    unsigned char b0 = 0, b1 = 0;
    size_t n = fread(&b0, 1, 1, f);
    n += fread(&b1, 1, 1, f);
    fclose(f);

    return n == 2 && b0 == 0x1F && b1 == 0x8B;
}

static moo_error_t read_raw_file(moo_reader_t *reader, const char *filename)
{
    FILE *f = fopen(filename, "rb");
    if (!f) {
        reader->last_error = MOO_ERR_FILE_OPEN;
        return reader->last_error;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    reader->data = malloc(size);
    if (!reader->data) {
        fclose(f);
        reader->last_error = MOO_ERR_OUT_OF_MEMORY;
        return reader->last_error;
    }

    if (fread(reader->data, 1, size, f) != (size_t)size) {
        fclose(f);
        free(reader->data);
        reader->data = NULL;
        reader->last_error = MOO_ERR_FILE_READ;
        return reader->last_error;
    }

    fclose(f);
    reader->data_size = size;
    return MOO_OK;
}

#ifdef MOO_USE_ZLIB
static moo_error_t read_gzip_file(moo_reader_t *reader, const char *filename)
{
    gzFile gz = gzopen(filename, "rb");
    if (!gz) {
        reader->last_error = MOO_ERR_FILE_OPEN;
        return reader->last_error;
    }

    size_t capacity = 1024 * 1024;
    size_t size = 0;
    uint8_t *data = malloc(capacity);
    if (!data) {
        gzclose(gz);
        reader->last_error = MOO_ERR_OUT_OF_MEMORY;
        return reader->last_error;
    }

    uint8_t buf[4096];
    int bytes_read;
    while ((bytes_read = gzread(gz, buf, sizeof(buf))) > 0) {
        if (size + bytes_read > capacity) {
            capacity *= 2;
            uint8_t *new_data = realloc(data, capacity);
            if (!new_data) {
                free(data);
                gzclose(gz);
                reader->last_error = MOO_ERR_OUT_OF_MEMORY;
                return reader->last_error;
            }
            data = new_data;
        }
        memcpy(data + size, buf, bytes_read);
        size += bytes_read;
    }

    int gzerr_no = 0;
    gzerror(gz, &gzerr_no);
    gzclose(gz);

    if (bytes_read < 0 || (gzerr_no != Z_OK && gzerr_no != Z_STREAM_END)) {
        free(data);
        reader->last_error = MOO_ERR_FILE_READ;
        return reader->last_error;
    }

    reader->data = data;
    reader->data_size = size;
    return MOO_OK;
}
#endif

moo_error_t moo_reader_load_file(moo_reader_t *reader, const char *filename)
{
    moo_error_t err;

#ifdef MOO_USE_ZLIB
    if (is_gzip_file(filename)) {
        err = read_gzip_file(reader, filename);
    }
    else {
        err = read_raw_file(reader, filename);
    }
#else
    err = read_raw_file(reader, filename);
#endif

    if (err != MOO_OK)
        return err;

    if (!analyze(reader)) {
        return reader->last_error;
    }

    reader->last_error = MOO_OK;
    return MOO_OK;
}

moo_error_t moo_reader_get_last_error(const moo_reader_t *reader)
{
    return reader->last_error;
}

const moo_header_t *moo_reader_get_header(const moo_reader_t *reader)
{
    return &reader->header;
}

size_t moo_reader_get_test_count(const moo_reader_t *reader)
{
    return reader->tests_count;
}

const moo_test_t *moo_reader_get_test(const moo_reader_t *reader, size_t index)
{
    if (index >= reader->tests_count)
        return NULL;
    return &reader->tests[index];
}

const char *moo_get_register_name(const moo_reader_t *reader, int bit_position)
{
    switch (reader->header.cpu_type) {
    case MOO_CPU_8088:
    case MOO_CPU_8086:
    case MOO_CPU_V20:
    case MOO_CPU_V30:
    case MOO_CPU_286:
        if (bit_position >= 0 && bit_position < 14)
            return reg16_names[bit_position];
        break;
    case MOO_CPU_386E:
        if (bit_position >= 0 && bit_position < 20)
            return reg32_names[bit_position];
        break;
    default:
        break;
    }
    return "unknown";
}

const char *moo_get_bus_status_name(const moo_reader_t *reader, uint8_t status)
{
    switch (reader->header.cpu_type) {
    case MOO_CPU_8088:
    case MOO_CPU_8086:
    case MOO_CPU_V20:
    case MOO_CPU_V30: {
        static const char *names[] = {"INTA", "IOR", "IOW", "MEMR", "MEMW", "HALT", "CODE", "PASV"};
        if (status < 8)
            return names[status];
        break;
    }
    case MOO_CPU_286: {
        static const char *names[] = {"INTA", "PASV", "PASV", "PASV", "HALT", "MEMR", "MEMW", "PASV",
                                      "PASV", "IOR ", "IOW ", "PASV", "PASV", "CODE", "PASV", "PASV"};
        if (status < 16)
            return names[status];
        break;
    }
    case MOO_CPU_386E: {
        static const char *names[] = {"INTA", "PASV", "IOR", "IOW", "CODE", "HALT", "MEMR", "MEMW"};
        if (status < 8)
            return names[status];
        break;
    }
    default:
        break;
    }
    return "UNKNOWN";
}

const char *moo_get_t_state_name(const moo_reader_t *reader, uint8_t t_state)
{
    switch (reader->header.cpu_type) {
    case MOO_CPU_8088:
    case MOO_CPU_8086:
    case MOO_CPU_V20:
    case MOO_CPU_V30: {
        static const char *names[] = {"Ti", "T1", "T2", "T3", "T4", "Tw"};
        if (t_state < 6)
            return names[t_state];
        break;
    }
    case MOO_CPU_286: {
        static const char *names[] = {"Ti", "Ts", "Tc"};
        if (t_state < 3)
            return names[t_state];
        break;
    }
    case MOO_CPU_386E: {
        static const char *names[] = {"Ti", "T1", "T2"};
        if (t_state < 3)
            return names[t_state];
        break;
    }
    default:
        break;
    }
    return "unknown";
}

const char *moo_get_queue_op_name(uint8_t queue_op)
{
    static const char *names[] = {"-", "F", "E", "S"};
    return names[queue_op & 0x03];
}

bool moo_register_state_has_reg16(const moo_register_state_t *state, moo_reg16_t reg)
{
    return (state->bitmask & (1U << (uint32_t)reg)) != 0;
}

uint16_t moo_register_state_get_reg16(const moo_register_state_t *state, moo_reg16_t reg)
{
    return (uint16_t)state->values[reg];
}

bool moo_register_state_has_reg32(const moo_register_state_t *state, moo_reg32_t reg)
{
    return (state->bitmask & (1U << (uint32_t)reg)) != 0;
}

uint32_t moo_register_state_get_reg32(const moo_register_state_t *state, moo_reg32_t reg)
{
    return state->values[reg];
}

uint16_t moo_test_get_initial_reg16(const moo_test_t *test, moo_reg16_t reg)
{
    return moo_register_state_get_reg16(&test->init_state.regs, reg);
}

uint16_t moo_test_get_final_reg16(const moo_test_t *test, moo_reg16_t reg, bool masked)
{
    if (moo_register_state_has_reg16(&test->final_state.regs, reg)) {
        uint16_t ret = moo_register_state_get_reg16(&test->final_state.regs, reg);
        if (masked && moo_register_state_has_reg16(&test->final_state.masks, reg)) {
            ret &= moo_register_state_get_reg16(&test->final_state.masks, reg);
        }
        return ret;
    }
    return moo_test_get_initial_reg16(test, reg);
}

uint32_t moo_test_get_initial_reg32(const moo_test_t *test, moo_reg32_t reg)
{
    return moo_register_state_get_reg32(&test->init_state.regs, reg);
}

uint32_t moo_test_get_final_reg32(const moo_test_t *test, moo_reg32_t reg, bool masked)
{
    if (moo_register_state_has_reg32(&test->final_state.regs, reg)) {
        uint32_t ret = moo_register_state_get_reg32(&test->final_state.regs, reg);
        if (masked && moo_register_state_has_reg32(&test->final_state.masks, reg)) {
            ret &= moo_register_state_get_reg32(&test->final_state.masks, reg);
        }
        return ret;
    }
    return moo_test_get_initial_reg32(test, reg);
}
