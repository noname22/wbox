/*
 * WBOX PE Loader
 * High-level API for loading PE executables with DLL support
 */
#ifndef WBOX_LOADER_H
#define WBOX_LOADER_H

#include <stdint.h>
#include <stdbool.h>
#include "module.h"
#include "stubs.h"
#include "imports.h"


/*
 * Loader context - holds all state for the loading process
 */
typedef struct loader_context {
    module_manager_t    modules;            /* Module tracking */
    stub_manager_t      stubs;              /* Stub code generation */
    import_stats_t      import_stats;       /* Import resolution statistics */

    loaded_module_t     *main_module;       /* The main executable */
    const char          *ntdll_path;        /* Path to ntdll.dll */
} loader_context_t;

/*
 * Initialize loader context
 * Must be called before any other loader functions
 * Returns 0 on success, -1 on error
 */
int loader_init(loader_context_t *ctx, vm_context_t *vm);

/*
 * Set the path to ntdll.dll
 * Must be called before loading an executable that imports from ntdll
 */
void loader_set_ntdll_path(loader_context_t *ctx, const char *path);

/*
 * Load the main executable and all its dependencies
 *
 * This function:
 * 1. Loads the main PE executable
 * 2. Recursively loads any DLLs it imports from
 * 3. Resolves all imports (using stubs for known functions)
 * 4. Creates LDR data structures in guest memory
 *
 * Parameters:
 *   ctx      - Loader context (must be initialized)
 *   vm       - VM context
 *   exe_path - Path to the main executable
 *
 * Returns 0 on success, -1 on error
 */
int loader_load_executable(loader_context_t *ctx, vm_context_t *vm,
                           const char *exe_path);

/*
 * Get the entry point VA of the loaded executable
 * Returns 0 if no executable is loaded
 */
uint32_t loader_get_entry_point(const loader_context_t *ctx);

/*
 * Get the base VA of the loaded executable
 * Returns 0 if no executable is loaded
 */
uint32_t loader_get_image_base(const loader_context_t *ctx);

/*
 * Get the main executable module
 * Returns NULL if no executable is loaded
 */
loaded_module_t *loader_get_main_module(loader_context_t *ctx);

/*
 * Free all loader resources
 */
void loader_free(loader_context_t *ctx);

/*
 * Print loader status (for debugging)
 */
void loader_print_status(const loader_context_t *ctx);

#endif /* WBOX_LOADER_H */
