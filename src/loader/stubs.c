/*
 * WBOX Stub Code Generator
 * Generates x86 stub code in guest memory for intercepted functions
 */
#include "stubs.h"
#include "module.h"
#include "../vm/vm.h"
#include "../vm/paging.h"
#include "../cpu/mem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_REGISTRY_CAPACITY 64

int stubs_init(stub_manager_t *mgr, vm_context_t *vm)
{
    memset(mgr, 0, sizeof(*mgr));

    mgr->stub_region_va = LOADER_STUB_REGION_VA;
    mgr->stub_region_size = LOADER_STUB_REGION_SIZE;
    mgr->stub_alloc_ptr = 0;

    /* Allocate physical memory for stub region */
    mgr->stub_region_phys = paging_alloc_phys(&vm->paging, LOADER_STUB_REGION_SIZE);
    if (mgr->stub_region_phys == 0) {
        fprintf(stderr, "stubs_init: Failed to allocate stub region\n");
        return -1;
    }

    /* Map stub region into guest address space (executable) */
    uint32_t num_pages = (LOADER_STUB_REGION_SIZE + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint32_t i = 0; i < num_pages; i++) {
        paging_map_page(&vm->paging,
                        mgr->stub_region_va + i * PAGE_SIZE,
                        mgr->stub_region_phys + i * PAGE_SIZE,
                        PTE_PRESENT | PTE_USER);  /* Read + Execute (no write needed) */
    }

    /* Clear the stub region */
    for (uint32_t i = 0; i < LOADER_STUB_REGION_SIZE; i++) {
        mem_writeb_phys(mgr->stub_region_phys + i, 0xCC);  /* INT3 for safety */
    }

    /* Initialize registry */
    mgr->registry = calloc(INITIAL_REGISTRY_CAPACITY, sizeof(stub_entry_t));
    if (!mgr->registry) {
        fprintf(stderr, "stubs_init: Out of memory for registry\n");
        return -1;
    }
    mgr->registry_capacity = INITIAL_REGISTRY_CAPACITY;
    mgr->registry_count = 0;

    printf("Stub manager: stub region at VA 0x%08X (phys 0x%08X)\n",
           mgr->stub_region_va, mgr->stub_region_phys);

    return 0;
}

static int registry_add(stub_manager_t *mgr, const char *name, uint32_t stub_va)
{
    /* Grow registry if needed */
    if (mgr->registry_count >= mgr->registry_capacity) {
        uint32_t new_capacity = mgr->registry_capacity * 2;
        stub_entry_t *new_registry = realloc(mgr->registry,
                                              new_capacity * sizeof(stub_entry_t));
        if (!new_registry) {
            return -1;
        }
        mgr->registry = new_registry;
        mgr->registry_capacity = new_capacity;
    }

    mgr->registry[mgr->registry_count].name = strdup(name);
    mgr->registry[mgr->registry_count].stub_va = stub_va;
    mgr->registry_count++;

    return 0;
}

uint32_t stubs_lookup(stub_manager_t *mgr, const char *name)
{
    for (uint32_t i = 0; i < mgr->registry_count; i++) {
        if (strcmp(mgr->registry[i].name, name) == 0) {
            return mgr->registry[i].stub_va;
        }
    }
    return 0;
}

uint32_t stubs_generate(stub_manager_t *mgr, vm_context_t *vm,
                        const stub_def_t *def)
{
    /* Check if we have space */
    if (mgr->stub_alloc_ptr + STUB_CODE_SIZE > mgr->stub_region_size) {
        fprintf(stderr, "stubs_generate: Out of stub space\n");
        return 0;
    }

    uint32_t stub_va = mgr->stub_region_va + mgr->stub_alloc_ptr;
    uint32_t stub_phys = mgr->stub_region_phys + mgr->stub_alloc_ptr;

    switch (def->type) {
    case STUB_TYPE_SYSCALL:
        /*
         * Syscall stub (12 bytes):
         *   mov eax, syscall_num    ; B8 xx xx xx xx (5 bytes)
         *   mov edx, esp            ; 89 E2          (2 bytes)
         *   sysenter                ; 0F 34          (2 bytes)
         *   ret num_args*4          ; C2 xx xx       (3 bytes)
         */
        mem_writeb_phys(stub_phys + 0, 0xB8);  /* MOV EAX, imm32 */
        mem_writel_phys(stub_phys + 1, def->syscall_num);
        mem_writeb_phys(stub_phys + 5, 0x89);  /* MOV EDX, ESP */
        mem_writeb_phys(stub_phys + 6, 0xE2);
        mem_writeb_phys(stub_phys + 7, 0x0F);  /* SYSENTER */
        mem_writeb_phys(stub_phys + 8, 0x34);
        mem_writeb_phys(stub_phys + 9, 0xC2);  /* RET imm16 */
        mem_writew_phys(stub_phys + 10, (uint16_t)(def->num_args * 4));
        break;

    case STUB_TYPE_RETURN_ZERO:
        /*
         * Return 0 stub:
         *   xor eax, eax            ; 31 C0          (2 bytes)
         *   ret num_args*4          ; C2 xx xx       (3 bytes)
         */
        mem_writeb_phys(stub_phys + 0, 0x31);  /* XOR EAX, EAX */
        mem_writeb_phys(stub_phys + 1, 0xC0);
        mem_writeb_phys(stub_phys + 2, 0xC2);  /* RET imm16 */
        mem_writew_phys(stub_phys + 3, (uint16_t)(def->num_args * 4));
        break;

    case STUB_TYPE_RETURN_ERROR:
        /*
         * Return error stub:
         *   mov eax, return_value   ; B8 xx xx xx xx (5 bytes)
         *   ret num_args*4          ; C2 xx xx       (3 bytes)
         */
        mem_writeb_phys(stub_phys + 0, 0xB8);  /* MOV EAX, imm32 */
        mem_writel_phys(stub_phys + 1, def->return_value);
        mem_writeb_phys(stub_phys + 5, 0xC2);  /* RET imm16 */
        mem_writew_phys(stub_phys + 6, (uint16_t)(def->num_args * 4));
        break;

    default:
        fprintf(stderr, "stubs_generate: Unknown stub type %d\n", def->type);
        return 0;
    }

    /* Advance allocation pointer */
    mgr->stub_alloc_ptr += STUB_CODE_SIZE;

    /* Add to registry */
    if (registry_add(mgr, def->name, stub_va) < 0) {
        fprintf(stderr, "stubs_generate: Failed to add to registry\n");
        return 0;
    }

    printf("Generated stub for %s at 0x%08X (syscall %u, %d args)\n",
           def->name, stub_va, def->syscall_num, def->num_args);

    return stub_va;
}

uint32_t stubs_get_or_create(stub_manager_t *mgr, vm_context_t *vm,
                             const stub_def_t *def)
{
    /* Check if stub already exists */
    uint32_t existing = stubs_lookup(mgr, def->name);
    if (existing != 0) {
        return existing;
    }

    /* Generate new stub */
    return stubs_generate(mgr, vm, def);
}

void stubs_free(stub_manager_t *mgr)
{
    if (mgr->registry) {
        for (uint32_t i = 0; i < mgr->registry_count; i++) {
            free(mgr->registry[i].name);
        }
        free(mgr->registry);
    }
    memset(mgr, 0, sizeof(*mgr));
}
