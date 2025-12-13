/*
 * WBOX PE Loader
 * High-level API for loading PE executables with DLL support
 */
#include "loader.h"
#include "module.h"
#include "exports.h"
#include "imports.h"
#include "stubs.h"
#include "../vm/vm.h"
#include "../vm/paging.h"
#include "../cpu/mem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <libgen.h>

/* Default base address for DLLs */
#define NTDLL_DEFAULT_BASE  0x7C800000

int loader_init(loader_context_t *ctx, vm_context_t *vm)
{
    memset(ctx, 0, sizeof(*ctx));

    /* Initialize module manager */
    if (module_manager_init(&ctx->modules, vm) < 0) {
        fprintf(stderr, "loader_init: Failed to initialize module manager\n");
        return -1;
    }

    /* Initialize stub manager */
    if (stubs_init(&ctx->stubs, vm) < 0) {
        fprintf(stderr, "loader_init: Failed to initialize stub manager\n");
        return -1;
    }

    /* Link stub manager to module manager */
    ctx->modules.stubs = &ctx->stubs;

    printf("Loader initialized\n");
    return 0;
}

void loader_set_ntdll_path(loader_context_t *ctx, const char *path)
{
    ctx->ntdll_path = path;
    module_manager_set_ntdll_path(&ctx->modules, path);
}

static loaded_module_t *load_pe_internal(loader_context_t *ctx, vm_context_t *vm,
                                          const char *path, uint32_t preferred_base,
                                          bool is_main_exe)
{
    pe_image_t pe;

    if (pe_load(path, &pe) != 0) {
        fprintf(stderr, "loader: Failed to load PE file: %s\n", path);
        return NULL;
    }

    pe_dump_info(&pe);

    /* Allocate module structure */
    loaded_module_t *mod = calloc(1, sizeof(*mod));
    if (!mod) {
        fprintf(stderr, "loader: Out of memory for module\n");
        pe_free(&pe);
        return NULL;
    }

    /* Extract base name for module name */
    const char *base_name = strrchr(path, '/');
    if (base_name) {
        base_name++;
    } else {
        base_name = strrchr(path, '\\');
        if (base_name) {
            base_name++;
        } else {
            base_name = path;
        }
    }
    strncpy(mod->name, base_name, sizeof(mod->name) - 1);

    /* Store PE info */
    mod->pe = pe;
    mod->is_main_exe = is_main_exe;

    /* Determine load address */
    uint32_t load_base;
    if (preferred_base != 0) {
        load_base = preferred_base;
    } else if (pe.image_base != 0) {
        load_base = pe.image_base;
    } else {
        load_base = 0x00400000;  /* Default */
    }

    mod->base_va = load_base;
    mod->size = pe.size_of_image;
    mod->entry_point = (pe.entry_point_rva != 0) ?
                       load_base + pe.entry_point_rva : 0;

    printf("Loading %s at 0x%08X, entry point 0x%08X\n",
           mod->name, mod->base_va, mod->entry_point);

    /* Allocate physical memory for the image */
    uint32_t image_phys = paging_alloc_phys(&vm->paging, pe.size_of_image);
    if (image_phys == 0) {
        fprintf(stderr, "loader: Failed to allocate physical memory\n");
        module_free(mod);
        return NULL;
    }
    mod->phys_base = image_phys;

    /* Copy PE headers */
    for (uint32_t i = 0; i < pe.size_of_headers; i++) {
        mem_writeb_phys(image_phys + i, pe.file_data[i]);
    }

    /* Copy sections */
    for (int i = 0; i < pe.num_sections; i++) {
        const pe_section_t *sec = &pe.sections[i];
        uint32_t sec_phys = image_phys + sec->virtual_address;
        uint32_t copy_size = (sec->raw_size < sec->virtual_size)
                           ? sec->raw_size : sec->virtual_size;

        printf("  Section %s: VA=0x%08X size=0x%X -> phys=0x%08X\n",
               sec->name, sec->virtual_address, sec->virtual_size, sec_phys);

        /* Copy raw data */
        for (uint32_t j = 0; j < copy_size; j++) {
            mem_writeb_phys(sec_phys + j, pe.file_data[sec->raw_offset + j]);
        }

        /* Zero-fill remainder (BSS-like) */
        if (sec->virtual_size > copy_size) {
            for (uint32_t j = copy_size; j < sec->virtual_size; j++) {
                mem_writeb_phys(sec_phys + j, 0);
            }
        }
    }

    /* Apply base relocations if needed */
    if (pe.data_dirs[IMAGE_DIRECTORY_ENTRY_BASERELOC].size > 0) {
        uint32_t reloc_rva = pe.data_dirs[IMAGE_DIRECTORY_ENTRY_BASERELOC].virtual_address;
        uint32_t reloc_size = pe.data_dirs[IMAGE_DIRECTORY_ENTRY_BASERELOC].size;
        int64_t delta = (int64_t)mod->base_va - (int64_t)pe.image_base;

        if (delta != 0) {
            printf("Applying relocations (delta=%lld)\n", (long long)delta);

            uint32_t offset = 0;
            while (offset < reloc_size) {
                uint32_t block_rva = *(uint32_t *)(pe.file_data +
                                      pe_rva_to_file_offset(&pe, reloc_rva + offset));
                uint32_t block_size = *(uint32_t *)(pe.file_data +
                                       pe_rva_to_file_offset(&pe, reloc_rva + offset + 4));

                if (block_size == 0)
                    break;

                uint32_t entry_count = (block_size - 8) / 2;
                for (uint32_t i = 0; i < entry_count; i++) {
                    uint16_t entry = *(uint16_t *)(pe.file_data +
                                      pe_rva_to_file_offset(&pe, reloc_rva + offset + 8 + i * 2));
                    uint8_t type = entry >> 12;
                    uint16_t off = entry & 0xFFF;

                    if (type == IMAGE_REL_BASED_HIGHLOW) {
                        uint32_t addr_phys = image_phys + block_rva + off;
                        uint32_t val = mem_readl_phys(addr_phys);
                        mem_writel_phys(addr_phys, val + (uint32_t)delta);
                    }
                }
                offset += block_size;
            }
        }
    }

    /* Map the PE image into virtual address space */
    uint32_t map_flags = PTE_USER | PTE_WRITABLE;
    if (paging_map_range(&vm->paging, mod->base_va, image_phys,
                         pe.size_of_image, map_flags) != 0) {
        fprintf(stderr, "loader: Failed to map PE image\n");
        module_free(mod);
        return NULL;
    }

    /* Parse exports */
    if (exports_parse(&pe, mod) < 0) {
        fprintf(stderr, "loader: Warning: Failed to parse exports for %s\n", mod->name);
    }

    /* Add to module list */
    mod->next = ctx->modules.modules;
    ctx->modules.modules = mod;
    ctx->modules.module_count++;

    return mod;
}

/* Implementation of module_load_by_name - loads DLL by name */
loaded_module_t *module_load_by_name(module_manager_t *mgr, vm_context_t *vm,
                                     const char *dll_name)
{
    /* First check if already loaded */
    loaded_module_t *existing = module_find_by_name(mgr, dll_name);
    if (existing) {
        return existing;
    }

    /* For ntdll.dll, use the configured path */
    if (strcasecmp(dll_name, "ntdll.dll") == 0 || strcasecmp(dll_name, "ntdll") == 0) {
        if (mgr->ntdll_path == NULL) {
            fprintf(stderr, "module_load_by_name: ntdll.dll requested but no path configured\n");
            fprintf(stderr, "  Use --ntdll <path> to specify ntdll.dll location\n");
            return NULL;
        }

        /* Get loader context from stub manager link */
        loader_context_t *ctx = (loader_context_t *)((char *)mgr -
                                offsetof(loader_context_t, modules));

        return load_pe_internal(ctx, vm, mgr->ntdll_path, NTDLL_DEFAULT_BASE, false);
    }

    /* For other DLLs, try to find them in the VFS */
    char dll_path[VFS_MAX_PATH];
    if (vfs_find_dll(&vm->vfs_jail, dll_name, dll_path) != 0) {
        fprintf(stderr, "module_load_by_name: Cannot find DLL '%s' in VFS\n", dll_name);
        return NULL;
    }

    /* Get loader context from stub manager link */
    loader_context_t *ctx = (loader_context_t *)((char *)mgr -
                            offsetof(loader_context_t, modules));

    printf("Loading DLL: %s from %s\n", dll_name, dll_path);
    return load_pe_internal(ctx, vm, dll_path, 0, false);
}

/* Implementation of module_load - loads PE from path */
loaded_module_t *module_load(module_manager_t *mgr, vm_context_t *vm,
                             const char *path, uint32_t preferred_base)
{
    /* Get loader context from stub manager link */
    loader_context_t *ctx = (loader_context_t *)((char *)mgr -
                            offsetof(loader_context_t, modules));

    return load_pe_internal(ctx, vm, path, preferred_base, false);
}

int loader_load_executable(loader_context_t *ctx, vm_context_t *vm,
                           const char *exe_path)
{
    printf("\n=== Loading executable: %s ===\n", exe_path);

    /* Initialize PEB_LDR_DATA */
    if (module_init_peb_ldr(&ctx->modules, vm) < 0) {
        fprintf(stderr, "loader: Failed to initialize PEB_LDR_DATA\n");
        return -1;
    }

    /* Load the main executable */
    ctx->main_module = load_pe_internal(ctx, vm, exe_path, 0, true);
    if (!ctx->main_module) {
        fprintf(stderr, "loader: Failed to load main executable\n");
        return -1;
    }

    /* Create LDR entry for main executable */
    if (module_create_ldr_entry(&ctx->modules, vm, ctx->main_module) < 0) {
        fprintf(stderr, "loader: Failed to create LDR entry for main exe\n");
        return -1;
    }

    /* Resolve imports for main executable
     * This will recursively load any required DLLs */
    memset(&ctx->import_stats, 0, sizeof(ctx->import_stats));
    if (imports_resolve(&ctx->modules, vm, ctx->main_module, &ctx->import_stats) < 0) {
        fprintf(stderr, "loader: Warning: Some imports failed to resolve\n");
    }

    /* Resolve imports for all loaded DLLs as well
     * DLLs like kernel32.dll import from ntdll.dll, etc.
     * We need to iterate multiple times because resolving imports may load new DLLs.
     * Keep iterating until no new DLLs are loaded in a full pass. */
    printf("\nResolving imports for dependent DLLs...\n");
    int imports_resolved;
    do {
        imports_resolved = 0;
        loaded_module_t *dll = ctx->modules.modules;
        while (dll) {
            /* Only process DLLs that haven't had imports resolved yet */
            if (!dll->is_main_exe && !dll->imports_resolved &&
                dll->pe.data_dirs[IMAGE_DIRECTORY_ENTRY_IMPORT].size > 0) {
                import_stats_t dll_stats = {0};
                printf("  Resolving imports for %s\n", dll->name);
                imports_resolve(&ctx->modules, vm, dll, &dll_stats);
                dll->imports_resolved = true;
                imports_resolved++;
                ctx->import_stats.total_imports += dll_stats.total_imports;
                ctx->import_stats.stubbed_imports += dll_stats.stubbed_imports;
                ctx->import_stats.direct_imports += dll_stats.direct_imports;
                ctx->import_stats.failed_imports += dll_stats.failed_imports;
            }
            dll = dll->next;
        }
    } while (imports_resolved > 0);

    /* Create LDR entries for any loaded DLLs */
    loaded_module_t *mod = ctx->modules.modules;
    while (mod) {
        if (!mod->is_main_exe && mod->ldr_entry_va == 0) {
            if (module_create_ldr_entry(&ctx->modules, vm, mod) < 0) {
                fprintf(stderr, "loader: Failed to create LDR entry for %s\n", mod->name);
            }
        }
        mod = mod->next;
    }

    /* Find ntdll.dll and initialize LdrpHashTable */
    loaded_module_t *ntdll = module_find_by_name(&ctx->modules, "ntdll.dll");
    if (ntdll) {
        /* Initialize the hash table with empty circular lists */
        module_init_ldrp_hash_table(&ctx->modules, vm, ntdll);

        /* Link all modules into the hash table */
        mod = ctx->modules.modules;
        while (mod) {
            if (mod->ldr_entry_va != 0) {
                module_link_to_hash_table(&ctx->modules, vm, ntdll, mod);
            }
            mod = mod->next;
        }

        /* Initialize RtlpTimeout in ntdll's BSS section.
         * This variable is normally initialized by LdrpInitialize which copies
         * PEB.CriticalSectionTimeout to RtlpTimeout. Since we don't run the full
         * ntdll initialization, we need to set this manually.
         * RVA 0x60768 is the location of RtlpTimeout in ReactOS ntdll.dll */
        #define NTDLL_RTLP_TIMEOUT_RVA  0x60768
        uint32_t rtlp_timeout_va = ntdll->base_va + NTDLL_RTLP_TIMEOUT_RVA;
        uint32_t rtlp_timeout_phys = paging_get_phys(&vm->paging, rtlp_timeout_va);
        if (rtlp_timeout_phys != 0) {
            /* Same value as PEB.CriticalSectionTimeout: -1,500,000,000 (150 seconds)
             * Stored as LARGE_INTEGER (8 bytes, little-endian) */
            mem_writel_phys(rtlp_timeout_phys, 0xA697D100);      /* Low DWORD */
            mem_writel_phys(rtlp_timeout_phys + 4, 0xFFFFFFFF);  /* High DWORD */
            printf("Initialized RtlpTimeout at 0x%08X to 150 seconds\n", rtlp_timeout_va);
        }
    }

    printf("\n=== Loading complete ===\n");
    loader_print_status(ctx);

    return 0;
}

uint32_t loader_get_entry_point(const loader_context_t *ctx)
{
    if (ctx->main_module) {
        return ctx->main_module->entry_point;
    }
    return 0;
}

uint32_t loader_get_image_base(const loader_context_t *ctx)
{
    if (ctx->main_module) {
        return ctx->main_module->base_va;
    }
    return 0;
}

loaded_module_t *loader_get_main_module(loader_context_t *ctx)
{
    return ctx->main_module;
}

void loader_free(loader_context_t *ctx)
{
    module_manager_free(&ctx->modules);
    stubs_free(&ctx->stubs);
    memset(ctx, 0, sizeof(*ctx));
}

void loader_print_status(const loader_context_t *ctx)
{
    printf("\nLoader Status:\n");
    printf("  Modules loaded: %u\n", ctx->modules.module_count);

    loaded_module_t *mod = ctx->modules.modules;
    while (mod) {
        printf("    %s: base=0x%08X size=0x%X entry=0x%08X%s\n",
               mod->name, mod->base_va, mod->size, mod->entry_point,
               mod->is_main_exe ? " [MAIN]" : "");
        mod = mod->next;
    }

    printf("\n  Import Statistics:\n");
    printf("    Total:   %u\n", ctx->import_stats.total_imports);
    printf("    Stubbed: %u\n", ctx->import_stats.stubbed_imports);
    printf("    Direct:  %u\n", ctx->import_stats.direct_imports);
    printf("    Failed:  %u\n", ctx->import_stats.failed_imports);
}
