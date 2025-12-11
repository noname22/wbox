/*
 * WBOX Module Tracking System
 * Manages loaded PE modules and LDR data structures
 */
#include "module.h"
#include "../vm/vm.h"
#include "../vm/paging.h"
#include "../cpu/mem.h"
#include "../process/process.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <libgen.h>

/* Helper: Write 32-bit value to virtual address */
static void write_virt_l(vm_context_t *vm, uint32_t virt, uint32_t val)
{
    uint32_t phys = paging_get_phys(&vm->paging, virt);
    if (phys != 0) {
        mem_writel_phys(phys, val);
    }
}

/* Helper: Write 16-bit value to virtual address */
static void write_virt_w(vm_context_t *vm, uint32_t virt, uint16_t val)
{
    uint32_t phys = paging_get_phys(&vm->paging, virt);
    if (phys != 0) {
        mem_writew_phys(phys, val);
    }
}

/* Helper: Write 8-bit value to virtual address */
static void write_virt_b(vm_context_t *vm, uint32_t virt, uint8_t val)
{
    uint32_t phys = paging_get_phys(&vm->paging, virt);
    if (phys != 0) {
        mem_writeb_phys(phys, val);
    }
}

int module_manager_init(module_manager_t *mgr, vm_context_t *vm)
{
    memset(mgr, 0, sizeof(*mgr));

    /* Allocate loader heap in guest */
    mgr->loader_heap_va = LOADER_HEAP_VA;
    mgr->loader_heap_size = LOADER_HEAP_SIZE;
    mgr->loader_heap_ptr = 0;

    /* Allocate physical memory for loader heap */
    mgr->loader_heap_phys = paging_alloc_phys(&vm->paging, LOADER_HEAP_SIZE);
    if (mgr->loader_heap_phys == 0) {
        fprintf(stderr, "module_manager_init: Failed to allocate loader heap\n");
        return -1;
    }

    /* Map loader heap into guest address space */
    uint32_t num_pages = (LOADER_HEAP_SIZE + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint32_t i = 0; i < num_pages; i++) {
        paging_map_page(&vm->paging,
                        mgr->loader_heap_va + i * PAGE_SIZE,
                        mgr->loader_heap_phys + i * PAGE_SIZE,
                        PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    }

    /* Clear the heap */
    for (uint32_t i = 0; i < LOADER_HEAP_SIZE; i++) {
        mem_writeb_phys(mgr->loader_heap_phys + i, 0);
    }

    printf("Module manager: loader heap at VA 0x%08X (phys 0x%08X)\n",
           mgr->loader_heap_va, mgr->loader_heap_phys);

    return 0;
}

void module_manager_set_ntdll_path(module_manager_t *mgr, const char *path)
{
    mgr->ntdll_path = path;
}

uint32_t module_heap_alloc(module_manager_t *mgr, uint32_t size)
{
    /* Align to 4 bytes */
    size = (size + 3) & ~3;

    if (mgr->loader_heap_ptr + size > mgr->loader_heap_size) {
        fprintf(stderr, "module_heap_alloc: Out of loader heap space\n");
        return 0;
    }

    uint32_t va = mgr->loader_heap_va + mgr->loader_heap_ptr;
    mgr->loader_heap_ptr += size;

    return va;
}

uint32_t write_wide_string(vm_context_t *vm, uint32_t va, const char *str)
{
    size_t len = strlen(str);
    for (size_t i = 0; i <= len; i++) {
        write_virt_w(vm, va + i * 2, (uint16_t)(unsigned char)str[i]);
    }
    return (uint32_t)(len * 2);  /* Return byte length (not including null) */
}

void list_insert_tail(vm_context_t *vm, uint32_t list_head_va, uint32_t entry_va)
{
    /* Read current Blink of list head (points to last entry) */
    uint32_t list_head_phys = paging_get_phys(&vm->paging, list_head_va);
    uint32_t last_entry_va = mem_readl_phys(list_head_phys + 4);  /* Blink */

    /* If list is empty, last_entry_va == list_head_va */
    if (last_entry_va == 0 || last_entry_va == list_head_va) {
        /* Empty list: new entry is both first and last */
        /* List head Flink = entry, Blink = entry */
        mem_writel_phys(list_head_phys + 0, entry_va);  /* Flink */
        mem_writel_phys(list_head_phys + 4, entry_va);  /* Blink */

        /* Entry Flink = list_head, Blink = list_head */
        uint32_t entry_phys = paging_get_phys(&vm->paging, entry_va);
        mem_writel_phys(entry_phys + 0, list_head_va);  /* Flink */
        mem_writel_phys(entry_phys + 4, list_head_va);  /* Blink */
    } else {
        /* Insert at end */
        uint32_t last_entry_phys = paging_get_phys(&vm->paging, last_entry_va);
        uint32_t entry_phys = paging_get_phys(&vm->paging, entry_va);

        /* Entry Flink = list_head, Blink = last_entry */
        mem_writel_phys(entry_phys + 0, list_head_va);
        mem_writel_phys(entry_phys + 4, last_entry_va);

        /* Last entry Flink = entry */
        mem_writel_phys(last_entry_phys + 0, entry_va);

        /* List head Blink = entry */
        mem_writel_phys(list_head_phys + 4, entry_va);
    }
}

int module_init_peb_ldr(module_manager_t *mgr, vm_context_t *vm)
{
    /* Allocate PEB_LDR_DATA in loader heap */
    uint32_t ldr_va = module_heap_alloc(mgr, sizeof(PEB_LDR_DATA32));
    if (ldr_va == 0) {
        return -1;
    }
    mgr->ldr_data_va = ldr_va;

    printf("Initializing PEB_LDR_DATA at 0x%08X\n", ldr_va);

    /* Initialize structure */
    write_virt_l(vm, ldr_va + 0x00, sizeof(PEB_LDR_DATA32));  /* Length */
    write_virt_b(vm, ldr_va + 0x04, 1);                       /* Initialized */
    write_virt_l(vm, ldr_va + 0x08, 0);                       /* SsHandle */

    /* Initialize list heads to point to themselves (empty circular lists) */
    uint32_t in_load_order = ldr_va + 0x0C;
    uint32_t in_memory_order = ldr_va + 0x14;
    uint32_t in_init_order = ldr_va + 0x1C;

    /* InLoadOrderModuleList */
    write_virt_l(vm, in_load_order + 0, in_load_order);  /* Flink -> self */
    write_virt_l(vm, in_load_order + 4, in_load_order);  /* Blink -> self */

    /* InMemoryOrderModuleList */
    write_virt_l(vm, in_memory_order + 0, in_memory_order);
    write_virt_l(vm, in_memory_order + 4, in_memory_order);

    /* InInitializationOrderModuleList */
    write_virt_l(vm, in_init_order + 0, in_init_order);
    write_virt_l(vm, in_init_order + 4, in_init_order);

    /* EntryInProgress */
    write_virt_l(vm, ldr_va + 0x24, 0);

    /* Update PEB.Ldr pointer */
    write_virt_l(vm, vm->peb_addr + PEB_LDR, ldr_va);

    printf("  PEB.Ldr updated to 0x%08X\n", ldr_va);

    return 0;
}

int module_create_ldr_entry(module_manager_t *mgr, vm_context_t *vm,
                            loaded_module_t *mod)
{
    /* Allocate LDR_DATA_TABLE_ENTRY */
    uint32_t entry_va = module_heap_alloc(mgr, sizeof(LDR_DATA_TABLE_ENTRY32));
    if (entry_va == 0) {
        return -1;
    }

    /* Allocate space for DLL name (wide string) */
    size_t name_len = strlen(mod->name);
    uint32_t name_va = module_heap_alloc(mgr, (name_len + 1) * 2);
    if (name_va == 0) {
        return -1;
    }

    /* Write wide string name */
    uint32_t name_bytes = write_wide_string(vm, name_va, mod->name);

    printf("Creating LDR entry for %s at 0x%08X\n", mod->name, entry_va);

    /* Fill in the entry */
    /* DllBase */
    write_virt_l(vm, entry_va + 0x18, mod->base_va);
    /* EntryPoint */
    write_virt_l(vm, entry_va + 0x1C, mod->entry_point);
    /* SizeOfImage */
    write_virt_l(vm, entry_va + 0x20, mod->size);

    /* FullDllName (UNICODE_STRING) */
    write_virt_w(vm, entry_va + 0x24, (uint16_t)name_bytes);       /* Length */
    write_virt_w(vm, entry_va + 0x26, (uint16_t)(name_bytes + 2)); /* MaxLength */
    write_virt_l(vm, entry_va + 0x28, name_va);                    /* Buffer */

    /* BaseDllName - same as FullDllName for now */
    write_virt_w(vm, entry_va + 0x2C, (uint16_t)name_bytes);
    write_virt_w(vm, entry_va + 0x2E, (uint16_t)(name_bytes + 2));
    write_virt_l(vm, entry_va + 0x30, name_va);

    /* Flags */
    write_virt_l(vm, entry_va + 0x34, 0x00004000);  /* LDRP_IMAGE_DLL or similar */
    /* LoadCount */
    write_virt_w(vm, entry_va + 0x38, 1);
    /* TlsIndex */
    write_virt_w(vm, entry_va + 0x3A, 0);

    /* HashLinks at offset 0x3C - initialize to point to self (empty list entry) */
    write_virt_l(vm, entry_va + 0x3C, entry_va + 0x3C);  /* Flink -> self */
    write_virt_l(vm, entry_va + 0x40, entry_va + 0x3C);  /* Blink -> self */

    /* TimeDateStamp at offset 0x44 - not critical, use 0 */
    write_virt_l(vm, entry_va + 0x44, 0);

    /* EntryPointActivationContext at offset 0x48 */
    write_virt_l(vm, entry_va + 0x48, 0);

    /* PatchInformation at offset 0x4C */
    write_virt_l(vm, entry_va + 0x4C, 0);

    /* Link into lists */
    /* InLoadOrderLinks at offset 0x00 */
    list_insert_tail(vm, mgr->ldr_data_va + 0x0C, entry_va + 0x00);
    /* InMemoryOrderLinks at offset 0x08 */
    list_insert_tail(vm, mgr->ldr_data_va + 0x14, entry_va + 0x08);
    /* Note: InInitializationOrderLinks (offset 0x10) is NOT populated at load time.
     * It should only be populated after DllMain has been called successfully.
     * For now, initialize the list entry to point to itself (unlinked state). */
    write_virt_l(vm, entry_va + 0x10, entry_va + 0x10);  /* Flink -> self */
    write_virt_l(vm, entry_va + 0x14, entry_va + 0x10);  /* Blink -> self */

    mod->ldr_entry_va = entry_va;

    printf("  DllBase=0x%08X Size=0x%X EntryPoint=0x%08X\n",
           mod->base_va, mod->size, mod->entry_point);

    return 0;
}

loaded_module_t *module_find_by_name(module_manager_t *mgr, const char *name)
{
    /* Extract base name if path is given */
    const char *base_name = strrchr(name, '/');
    if (base_name) {
        base_name++;
    } else {
        base_name = strrchr(name, '\\');
        if (base_name) {
            base_name++;
        } else {
            base_name = name;
        }
    }

    loaded_module_t *mod = mgr->modules;
    while (mod) {
        /* Extract base name from module */
        const char *mod_base = strrchr(mod->name, '/');
        if (mod_base) {
            mod_base++;
        } else {
            mod_base = strrchr(mod->name, '\\');
            if (mod_base) {
                mod_base++;
            } else {
                mod_base = mod->name;
            }
        }

        if (strcasecmp(base_name, mod_base) == 0) {
            return mod;
        }
        mod = mod->next;
    }
    return NULL;
}

loaded_module_t *module_find_by_base(module_manager_t *mgr, uint32_t base)
{
    loaded_module_t *mod = mgr->modules;
    while (mod) {
        if (mod->base_va == base) {
            return mod;
        }
        mod = mod->next;
    }
    return NULL;
}

void module_free(loaded_module_t *mod)
{
    if (!mod) return;

    /* Free export cache */
    if (mod->exports) {
        for (uint32_t i = 0; i < mod->num_exports; i++) {
            free(mod->exports[i].name);
            free(mod->exports[i].forwarder_name);
        }
        free(mod->exports);
    }

    /* Free PE image */
    pe_free(&mod->pe);

    free(mod);
}

void module_manager_free(module_manager_t *mgr)
{
    loaded_module_t *mod = mgr->modules;
    while (mod) {
        loaded_module_t *next = mod->next;
        module_free(mod);
        mod = next;
    }
    mgr->modules = NULL;
    mgr->module_count = 0;
}
