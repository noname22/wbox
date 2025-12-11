/*
 * WBOX USER Syscall Implementations
 * Bootstrap syscalls needed for DLL initialization
 */
#include "user_shared.h"
#include "user_handle_table.h"
#include "user_class.h"
#include "../nt/syscalls.h"
#include "../nt/win32k_syscalls.h"
#include "../vm/vm.h"
#include "../vm/paging.h"
#include "../cpu/cpu.h"
#include "../cpu/mem.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Client callback function pointers (from NtUserInitializeClientPfnArrays) */
static uint32_t g_pfnClientA = 0;
static uint32_t g_pfnClientW = 0;
static uint32_t g_pfnClientWorker = 0;
static uint32_t g_hmodUser32 = 0;
static bool g_client_pfn_init = false;

/* USER subsystem initialization state */
static bool g_user_initialized = false;

/*
 * Read a stack argument (win32k syscall convention)
 * Stack layout at SYSENTER:
 *   ESP+0:  return address from "call [0x7FFE0300]" (back to syscall stub)
 *   ESP+4:  return address from "call NtUser*" (back to caller)
 *   ESP+8:  arg 0
 *   ESP+12: arg 1
 *   etc.
 */
static uint32_t read_stack_arg(int index)
{
    return readmemll(ESP + 8 + (index * 4));
}

/*
 * Write to guest memory
 */
static void write_guest_mem(uint32_t va, const void *data, size_t size)
{
    vm_context_t *vm = vm_get_context();
    if (!vm) return;

    for (size_t i = 0; i < size; i++) {
        uint32_t phys = paging_get_phys(&vm->paging, va + i);
        if (phys) {
            mem_writeb_phys(phys, ((const uint8_t *)data)[i]);
        }
    }
}

/*
 * Read from guest memory
 */
static void read_guest_mem(uint32_t va, void *data, size_t size)
{
    vm_context_t *vm = vm_get_context();
    if (!vm) return;

    for (size_t i = 0; i < size; i++) {
        uint32_t phys = paging_get_phys(&vm->paging, va + i);
        if (phys) {
            ((uint8_t *)data)[i] = mem_readb_phys(phys);
        }
    }
}

/*
 * Write a DWORD to guest memory
 */
static void write_guest_dword(uint32_t va, uint32_t value)
{
    vm_context_t *vm = vm_get_context();
    if (!vm) return;

    uint32_t phys = paging_get_phys(&vm->paging, va);
    if (phys) {
        mem_writel_phys(phys, value);
    }
}

/*
 * Read a UNICODE_STRING from guest memory
 */
static void read_guest_unicode_string(uint32_t va, wchar_t *buffer, size_t maxlen)
{
    vm_context_t *vm = vm_get_context();
    if (!vm || !buffer) return;

    buffer[0] = 0;

    /* UNICODE_STRING: Length (2), MaximumLength (2), Buffer (4) */
    uint16_t length = 0;
    uint32_t buf_ptr = 0;

    uint32_t phys = paging_get_phys(&vm->paging, va);
    if (!phys) return;

    length = mem_readw_phys(phys);
    buf_ptr = mem_readl_phys(phys + 4);

    if (buf_ptr == 0 || length == 0) return;

    /* Read the string */
    size_t chars = length / 2;
    if (chars >= maxlen) chars = maxlen - 1;

    for (size_t i = 0; i < chars; i++) {
        uint32_t char_phys = paging_get_phys(&vm->paging, buf_ptr + i * 2);
        if (char_phys) {
            buffer[i] = mem_readw_phys(char_phys);
        } else {
            buffer[i] = 0;
            break;
        }
    }
    buffer[chars] = 0;
}

/*
 * Initialize USER subsystem
 */
static int user_ensure_init(void)
{
    if (g_user_initialized) {
        return 0;
    }

    /* Initialize shared info */
    if (user_shared_init() < 0) {
        printf("USER: Failed to initialize shared info\n");
        return -1;
    }

    /* Initialize handle table */
    if (user_handle_table_global_init() < 0) {
        printf("USER: Failed to initialize handle table\n");
        return -1;
    }

    /* Initialize class subsystem */
    if (user_class_init() < 0) {
        printf("USER: Failed to initialize class subsystem\n");
        return -1;
    }

    g_user_initialized = true;
    printf("USER: Subsystem initialized\n");
    return 0;
}

/*
 * NtUserProcessConnect - establish shared memory region
 * Called by user32.dll during initialization
 */
ntstatus_t sys_NtUserProcessConnect(void)
{
    uint32_t hProcess = read_stack_arg(0);
    uint32_t pUserConnect = read_stack_arg(1);
    uint32_t dwSize = read_stack_arg(2);

    (void)hProcess;
    (void)dwSize;

    fprintf(stderr, "USER: NtUserProcessConnect(hProcess=0x%X, pUserConnect=0x%X, size=%d)\n",
           hProcess, pUserConnect, dwSize);

    /* Ensure USER subsystem is initialized */
    if (user_ensure_init() < 0) {
        EAX = STATUS_UNSUCCESSFUL;
        return STATUS_UNSUCCESSFUL;
    }

    /* Fill USERCONNECT structure */
    WBOX_USERCONNECT uc;
    user_fill_userconnect(&uc);

    /* Write to guest memory */
    write_guest_mem(pUserConnect, &uc, sizeof(WBOX_USERCONNECT));

    EAX = STATUS_SUCCESS;
    return STATUS_SUCCESS;
}

/*
 * NtUserInitializeClientPfnArrays - register client callbacks
 * Called by user32.dll to register window procedure callbacks
 */
ntstatus_t sys_NtUserInitializeClientPfnArrays(void)
{
    uint32_t pfnClientA = read_stack_arg(0);
    uint32_t pfnClientW = read_stack_arg(1);
    uint32_t pfnClientWorker = read_stack_arg(2);
    uint32_t hmodUser = read_stack_arg(3);

    printf("USER: NtUserInitializeClientPfnArrays(A=0x%X, W=0x%X, Worker=0x%X, hmod=0x%X)\n",
           pfnClientA, pfnClientW, pfnClientWorker, hmodUser);

    /* Store the callback pointers */
    g_pfnClientA = pfnClientA;
    g_pfnClientW = pfnClientW;
    g_pfnClientWorker = pfnClientWorker;
    g_hmodUser32 = hmodUser;
    g_client_pfn_init = true;

    EAX = STATUS_SUCCESS;
    return STATUS_SUCCESS;
}

/*
 * NtUserGetClassInfo - get window class information
 * This is the syscall that was causing the crash!
 */
ntstatus_t sys_NtUserGetClassInfo(void)
{
    uint32_t hInstance = read_stack_arg(0);
    uint32_t pClassName = read_stack_arg(1);    /* PUNICODE_STRING */
    uint32_t pWndClass = read_stack_arg(2);     /* PWNDCLASSEXW */
    uint32_t ppMenuName = read_stack_arg(3);    /* LPWSTR* */
    uint32_t bAnsi = read_stack_arg(4);

    (void)bAnsi;

    /* Read class name from guest */
    wchar_t className[MAX_CLASSNAME];
    read_guest_unicode_string(pClassName, className, MAX_CLASSNAME);

    printf("USER: NtUserGetClassInfo(hInstance=0x%X, class='%ls')\n",
           hInstance, className);

    /* Ensure USER initialized */
    if (user_ensure_init() < 0) {
        EAX = 0;
        return STATUS_SUCCESS;
    }

    /* Look up class */
    WBOX_WNDCLASSEXW wcx;
    uint16_t atom = user_class_get_info(className, hInstance, &wcx);

    if (atom == 0) {
        printf("USER: Class '%ls' not found\n", className);
        EAX = 0;
        return STATUS_SUCCESS;
    }

    /* Write WNDCLASSEXW to guest memory */
    write_guest_mem(pWndClass, &wcx, sizeof(WBOX_WNDCLASSEXW));

    /* Write menu name pointer if requested */
    if (ppMenuName != 0) {
        write_guest_dword(ppMenuName, 0);  /* No menu name for now */
    }

    printf("USER: Class '%ls' found, atom=0x%04X\n", className, atom);
    EAX = atom;
    return STATUS_SUCCESS;
}

/*
 * NtUserRegisterClassExWOW - register a window class
 */
ntstatus_t sys_NtUserRegisterClassExWOW(void)
{
    uint32_t pWndClass = read_stack_arg(0);      /* PWNDCLASSEXW */
    uint32_t pClassName = read_stack_arg(1);     /* PUNICODE_STRING */
    uint32_t pClsNVClassName = read_stack_arg(2);
    uint32_t pClsMenuName = read_stack_arg(3);   /* PUNICODE_STRING */
    uint16_t fnID = (uint16_t)read_stack_arg(4);
    uint32_t dwFlags = read_stack_arg(5);
    uint32_t pdwWow = read_stack_arg(6);

    (void)pClsNVClassName;
    (void)pClsMenuName;
    (void)dwFlags;
    (void)pdwWow;

    /* Ensure USER initialized */
    if (user_ensure_init() < 0) {
        EAX = 0;
        return STATUS_SUCCESS;
    }

    /* Read WNDCLASSEXW from guest */
    WBOX_WNDCLASSEXW wcx;
    read_guest_mem(pWndClass, &wcx, sizeof(WBOX_WNDCLASSEXW));

    /* Read class name */
    wchar_t className[MAX_CLASSNAME];
    read_guest_unicode_string(pClassName, className, MAX_CLASSNAME);

    printf("USER: NtUserRegisterClassExWOW(class='%ls', style=0x%X, wndproc=0x%X)\n",
           className, wcx.style, wcx.lpfnWndProc);

    /* Check if already registered */
    if (user_class_find(className, wcx.hInstance)) {
        printf("USER: Class '%ls' already registered\n", className);
        WBOX_CLS *existing = user_class_find(className, wcx.hInstance);
        EAX = existing->atomClassName;
        return STATUS_SUCCESS;
    }

    /* Allocate new class */
    WBOX_CLS *cls = calloc(1, sizeof(WBOX_CLS));
    if (!cls) {
        EAX = 0;
        return STATUS_SUCCESS;
    }

    /* Fill class info */
    wcsncpy(cls->szClassName, className, MAX_CLASSNAME - 1);
    cls->style = wcx.style;
    cls->lpfnWndProc = wcx.lpfnWndProc;
    cls->cbClsExtra = wcx.cbClsExtra;
    cls->cbWndExtra = wcx.cbWndExtra;
    cls->hModule = wcx.hInstance;
    cls->hIcon = wcx.hIcon;
    cls->hCursor = wcx.hCursor;
    cls->hbrBackground = wcx.hbrBackground;
    cls->hIconSm = wcx.hIconSm;
    cls->fnid = fnID;

    /* Register */
    uint16_t atom = user_class_register(cls);
    if (atom == 0) {
        free(cls);
        EAX = 0;
        return STATUS_SUCCESS;
    }

    EAX = atom;
    return STATUS_SUCCESS;
}

/*
 * NtUserGetClassInfoEx - variant of GetClassInfo
 */
ntstatus_t sys_NtUserGetClassInfoEx(void)
{
    /* Same as NtUserGetClassInfo for our purposes */
    return sys_NtUserGetClassInfo();
}

/*
 * Get client callback pointers (for other modules)
 */
uint32_t user_get_pfn_client_a(void) { return g_pfnClientA; }
uint32_t user_get_pfn_client_w(void) { return g_pfnClientW; }
uint32_t user_get_hmod_user32(void) { return g_hmodUser32; }
bool user_is_client_pfn_init(void) { return g_client_pfn_init; }

/*
 * Check if USER subsystem is initialized
 */
bool user_is_initialized(void) { return g_user_initialized; }
