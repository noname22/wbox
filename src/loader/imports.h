/*
 * WBOX Import Resolver
 * Parses PE import directory and resolves imports via exports or stubs
 */
#ifndef WBOX_IMPORTS_H
#define WBOX_IMPORTS_H

#include <stdint.h>
#include <stdbool.h>
#include "module.h"

/* Forward declaration for stub_manager (defined in stubs.h) */
struct stub_manager;

/* IMAGE_IMPORT_DESCRIPTOR structure */
typedef struct {
    union {
        uint32_t Characteristics;        /* 0 for terminating null */
        uint32_t OriginalFirstThunk;     /* RVA to INT (Import Name Table) */
    };
    uint32_t TimeDateStamp;              /* 0 if not bound */
    uint32_t ForwarderChain;             /* -1 if no forwarders */
    uint32_t Name;                       /* RVA to DLL name string */
    uint32_t FirstThunk;                 /* RVA to IAT (Import Address Table) */
} IMAGE_IMPORT_DESCRIPTOR;

/* IMAGE_THUNK_DATA32 - entry in INT/IAT */
typedef struct {
    union {
        uint32_t ForwarderString;        /* RVA to forwarder string */
        uint32_t Function;               /* Address of imported function */
        uint32_t Ordinal;                /* Ordinal value if IMAGE_ORDINAL_FLAG set */
        uint32_t AddressOfData;          /* RVA to IMAGE_IMPORT_BY_NAME */
    };
} IMAGE_THUNK_DATA32;

/* IMAGE_IMPORT_BY_NAME structure */
typedef struct {
    uint16_t Hint;                       /* Ordinal hint */
    char     Name[1];                    /* Function name (null-terminated) */
} IMAGE_IMPORT_BY_NAME;

/* Ordinal flag - bit 31 set means import by ordinal */
#define IMAGE_ORDINAL_FLAG32     0x80000000
#define IMAGE_ORDINAL32(ordinal) ((ordinal) & 0xFFFF)

/* Import statistics */
typedef struct {
    uint32_t total_imports;              /* Total imports resolved */
    uint32_t stubbed_imports;            /* Imports resolved via stub */
    uint32_t direct_imports;             /* Imports resolved directly */
    uint32_t failed_imports;             /* Imports that failed to resolve */
} import_stats_t;

/*
 * Resolve all imports for a loaded module
 * This is the main entry point - resolves imports from all DLLs
 *
 * Parameters:
 *   mgr       - Module manager (for loading DLLs and stub generation)
 *   vm        - VM context (for memory access)
 *   mod       - Module whose imports should be resolved
 *   stats     - Optional statistics output
 *
 * Returns 0 on success, -1 on error
 */
int imports_resolve(module_manager_t *mgr, vm_context_t *vm,
                    loaded_module_t *mod, import_stats_t *stats);

/*
 * Parse import directory from a PE image
 * Returns pointer to first IMAGE_IMPORT_DESCRIPTOR, or NULL if no imports
 */
const IMAGE_IMPORT_DESCRIPTOR *imports_get_directory(const pe_image_t *pe);

/*
 * Resolve a single function import
 * Returns resolved VA (stub or direct), or 0 on failure
 *
 * Parameters:
 *   mgr        - Module manager
 *   vm         - VM context
 *   stubs      - Stub manager for generating stubs
 *   dll_mod    - Module to resolve from (the DLL)
 *   func_name  - Function name (NULL for ordinal import)
 *   ordinal    - Ordinal value (if func_name is NULL)
 *   is_stub    - Output: set to true if resolved via stub
 */
uint32_t imports_resolve_function(module_manager_t *mgr, vm_context_t *vm,
                                  struct stub_manager *stubs,
                                  loaded_module_t *dll_mod,
                                  const char *func_name, uint16_t ordinal,
                                  bool *is_stub);

/*
 * Check if a DLL is known and should have stubs generated
 * Currently only ntdll.dll returns true
 */
bool imports_dll_uses_stubs(const char *dll_name);

/*
 * Print import statistics
 */
void imports_print_stats(const import_stats_t *stats);

#endif /* WBOX_IMPORTS_H */
