/*
 * WBOX Process Structures
 * TEB (Thread Environment Block) and PEB (Process Environment Block) initialization
 */
#include "process.h"
#include "../cpu/mem.h"
#include "../vm/paging.h"

#include <stdio.h>

/* Helper: Write 32-bit value to virtual address (uses paging context) */
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

void process_init_teb(vm_context_t *vm)
{
    uint32_t teb = vm->teb_addr;

    printf("Initializing TEB at 0x%08X\n", teb);

    /* Clear TEB first */
    uint32_t teb_phys = paging_get_phys(&vm->paging, teb);
    if (teb_phys == 0) {
        fprintf(stderr, "process_init_teb: TEB not mapped\n");
        return;
    }
    for (uint32_t i = 0; i < PAGE_SIZE; i++) {
        mem_writeb_phys(teb_phys + i, 0);
    }

    /* Exception list - NULL (end of chain marker is -1) */
    write_virt_l(vm, teb + TEB_EXCEPTION_LIST, 0xFFFFFFFF);

    /* Stack base (top of stack) */
    write_virt_l(vm, teb + TEB_STACK_BASE, vm->stack_top);

    /* Stack limit (bottom of stack) */
    write_virt_l(vm, teb + TEB_STACK_LIMIT, vm->stack_base);

    /* Self pointer - linear address of TEB (for fs:[0x18]) */
    write_virt_l(vm, teb + TEB_SELF, teb);

    /* Process ID */
    write_virt_l(vm, teb + TEB_PROCESS_ID, WBOX_PROCESS_ID);

    /* Thread ID */
    write_virt_l(vm, teb + TEB_THREAD_ID, WBOX_THREAD_ID);

    /* PEB pointer */
    write_virt_l(vm, teb + TEB_PEB_POINTER, vm->peb_addr);

    /* Last error = 0 (no error) */
    write_virt_l(vm, teb + TEB_LAST_ERROR, 0);

    printf("  StackBase=0x%08X StackLimit=0x%08X\n", vm->stack_top, vm->stack_base);
    printf("  Self=0x%08X PEB=0x%08X\n", teb, vm->peb_addr);
    printf("  ProcessId=%d ThreadId=%d\n", WBOX_PROCESS_ID, WBOX_THREAD_ID);
}

void process_init_peb(vm_context_t *vm)
{
    uint32_t peb = vm->peb_addr;

    printf("Initializing PEB at 0x%08X\n", peb);

    /* Clear PEB first */
    uint32_t peb_phys = paging_get_phys(&vm->paging, peb);
    if (peb_phys == 0) {
        fprintf(stderr, "process_init_peb: PEB not mapped\n");
        return;
    }
    for (uint32_t i = 0; i < PAGE_SIZE; i++) {
        mem_writeb_phys(peb_phys + i, 0);
    }

    /* Not being debugged */
    write_virt_b(vm, peb + PEB_BEING_DEBUGGED, 0);

    /* Image base address */
    write_virt_l(vm, peb + PEB_IMAGE_BASE_ADDRESS, vm->image_base);

    /* Ldr - NULL for now (static executables don't need it) */
    write_virt_l(vm, peb + PEB_LDR, 0);

    /* ProcessParameters - NULL for now */
    write_virt_l(vm, peb + PEB_PROCESS_PARAMETERS, 0);

    /* ProcessHeap - NULL for now */
    write_virt_l(vm, peb + PEB_PROCESS_HEAP, 0);

    /* Number of processors */
    write_virt_l(vm, peb + PEB_NUMBER_OF_PROCESSORS, 1);

    /* OS version info (Windows XP SP3) */
    write_virt_l(vm, peb + PEB_OS_MAJOR_VERSION, WBOX_OS_MAJOR_VERSION);
    write_virt_l(vm, peb + PEB_OS_MINOR_VERSION, WBOX_OS_MINOR_VERSION);
    write_virt_w(vm, peb + PEB_OS_BUILD_NUMBER, WBOX_OS_BUILD_NUMBER);
    write_virt_l(vm, peb + PEB_OS_PLATFORM_ID, WBOX_OS_PLATFORM_ID);

    /* Subsystem info (CUI = 3) */
    write_virt_l(vm, peb + PEB_IMAGE_SUBSYSTEM, IMAGE_SUBSYSTEM_WINDOWS_CUI);
    write_virt_l(vm, peb + PEB_IMAGE_SUBSYSTEM_MAJOR, WBOX_OS_MAJOR_VERSION);
    write_virt_l(vm, peb + PEB_IMAGE_SUBSYSTEM_MINOR, WBOX_OS_MINOR_VERSION);

    /* NtGlobalFlag = 0 */
    write_virt_l(vm, peb + PEB_NT_GLOBAL_FLAG, 0);

    /* Session ID = 0 */
    write_virt_l(vm, peb + PEB_SESSION_ID, 0);

    printf("  ImageBase=0x%08X\n", vm->image_base);
    printf("  OS Version: %d.%d.%d (Platform %d)\n",
           WBOX_OS_MAJOR_VERSION, WBOX_OS_MINOR_VERSION,
           WBOX_OS_BUILD_NUMBER, WBOX_OS_PLATFORM_ID);
}

uint32_t process_get_teb_phys(vm_context_t *vm)
{
    return paging_get_phys(&vm->paging, vm->teb_addr);
}

uint32_t process_get_peb_phys(vm_context_t *vm)
{
    return paging_get_phys(&vm->paging, vm->peb_addr);
}
