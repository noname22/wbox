/*
 * WBOX Export Table Parser
 * Parses PE export directory for DLL symbol resolution
 */
#ifndef WBOX_EXPORTS_H
#define WBOX_EXPORTS_H

#include <stdint.h>
#include <stdbool.h>
#include "../pe/pe_loader.h"
#include "module.h"

/* IMAGE_EXPORT_DIRECTORY structure */
typedef struct {
    uint32_t Characteristics;
    uint32_t TimeDateStamp;
    uint16_t MajorVersion;
    uint16_t MinorVersion;
    uint32_t Name;                  /* RVA to DLL name */
    uint32_t Base;                  /* Ordinal base */
    uint32_t NumberOfFunctions;     /* Number of entries in EAT */
    uint32_t NumberOfNames;         /* Number of named exports */
    uint32_t AddressOfFunctions;    /* RVA to Export Address Table (EAT) */
    uint32_t AddressOfNames;        /* RVA to name pointer table */
    uint32_t AddressOfNameOrdinals; /* RVA to ordinal table */
} IMAGE_EXPORT_DIRECTORY;

/* Export lookup result */
typedef struct {
    bool        found;
    uint32_t    rva;                /* RVA of function */
    uint16_t    ordinal;            /* Ordinal value */
    bool        is_forwarder;       /* Is this a forwarder string? */
    const char  *forwarder;         /* Forwarder string (DLL.Function) */
} export_lookup_t;

/*
 * Parse export directory of a PE image
 * Populates mod->exports array with all exported functions
 * Returns 0 on success, -1 on error
 */
int exports_parse(const pe_image_t *pe, loaded_module_t *mod);

/*
 * Lookup export by name
 * Returns result with found=false if not found
 */
export_lookup_t exports_lookup_by_name(const loaded_module_t *mod,
                                       const char *name);

/*
 * Lookup export by ordinal
 * Returns result with found=false if not found
 */
export_lookup_t exports_lookup_by_ordinal(const loaded_module_t *mod,
                                          uint16_t ordinal);

/*
 * Get pointer to export directory from PE image
 * Returns NULL if no export directory exists
 */
const IMAGE_EXPORT_DIRECTORY *exports_get_directory(const pe_image_t *pe);

#endif /* WBOX_EXPORTS_H */
