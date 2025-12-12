/*
 * WBOX Heap Manager
 * Provides process heap functionality by intercepting RtlAllocateHeap/RtlFreeHeap
 */
#include "heap.h"
#include "syscalls.h"
#include "../vm/vm.h"
#include "../cpu/mem.h"
#include "../cpu/cpu.h"
#include "../vm/paging.h"
#include "../loader/loader.h"
#include "../loader/module.h"
#include "../loader/exports.h"

#include <stdio.h>
#include <string.h>

/* Hook addresses (filled in by heap_install_hooks) */
static uint32_t hook_rtl_allocate_heap = 0;
static uint32_t hook_rtl_free_heap = 0;
static uint32_t hook_rtl_realloc_heap = 0;
static uint32_t hook_rtl_size_heap = 0;

/* String conversion hook addresses */
static uint32_t hook_rtl_mb_to_unicode = 0;
static uint32_t hook_rtl_unicode_to_mb = 0;
static uint32_t hook_rtl_mb_size = 0;
static uint32_t hook_rtl_unicode_size = 0;

/* Helper: Write 32-bit value to virtual address */
static void write_virt_l(vm_context_t *vm, uint32_t virt, uint32_t val)
{
    uint32_t phys = paging_get_phys(&vm->paging, virt);
    if (phys != 0) {
        mem_writel_phys(phys, val);
    }
}

/* Helper: Read 32-bit value from virtual address */
static uint32_t read_virt_l(vm_context_t *vm, uint32_t virt)
{
    uint32_t phys = paging_get_phys(&vm->paging, virt);
    if (phys != 0) {
        return mem_readl_phys(phys);
    }
    return 0;
}

int heap_init(heap_state_t *heap, vm_context_t *vm)
{
    memset(heap, 0, sizeof(*heap));

    heap->base_va = HEAP_REGION_VA;
    heap->size = HEAP_REGION_SIZE;
    heap->alloc_ptr = 0;

    /* Allocate physical memory for heap */
    heap->base_phys = paging_alloc_phys(&vm->paging, HEAP_REGION_SIZE);
    if (heap->base_phys == 0) {
        fprintf(stderr, "heap_init: Failed to allocate %d MB for heap\n",
                HEAP_REGION_SIZE / (1024 * 1024));
        return -1;
    }

    /* Map heap into guest address space */
    uint32_t num_pages = (HEAP_REGION_SIZE + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint32_t i = 0; i < num_pages; i++) {
        paging_map_page(&vm->paging,
                        heap->base_va + i * PAGE_SIZE,
                        heap->base_phys + i * PAGE_SIZE,
                        PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    }

    /* Clear heap memory */
    for (uint32_t i = 0; i < HEAP_REGION_SIZE; i++) {
        mem_writeb_phys(heap->base_phys + i, 0);
    }

    printf("Heap initialized: VA 0x%08X-0x%08X (%d MB)\n",
           heap->base_va, heap->base_va + heap->size, heap->size / (1024 * 1024));

    return 0;
}

uint32_t heap_alloc(heap_state_t *heap, vm_context_t *vm,
                    uint32_t heap_handle, uint32_t flags, uint32_t size)
{
    /* Align size to 8 bytes */
    size = (size + 7) & ~7;

    /* Total size including header */
    uint32_t total_size = size + sizeof(heap_alloc_header_t);

    /* Check if we have space */
    if (heap->alloc_ptr + total_size > heap->size) {
        fprintf(stderr, "heap_alloc: Out of heap space (requested %u bytes)\n", size);
        return 0;
    }

    /* Allocate */
    uint32_t header_va = heap->base_va + heap->alloc_ptr;
    uint32_t data_va = header_va + sizeof(heap_alloc_header_t);

    /* Write header */
    write_virt_l(vm, header_va + 0, HEAP_ALLOC_MAGIC);
    write_virt_l(vm, header_va + 4, size);
    write_virt_l(vm, header_va + 8, flags);

    /* Zero the allocated memory if HEAP_ZERO_MEMORY flag is set */
    if (flags & 0x08) {  /* HEAP_ZERO_MEMORY */
        uint32_t phys = paging_get_phys(&vm->paging, data_va);
        for (uint32_t i = 0; i < size; i++) {
            mem_writeb_phys(phys + i, 0);
        }
    }

    heap->alloc_ptr += total_size;
    heap->total_allocated += size;
    heap->num_allocations++;

    return data_va;
}

bool heap_free(heap_state_t *heap, vm_context_t *vm,
               uint32_t heap_handle, uint32_t flags, uint32_t ptr)
{
    if (ptr == 0) {
        return true;  /* Freeing NULL is OK */
    }

    /* Check if pointer is in our heap range */
    if (ptr < heap->base_va + sizeof(heap_alloc_header_t) ||
        ptr >= heap->base_va + heap->size) {
        fprintf(stderr, "heap_free: Invalid pointer 0x%08X (outside heap range)\n", ptr);
        return false;
    }

    /* Read header */
    uint32_t header_va = ptr - sizeof(heap_alloc_header_t);
    uint32_t magic = read_virt_l(vm, header_va + 0);
    uint32_t size = read_virt_l(vm, header_va + 4);

    if (magic != HEAP_ALLOC_MAGIC) {
        if (magic == HEAP_FREE_MAGIC) {
            fprintf(stderr, "heap_free: Double free detected at 0x%08X\n", ptr);
        } else {
            fprintf(stderr, "heap_free: Invalid header magic at 0x%08X (got 0x%08X)\n",
                    ptr, magic);
        }
        return false;
    }

    /* Mark as freed */
    write_virt_l(vm, header_va + 0, HEAP_FREE_MAGIC);

    heap->total_freed += size;

    /* Note: This is a simple bump allocator - we don't actually reuse freed memory.
     * For a real implementation, we'd need a free list. */

    return true;
}

uint32_t heap_realloc(heap_state_t *heap, vm_context_t *vm,
                      uint32_t heap_handle, uint32_t flags,
                      uint32_t ptr, uint32_t size)
{
    if (ptr == 0) {
        /* realloc(NULL, size) is equivalent to malloc(size) */
        return heap_alloc(heap, vm, heap_handle, flags, size);
    }

    if (size == 0) {
        /* realloc(ptr, 0) is equivalent to free(ptr) */
        heap_free(heap, vm, heap_handle, flags, ptr);
        return 0;
    }

    /* Get old size */
    uint32_t header_va = ptr - sizeof(heap_alloc_header_t);
    uint32_t magic = read_virt_l(vm, header_va + 0);
    uint32_t old_size = read_virt_l(vm, header_va + 4);

    if (magic != HEAP_ALLOC_MAGIC) {
        fprintf(stderr, "heap_realloc: Invalid header at 0x%08X\n", ptr);
        return 0;
    }

    /* Allocate new block */
    uint32_t new_ptr = heap_alloc(heap, vm, heap_handle, flags, size);
    if (new_ptr == 0) {
        return 0;
    }

    /* Copy data */
    uint32_t copy_size = (size < old_size) ? size : old_size;
    uint32_t src_phys = paging_get_phys(&vm->paging, ptr);
    uint32_t dst_phys = paging_get_phys(&vm->paging, new_ptr);
    for (uint32_t i = 0; i < copy_size; i++) {
        mem_writeb_phys(dst_phys + i, mem_readb_phys(src_phys + i));
    }

    /* Free old block */
    heap_free(heap, vm, heap_handle, flags, ptr);

    return new_ptr;
}

uint32_t heap_size(heap_state_t *heap, vm_context_t *vm,
                   uint32_t heap_handle, uint32_t flags, uint32_t ptr)
{
    if (ptr == 0) {
        return 0;
    }

    /* Check if pointer is in our heap range */
    if (ptr < heap->base_va + sizeof(heap_alloc_header_t) ||
        ptr >= heap->base_va + heap->size) {
        return (uint32_t)-1;  /* Return -1 for invalid pointer */
    }

    /* Read header */
    uint32_t header_va = ptr - sizeof(heap_alloc_header_t);
    uint32_t magic = read_virt_l(vm, header_va + 0);
    uint32_t size = read_virt_l(vm, header_va + 4);

    if (magic != HEAP_ALLOC_MAGIC) {
        return (uint32_t)-1;
    }

    return size;
}

/* Write a byte to virtual address */
static void write_virt_b(vm_context_t *vm, uint32_t virt, uint8_t val)
{
    uint32_t phys = paging_get_phys(&vm->paging, virt);
    if (phys != 0) {
        mem_writeb_phys(phys, val);
    }
}

/* Patch function entry with syscall stub:
 * B8 XX XX XX XX   ; MOV EAX, syscall_number
 * 0F 34            ; SYSENTER
 * Total: 7 bytes
 */
static void patch_function_entry(vm_context_t *vm, uint32_t func_va, uint32_t syscall_num)
{
    write_virt_b(vm, func_va + 0, 0xB8);  /* MOV EAX, imm32 */
    write_virt_b(vm, func_va + 1, (syscall_num >> 0) & 0xFF);
    write_virt_b(vm, func_va + 2, (syscall_num >> 8) & 0xFF);
    write_virt_b(vm, func_va + 3, (syscall_num >> 16) & 0xFF);
    write_virt_b(vm, func_va + 4, (syscall_num >> 24) & 0xFF);
    write_virt_b(vm, func_va + 5, 0x0F);  /* SYSENTER prefix */
    write_virt_b(vm, func_va + 6, 0x34);  /* SYSENTER */
}

int heap_install_hooks(heap_state_t *heap, vm_context_t *vm)
{
    /* Find ntdll.dll */
    if (!vm->loader) {
        fprintf(stderr, "heap_install_hooks: No loader context\n");
        return -1;
    }

    loader_context_t *loader = vm->loader;
    loaded_module_t *ntdll = module_find_by_name(&loader->modules, "ntdll.dll");
    if (!ntdll) {
        fprintf(stderr, "heap_install_hooks: ntdll.dll not loaded\n");
        return -1;
    }

    printf("Installing heap function hooks...\n");

    /* Look up and patch heap functions */
    export_lookup_t result;

    result = exports_lookup_by_name(ntdll, "RtlAllocateHeap");
    if (result.found && !result.is_forwarder) {
        hook_rtl_allocate_heap = ntdll->base_va + result.rva;
        patch_function_entry(vm, hook_rtl_allocate_heap, WBOX_SYSCALL_HEAP_ALLOC);
        printf("  Patched RtlAllocateHeap at 0x%08X\n", hook_rtl_allocate_heap);
    }

    result = exports_lookup_by_name(ntdll, "RtlFreeHeap");
    if (result.found && !result.is_forwarder) {
        hook_rtl_free_heap = ntdll->base_va + result.rva;
        patch_function_entry(vm, hook_rtl_free_heap, WBOX_SYSCALL_HEAP_FREE);
        printf("  Patched RtlFreeHeap at 0x%08X\n", hook_rtl_free_heap);
    }

    result = exports_lookup_by_name(ntdll, "RtlReAllocateHeap");
    if (result.found && !result.is_forwarder) {
        hook_rtl_realloc_heap = ntdll->base_va + result.rva;
        patch_function_entry(vm, hook_rtl_realloc_heap, WBOX_SYSCALL_HEAP_REALLOC);
        printf("  Patched RtlReAllocateHeap at 0x%08X\n", hook_rtl_realloc_heap);
    }

    result = exports_lookup_by_name(ntdll, "RtlSizeHeap");
    if (result.found && !result.is_forwarder) {
        hook_rtl_size_heap = ntdll->base_va + result.rva;
        patch_function_entry(vm, hook_rtl_size_heap, WBOX_SYSCALL_HEAP_SIZE);
        printf("  Patched RtlSizeHeap at 0x%08X\n", hook_rtl_size_heap);
    }

    /* Hook string conversion functions to avoid NLS table dependency */
    printf("Installing string conversion hooks...\n");

    result = exports_lookup_by_name(ntdll, "RtlMultiByteToUnicodeN");
    if (result.found && !result.is_forwarder) {
        hook_rtl_mb_to_unicode = ntdll->base_va + result.rva;
        patch_function_entry(vm, hook_rtl_mb_to_unicode, WBOX_SYSCALL_MBSTR_TO_UNICODE);
        printf("  Patched RtlMultiByteToUnicodeN at 0x%08X\n", hook_rtl_mb_to_unicode);
    }

    result = exports_lookup_by_name(ntdll, "RtlUnicodeToMultiByteN");
    if (result.found && !result.is_forwarder) {
        hook_rtl_unicode_to_mb = ntdll->base_va + result.rva;
        patch_function_entry(vm, hook_rtl_unicode_to_mb, WBOX_SYSCALL_UNICODE_TO_MBSTR);
        printf("  Patched RtlUnicodeToMultiByteN at 0x%08X\n", hook_rtl_unicode_to_mb);
    }

    result = exports_lookup_by_name(ntdll, "RtlMultiByteToUnicodeSize");
    if (result.found && !result.is_forwarder) {
        hook_rtl_mb_size = ntdll->base_va + result.rva;
        patch_function_entry(vm, hook_rtl_mb_size, WBOX_SYSCALL_MBSTR_SIZE);
        printf("  Patched RtlMultiByteToUnicodeSize at 0x%08X\n", hook_rtl_mb_size);
    }

    result = exports_lookup_by_name(ntdll, "RtlUnicodeToMultiByteSize");
    if (result.found && !result.is_forwarder) {
        hook_rtl_unicode_size = ntdll->base_va + result.rva;
        patch_function_entry(vm, hook_rtl_unicode_size, WBOX_SYSCALL_UNICODE_SIZE);
        printf("  Patched RtlUnicodeToMultiByteSize at 0x%08X\n", hook_rtl_unicode_size);
    }

    /* OEM string conversion hooks */
    result = exports_lookup_by_name(ntdll, "RtlOemToUnicodeN");
    if (result.found && !result.is_forwarder) {
        uint32_t hook_addr = ntdll->base_va + result.rva;
        patch_function_entry(vm, hook_addr, WBOX_SYSCALL_OEM_TO_UNICODE);
        printf("  Patched RtlOemToUnicodeN at 0x%08X\n", hook_addr);
    }

    result = exports_lookup_by_name(ntdll, "RtlUnicodeToOemN");
    if (result.found && !result.is_forwarder) {
        uint32_t hook_addr = ntdll->base_va + result.rva;
        patch_function_entry(vm, hook_addr, WBOX_SYSCALL_UNICODE_TO_OEM);
        printf("  Patched RtlUnicodeToOemN at 0x%08X\n", hook_addr);
    }

    return 0;
}

int heap_install_kernel32_hooks(vm_context_t *vm, loaded_module_t *kernel32)
{
    if (!kernel32) return -1;

    export_lookup_t result;

    printf("Installing kernel32 hooks...\n");

    /* Hook GetCommandLineA */
    result = exports_lookup_by_name(kernel32, "GetCommandLineA");
    if (result.found && !result.is_forwarder) {
        uint32_t hook_addr = kernel32->base_va + result.rva;
        patch_function_entry(vm, hook_addr, WBOX_SYSCALL_GET_CMD_LINE_A);
        printf("  Patched GetCommandLineA at 0x%08X\n", hook_addr);
    }

    /* Hook GetCommandLineW */
    result = exports_lookup_by_name(kernel32, "GetCommandLineW");
    if (result.found && !result.is_forwarder) {
        uint32_t hook_addr = kernel32->base_va + result.rva;
        patch_function_entry(vm, hook_addr, WBOX_SYSCALL_GET_CMD_LINE_W);
        printf("  Patched GetCommandLineW at 0x%08X\n", hook_addr);
    }

    return 0;
}

bool heap_is_hooked_addr(heap_state_t *heap, uint32_t addr)
{
    return (addr == hook_rtl_allocate_heap && hook_rtl_allocate_heap != 0) ||
           (addr == hook_rtl_free_heap && hook_rtl_free_heap != 0) ||
           (addr == hook_rtl_realloc_heap && hook_rtl_realloc_heap != 0) ||
           (addr == hook_rtl_size_heap && hook_rtl_size_heap != 0);
}

bool heap_handle_call(heap_state_t *heap, vm_context_t *vm, uint32_t addr)
{
    /* Read return address and parameters from stack */
    uint32_t esp = ESP;
    uint32_t return_addr = read_virt_l(vm, esp);
    uint32_t param1 = read_virt_l(vm, esp + 4);   /* HeapHandle */
    uint32_t param2 = read_virt_l(vm, esp + 8);   /* Flags */
    uint32_t param3 = read_virt_l(vm, esp + 12);  /* Size or Ptr */
    uint32_t param4 = read_virt_l(vm, esp + 16);  /* Size (for realloc) */

    uint32_t result = 0;
    int stack_cleanup = 0;

    if (addr == hook_rtl_allocate_heap) {
        /* RtlAllocateHeap(HeapHandle, Flags, Size) - stdcall, 3 params */
        result = heap_alloc(heap, vm, param1, param2, param3);
        stack_cleanup = 12;
    } else if (addr == hook_rtl_free_heap) {
        /* RtlFreeHeap(HeapHandle, Flags, Ptr) - stdcall, 3 params */
        result = heap_free(heap, vm, param1, param2, param3) ? 1 : 0;
        stack_cleanup = 12;
    } else if (addr == hook_rtl_realloc_heap) {
        /* RtlReAllocateHeap(HeapHandle, Flags, Ptr, Size) - stdcall, 4 params */
        result = heap_realloc(heap, vm, param1, param2, param3, param4);
        stack_cleanup = 16;
    } else if (addr == hook_rtl_size_heap) {
        /* RtlSizeHeap(HeapHandle, Flags, Ptr) - stdcall, 3 params */
        result = heap_size(heap, vm, param1, param2, param3);
        stack_cleanup = 12;
    } else {
        return false;
    }

    /* Set return value */
    EAX = result;

    /* Pop return address and parameters (stdcall convention) */
    ESP = esp + 4 + stack_cleanup;

    /* Jump to return address */
    cpu_state.pc = return_addr;

    return true;
}
