/*
 * WBOX Module Tracking System
 * Manages loaded PE modules and LDR data structures
 */
#ifndef WBOX_MODULE_H
#define WBOX_MODULE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../pe/pe_loader.h"
#include "../vm/vm.h"

/* Maximum DLL name length */
#define MAX_DLL_NAME 260

/* Memory layout for loader structures */
#define LOADER_STUB_REGION_VA    0x7F000000
#define LOADER_STUB_REGION_SIZE  (64 * 1024)
#define LOADER_HEAP_VA           0x7F010000
#define LOADER_HEAP_SIZE         (64 * 1024)

/* Forward declarations */
struct stub_manager;

/*
 * Guest-side structures (must match Windows XP layout exactly)
 */

/* UNICODE_STRING (32-bit) */
typedef struct {
    uint16_t Length;          /* Byte length (not including null) */
    uint16_t MaximumLength;   /* Buffer size in bytes */
    uint32_t Buffer;          /* Guest pointer to wide string */
} UNICODE_STRING32;

/* LIST_ENTRY for doubly-linked lists */
typedef struct {
    uint32_t Flink;  /* Guest pointer to next entry */
    uint32_t Blink;  /* Guest pointer to previous entry */
} LIST_ENTRY32;

/* PEB_LDR_DATA - loader data in PEB
 * Located at PEB+0x0C */
typedef struct {
    uint32_t        Length;                              /* 0x00: sizeof(PEB_LDR_DATA) = 0x28 */
    uint8_t         Initialized;                         /* 0x04: 1 when initialized */
    uint8_t         Padding[3];                          /* 0x05: alignment */
    uint32_t        SsHandle;                            /* 0x08: reserved */
    LIST_ENTRY32    InLoadOrderModuleList;               /* 0x0C */
    LIST_ENTRY32    InMemoryOrderModuleList;             /* 0x14 */
    LIST_ENTRY32    InInitializationOrderModuleList;     /* 0x1C */
    uint32_t        EntryInProgress;                     /* 0x24 */
} PEB_LDR_DATA32;
/* Size: 0x28 (40 bytes) */

/* LDR_DATA_TABLE_ENTRY - per-module structure in guest memory */
typedef struct {
    LIST_ENTRY32    InLoadOrderLinks;                    /* 0x00 */
    LIST_ENTRY32    InMemoryOrderLinks;                  /* 0x08 */
    LIST_ENTRY32    InInitializationOrderLinks;          /* 0x10 */
    uint32_t        DllBase;                             /* 0x18: Base address */
    uint32_t        EntryPoint;                          /* 0x1C: Entry point (DllMain) */
    uint32_t        SizeOfImage;                         /* 0x20 */
    UNICODE_STRING32 FullDllName;                        /* 0x24 */
    UNICODE_STRING32 BaseDllName;                        /* 0x2C */
    uint32_t        Flags;                               /* 0x34 */
    uint16_t        LoadCount;                           /* 0x38 */
    uint16_t        TlsIndex;                            /* 0x3A */
    LIST_ENTRY32    HashLinks;                           /* 0x3C */
    uint32_t        TimeDateStamp;                       /* 0x44 */
    uint32_t        EntryPointActivationContext;         /* 0x48 */
    uint32_t        PatchInformation;                    /* 0x4C */
} LDR_DATA_TABLE_ENTRY32;
/* Size: 0x50 (80 bytes) */

/*
 * Host-side structures
 */

/* Export entry (cached on host) */
typedef struct {
    char        *name;              /* Export name (NULL for ordinal-only) */
    uint16_t    ordinal;            /* Ordinal value */
    uint32_t    rva;                /* RVA of function */
    bool        is_forwarder;       /* Is this a forwarder? */
    char        *forwarder_name;    /* Forwarder string if applicable */
} export_entry_t;

/* Loaded module (host-side tracking) */
typedef struct loaded_module {
    struct loaded_module *next;

    char            name[MAX_DLL_NAME];     /* DLL filename (ASCII) */
    pe_image_t      pe;                     /* Parsed PE image */
    uint32_t        base_va;                /* Virtual address in guest */
    uint32_t        phys_base;              /* Physical address of image */
    uint32_t        size;                   /* Size of image */
    uint32_t        entry_point;            /* DllMain address (0 if none) */

    /* Guest structure address */
    uint32_t        ldr_entry_va;           /* LDR_DATA_TABLE_ENTRY guest VA */

    /* Export table cache */
    uint32_t        num_exports;
    uint32_t        ordinal_base;           /* Base ordinal number */
    export_entry_t  *exports;

    bool            is_main_exe;            /* Is this the main executable? */
    bool            dll_main_called;        /* Has DllMain been called? */
    bool            imports_resolved;       /* Have imports been resolved? */
} loaded_module_t;

/* Module manager state */
typedef struct module_manager {
    loaded_module_t *modules;               /* Linked list of loaded modules */
    uint32_t        module_count;

    /* Guest memory allocator for loader structures */
    uint32_t        loader_heap_va;         /* Base of loader heap in guest */
    uint32_t        loader_heap_phys;       /* Physical address of heap */
    uint32_t        loader_heap_ptr;        /* Current allocation offset */
    uint32_t        loader_heap_size;       /* Total heap size */

    /* PEB_LDR_DATA address */
    uint32_t        ldr_data_va;

    /* Stub manager (forward declared) */
    struct stub_manager *stubs;

    /* Path to ntdll.dll */
    const char      *ntdll_path;
} module_manager_t;

/*
 * Module manager functions
 */

/* Initialize module manager */
int module_manager_init(module_manager_t *mgr, vm_context_t *vm);

/* Set ntdll.dll path */
void module_manager_set_ntdll_path(module_manager_t *mgr, const char *path);

/* Allocate from loader heap */
uint32_t module_heap_alloc(module_manager_t *mgr, uint32_t size);

/* Load a module (PE file) */
loaded_module_t *module_load(module_manager_t *mgr, vm_context_t *vm,
                             const char *path, uint32_t preferred_base);

/* Load a module from the module manager's search paths */
loaded_module_t *module_load_by_name(module_manager_t *mgr, vm_context_t *vm,
                                     const char *dll_name);

/* Find module by name */
loaded_module_t *module_find_by_name(module_manager_t *mgr, const char *name);

/* Find module by base address */
loaded_module_t *module_find_by_base(module_manager_t *mgr, uint32_t base);

/* Initialize PEB_LDR_DATA structure */
int module_init_peb_ldr(module_manager_t *mgr, vm_context_t *vm);

/* Create LDR_DATA_TABLE_ENTRY in guest memory */
int module_create_ldr_entry(module_manager_t *mgr, vm_context_t *vm,
                            loaded_module_t *mod);

/* Initialize LdrpHashTable in ntdll.dll (must be called after ntdll is loaded) */
int module_init_ldrp_hash_table(module_manager_t *mgr, vm_context_t *vm, loaded_module_t *ntdll);

/* Link a module into the LdrpHashTable (must be called after module_create_ldr_entry) */
int module_link_to_hash_table(module_manager_t *mgr, vm_context_t *vm,
                              loaded_module_t *ntdll, loaded_module_t *mod);

/* Free a single module */
void module_free(loaded_module_t *mod);

/* Free all modules and cleanup */
void module_manager_free(module_manager_t *mgr);

/*
 * Helper functions for guest memory list operations
 */

/* Insert entry at tail of doubly-linked list */
void list_insert_tail(vm_context_t *vm, uint32_t list_head_va, uint32_t entry_va);

/* Write wide string to guest memory, return length in bytes */
uint32_t write_wide_string(vm_context_t *vm, uint32_t va, const char *str);

#endif /* WBOX_MODULE_H */
