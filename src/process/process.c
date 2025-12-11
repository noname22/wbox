/*
 * WBOX Process Structures
 * TEB (Thread Environment Block) and PEB (Process Environment Block) initialization
 */
#include "process.h"
#include "../cpu/mem.h"
#include "../vm/paging.h"
#include "../nt/handles.h"

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

/* Helper: Write wide string to virtual address, returns bytes written */
static uint32_t write_virt_wstr(vm_context_t *vm, uint32_t virt, const char *str)
{
    uint32_t offset = 0;
    while (*str) {
        write_virt_w(vm, virt + offset, (uint16_t)(unsigned char)*str);
        offset += 2;
        str++;
    }
    write_virt_w(vm, virt + offset, 0);  /* Null terminator */
    offset += 2;
    return offset;
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

/* Address for RTL_USER_PROCESS_PARAMETERS - in same page as PEB for simplicity */
#define VM_PROCESS_PARAMS_ADDR  (VM_PEB_ADDR + 0x200)
/* Address for environment block - after process params */
#define VM_ENVIRONMENT_ADDR     (VM_PEB_ADDR + 0x400)
/* Address for critical sections - after environment */
#define VM_FAST_PEB_LOCK_ADDR   (VM_PEB_ADDR + 0x800)  /* RTL_CRITICAL_SECTION for FastPebLock */
#define VM_LOADER_LOCK_ADDR     (VM_PEB_ADDR + 0x820)  /* RTL_CRITICAL_SECTION for LoaderLock */

/* RTL_CRITICAL_SECTION structure offsets */
#define CS_DEBUG_INFO           0x00  /* PRTL_CRITICAL_SECTION_DEBUG */
#define CS_LOCK_COUNT           0x04  /* LONG - -1 means unlocked */
#define CS_RECURSION_COUNT      0x08  /* LONG */
#define CS_OWNING_THREAD        0x0C  /* HANDLE */
#define CS_LOCK_SEMAPHORE       0x10  /* HANDLE */
#define CS_SPIN_COUNT           0x14  /* ULONG_PTR */
#define CS_SIZE                 0x18  /* 24 bytes */

/* PEB.LoaderLock offset (not defined in process.h) */
#define PEB_LOADER_LOCK         0xA0  /* LoaderLock - RTL_CRITICAL_SECTION pointer */

/* Initialize an RTL_CRITICAL_SECTION at the given address */
static void init_critical_section(vm_context_t *vm, uint32_t addr)
{
    /* DebugInfo = NULL */
    write_virt_l(vm, addr + CS_DEBUG_INFO, 0);
    /* LockCount = -1 (unlocked) */
    write_virt_l(vm, addr + CS_LOCK_COUNT, 0xFFFFFFFF);
    /* RecursionCount = 0 */
    write_virt_l(vm, addr + CS_RECURSION_COUNT, 0);
    /* OwningThread = NULL */
    write_virt_l(vm, addr + CS_OWNING_THREAD, 0);
    /* LockSemaphore = NULL */
    write_virt_l(vm, addr + CS_LOCK_SEMAPHORE, 0);
    /* SpinCount = 0 */
    write_virt_l(vm, addr + CS_SPIN_COUNT, 0);
}

void process_init_peb(vm_context_t *vm)
{
    uint32_t peb = vm->peb_addr;
    uint32_t params = VM_PROCESS_PARAMS_ADDR;

    printf("Initializing PEB at 0x%08X\n", peb);

    uint32_t peb_phys = paging_get_phys(&vm->paging, peb);
    if (peb_phys == 0) {
        fprintf(stderr, "process_init_peb: PEB not mapped\n");
        return;
    }

    /* Save PEB.Ldr if already set by loader (don't clobber it when clearing) */
    uint32_t saved_ldr = mem_readl_phys(peb_phys + PEB_LDR);

    /* Clear PEB (except we'll restore Ldr) */
    for (uint32_t i = 0; i < PAGE_SIZE; i++) {
        mem_writeb_phys(peb_phys + i, 0);
    }

    /* Restore PEB.Ldr if it was set */
    if (saved_ldr != 0) {
        mem_writel_phys(peb_phys + PEB_LDR, saved_ldr);
    }

    /* Not being debugged */
    write_virt_b(vm, peb + PEB_BEING_DEBUGGED, 0);

    /* Image base address */
    write_virt_l(vm, peb + PEB_IMAGE_BASE_ADDRESS, vm->image_base);

    /* Ldr was already restored from saved_ldr if set by loader
     * For static executables without DLLs, it will remain NULL */

    /* Set up RTL_USER_PROCESS_PARAMETERS at params address
     * This is required for GetStartupInfoW and similar functions */
    printf("  ProcessParameters at 0x%08X\n", params);

    /* Basic structure info */
    write_virt_l(vm, params + RUPP_MAX_LENGTH, RUPP_SIZE);
    write_virt_l(vm, params + RUPP_LENGTH, RUPP_SIZE);
    write_virt_l(vm, params + RUPP_FLAGS, 0);

    /* Console handles - use standard pseudo handles */
    write_virt_l(vm, params + RUPP_STDIN_HANDLE, STD_INPUT_HANDLE);
    write_virt_l(vm, params + RUPP_STDOUT_HANDLE, STD_OUTPUT_HANDLE);
    write_virt_l(vm, params + RUPP_STDERR_HANDLE, STD_ERROR_HANDLE);

    /* Window position/size - use defaults */
    write_virt_l(vm, params + RUPP_STARTING_X, 0);
    write_virt_l(vm, params + RUPP_STARTING_Y, 0);
    write_virt_l(vm, params + RUPP_COUNT_X, 800);  /* Default width */
    write_virt_l(vm, params + RUPP_COUNT_Y, 600);  /* Default height */
    write_virt_l(vm, params + RUPP_COUNT_CHARS_X, 80);  /* Console cols */
    write_virt_l(vm, params + RUPP_COUNT_CHARS_Y, 25);  /* Console rows */
    write_virt_l(vm, params + RUPP_FILL_ATTRIBUTE, 0);
    write_virt_l(vm, params + RUPP_WINDOW_FLAGS, 0);
    write_virt_l(vm, params + RUPP_SHOW_WINDOW_FLAGS, 1);  /* SW_SHOWNORMAL */

    /* All UNICODE_STRING fields (CommandLine, WindowTitle, etc.) are left as zero
     * which means Length=0, MaxLength=0, Buffer=NULL - empty strings */

    /* Set up environment block
     * Format: null-terminated wide strings, ending with an extra null */
    uint32_t env = VM_ENVIRONMENT_ADDR;
    uint32_t env_offset = 0;
    printf("  Environment at 0x%08X\n", env);

    /* Add minimal environment variables */
    env_offset += write_virt_wstr(vm, env + env_offset, "COMPUTERNAME=WBOX");
    env_offset += write_virt_wstr(vm, env + env_offset, "PATH=C:\\WINDOWS\\system32;C:\\WINDOWS");
    env_offset += write_virt_wstr(vm, env + env_offset, "SYSTEMDRIVE=C:");
    env_offset += write_virt_wstr(vm, env + env_offset, "SYSTEMROOT=C:\\WINDOWS");
    env_offset += write_virt_wstr(vm, env + env_offset, "WINDIR=C:\\WINDOWS");
    env_offset += write_virt_wstr(vm, env + env_offset, "TEMP=C:\\WINDOWS\\TEMP");
    env_offset += write_virt_wstr(vm, env + env_offset, "TMP=C:\\WINDOWS\\TEMP");
    env_offset += write_virt_wstr(vm, env + env_offset, "USERNAME=WBOX");
    env_offset += write_virt_wstr(vm, env + env_offset, "USERPROFILE=C:\\Documents and Settings\\WBOX");
    /* Final null terminator (empty string ends the block) */
    write_virt_w(vm, env + env_offset, 0);

    /* Set Environment pointer in ProcessParameters */
    write_virt_l(vm, params + RUPP_ENVIRONMENT, env);

    /* Point PEB to ProcessParameters */
    write_virt_l(vm, peb + PEB_PROCESS_PARAMETERS, params);

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

    /* Initialize critical sections for FastPebLock and LoaderLock */
    printf("  Initializing critical sections...\n");

    /* FastPebLock */
    init_critical_section(vm, VM_FAST_PEB_LOCK_ADDR);
    write_virt_l(vm, peb + PEB_FAST_PEB_LOCK, VM_FAST_PEB_LOCK_ADDR);
    printf("  FastPebLock at 0x%08X\n", VM_FAST_PEB_LOCK_ADDR);

    /* LoaderLock */
    init_critical_section(vm, VM_LOADER_LOCK_ADDR);
    write_virt_l(vm, peb + PEB_LOADER_LOCK, VM_LOADER_LOCK_ADDR);
    printf("  LoaderLock at 0x%08X\n", VM_LOADER_LOCK_ADDR);

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
