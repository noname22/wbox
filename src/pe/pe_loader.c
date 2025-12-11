/*
 * WBOX PE Loader
 * Parses and loads 32-bit Windows PE executables
 */
#include "pe_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* DOS header (simplified) */
typedef struct {
    uint16_t e_magic;      /* MZ signature */
    uint16_t e_cblp;
    uint16_t e_cp;
    uint16_t e_crlc;
    uint16_t e_cparhdr;
    uint16_t e_minalloc;
    uint16_t e_maxalloc;
    uint16_t e_ss;
    uint16_t e_sp;
    uint16_t e_csum;
    uint16_t e_ip;
    uint16_t e_cs;
    uint16_t e_lfarlc;
    uint16_t e_ovno;
    uint16_t e_res[4];
    uint16_t e_oemid;
    uint16_t e_oeminfo;
    uint16_t e_res2[10];
    uint32_t e_lfanew;     /* Offset to PE header */
} dos_header_t;

/* COFF file header */
typedef struct {
    uint16_t machine;
    uint16_t number_of_sections;
    uint32_t time_date_stamp;
    uint32_t pointer_to_symbol_table;
    uint32_t number_of_symbols;
    uint16_t size_of_optional_header;
    uint16_t characteristics;
} coff_header_t;

/* PE32 optional header */
typedef struct {
    uint16_t magic;
    uint8_t  major_linker_version;
    uint8_t  minor_linker_version;
    uint32_t size_of_code;
    uint32_t size_of_initialized_data;
    uint32_t size_of_uninitialized_data;
    uint32_t address_of_entry_point;
    uint32_t base_of_code;
    uint32_t base_of_data;
    uint32_t image_base;
    uint32_t section_alignment;
    uint32_t file_alignment;
    uint16_t major_operating_system_version;
    uint16_t minor_operating_system_version;
    uint16_t major_image_version;
    uint16_t minor_image_version;
    uint16_t major_subsystem_version;
    uint16_t minor_subsystem_version;
    uint32_t win32_version_value;
    uint32_t size_of_image;
    uint32_t size_of_headers;
    uint32_t checksum;
    uint16_t subsystem;
    uint16_t dll_characteristics;
    uint32_t size_of_stack_reserve;
    uint32_t size_of_stack_commit;
    uint32_t size_of_heap_reserve;
    uint32_t size_of_heap_commit;
    uint32_t loader_flags;
    uint32_t number_of_rva_and_sizes;
    /* Followed by data directories */
} optional_header32_t;

/* Section header */
typedef struct {
    char     name[8];
    uint32_t virtual_size;
    uint32_t virtual_address;
    uint32_t size_of_raw_data;
    uint32_t pointer_to_raw_data;
    uint32_t pointer_to_relocations;
    uint32_t pointer_to_linenumbers;
    uint16_t number_of_relocations;
    uint16_t number_of_linenumbers;
    uint32_t characteristics;
} section_header_t;

static int read_file(const char *path, uint8_t **data, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "pe_load: cannot open file '%s'\n", path);
        return -1;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (len <= 0) {
        fprintf(stderr, "pe_load: empty or invalid file '%s'\n", path);
        fclose(f);
        return -1;
    }

    *data = malloc(len);
    if (!*data) {
        fprintf(stderr, "pe_load: out of memory\n");
        fclose(f);
        return -1;
    }

    if (fread(*data, 1, len, f) != (size_t)len) {
        fprintf(stderr, "pe_load: read error on '%s'\n", path);
        free(*data);
        fclose(f);
        return -1;
    }

    fclose(f);
    *size = len;
    return 0;
}

int pe_load(const char *path, pe_image_t *pe)
{
    memset(pe, 0, sizeof(*pe));

    /* Read entire file into memory */
    if (read_file(path, &pe->file_data, &pe->file_size) != 0) {
        return -1;
    }

    uint8_t *data = pe->file_data;
    size_t size = pe->file_size;

    /* Validate DOS header */
    if (size < sizeof(dos_header_t)) {
        fprintf(stderr, "pe_load: file too small for DOS header\n");
        goto fail;
    }

    dos_header_t *dos = (dos_header_t *)data;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        fprintf(stderr, "pe_load: invalid DOS signature (expected MZ)\n");
        goto fail;
    }

    /* Find PE header */
    uint32_t pe_offset = dos->e_lfanew;
    if (pe_offset + sizeof(uint32_t) + sizeof(coff_header_t) > size) {
        fprintf(stderr, "pe_load: invalid PE offset\n");
        goto fail;
    }

    /* Validate PE signature */
    uint32_t pe_sig = *(uint32_t *)(data + pe_offset);
    if (pe_sig != IMAGE_NT_SIGNATURE) {
        fprintf(stderr, "pe_load: invalid PE signature\n");
        goto fail;
    }

    /* Parse COFF header */
    coff_header_t *coff = (coff_header_t *)(data + pe_offset + 4);
    if (coff->machine != IMAGE_FILE_MACHINE_I386) {
        fprintf(stderr, "pe_load: not a 32-bit x86 PE (machine=0x%04x)\n", coff->machine);
        goto fail;
    }

    if (coff->size_of_optional_header < sizeof(optional_header32_t)) {
        fprintf(stderr, "pe_load: optional header too small\n");
        goto fail;
    }

    /* Parse optional header */
    optional_header32_t *opt = (optional_header32_t *)(data + pe_offset + 4 + sizeof(coff_header_t));
    if (opt->magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        fprintf(stderr, "pe_load: not a PE32 image (magic=0x%04x)\n", opt->magic);
        goto fail;
    }

    pe->image_base = opt->image_base;
    pe->entry_point_rva = opt->address_of_entry_point;
    pe->size_of_image = opt->size_of_image;
    pe->size_of_headers = opt->size_of_headers;
    pe->section_alignment = opt->section_alignment;
    pe->file_alignment = opt->file_alignment;
    pe->subsystem = opt->subsystem;
    pe->dll_characteristics = opt->dll_characteristics;
    pe->size_of_stack_reserve = opt->size_of_stack_reserve;
    pe->size_of_stack_commit = opt->size_of_stack_commit;
    pe->size_of_heap_reserve = opt->size_of_heap_reserve;
    pe->size_of_heap_commit = opt->size_of_heap_commit;

    /* Parse data directories */
    uint32_t num_dirs = opt->number_of_rva_and_sizes;
    if (num_dirs > IMAGE_NUMBEROF_DIRECTORY_ENTRIES) {
        num_dirs = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;
    }

    uint8_t *dir_ptr = (uint8_t *)opt + sizeof(optional_header32_t);
    for (uint32_t i = 0; i < num_dirs; i++) {
        pe->data_dirs[i].virtual_address = *(uint32_t *)(dir_ptr + i * 8);
        pe->data_dirs[i].size = *(uint32_t *)(dir_ptr + i * 8 + 4);
    }

    /* Parse section headers */
    pe->num_sections = coff->number_of_sections;
    if (pe->num_sections == 0) {
        fprintf(stderr, "pe_load: no sections\n");
        goto fail;
    }

    pe->sections = calloc(pe->num_sections, sizeof(pe_section_t));
    if (!pe->sections) {
        fprintf(stderr, "pe_load: out of memory for sections\n");
        goto fail;
    }

    uint8_t *section_ptr = data + pe_offset + 4 + sizeof(coff_header_t) + coff->size_of_optional_header;
    for (uint16_t i = 0; i < pe->num_sections; i++) {
        section_header_t *sh = (section_header_t *)(section_ptr + i * sizeof(section_header_t));

        /* Copy name (ensure null termination) */
        memcpy(pe->sections[i].name, sh->name, 8);
        pe->sections[i].name[8] = '\0';

        pe->sections[i].virtual_size = sh->virtual_size;
        pe->sections[i].virtual_address = sh->virtual_address;
        pe->sections[i].raw_size = sh->size_of_raw_data;
        pe->sections[i].raw_offset = sh->pointer_to_raw_data;
        pe->sections[i].characteristics = sh->characteristics;
    }

    return 0;

fail:
    pe_free(pe);
    return -1;
}

void pe_free(pe_image_t *pe)
{
    if (pe->file_data) {
        free(pe->file_data);
        pe->file_data = NULL;
    }
    if (pe->sections) {
        free(pe->sections);
        pe->sections = NULL;
    }
    pe->num_sections = 0;
}

const pe_section_t *pe_get_section_by_rva(const pe_image_t *pe, uint32_t rva)
{
    for (uint16_t i = 0; i < pe->num_sections; i++) {
        const pe_section_t *s = &pe->sections[i];
        uint32_t section_size = s->virtual_size;
        if (section_size == 0) {
            section_size = s->raw_size;
        }
        if (rva >= s->virtual_address && rva < s->virtual_address + section_size) {
            return s;
        }
    }
    return NULL;
}

uint32_t pe_rva_to_file_offset(const pe_image_t *pe, uint32_t rva)
{
    /* Check if RVA is in headers */
    if (rva < pe->size_of_headers) {
        return rva;
    }

    /* Find section containing RVA */
    const pe_section_t *s = pe_get_section_by_rva(pe, rva);
    if (!s || s->raw_size == 0) {
        return 0;
    }

    uint32_t offset_in_section = rva - s->virtual_address;
    if (offset_in_section >= s->raw_size) {
        return 0;
    }

    return s->raw_offset + offset_in_section;
}

const void *pe_rva_to_ptr(const pe_image_t *pe, uint32_t rva)
{
    uint32_t file_offset = pe_rva_to_file_offset(pe, rva);
    if (file_offset == 0 && rva != 0) {
        return NULL;
    }
    if (file_offset >= pe->file_size) {
        return NULL;
    }
    return pe->file_data + file_offset;
}

void pe_dump_info(const pe_image_t *pe)
{
    printf("PE Image Info:\n");
    printf("  ImageBase:        0x%08X\n", pe->image_base);
    printf("  EntryPoint (RVA): 0x%08X\n", pe->entry_point_rva);
    printf("  EntryPoint (VA):  0x%08X\n", pe->image_base + pe->entry_point_rva);
    printf("  SizeOfImage:      0x%08X (%u KB)\n", pe->size_of_image, pe->size_of_image / 1024);
    printf("  SizeOfHeaders:    0x%08X\n", pe->size_of_headers);
    printf("  SectionAlignment: 0x%08X\n", pe->section_alignment);
    printf("  FileAlignment:    0x%08X\n", pe->file_alignment);
    printf("  Subsystem:        %u (%s)\n", pe->subsystem,
           pe->subsystem == IMAGE_SUBSYSTEM_WINDOWS_CUI ? "Console" :
           pe->subsystem == IMAGE_SUBSYSTEM_WINDOWS_GUI ? "GUI" :
           pe->subsystem == IMAGE_SUBSYSTEM_NATIVE ? "Native" : "Unknown");
    printf("  StackReserve:     0x%08X\n", pe->size_of_stack_reserve);
    printf("  StackCommit:      0x%08X\n", pe->size_of_stack_commit);
    printf("\n");

    printf("Sections (%u):\n", pe->num_sections);
    for (uint16_t i = 0; i < pe->num_sections; i++) {
        const pe_section_t *s = &pe->sections[i];
        printf("  [%u] %-8s  VA=0x%08X  VSize=0x%08X  Raw=0x%08X  RawSize=0x%08X  ",
               i, s->name, s->virtual_address, s->virtual_size,
               s->raw_offset, s->raw_size);

        if (s->characteristics & IMAGE_SCN_MEM_READ)    printf("R");
        if (s->characteristics & IMAGE_SCN_MEM_WRITE)   printf("W");
        if (s->characteristics & IMAGE_SCN_MEM_EXECUTE) printf("X");
        if (s->characteristics & IMAGE_SCN_CNT_CODE)    printf(" CODE");
        if (s->characteristics & IMAGE_SCN_CNT_INITIALIZED_DATA)   printf(" IDATA");
        if (s->characteristics & IMAGE_SCN_CNT_UNINITIALIZED_DATA) printf(" UDATA");
        printf("\n");
    }
    printf("\n");

    /* Data directories of interest */
    printf("Data Directories:\n");
    const char *dir_names[] = {
        "Export", "Import", "Resource", "Exception", "Security",
        "BaseReloc", "Debug", "Architecture", "GlobalPtr", "TLS",
        "LoadConfig", "BoundImport", "IAT", "DelayImport", "CLR", "Reserved"
    };
    for (int i = 0; i < IMAGE_NUMBEROF_DIRECTORY_ENTRIES; i++) {
        if (pe->data_dirs[i].virtual_address != 0) {
            printf("  [%2d] %-12s  VA=0x%08X  Size=0x%08X\n",
                   i, dir_names[i], pe->data_dirs[i].virtual_address, pe->data_dirs[i].size);
        }
    }
}
