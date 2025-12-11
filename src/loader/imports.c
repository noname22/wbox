/*
 * WBOX Import Resolver
 * Parses PE import directory and resolves imports via exports or stubs
 */
#include "imports.h"
#include "exports.h"
#include "stubs.h"
#include "ntdll_stubs.h"
#include "win32k_stubs.h"
#include "../vm/vm.h"
#include "../cpu/mem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>  /* For strcasecmp */

const IMAGE_IMPORT_DESCRIPTOR *imports_get_directory(const pe_image_t *pe)
{
    if (!pe || pe->data_dirs[IMAGE_DIRECTORY_ENTRY_IMPORT].virtual_address == 0) {
        return NULL;
    }

    return (const IMAGE_IMPORT_DESCRIPTOR *)pe_rva_to_ptr(
        pe, pe->data_dirs[IMAGE_DIRECTORY_ENTRY_IMPORT].virtual_address);
}

bool imports_dll_uses_stubs(const char *dll_name)
{
    /* ntdll.dll and win32u.dll use stubs */
    return strcasecmp(dll_name, "ntdll.dll") == 0 ||
           strcasecmp(dll_name, "ntdll") == 0 ||
           strcasecmp(dll_name, "win32u.dll") == 0 ||
           strcasecmp(dll_name, "win32u") == 0;
}

uint32_t imports_resolve_function(module_manager_t *mgr, vm_context_t *vm,
                                  struct stub_manager *stubs,
                                  loaded_module_t *dll_mod,
                                  const char *func_name, uint16_t ordinal,
                                  bool *is_stub)
{
    *is_stub = false;

    /* First, check if this is a known function that should use a stub */
    if (func_name && imports_dll_uses_stubs(dll_mod->name)) {
        const stub_def_t *stub_def = NULL;

        /* Check ntdll stubs */
        if (strcasecmp(dll_mod->name, "ntdll.dll") == 0 ||
            strcasecmp(dll_mod->name, "ntdll") == 0) {
            stub_def = ntdll_lookup_stub(func_name);
        }
        /* Check win32k stubs (win32u.dll) */
        else if (strcasecmp(dll_mod->name, "win32u.dll") == 0 ||
                 strcasecmp(dll_mod->name, "win32u") == 0) {
            stub_def = win32k_lookup_stub(func_name);
        }

        if (stub_def) {
            /* Generate or reuse stub */
            uint32_t stub_va = stubs_get_or_create(stubs, vm, stub_def);
            if (stub_va != 0) {
                *is_stub = true;
                return stub_va;
            }
            /* Fall through to try direct resolution */
        }
    }

    /* Try to resolve from the DLL's exports */
    export_lookup_t lookup;
    if (func_name) {
        lookup = exports_lookup_by_name(dll_mod, func_name);
    } else {
        lookup = exports_lookup_by_ordinal(dll_mod, ordinal);
    }

    if (!lookup.found) {
        return 0;
    }

    /* Handle forwarders by recursively resolving */
    if (lookup.is_forwarder) {
        /* Parse forwarder string: "DLL.Function" or "DLL.#Ordinal" */
        char fwd_dll[256];
        char fwd_func[256];
        const char *dot = strchr(lookup.forwarder, '.');
        if (!dot) {
            fprintf(stderr, "Warning: Invalid forwarder format '%s'\n", lookup.forwarder);
            return 0;
        }

        size_t dll_len = dot - lookup.forwarder;
        if (dll_len >= sizeof(fwd_dll)) dll_len = sizeof(fwd_dll) - 1;
        strncpy(fwd_dll, lookup.forwarder, dll_len);
        fwd_dll[dll_len] = '\0';

        /* Add .dll extension if not present */
        if (!strchr(fwd_dll, '.')) {
            strncat(fwd_dll, ".dll", sizeof(fwd_dll) - strlen(fwd_dll) - 1);
        }

        strncpy(fwd_func, dot + 1, sizeof(fwd_func) - 1);
        fwd_func[sizeof(fwd_func) - 1] = '\0';

        /* Find or load target DLL */
        loaded_module_t *fwd_mod = module_find_by_name(mgr, fwd_dll);
        if (!fwd_mod) {
            fwd_mod = module_load_by_name(mgr, vm, fwd_dll);
            if (!fwd_mod) {
                fprintf(stderr, "Warning: Cannot load forwarder target DLL '%s'\n", fwd_dll);
                return 0;
            }
        }

        /* Resolve in target DLL */
        if (fwd_func[0] == '#') {
            /* Ordinal forwarding: #123 */
            uint16_t fwd_ord = (uint16_t)atoi(fwd_func + 1);
            return imports_resolve_function(mgr, vm, stubs, fwd_mod, NULL, fwd_ord, is_stub);
        } else {
            /* Named forwarding */
            return imports_resolve_function(mgr, vm, stubs, fwd_mod, fwd_func, 0, is_stub);
        }
    }

    /* Return VA of function (DLL base + RVA) */
    return dll_mod->base_va + lookup.rva;
}

static int resolve_dll_imports(module_manager_t *mgr, vm_context_t *vm,
                               loaded_module_t *mod, loaded_module_t *dll_mod,
                               const IMAGE_IMPORT_DESCRIPTOR *imp_desc,
                               import_stats_t *stats)
{
    /* Get INT and IAT pointers */
    uint32_t int_rva = imp_desc->OriginalFirstThunk;
    uint32_t iat_rva = imp_desc->FirstThunk;

    /* If INT is 0, use IAT as both (some linkers do this) */
    if (int_rva == 0) {
        int_rva = iat_rva;
    }

    /* Get pointer to INT in file data */
    const IMAGE_THUNK_DATA32 *int_entry =
        (const IMAGE_THUNK_DATA32 *)pe_rva_to_ptr(&mod->pe, int_rva);
    if (!int_entry) {
        fprintf(stderr, "imports: Failed to get INT pointer\n");
        return -1;
    }

    /* Calculate IAT VA in guest memory */
    uint32_t iat_va = mod->base_va + iat_rva;

    /* Walk INT entries */
    for (int i = 0; int_entry->AddressOfData != 0; int_entry++, i++) {
        const char *func_name = NULL;
        uint16_t ordinal = 0;
        bool by_ordinal = (int_entry->Ordinal & IMAGE_ORDINAL_FLAG32) != 0;

        if (by_ordinal) {
            ordinal = IMAGE_ORDINAL32(int_entry->Ordinal);
        } else {
            /* Get name from IMAGE_IMPORT_BY_NAME */
            const IMAGE_IMPORT_BY_NAME *name_entry =
                (const IMAGE_IMPORT_BY_NAME *)pe_rva_to_ptr(
                    &mod->pe, int_entry->AddressOfData);
            if (!name_entry) {
                fprintf(stderr, "imports: Failed to get import name\n");
                stats->failed_imports++;
                continue;
            }
            func_name = name_entry->Name;
        }

        /* Resolve the function */
        bool is_stub = false;
        uint32_t resolved_va = imports_resolve_function(
            mgr, vm, mgr->stubs, dll_mod, func_name, ordinal, &is_stub);

        if (resolved_va == 0) {
            if (func_name) {
                fprintf(stderr, "imports: Unresolved import %s!%s\n",
                        dll_mod->name, func_name);
            } else {
                fprintf(stderr, "imports: Unresolved import %s!#%u\n",
                        dll_mod->name, ordinal);
            }
            stats->failed_imports++;
            continue;
        }

        /* Patch IAT entry in guest memory */
        uint32_t iat_entry_va = iat_va + i * sizeof(uint32_t);
        uint32_t iat_entry_phys = vm_va_to_phys(vm, iat_entry_va);
        if (iat_entry_phys == 0) {
            fprintf(stderr, "imports: Failed to translate IAT VA 0x%08X\n",
                    iat_entry_va);
            stats->failed_imports++;
            continue;
        }
        mem_writel_phys(iat_entry_phys, resolved_va);

        /* Update stats */
        stats->total_imports++;
        if (is_stub) {
            stats->stubbed_imports++;
        } else {
            stats->direct_imports++;
        }

        /* Debug output for known functions */
        if (func_name) {
            printf("  %s -> 0x%08X%s\n", func_name, resolved_va,
                   is_stub ? " (stub)" : "");
        } else {
            printf("  #%u -> 0x%08X%s\n", ordinal, resolved_va,
                   is_stub ? " (stub)" : "");
        }
    }

    return 0;
}

int imports_resolve(module_manager_t *mgr, vm_context_t *vm,
                    loaded_module_t *mod, import_stats_t *stats)
{
    import_stats_t local_stats = {0};
    if (!stats) {
        stats = &local_stats;
    }

    const IMAGE_IMPORT_DESCRIPTOR *imp_dir = imports_get_directory(&mod->pe);
    if (!imp_dir) {
        /* No imports - that's fine */
        printf("No imports in module %s\n", mod->name);
        return 0;
    }

    printf("Resolving imports for %s:\n", mod->name);

    /* Walk import descriptors */
    for (; imp_dir->Name != 0; imp_dir++) {
        /* Get DLL name */
        const char *dll_name = (const char *)pe_rva_to_ptr(&mod->pe, imp_dir->Name);
        if (!dll_name) {
            fprintf(stderr, "imports: Invalid DLL name RVA\n");
            continue;
        }

        printf("  DLL: %s\n", dll_name);

        /* Load or find the DLL */
        loaded_module_t *dll_mod = module_find_by_name(mgr, dll_name);
        if (!dll_mod) {
            /* Try to load it */
            dll_mod = module_load_by_name(mgr, vm, dll_name);
            if (!dll_mod) {
                fprintf(stderr, "imports: Failed to load DLL: %s\n", dll_name);
                stats->failed_imports++;
                continue;
            }
        }

        /* Resolve imports from this DLL */
        if (resolve_dll_imports(mgr, vm, mod, dll_mod, imp_dir, stats) < 0) {
            fprintf(stderr, "imports: Failed to resolve imports from %s\n", dll_name);
        }
    }

    printf("Import resolution complete: %u total, %u stubbed, %u direct, %u failed\n",
           stats->total_imports, stats->stubbed_imports, stats->direct_imports,
           stats->failed_imports);

    return (stats->failed_imports > 0) ? -1 : 0;
}

void imports_print_stats(const import_stats_t *stats)
{
    printf("Import Statistics:\n");
    printf("  Total:   %u\n", stats->total_imports);
    printf("  Stubbed: %u\n", stats->stubbed_imports);
    printf("  Direct:  %u\n", stats->direct_imports);
    printf("  Failed:  %u\n", stats->failed_imports);
}
