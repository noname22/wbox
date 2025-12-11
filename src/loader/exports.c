/*
 * WBOX Export Table Parser
 * Parses PE export directory for DLL symbol resolution
 */
#include "exports.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const IMAGE_EXPORT_DIRECTORY *exports_get_directory(const pe_image_t *pe)
{
    if (!pe || pe->data_dirs[IMAGE_DIRECTORY_ENTRY_EXPORT].virtual_address == 0) {
        return NULL;
    }

    return (const IMAGE_EXPORT_DIRECTORY *)pe_rva_to_ptr(
        pe, pe->data_dirs[IMAGE_DIRECTORY_ENTRY_EXPORT].virtual_address);
}

int exports_parse(const pe_image_t *pe, loaded_module_t *mod)
{
    const IMAGE_EXPORT_DIRECTORY *exp_dir = exports_get_directory(pe);
    if (!exp_dir) {
        /* No exports - that's OK for executables */
        mod->num_exports = 0;
        mod->exports = NULL;
        mod->ordinal_base = 0;
        return 0;
    }

    uint32_t exp_dir_rva = pe->data_dirs[IMAGE_DIRECTORY_ENTRY_EXPORT].virtual_address;
    uint32_t exp_dir_size = pe->data_dirs[IMAGE_DIRECTORY_ENTRY_EXPORT].size;
    uint32_t exp_dir_end = exp_dir_rva + exp_dir_size;

    mod->num_exports = exp_dir->NumberOfFunctions;
    mod->ordinal_base = exp_dir->Base;

    if (mod->num_exports == 0) {
        mod->exports = NULL;
        return 0;
    }

    mod->exports = calloc(mod->num_exports, sizeof(export_entry_t));
    if (!mod->exports) {
        fprintf(stderr, "exports_parse: Out of memory\n");
        return -1;
    }

    /* Get pointers to the three tables */
    const uint32_t *eat = (const uint32_t *)pe_rva_to_ptr(pe, exp_dir->AddressOfFunctions);
    const uint32_t *name_ptrs = exp_dir->AddressOfNames ?
        (const uint32_t *)pe_rva_to_ptr(pe, exp_dir->AddressOfNames) : NULL;
    const uint16_t *ordinals = exp_dir->AddressOfNameOrdinals ?
        (const uint16_t *)pe_rva_to_ptr(pe, exp_dir->AddressOfNameOrdinals) : NULL;

    if (!eat) {
        fprintf(stderr, "exports_parse: Invalid EAT pointer\n");
        free(mod->exports);
        mod->exports = NULL;
        mod->num_exports = 0;
        return -1;
    }

    /* Build export table */
    for (uint32_t i = 0; i < mod->num_exports; i++) {
        uint32_t func_rva = eat[i];

        mod->exports[i].ordinal = (uint16_t)(exp_dir->Base + i);
        mod->exports[i].rva = func_rva;
        mod->exports[i].name = NULL;
        mod->exports[i].is_forwarder = false;
        mod->exports[i].forwarder_name = NULL;

        /* Check if this is a forwarder (RVA points inside export directory) */
        if (func_rva >= exp_dir_rva && func_rva < exp_dir_end) {
            const char *forwarder = (const char *)pe_rva_to_ptr(pe, func_rva);
            if (forwarder) {
                mod->exports[i].is_forwarder = true;
                mod->exports[i].forwarder_name = strdup(forwarder);
            }
        }
    }

    /* Associate names with ordinals */
    if (name_ptrs && ordinals) {
        for (uint32_t i = 0; i < exp_dir->NumberOfNames; i++) {
            uint16_t ordinal_idx = ordinals[i];
            if (ordinal_idx < mod->num_exports) {
                const char *name = (const char *)pe_rva_to_ptr(pe, name_ptrs[i]);
                if (name) {
                    mod->exports[ordinal_idx].name = strdup(name);
                }
            }
        }
    }

    /* Print summary */
    const char *dll_name = exp_dir->Name ?
        (const char *)pe_rva_to_ptr(pe, exp_dir->Name) : "unknown";
    printf("Parsed exports for %s: %u functions, %u named, ordinal base %u\n",
           dll_name, exp_dir->NumberOfFunctions, exp_dir->NumberOfNames, exp_dir->Base);

    return 0;
}

export_lookup_t exports_lookup_by_name(const loaded_module_t *mod, const char *name)
{
    export_lookup_t result = { .found = false };

    if (!mod || !name || !mod->exports) {
        return result;
    }

    /* Linear search through exports looking for matching name */
    for (uint32_t i = 0; i < mod->num_exports; i++) {
        if (mod->exports[i].name && strcmp(mod->exports[i].name, name) == 0) {
            result.found = true;
            result.rva = mod->exports[i].rva;
            result.ordinal = mod->exports[i].ordinal;
            result.is_forwarder = mod->exports[i].is_forwarder;
            result.forwarder = mod->exports[i].forwarder_name;
            return result;
        }
    }

    return result;
}

export_lookup_t exports_lookup_by_ordinal(const loaded_module_t *mod, uint16_t ordinal)
{
    export_lookup_t result = { .found = false };

    if (!mod || !mod->exports) {
        return result;
    }

    /* Convert ordinal to index */
    if (ordinal < mod->ordinal_base) {
        return result;
    }

    uint32_t idx = ordinal - mod->ordinal_base;
    if (idx >= mod->num_exports) {
        return result;
    }

    /* Check if this entry is valid (RVA != 0) */
    if (mod->exports[idx].rva == 0) {
        return result;
    }

    result.found = true;
    result.rva = mod->exports[idx].rva;
    result.ordinal = mod->exports[idx].ordinal;
    result.is_forwarder = mod->exports[idx].is_forwarder;
    result.forwarder = mod->exports[idx].forwarder_name;

    return result;
}
