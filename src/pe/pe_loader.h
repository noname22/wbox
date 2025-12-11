/*
 * WBOX PE Loader
 * Parses and loads 32-bit Windows PE executables
 */
#ifndef WBOX_PE_LOADER_H
#define WBOX_PE_LOADER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* PE section characteristics */
#define IMAGE_SCN_CNT_CODE               0x00000020
#define IMAGE_SCN_CNT_INITIALIZED_DATA   0x00000040
#define IMAGE_SCN_CNT_UNINITIALIZED_DATA 0x00000080
#define IMAGE_SCN_MEM_EXECUTE            0x20000000
#define IMAGE_SCN_MEM_READ               0x40000000
#define IMAGE_SCN_MEM_WRITE              0x80000000

/* PE machine type */
#define IMAGE_FILE_MACHINE_I386          0x014c

/* PE magic numbers */
#define IMAGE_DOS_SIGNATURE              0x5A4D      /* MZ */
#define IMAGE_NT_SIGNATURE               0x00004550  /* PE\0\0 */
#define IMAGE_NT_OPTIONAL_HDR32_MAGIC    0x10b

/* Data directory indices */
#define IMAGE_DIRECTORY_ENTRY_EXPORT     0
#define IMAGE_DIRECTORY_ENTRY_IMPORT     1
#define IMAGE_DIRECTORY_ENTRY_RESOURCE   2
#define IMAGE_DIRECTORY_ENTRY_EXCEPTION  3
#define IMAGE_DIRECTORY_ENTRY_SECURITY   4
#define IMAGE_DIRECTORY_ENTRY_BASERELOC  5
#define IMAGE_DIRECTORY_ENTRY_DEBUG      6
#define IMAGE_DIRECTORY_ENTRY_TLS        9
#define IMAGE_DIRECTORY_ENTRY_IAT        12

#define IMAGE_NUMBEROF_DIRECTORY_ENTRIES 16

/* Base relocation types */
#define IMAGE_REL_BASED_ABSOLUTE         0
#define IMAGE_REL_BASED_HIGHLOW          3

/* Section info */
typedef struct {
    char     name[9];          /* Null-terminated section name */
    uint32_t virtual_size;     /* Size in memory */
    uint32_t virtual_address;  /* RVA */
    uint32_t raw_size;         /* Size in file */
    uint32_t raw_offset;       /* Offset in file */
    uint32_t characteristics;  /* R/W/X flags */
} pe_section_t;

/* Data directory entry */
typedef struct {
    uint32_t virtual_address;
    uint32_t size;
} pe_data_directory_t;

/* Loaded PE image info */
typedef struct {
    /* Image info from optional header */
    uint32_t image_base;           /* Preferred load address */
    uint32_t entry_point_rva;      /* Entry point RVA */
    uint32_t size_of_image;        /* Total image size when mapped */
    uint32_t size_of_headers;      /* Size of all headers */
    uint32_t section_alignment;    /* Memory section alignment */
    uint32_t file_alignment;       /* File section alignment */

    /* Sections */
    uint16_t      num_sections;
    pe_section_t *sections;

    /* Data directories */
    pe_data_directory_t data_dirs[IMAGE_NUMBEROF_DIRECTORY_ENTRIES];

    /* Raw file data (owned by pe_image_t) */
    uint8_t *file_data;
    size_t   file_size;

    /* Subsystem info */
    uint16_t subsystem;
    uint16_t dll_characteristics;
    uint32_t size_of_stack_reserve;
    uint32_t size_of_stack_commit;
    uint32_t size_of_heap_reserve;
    uint32_t size_of_heap_commit;
} pe_image_t;

/* Subsystem values */
#define IMAGE_SUBSYSTEM_UNKNOWN          0
#define IMAGE_SUBSYSTEM_NATIVE           1
#define IMAGE_SUBSYSTEM_WINDOWS_GUI      2
#define IMAGE_SUBSYSTEM_WINDOWS_CUI      3

/*
 * Load a PE file from disk
 * Returns 0 on success, -1 on error
 */
int pe_load(const char *path, pe_image_t *pe);

/*
 * Free resources allocated by pe_load
 */
void pe_free(pe_image_t *pe);

/*
 * Get section containing a given RVA
 * Returns NULL if RVA is not in any section
 */
const pe_section_t *pe_get_section_by_rva(const pe_image_t *pe, uint32_t rva);

/*
 * Convert RVA to file offset
 * Returns 0 on failure (RVA not mapped to file)
 */
uint32_t pe_rva_to_file_offset(const pe_image_t *pe, uint32_t rva);

/*
 * Get pointer to data at given RVA within file_data
 * Returns NULL if RVA is invalid
 */
const void *pe_rva_to_ptr(const pe_image_t *pe, uint32_t rva);

/*
 * Print PE info to stdout (for debugging)
 */
void pe_dump_info(const pe_image_t *pe);

#endif /* WBOX_PE_LOADER_H */
