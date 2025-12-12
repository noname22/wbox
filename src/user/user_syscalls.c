/*
 * WBOX USER Syscall Implementations
 * Bootstrap syscalls needed for DLL initialization
 */
#include "user_shared.h"
#include "user_handle_table.h"
#include "user_class.h"
#include "user_window.h"
#include "user_message.h"
#include "user_callback.h"
#include "desktop_heap.h"
#include "guest_wnd.h"
#include "../nt/syscalls.h"
#include "../nt/win32k_syscalls.h"
#include "../vm/vm.h"
#include "../vm/paging.h"
#include "../gdi/display.h"
#include "../cpu/cpu.h"
#include "../cpu/mem.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

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
 * Check if a pointer value is an atom (low 16-bit value)
 * In Windows, MAKEINTATOM produces values where HIWORD == 0
 */
static inline bool is_atom(uint32_t ptr)
{
    return (ptr >> 16) == 0;
}

/*
 * Read a UNICODE_STRING from guest memory
 * Returns atom value if the string represents an atom (Length=0, Buffer is atom)
 * Returns 0 and fills buffer if it's a regular string
 */
static uint16_t read_guest_unicode_string(uint32_t va, wchar_t *buffer, size_t maxlen)
{
    vm_context_t *vm = vm_get_context();
    if (!vm || !buffer) return 0;

    buffer[0] = 0;

    if (va == 0) {
        return 0;
    }

    /* UNICODE_STRING: Length (2), MaximumLength (2), Buffer (4) */
    uint16_t length = 0;
    uint16_t maxLength = 0;
    uint32_t buf_ptr = 0;

    uint32_t phys = paging_get_phys(&vm->paging, va);
    if (!phys) {
        fprintf(stderr, "DEBUG: read_guest_unicode_string: va=0x%08X - no mapping\n", va);
        return 0;
    }

    length = mem_readw_phys(phys);
    maxLength = mem_readw_phys(phys + 2);
    buf_ptr = mem_readl_phys(phys + 4);

    fprintf(stderr, "DEBUG: UNICODE_STRING at 0x%08X: Length=%u, MaxLen=%u, Buffer=0x%08X\n",
            va, length, maxLength, buf_ptr);

    /* Check for atom: Length=0 but Buffer is a valid atom value (HIWORD == 0) */
    if (length == 0 && buf_ptr != 0 && is_atom(buf_ptr)) {
        fprintf(stderr, "DEBUG: UNICODE_STRING contains atom 0x%04X\n", (uint16_t)buf_ptr);
        return (uint16_t)buf_ptr;
    }

    if (buf_ptr == 0 || length == 0) return 0;

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
 * LARGE_STRING structure (used by NtUserCreateWindowEx)
 * Note: MaximumLength is 31 bits, bAnsi is 1 bit (high bit of the DWORD)
 * Total size: 12 bytes
 */
typedef struct _LARGE_STRING {
    uint32_t Length;                /* Length in bytes (not including null) */
    uint32_t MaxLenAndAnsi;         /* bits 0-30: MaximumLength, bit 31: bAnsi */
    uint32_t Buffer;                /* Guest pointer to string data */
} LARGE_STRING;

#define LARGE_STRING_MAX_LEN(x)  ((x).MaxLenAndAnsi & 0x7FFFFFFF)
#define LARGE_STRING_IS_ANSI(x)  (((x).MaxLenAndAnsi >> 31) & 1)

/*
 * Read a LARGE_STRING from guest memory
 * Returns atom value if pClassName is an atom (caller should look up class by atom)
 * Returns 0 if it's a string (and fills buffer)
 */
static uint16_t read_guest_large_string(uint32_t va, wchar_t *buffer, size_t maxlen)
{
    vm_context_t *vm = vm_get_context();
    if (!vm || !buffer) return 0;

    buffer[0] = 0;

    if (va == 0) {
        return 0;
    }

    /* Check if this is an atom value rather than a pointer */
    if (is_atom(va)) {
        /* This is an atom, not a pointer to LARGE_STRING */
        fprintf(stderr, "DEBUG: read_guest_large_string: 0x%08X is an atom\n", va);
        return (uint16_t)va;
    }

    /* Read LARGE_STRING structure (12 bytes) */
    LARGE_STRING ls;
    read_guest_mem(va, &ls, sizeof(LARGE_STRING));

    uint32_t maxLen = LARGE_STRING_MAX_LEN(ls);
    uint32_t bAnsi = LARGE_STRING_IS_ANSI(ls);

    fprintf(stderr, "DEBUG: LARGE_STRING at 0x%08X: Length=%u, MaxLen=%u, bAnsi=%u, Buffer=0x%08X\n",
            va, ls.Length, maxLen, bAnsi, ls.Buffer);

    if (ls.Buffer == 0 || ls.Length == 0) return 0;

    if (bAnsi) {
        /* ANSI string - convert to wide char */
        size_t chars = ls.Length;
        if (chars >= maxlen) chars = maxlen - 1;

        for (size_t i = 0; i < chars; i++) {
            uint32_t phys = paging_get_phys(&vm->paging, ls.Buffer + i);
            if (phys) {
                buffer[i] = (wchar_t)mem_readb_phys(phys);
            } else {
                buffer[i] = 0;
                break;
            }
        }
        buffer[chars] = 0;
    } else {
        /* Unicode string */
        size_t chars = ls.Length / 2;
        if (chars >= maxlen) chars = maxlen - 1;

        for (size_t i = 0; i < chars; i++) {
            uint32_t phys = paging_get_phys(&vm->paging, ls.Buffer + i * 2);
            if (phys) {
                buffer[i] = mem_readw_phys(phys);
            } else {
                buffer[i] = 0;
                break;
            }
        }
        buffer[chars] = 0;
    }

    return 0;  /* Not an atom */
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

    /* Initialize desktop heap (before class/window init so they can allocate guest structures) */
    vm_context_t *vm = vm_get_context();
    if (vm && desktop_heap_init(vm) < 0) {
        printf("USER: Failed to initialize desktop heap\n");
        return -1;
    }

    /* Initialize class subsystem */
    if (user_class_init() < 0) {
        printf("USER: Failed to initialize class subsystem\n");
        return -1;
    }

    /* Initialize message queue */
    msg_queue_init();

    g_user_initialized = true;
    printf("USER: Subsystem initialized\n");
    return 0;
}

/* Address for DESKTOPINFO in KUSD page (after other stubs) */
#define DESKTOPINFO_GUEST_VA    0x7FFE0400
/* Address for fake THREADINFO structure (just needs to be non-NULL) */
#define THREADINFO_GUEST_VA     0x7FFE0500

/* DESKTOPINFO has already been initialized */
static bool g_desktopinfo_initialized = false;

/*
 * Initialize Win32ClientInfo in TEB
 * This is critical for user32's callback mechanism to work
 */
static void init_win32_client_info(vm_context_t *vm, uint32_t desktopinfo_va)
{
    uint32_t teb_va = 0x7FFDF000;  /* Standard TEB address */
    uint32_t clientinfo_va = teb_va + 0x6CC;  /* TEB.Win32ClientInfo */

    fprintf(stderr, "USER: Initializing Win32ClientInfo at TEB+0x6CC (0x%08X)\n", clientinfo_va);

    /*
     * CLIENTINFO structure layout (overlaid on Win32ClientInfo[62] = 248 bytes):
     *   +0x00: CI_flags (ULONG_PTR)
     *   +0x04: cSpins (ULONG_PTR)
     *   +0x08: dwExpWinVer (DWORD)
     *   +0x0C: dwCompatFlags (DWORD)
     *   +0x10: dwCompatFlags2 (DWORD)
     *   +0x14: dwTIFlags (DWORD)
     *   +0x18: pDeskInfo (PDESKTOPINFO)
     *   +0x1C: ulClientDelta (ULONG_PTR)
     *   +0x20: phkCurrent (PHOOK)
     *   +0x24: fsHooks (ULONG)
     *   +0x28: CallbackWnd.hWnd (HWND) - 4 bytes
     *   +0x2C: CallbackWnd.pWnd (PWND) - 4 bytes
     *   +0x30: CallbackWnd.pActCtx (PVOID) - 4 bytes
     *   +0x34: dwHookCurrent (DWORD)
     *   +0x38: cInDDEMLCallback (INT)
     *   +0x3C: pClientThreadInfo (PCLIENTTHREADINFO)
     *   +0x40: dwHookData (ULONG_PTR)
     *   +0x44: dwKeyCache (DWORD)
     *   +0x48: afKeyState[8] (BYTE[8])
     *   +0x50: dwAsyncKeyCache (DWORD)
     *   +0x54: afAsyncKeyState[8] (BYTE[8])
     *   +0x5C: afAsyncKeyStateRecentDow[8] (BYTE[8])
     *   +0x64: hKL (HKL) - 4 bytes
     *   +0x68: CodePage (USHORT) - 2 bytes
     *   ...and more fields
     *
     * Total CLIENTINFO is about 0xF8 (248) bytes to fit in Win32ClientInfo[62]
     */

    /* Clear the entire CLIENTINFO area first (62 DWORDs = 248 bytes) */
    for (int i = 0; i < 62; i++) {
        write_guest_dword(clientinfo_va + i * 4, 0);
    }

    /* CI_flags - mark thread as initialized */
    write_guest_dword(clientinfo_va + 0x00, 0x00000008);  /* CI_INITTHREAD */

    /* cSpins */
    write_guest_dword(clientinfo_va + 0x04, 0);

    /* dwExpWinVer - Windows XP (5.1) */
    write_guest_dword(clientinfo_va + 0x08, 0x0501);

    /* dwCompatFlags, dwCompatFlags2 */
    write_guest_dword(clientinfo_va + 0x0C, 0);
    write_guest_dword(clientinfo_va + 0x10, 0);

    /* dwTIFlags */
    write_guest_dword(clientinfo_va + 0x14, 0);

    /* pDeskInfo - CRITICAL! Points to our DESKTOPINFO structure */
    write_guest_dword(clientinfo_va + 0x18, desktopinfo_va);

    /* ulClientDelta - set to 0 (no shared memory offset adjustment) */
    write_guest_dword(clientinfo_va + 0x1C, 0);

    /* phkCurrent */
    write_guest_dword(clientinfo_va + 0x20, 0);

    /* fsHooks */
    write_guest_dword(clientinfo_va + 0x24, 0);

    /* CallbackWnd - initialized to 0 (will be set during callbacks) */
    write_guest_dword(clientinfo_va + 0x28, 0);  /* hWnd */
    write_guest_dword(clientinfo_va + 0x2C, 0);  /* pWnd */
    write_guest_dword(clientinfo_va + 0x30, 0);  /* pActCtx */

    /* dwHookCurrent */
    write_guest_dword(clientinfo_va + 0x34, 0);

    /* cInDDEMLCallback */
    write_guest_dword(clientinfo_va + 0x38, 0);

    /* pClientThreadInfo - NULL (no separate client thread info) */
    write_guest_dword(clientinfo_va + 0x3C, 0);

    /* dwHookData */
    write_guest_dword(clientinfo_va + 0x40, 0);

    fprintf(stderr, "USER: Win32ClientInfo.pDeskInfo = 0x%08X\n", desktopinfo_va);
}

/*
 * Initialize DESKTOPINFO structure in guest memory
 */
static void init_desktopinfo(vm_context_t *vm, uint32_t desktopinfo_va)
{
    fprintf(stderr, "USER: Initializing DESKTOPINFO at 0x%08X\n", desktopinfo_va);

    /*
     * DESKTOPINFO structure layout:
     *   +0x00: pvDesktopBase (PVOID) - desktop heap base
     *   +0x04: pvDesktopLimit (PVOID) - desktop heap limit
     *   +0x08: spwnd (WND*) - desktop window
     *   +0x0C: fsHooks (DWORD) - global hook flags
     *   +0x10: aphkStart[16] (LIST_ENTRY[16]) - hook chains (128 bytes)
     *   +0x90: hTaskManWindow (HWND)
     *   +0x94: hProgmanWindow (HWND)
     *   +0x98: hShellWindow (HWND)
     *   ...
     */

    /* Get desktop heap info */
    desktop_heap_t *heap = desktop_heap_get();
    uint32_t heap_base = heap ? heap->base_va : 0x01000000;
    uint32_t heap_limit = heap ? heap->limit_va : 0x01100000;

    /* pvDesktopBase - actual desktop heap address */
    write_guest_dword(desktopinfo_va + 0x00, heap_base);

    /* pvDesktopLimit */
    write_guest_dword(desktopinfo_va + 0x04, heap_limit);

    /* spwnd - get desktop window's guest WND */
    WBOX_WND *desktop = user_window_get_desktop();
    uint32_t desktop_wnd_va = desktop ? desktop->guest_wnd_va : 0;
    write_guest_dword(desktopinfo_va + 0x08, desktop_wnd_va);

    fprintf(stderr, "USER: DESKTOPINFO heap=0x%08X-0x%08X spwnd=0x%08X\n",
            heap_base, heap_limit, desktop_wnd_va);

    /* fsHooks - no global hooks */
    write_guest_dword(desktopinfo_va + 0x0C, 0);

    /* Initialize aphkStart[16] as empty LIST_ENTRYs (each points to itself) */
    for (int i = 0; i < 16; i++) {
        uint32_t list_entry_va = desktopinfo_va + 0x10 + (i * 8);
        write_guest_dword(list_entry_va + 0, list_entry_va);  /* Flink = self */
        write_guest_dword(list_entry_va + 4, list_entry_va);  /* Blink = self */
    }

    /* hTaskManWindow, hProgmanWindow, hShellWindow = NULL */
    write_guest_dword(desktopinfo_va + 0x90, 0);
    write_guest_dword(desktopinfo_va + 0x94, 0);
    write_guest_dword(desktopinfo_va + 0x98, 0);
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

    /* Initialize Win32ClientInfo in TEB (one-time setup) */
    vm_context_t *vm = vm_get_context();
    if (vm && !g_desktopinfo_initialized) {
        /* Initialize DESKTOPINFO structure */
        init_desktopinfo(vm, DESKTOPINFO_GUEST_VA);

        /* Initialize Win32ClientInfo in TEB */
        init_win32_client_info(vm, DESKTOPINFO_GUEST_VA);

        /* Initialize Win32ThreadInfo (TEB+0x40) - must be non-NULL for user32 to work
         * This is checked by various user32 functions before calling hooks, etc.
         * We point it to a simple stub structure. */
        uint32_t teb_va = 0x7FFDF000;
        write_guest_dword(teb_va + 0x40, THREADINFO_GUEST_VA);
        fprintf(stderr, "USER: TEB.Win32ThreadInfo = 0x%08X\n", THREADINFO_GUEST_VA);

        /* Initialize minimal THREADINFO at THREADINFO_GUEST_VA
         * The structure is complex but we just need key fields to not crash */
        write_guest_dword(THREADINFO_GUEST_VA + 0x00, 0);  /* Clear first few fields */
        write_guest_dword(THREADINFO_GUEST_VA + 0x04, 0);
        write_guest_dword(THREADINFO_GUEST_VA + 0x08, 0);

        g_desktopinfo_initialized = true;
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

    /* Read class name from guest - may be a string or an atom */
    wchar_t className[MAX_CLASSNAME];
    uint16_t inputAtom = read_guest_unicode_string(pClassName, className, MAX_CLASSNAME);

    /* Ensure USER initialized */
    if (user_ensure_init() < 0) {
        EAX = 0;
        return STATUS_SUCCESS;
    }

    /* Look up class - by atom or by name */
    WBOX_WNDCLASSEXW wcx;
    uint16_t atom = 0;

    if (inputAtom != 0) {
        printf("USER: NtUserGetClassInfo(hInstance=0x%X, atom=0x%04X)\n",
               hInstance, inputAtom);
        WBOX_CLS *cls = user_class_find_by_atom(inputAtom);
        if (cls) {
            atom = user_class_get_info(cls->szClassName, hInstance, &wcx);
        }
    } else {
        printf("USER: NtUserGetClassInfo(hInstance=0x%X, class='%ls')\n",
               hInstance, className);
        atom = user_class_get_info(className, hInstance, &wcx);
    }

    if (atom == 0) {
        printf("USER: Class '%ls' (atom=0x%04X) not found\n", className, inputAtom);
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

    /* Read class name - may be a string or an atom */
    wchar_t className[MAX_CLASSNAME];
    uint16_t classAtom = read_guest_unicode_string(pClassName, className, MAX_CLASSNAME);

    /* Handle class specified by atom */
    if (classAtom != 0) {
        /* Class specified by atom - check if it exists */
        WBOX_CLS *existing = user_class_find_by_atom(classAtom);
        if (existing) {
            printf("USER: NtUserRegisterClassExWOW(atom=0x%04X '%ls') - already registered\n",
                   classAtom, existing->szClassName);
            EAX = classAtom;
            return STATUS_SUCCESS;
        }

        /* Atom class doesn't exist - register it with a synthetic name */
        printf("USER: NtUserRegisterClassExWOW(atom=0x%04X, style=0x%X, wndproc=0x%X) - registering new\n",
               classAtom, wcx.style, wcx.lpfnWndProc);

        /* Allocate new class */
        WBOX_CLS *cls = calloc(1, sizeof(WBOX_CLS));
        if (!cls) {
            EAX = 0;
            return STATUS_SUCCESS;
        }

        /* Generate synthetic name from atom */
        swprintf(cls->szClassName, MAX_CLASSNAME, L"#%04X", classAtom);
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
        cls->atomClassName = classAtom;  /* Use the provided atom */

        /* Register - this will skip atom allocation since we already set it */
        uint16_t atom = user_class_register(cls);
        if (atom == 0) {
            free(cls);
            EAX = 0;
            return STATUS_SUCCESS;
        }

        EAX = atom;
        return STATUS_SUCCESS;
    }

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
 * NtUserCreateWindowEx - create a window
 * Syscall number: 348 (0x15C)
 *
 * Parameters:
 *   arg0:  DWORD dwExStyle           - Extended style
 *   arg1:  PLARGE_STRING className   - Class name
 *   arg2:  PLARGE_STRING clsVersion  - Versioned class name (can be NULL)
 *   arg3:  PLARGE_STRING windowName  - Window title
 *   arg4:  DWORD dwStyle             - Style
 *   arg5:  int x                     - X position
 *   arg6:  int y                     - Y position
 *   arg7:  int nWidth                - Width
 *   arg8:  int nHeight               - Height
 *   arg9:  HWND hWndParent           - Parent window
 *   arg10: HMENU hMenu               - Menu handle
 *   arg11: HINSTANCE hInstance       - Instance
 *   arg12: LPVOID lpParam            - Creation param
 *   arg13: DWORD dwFlags             - Internal flags
 *   arg14: PVOID acbiBuffer          - Activation context
 *
 * Returns: HWND (in EAX)
 */
ntstatus_t sys_NtUserCreateWindowEx(void)
{
    uint32_t dwExStyle    = read_stack_arg(0);
    uint32_t pClassName   = read_stack_arg(1);   /* PLARGE_STRING */
    uint32_t pClsVersion  = read_stack_arg(2);   /* PLARGE_STRING (unused) */
    uint32_t pWindowName  = read_stack_arg(3);   /* PLARGE_STRING */
    uint32_t dwStyle      = read_stack_arg(4);
    int32_t  x            = (int32_t)read_stack_arg(5);
    int32_t  y            = (int32_t)read_stack_arg(6);
    int32_t  nWidth       = (int32_t)read_stack_arg(7);
    int32_t  nHeight      = (int32_t)read_stack_arg(8);
    uint32_t hWndParent   = read_stack_arg(9);
    uint32_t hMenu        = read_stack_arg(10);
    uint32_t hInstance    = read_stack_arg(11);
    uint32_t lpParam      = read_stack_arg(12);
    uint32_t dwFlags      = read_stack_arg(13);
    uint32_t acbiBuffer   = read_stack_arg(14);

    (void)pClsVersion;
    (void)dwFlags;
    (void)acbiBuffer;

    /* Ensure USER initialized */
    if (user_ensure_init() < 0) {
        EAX = 0;
        return STATUS_SUCCESS;
    }

    /* Initialize window subsystem if needed */
    static bool window_init_done = false;
    if (!window_init_done) {
        if (user_window_init() < 0) {
            fprintf(stderr, "USER: Failed to initialize window subsystem\n");
            EAX = 0;
            return STATUS_SUCCESS;
        }
        window_init_done = true;
    }

    /* Read class name - may be an atom or a LARGE_STRING pointer */
    wchar_t className[MAX_CLASSNAME];
    uint16_t classAtom = read_guest_large_string(pClassName, className, MAX_CLASSNAME);

    /* Read window name */
    wchar_t windowName[256];
    read_guest_large_string(pWindowName, windowName, 256);

    /* Find class - by atom or by name */
    WBOX_CLS *cls = NULL;
    if (classAtom != 0) {
        /* Class specified by atom */
        printf("USER: NtUserCreateWindowEx(classAtom=0x%04X, title='%ls', style=0x%08X, exStyle=0x%08X)\n",
               classAtom, windowName, dwStyle, dwExStyle);
        cls = user_class_find_by_atom(classAtom);
    } else {
        /* Class specified by name */
        printf("USER: NtUserCreateWindowEx(class='%ls', title='%ls', style=0x%08X, exStyle=0x%08X)\n",
               className, windowName, dwStyle, dwExStyle);
        cls = user_class_find(className, hInstance);
        if (!cls) {
            /* Try with NULL instance (global class) */
            cls = user_class_find(className, 0);
        }
    }
    printf("      pos=(%d,%d) size=(%d,%d) parent=0x%X menu=0x%X\n",
           x, y, nWidth, nHeight, hWndParent, hMenu);

    if (!cls) {
        fprintf(stderr, "USER: CreateWindowEx - class '%ls' (atom=0x%04X) not found\n",
                className, classAtom);
        EAX = 0;
        return STATUS_SUCCESS;
    }

    /* Get parent window */
    WBOX_WND *parent = NULL;
    if (hWndParent != 0) {
        parent = user_window_from_hwnd(hWndParent);
    }

    /* Create window */
    WBOX_WND *wnd = user_window_create(
        cls,
        windowName,
        dwStyle,
        dwExStyle,
        x, y, nWidth, nHeight,
        parent,
        NULL,   /* owner */
        hInstance,
        hMenu,
        lpParam
    );

    if (!wnd) {
        fprintf(stderr, "USER: CreateWindowEx - failed to create window\n");
        EAX = 0;
        return STATUS_SUCCESS;
    }

    printf("USER: Created window hwnd=0x%08X, wndproc=0x%08X\n", wnd->hwnd, wnd->lpfnWndProc);

    /* Allocate CREATESTRUCT on the guest stack for WM_NCCREATE/WM_CREATE
     * CREATESTRUCTW layout:
     *   +0  lpCreateParams  (4 bytes)
     *   +4  hInstance       (4 bytes)
     *   +8  hMenu           (4 bytes)
     *   +12 hwndParent      (4 bytes)
     *   +16 cy (height)     (4 bytes)
     *   +20 cx (width)      (4 bytes)
     *   +24 y               (4 bytes)
     *   +28 x               (4 bytes)
     *   +32 style           (4 bytes)
     *   +36 lpszName        (4 bytes, pointer)
     *   +40 lpszClass       (4 bytes, pointer)
     *   +44 dwExStyle       (4 bytes)
     * Total: 48 bytes
     *
     * We also need space for the window name string after the struct.
     */
    vm_context_t *vm = vm_get_context();
    uint32_t saved_esp = ESP;

    /* Allocate space on stack: 48 (CREATESTRUCT) + 512 (name) + 256 (class) */
    #define CREATESTRUCT_SIZE 48
    #define NAME_BUF_SIZE 512
    #define CLASS_BUF_SIZE 256
    uint32_t total_alloc = CREATESTRUCT_SIZE + NAME_BUF_SIZE + CLASS_BUF_SIZE;
    ESP -= total_alloc;
    uint32_t createstruct_va = ESP;
    uint32_t name_buf_va = ESP + CREATESTRUCT_SIZE;
    uint32_t class_buf_va = ESP + CREATESTRUCT_SIZE + NAME_BUF_SIZE;

    /* Write window name to guest memory (wide chars) */
    size_t name_len = wcslen(windowName);
    if (name_len > 255) name_len = 255;
    for (size_t i = 0; i <= name_len; i++) {
        writememwl(name_buf_va + i * 2, (uint16_t)windowName[i]);
    }

    /* Write class name to guest memory (wide chars) */
    const wchar_t *classNameStr = cls->szClassName;
    size_t class_len = wcslen(classNameStr);
    if (class_len > 127) class_len = 127;
    for (size_t i = 0; i <= class_len; i++) {
        writememwl(class_buf_va + i * 2, (uint16_t)classNameStr[i]);
    }

    /* Fill CREATESTRUCT */
    writememll(createstruct_va + 0, lpParam);       /* lpCreateParams */
    writememll(createstruct_va + 4, hInstance);     /* hInstance */
    writememll(createstruct_va + 8, hMenu);         /* hMenu */
    writememll(createstruct_va + 12, hWndParent);   /* hwndParent */
    writememll(createstruct_va + 16, nHeight);      /* cy */
    writememll(createstruct_va + 20, nWidth);       /* cx */
    writememll(createstruct_va + 24, y);            /* y */
    writememll(createstruct_va + 28, x);            /* x */
    writememll(createstruct_va + 32, dwStyle);      /* style */
    writememll(createstruct_va + 36, name_buf_va);  /* lpszName */
    writememll(createstruct_va + 40, class_buf_va); /* lpszClass */
    writememll(createstruct_va + 44, dwExStyle);    /* dwExStyle */

    /* Send WM_NCCREATE message via callback with CREATESTRUCT pointer */
    uint32_t result = user_call_wndproc(vm, wnd, WM_NCCREATE, 0, createstruct_va);
    if (result == 0) {
        /* WM_NCCREATE returned FALSE - normally we'd destroy and fail
         * For debugging, continue anyway to see what happens next */
        fprintf(stderr, "USER: WM_NCCREATE returned FALSE (ignoring for debug)\n");
    }

    /* Send WM_CREATE message via callback with CREATESTRUCT pointer */
    result = user_call_wndproc(vm, wnd, WM_CREATE, 0, createstruct_va);
    if (result == (uint32_t)-1) {
        /* WM_CREATE returned -1 - destroy window and fail */
        fprintf(stderr, "USER: WM_CREATE returned -1, destroying window\n");
        ESP = saved_esp;  /* Restore stack */
        user_window_destroy(wnd);
        EAX = 0;
        return STATUS_SUCCESS;
    }

    ESP = saved_esp;  /* Restore stack */

    EAX = wnd->hwnd;
    return STATUS_SUCCESS;
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

/*
 * NtUserPeekMessage - peek at message queue
 * Syscall number: 479
 *
 * Parameters:
 *   arg0: PMSG pMsg            - Output message buffer
 *   arg1: HWND hwnd            - Window filter (0 = all)
 *   arg2: UINT msgFilterMin    - Message filter minimum
 *   arg3: UINT msgFilterMax    - Message filter maximum
 *   arg4: UINT removeFlags     - PM_NOREMOVE, PM_REMOVE
 *
 * Returns: BOOL (TRUE if message available)
 */
ntstatus_t sys_NtUserPeekMessage(void)
{
    uint32_t pMsg         = read_stack_arg(0);
    uint32_t hwnd         = read_stack_arg(1);
    uint32_t msgFilterMin = read_stack_arg(2);
    uint32_t msgFilterMax = read_stack_arg(3);
    uint32_t removeFlags  = read_stack_arg(4);

    vm_context_t *vm = vm_get_context();

    /* Poll SDL events first to generate new messages */
    if (vm && vm->gui_mode) {
        display_poll_events(&vm->display);

        /* Check if quit was requested via SDL */
        if (vm->display.quit_requested) {
            msg_queue_post_quit(0);
        }
    }

    /* Try to get a message */
    WBOX_MSG msg;
    bool found = msg_queue_peek(&msg, hwnd, msgFilterMin, msgFilterMax, removeFlags);

    if (found && pMsg != 0) {
        msg_write_to_guest(vm, pMsg, &msg);
    }

    EAX = found ? 1 : 0;
    return STATUS_SUCCESS;
}

/*
 * NtUserGetMessage - get message from queue (blocking)
 * Syscall number: 426
 *
 * Parameters:
 *   arg0: PMSG pMsg            - Output message buffer
 *   arg1: HWND hwnd            - Window filter (0 = all)
 *   arg2: UINT msgFilterMin    - Message filter minimum
 *   arg3: UINT msgFilterMax    - Message filter maximum
 *
 * Returns: BOOL (FALSE for WM_QUIT, TRUE otherwise, -1 on error)
 */
ntstatus_t sys_NtUserGetMessage(void)
{
    uint32_t pMsg         = read_stack_arg(0);
    uint32_t hwnd         = read_stack_arg(1);
    uint32_t msgFilterMin = read_stack_arg(2);
    uint32_t msgFilterMax = read_stack_arg(3);

    vm_context_t *vm = vm_get_context();

    /* Block until message available */
    WBOX_MSG msg;
    bool found = false;

    while (!found) {
        /* Poll SDL events */
        if (vm && vm->gui_mode) {
            display_poll_events(&vm->display);

            /* Check if quit was requested via SDL */
            if (vm->display.quit_requested) {
                msg_queue_post_quit(0);
            }
        }

        /* Check for message */
        found = msg_queue_peek(&msg, hwnd, msgFilterMin, msgFilterMax, PM_REMOVE);

        if (!found) {
            /* No message - yield briefly and try again */
            /* In a real implementation we'd use select() or similar */
            /* For now, just present the display and continue */
            if (vm && vm->gui_mode) {
                display_present(&vm->display);
            }

            /* Small delay to avoid busy-spinning */
            struct timespec ts = { 0, 10000000 };  /* 10ms */
            nanosleep(&ts, NULL);
        }
    }

    if (pMsg != 0) {
        msg_write_to_guest(vm, pMsg, &msg);
    }

    /* Return FALSE only for WM_QUIT */
    if (msg.message == WM_QUIT) {
        EAX = 0;
    } else {
        EAX = 1;
    }

    return STATUS_SUCCESS;
}

/*
 * NtUserTranslateMessage - translate virtual key messages to char messages
 * Syscall number: 571
 *
 * Parameters:
 *   arg0: const MSG *pMsg      - Message to translate
 *   arg1: UINT flags           - Flags (usually 0)
 *
 * Returns: BOOL (TRUE if translated)
 */
ntstatus_t sys_NtUserTranslateMessage(void)
{
    uint32_t pMsg  = read_stack_arg(0);
    uint32_t flags = read_stack_arg(1);
    (void)flags;

    vm_context_t *vm = vm_get_context();

    /* Read the message */
    WBOX_MSG msg;
    msg_read_from_guest(vm, pMsg, &msg);

    /* Only translate keyboard messages */
    if (msg.message != WM_KEYDOWN && msg.message != WM_SYSKEYDOWN) {
        EAX = 0;
        return STATUS_SUCCESS;
    }

    /* TODO: Proper keyboard translation using keyboard layout */
    /* For now, simple ASCII mapping for printable characters */
    uint32_t vk = msg.wParam;

    /* Simple translation: if key is printable ASCII, post WM_CHAR */
    char ch = 0;
    if (vk >= 'A' && vk <= 'Z') {
        /* Check shift state */
        bool shift = (g_msg_queue.keyState[0x10] & 0x80) != 0;  /* VK_SHIFT */
        ch = shift ? vk : (vk + 32);  /* Lowercase if not shifted */
    } else if (vk >= '0' && vk <= '9') {
        ch = (char)vk;
    } else if (vk == 0x20) {  /* VK_SPACE */
        ch = ' ';
    } else if (vk == 0x0D) {  /* VK_RETURN */
        ch = '\r';
    }

    if (ch != 0) {
        msg_queue_post(msg.hwnd, WM_CHAR, ch, msg.lParam);
        EAX = 1;
    } else {
        EAX = 0;
    }

    return STATUS_SUCCESS;
}

/*
 * NtUserDispatchMessage - dispatch message to window procedure
 * Syscall number: 362
 *
 * Parameters:
 *   arg0: const MSG *pMsg      - Message to dispatch
 *
 * Returns: LRESULT
 */
ntstatus_t sys_NtUserDispatchMessage(void)
{
    uint32_t pMsg = read_stack_arg(0);

    vm_context_t *vm = vm_get_context();

    /* Read the message */
    WBOX_MSG msg;
    msg_read_from_guest(vm, pMsg, &msg);

    /* Find the window */
    WBOX_WND *wnd = user_window_from_hwnd(msg.hwnd);
    if (!wnd) {
        /* No window - return 0 */
        EAX = 0;
        return STATUS_SUCCESS;
    }

    /* Get window procedure */
    uint32_t wndproc = wnd->lpfnWndProc;
    if (wndproc == 0 && wnd->pcls) {
        wndproc = wnd->pcls->lpfnWndProc;
    }

    if (wndproc == 0) {
        /* No window procedure */
        EAX = 0;
        return STATUS_SUCCESS;
    }

    /* Call the window procedure via callback mechanism */
    uint32_t result = user_call_wndproc_addr(vm, wndproc, msg.hwnd,
                                              msg.message, msg.wParam, msg.lParam);

    EAX = result;
    return STATUS_SUCCESS;
}

/*
 * NtUserPostMessage - post a message to a window
 * Syscall number: 497
 *
 * Parameters:
 *   arg0: HWND hwnd            - Target window
 *   arg1: UINT msg             - Message ID
 *   arg2: WPARAM wParam        - Word parameter
 *   arg3: LPARAM lParam        - Long parameter
 *
 * Returns: BOOL (TRUE on success)
 */
ntstatus_t sys_NtUserPostMessage(void)
{
    uint32_t hwnd   = read_stack_arg(0);
    uint32_t message = read_stack_arg(1);
    uint32_t wParam = read_stack_arg(2);
    uint32_t lParam = read_stack_arg(3);

    bool result = msg_queue_post(hwnd, message, wParam, lParam);
    EAX = result ? 1 : 0;
    return STATUS_SUCCESS;
}

/*
 * NtUserPostQuitMessage - post WM_QUIT
 * Syscall number: 498
 *
 * Parameters:
 *   arg0: int exitCode         - Exit code
 */
ntstatus_t sys_NtUserPostQuitMessage(void)
{
    int exitCode = (int)read_stack_arg(0);

    msg_queue_post_quit(exitCode);
    EAX = 0;
    return STATUS_SUCCESS;
}

/*
 * NtUserShowWindow - show or hide a window
 * Syscall number: 554
 *
 * Parameters:
 *   arg0: HWND hwnd            - Window handle
 *   arg1: int nCmdShow         - Show command (SW_*)
 *
 * Returns: BOOL (previous visibility state)
 */
ntstatus_t sys_NtUserShowWindow(void)
{
    uint32_t hwnd    = read_stack_arg(0);
    int32_t nCmdShow = (int32_t)read_stack_arg(1);

    WBOX_WND *wnd = user_window_from_hwnd(hwnd);
    if (!wnd) {
        EAX = 0;
        return STATUS_SUCCESS;
    }

    bool wasVisible = user_window_is_visible(wnd);

    /* Update visibility */
    user_window_show(wnd, nCmdShow);

    /* If becoming visible, mark for painting */
    if (!wasVisible && user_window_is_visible(wnd)) {
        /* Post WM_SHOWWINDOW */
        msg_queue_post(hwnd, WM_SHOWWINDOW, 1, 0);

        /* Post WM_SIZE */
        int width = wnd->rcClient.right - wnd->rcClient.left;
        int height = wnd->rcClient.bottom - wnd->rcClient.top;
        msg_queue_post(hwnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM(width, height));

        /* Mark for repaint */
        wnd->state |= WNDS_SENDNCPAINT | WNDS_SENDERASEBACKGROUND;
    }

    printf("USER: ShowWindow(hwnd=0x%X, cmd=%d) -> wasVisible=%d\n",
           hwnd, nCmdShow, wasVisible);

    EAX = wasVisible ? 1 : 0;
    return STATUS_SUCCESS;
}

/*
 * NtUserSetFocus - set keyboard focus
 * Syscall number: 527
 *
 * Parameters:
 *   arg0: HWND hwnd            - Window to receive focus
 *
 * Returns: HWND (previous focus window)
 */
ntstatus_t sys_NtUserSetFocus(void)
{
    uint32_t hwnd = read_stack_arg(0);

    uint32_t oldFocus = g_msg_queue.hwndFocus;

    if (oldFocus != hwnd) {
        /* Send WM_KILLFOCUS to old window */
        if (oldFocus != 0) {
            msg_queue_post(oldFocus, WM_KILLFOCUS, hwnd, 0);
        }

        /* Update focus */
        g_msg_queue.hwndFocus = hwnd;

        /* Send WM_SETFOCUS to new window */
        if (hwnd != 0) {
            msg_queue_post(hwnd, WM_SETFOCUS, oldFocus, 0);
        }
    }

    EAX = oldFocus;
    return STATUS_SUCCESS;
}

/*
 * NtUserGetForegroundWindow - get foreground window
 * Syscall number: 405
 *
 * Returns: HWND
 */
ntstatus_t sys_NtUserGetForegroundWindow(void)
{
    EAX = g_msg_queue.hwndActive;
    return STATUS_SUCCESS;
}

/*
 * NtUserSetActiveWindow - set active window
 * Syscall number: 508
 *
 * Parameters:
 *   arg0: HWND hwnd            - Window to activate
 *
 * Returns: HWND (previous active window)
 */
ntstatus_t sys_NtUserSetActiveWindow(void)
{
    uint32_t hwnd = read_stack_arg(0);

    uint32_t oldActive = g_msg_queue.hwndActive;

    if (oldActive != hwnd) {
        /* Deactivate old window */
        if (oldActive != 0) {
            msg_queue_post(oldActive, WM_ACTIVATE, WA_INACTIVE, hwnd);
        }

        /* Update active window */
        g_msg_queue.hwndActive = hwnd;

        /* Activate new window */
        if (hwnd != 0) {
            msg_queue_post(hwnd, WM_ACTIVATE, WA_ACTIVE, oldActive);
        }
    }

    EAX = oldActive;
    return STATUS_SUCCESS;
}

/*
 * NtUserGetKeyState - get key state
 * Syscall number: 411
 *
 * Parameters:
 *   arg0: int vKey             - Virtual key code
 *
 * Returns: SHORT (high bit = down, low bit = toggled)
 */
ntstatus_t sys_NtUserGetKeyState(void)
{
    int vKey = (int)read_stack_arg(0);

    uint8_t state = g_msg_queue.keyState[vKey & 0xFF];

    /* Return format: high bit set if key is down */
    uint16_t result = (state & 0x80) ? 0x8000 : 0;

    /* Low bit is toggle state (for caps lock, etc.) */
    result |= (state & 0x01);

    EAX = result;
    return STATUS_SUCCESS;
}

/*
 * NtUserGetAsyncKeyState - get async key state
 * Syscall number: 389
 *
 * Parameters:
 *   arg0: int vKey             - Virtual key code
 *
 * Returns: SHORT
 */
ntstatus_t sys_NtUserGetAsyncKeyState(void)
{
    /* For now, same as GetKeyState */
    return sys_NtUserGetKeyState();
}

/* DISPLAY_DEVICE state flags */
#define DISPLAY_DEVICE_ATTACHED_TO_DESKTOP  0x00000001
#define DISPLAY_DEVICE_MULTI_DRIVER         0x00000002
#define DISPLAY_DEVICE_PRIMARY_DEVICE       0x00000004
#define DISPLAY_DEVICE_MIRRORING_DRIVER     0x00000008
#define DISPLAY_DEVICE_VGA_COMPATIBLE       0x00000010
#define DISPLAY_DEVICE_REMOVABLE            0x00000020
#define DISPLAY_DEVICE_MODESPRUNED          0x08000000
#define DISPLAY_DEVICE_ACTIVE               0x00000001

/*
 * NtUserEnumDisplayDevices - enumerate display devices
 * Syscall number: 376
 *
 * Parameters (from ReactOS):
 *   arg0: PUNICODE_STRING lpDevice - device name to query (NULL for primary)
 *   arg1: DWORD iDevNum           - device index (0, 1, 2, ...)
 *   arg2: PDISPLAY_DEVICEW lpDD   - output structure pointer
 *   arg3: DWORD dwFlags           - flags
 *
 * Returns: BOOL (TRUE if device found, FALSE otherwise)
 */
ntstatus_t sys_NtUserEnumDisplayDevices(void)
{
    static int call_count = 0;
    call_count++;

    uint32_t lpDevice = read_stack_arg(0);  /* PUNICODE_STRING */
    uint32_t arg1_raw = read_stack_arg(1);  /* Could be pointer or value */
    uint32_t lpDD = read_stack_arg(2);      /* PDISPLAY_DEVICEW */
    uint32_t dwFlags = read_stack_arg(3);   /* Flags */

    (void)dwFlags;   /* Not used */

    /* ReactOS win32u passes iDevNum by pointer, not by value!
     * The user32.dll passes by value, but win32u wraps it in a pointer. */
    uint32_t iDevNum = 0;
    if (arg1_raw >= 0x10000 && arg1_raw < 0x80000000) {
        /* It's a pointer, dereference it */
        iDevNum = readmemll(arg1_raw);
    } else {
        /* It's a direct value */
        iDevNum = arg1_raw;
    }

    if (call_count <= 5) {
        fprintf(stderr, "EnumDisplayDevices[%d]: arg1_raw=0x%X => iDevNum=%u\n",
                call_count, arg1_raw, iDevNum);
    }

    /* Check if lpDevice UNICODE_STRING points to a device name (monitor enumeration)
     * vs NULL/empty (adapter enumeration) */
    bool isAdapterEnum = true;
    if (lpDevice >= 0x10000 && lpDevice < 0x80000000) {
        uint16_t len = readmemwl(lpDevice);
        uint32_t buf = readmemll(lpDevice + 4);
        if (len > 0 && buf >= 0x10000 && buf < 0x80000000) {
            wchar_t firstChar = readmemwl(buf);
            if (firstChar == L'\\') {
                isAdapterEnum = false;  /* Monitor enum: lpDevice="\\\\.\\DISPLAYn" */
            }
        }
    }

    /* Debug: show first 10 calls and any call where iDevNum > 0 */
    if (call_count <= 10 || iDevNum > 0) {
        fprintf(stderr, "EnumDisplayDevices[%d]: isAdapterEnum=%d iDevNum=%u lpDD=0x%X\n",
                call_count, isAdapterEnum, iDevNum, lpDD);
    }

    /* Only device index 0 exists for both adapters and monitors.
     * For iDevNum > 0, return FALSE (no more devices). */
    if (iDevNum > 0) {
        if (call_count <= 10 || iDevNum > 0) {
            fprintf(stderr, "EnumDisplayDevices[%d]: iDevNum=%u > 0, returning FALSE\n", call_count, iDevNum);
        }
        EAX = 0;  /* FALSE = no more devices */
        return STATUS_SUCCESS;
    }

    /* Validate output pointer - should be in user space */
    if (lpDD < 0x10000 || lpDD >= 0x80000000) {
        fprintf(stderr, "EnumDisplayDevices: Invalid lpDD pointer 0x%X\n", lpDD);
        EAX = 0;
        return STATUS_SUCCESS;
    }

    /* Read cb to verify structure size - caller must set this before calling */
    uint32_t cb = readmemll(lpDD);
    if (call_count <= 10) {
        fprintf(stderr, "EnumDisplayDevices[%d]: lpDD=0x%X cb=0x%X (expected 0x348=840)\n",
                call_count, lpDD, cb);
        /* Debug: dump first 8 bytes of DISPLAY_DEVICEW structure */
        fprintf(stderr, "  lpDD[0..7]: %02X %02X %02X %02X %02X %02X %02X %02X\n",
                readmembl(lpDD+0), readmembl(lpDD+1), readmembl(lpDD+2), readmembl(lpDD+3),
                readmembl(lpDD+4), readmembl(lpDD+5), readmembl(lpDD+6), readmembl(lpDD+7));
    }

    /* If cb is 0 or too small, this could be a different calling convention.
     * ReactOS may set cb after the call returns, so accept cb=0.
     * Just ensure we don't write past reasonable bounds. */
    if (cb != 0 && cb < 0x348) {
        fprintf(stderr, "EnumDisplayDevices: cb too small (%u), failing\n", cb);
        EAX = 0;
        return STATUS_SUCCESS;
    }

    /* Compute safe write limit - don't write past 0x08000000 */
    uint32_t maxOffset = (lpDD < 0x08000000) ? (0x08000000 - lpDD) : 0;

    /* Fill DISPLAY_DEVICEW structure (840 bytes = 0x348)
     * Only write essential fields within safe bounds.
     * Offsets:
     *   0x000: DWORD cb               (size of structure)
     *   0x004: WCHAR DeviceName[32]   (64 bytes)
     *   0x044: WCHAR DeviceString[128] (256 bytes)
     *   0x144: DWORD StateFlags
     *   0x148: WCHAR DeviceID[128]    (256 bytes)
     *   0x248: WCHAR DeviceKey[128]   (256 bytes)
     */

    /* Write cb = 840 */
    if (maxOffset >= 4) {
        writememll(lpDD + 0x000, 0x348);
        if (call_count <= 5) {
            /* Verify the write immediately */
            uint32_t verify = readmemll(lpDD);
            fprintf(stderr, "EnumDisplayDevices[%d]: wrote cb=0x348, readback=0x%X\n",
                    call_count, verify);
        }
    }

    /* DeviceName: "\\\\.\\DISPLAY1" */
    static const wchar_t devName[] = L"\\\\.\\DISPLAY1";
    for (int i = 0; i < 14 && devName[i]; i++) {
        if (0x004 + i*2 + 2 <= maxOffset) {
            writememwl(lpDD + 0x004 + i*2, devName[i]);
        }
    }
    if (0x004 + 14*2 + 2 <= maxOffset) {
        writememwl(lpDD + 0x004 + 14*2, 0);  /* null terminate */
    }

    /* DeviceString: "WBOX Display" */
    static const wchar_t devStr[] = L"WBOX Display";
    for (int i = 0; i < 12 && devStr[i]; i++) {
        if (0x044 + i*2 + 2 <= maxOffset) {
            writememwl(lpDD + 0x044 + i*2, devStr[i]);
        }
    }
    if (0x044 + 12*2 + 2 <= maxOffset) {
        writememwl(lpDD + 0x044 + 12*2, 0);  /* null terminate */
    }

    /* StateFlags: Primary device, attached to desktop, active */
    uint32_t stateFlags = DISPLAY_DEVICE_PRIMARY_DEVICE |
                          DISPLAY_DEVICE_ATTACHED_TO_DESKTOP |
                          DISPLAY_DEVICE_ACTIVE;
    if (0x144 + 4 <= maxOffset) {
        writememll(lpDD + 0x144, stateFlags);
    }

    /* DeviceID and DeviceKey - write empty strings if safe */
    if (0x148 + 2 <= maxOffset) {
        writememwl(lpDD + 0x148, 0);
    }
    if (0x248 + 2 <= maxOffset) {
        writememwl(lpDD + 0x248, 0);
    }

    if (call_count <= 10) {
        fprintf(stderr, "EnumDisplayDevices[%d]: Returning TRUE for %s device %u\n",
                call_count, isAdapterEnum ? "adapter" : "monitor", iDevNum);
    }

    EAX = 1;  /* TRUE = device found */
    return STATUS_SUCCESS;
}

/*
 * NtUserGetAncestor - get ancestor window
 * Syscall number: 386
 *
 * Parameters:
 *   arg0: HWND hwnd - Window handle
 *   arg1: UINT gaFlags - Type of ancestor (GA_PARENT, GA_ROOT, GA_ROOTOWNER)
 *
 * Returns: HWND of ancestor, or NULL
 */
#define GA_PARENT       1
#define GA_ROOT         2
#define GA_ROOTOWNER    3

ntstatus_t sys_NtUserGetAncestor(void)
{
    uint32_t hwnd = read_stack_arg(0);
    uint32_t gaFlags = read_stack_arg(1);

    if (hwnd == 0) {
        EAX = 0;
        return STATUS_SUCCESS;
    }

    WBOX_WND *wnd = user_window_from_hwnd(hwnd);
    if (!wnd) {
        EAX = 0;
        return STATUS_SUCCESS;
    }

    switch (gaFlags) {
        case GA_PARENT:
            /* Return parent window */
            EAX = wnd->spwndParent ? wnd->spwndParent->hwnd : 0;
            break;

        case GA_ROOT:
            /* Walk up to top-level window (no parent or desktop parent) */
            {
                WBOX_WND *current = wnd;
                while (current->spwndParent != NULL) {
                    current = current->spwndParent;
                }
                EAX = current->hwnd;
            }
            break;

        case GA_ROOTOWNER:
            /* Walk up to owned root window */
            {
                WBOX_WND *current = wnd;
                while (current->spwndParent != NULL || current->spwndOwner != NULL) {
                    WBOX_WND *next = current->spwndOwner ? current->spwndOwner : current->spwndParent;
                    if (!next) break;
                    current = next;
                }
                EAX = current->hwnd;
            }
            break;

        default:
            EAX = 0;
            break;
    }

    return STATUS_SUCCESS;
}

/*
 * NtUserFindWindowEx - find a window by class/title
 * Syscall number: 383
 *
 * Parameters:
 *   arg0: HWND hwndParent - Parent window (NULL = desktop children)
 *   arg1: HWND hwndChildAfter - Child to start search after
 *   arg2: PUNICODE_STRING pucClassName - Class name (or atom)
 *   arg3: PUNICODE_STRING pucWindowName - Window title
 *   arg4: DWORD dwType - Search type
 *
 * Returns: HWND of matching window, or NULL
 */
ntstatus_t sys_NtUserFindWindowEx(void)
{
    uint32_t hwndParent = read_stack_arg(0);
    uint32_t hwndChildAfter = read_stack_arg(1);
    uint32_t pucClassName = read_stack_arg(2);
    uint32_t pucWindowName = read_stack_arg(3);
    /* arg4 is dwType - currently ignored */

    wchar_t classNameBuf[256];
    wchar_t windowNameBuf[256];

    fprintf(stderr, "SYSCALL: NtUserFindWindowEx(parent=0x%X, after=0x%X, class=0x%X, name=0x%X)\n",
            hwndParent, hwndChildAfter, pucClassName, pucWindowName);

    /* 1. Resolve parent window */
    WBOX_WND *parent = NULL;
    if (hwndParent == 0) {
        parent = user_window_get_desktop();
    } else {
        parent = user_window_from_hwnd(hwndParent);
        if (!parent) {
            fprintf(stderr, "  -> Invalid parent hwnd\n");
            EAX = 0;
            return STATUS_SUCCESS;
        }
    }

    /* 2. Resolve child_after (if specified) */
    WBOX_WND *child_after = NULL;
    if (hwndChildAfter != 0) {
        child_after = user_window_from_hwnd(hwndChildAfter);
        /* If child_after is invalid, we start from the beginning */
    }

    /* 3. Parse class name (UNICODE_STRING or atom) */
    uint16_t class_atom = 0;
    if (pucClassName != 0) {
        uint16_t atom_result = read_guest_unicode_string(pucClassName, classNameBuf, 256);
        if (atom_result != 0) {
            /* It's an atom */
            class_atom = atom_result;
            fprintf(stderr, "  -> Class atom: 0x%04X\n", class_atom);
        } else if (classNameBuf[0] != 0) {
            /* It's a string - look up class by name to get atom */
            fprintf(stderr, "  -> Class name: '%ls'\n", classNameBuf);
            WBOX_CLS *cls = user_class_find(classNameBuf, 0);
            if (cls) {
                class_atom = cls->atomClassName;
                fprintf(stderr, "  -> Found class with atom: 0x%04X\n", class_atom);
            } else {
                /* Class not found - no windows can match */
                fprintf(stderr, "  -> Class not found, returning NULL\n");
                EAX = 0;
                return STATUS_SUCCESS;
            }
        }
    }

    /* 4. Parse window name (UNICODE_STRING) */
    const wchar_t *window_name = NULL;
    if (pucWindowName != 0) {
        read_guest_unicode_string(pucWindowName, windowNameBuf, 256);
        if (windowNameBuf[0] != 0) {
            window_name = windowNameBuf;
            fprintf(stderr, "  -> Window name: '%ls'\n", window_name);
        }
    }

    /* 5. Search for matching window */
    WBOX_WND *found = NULL;

    if (hwndParent == 0 || parent == user_window_get_desktop()) {
        /* Desktop search: search recursively through all windows */
        if (child_after != NULL) {
            /* Start from child_after's next sibling, then search recursively */
            found = user_window_find_child(parent, child_after, class_atom, window_name);
        } else {
            found = user_window_find_recursive(parent, class_atom, window_name);
        }
    } else {
        /* Non-desktop: search only direct children */
        found = user_window_find_child(parent, child_after, class_atom, window_name);
    }

    /* 6. Return result */
    if (found) {
        fprintf(stderr, "  -> Found: hwnd=0x%X\n", found->hwnd);
        EAX = found->hwnd;
    } else {
        fprintf(stderr, "  -> Not found\n");
        EAX = 0;
    }
    return STATUS_SUCCESS;
}

/*
 * NtUserQuerySendMessage - query pending sent message
 * Syscall number: 486
 *
 * Returns info about messages sent to this thread that are awaiting reply.
 * For now, just return 0 (no pending sent messages).
 */
ntstatus_t sys_NtUserQuerySendMessage(void)
{
    uint32_t pMsg = read_stack_arg(0);

    (void)pMsg;

    /* No pending sent messages */
    EAX = 0;
    return STATUS_SUCCESS;
}

/*
 * NtUserCountClipboardFormats - count available clipboard formats
 * Syscall number: 342
 *
 * Returns the number of different data formats available on the clipboard.
 */
ntstatus_t sys_NtUserCountClipboardFormats(void)
{
    /* No clipboard data */
    EAX = 0;
    return STATUS_SUCCESS;
}

/*
 * NtUserGetComboBoxInfo - get combobox information
 * Syscall number: 400
 *
 * Parameters:
 *   arg0: HWND hwndCombo
 *   arg1: PCOMBOBOXINFO pcbi
 *
 * Returns: BOOL success
 */
ntstatus_t sys_NtUserGetComboBoxInfo(void)
{
    uint32_t hwndCombo = read_stack_arg(0);
    uint32_t pcbi = read_stack_arg(1);

    (void)hwndCombo;
    (void)pcbi;

    /* Not implemented - return failure */
    EAX = 0;
    return STATUS_SUCCESS;
}

/*
 * NtUserCallHwndLock - misc window operations
 * Syscall number: 321
 *
 * Parameters:
 *   arg0: HWND hwnd
 *   arg1: DWORD routine index
 *
 * Returns: varies by routine
 */
ntstatus_t sys_NtUserCallHwndLock(void)
{
    uint32_t hwnd = read_stack_arg(0);
    uint32_t routine = read_stack_arg(1);

    (void)hwnd;
    (void)routine;

    /* Default: return success (1) */
    EAX = 1;
    return STATUS_SUCCESS;
}

/*
 * NtGdiGetTextMetricsW - get text metrics for DC
 * Syscall number: 206
 *
 * Parameters:
 *   arg0: HDC hdc
 *   arg1: LPTEXTMETRICW lptm
 *   arg2: ULONG cj (size)
 *
 * Returns: BOOL success
 */
ntstatus_t sys_NtGdiGetTextMetricsW(void)
{
    uint32_t hdc = read_stack_arg(0);
    uint32_t lptm = read_stack_arg(1);
    uint32_t cj = read_stack_arg(2);

    (void)hdc;
    (void)cj;

    /* Fill in minimal TEXTMETRICW structure */
    if (lptm >= 0x10000 && lptm < 0x80000000) {
        /*
         * TEXTMETRICW structure (60 bytes):
         *   0x00: LONG tmHeight
         *   0x04: LONG tmAscent
         *   0x08: LONG tmDescent
         *   ... more fields ...
         */
        writememll(lptm + 0x00, 16);  /* tmHeight = 16 */
        writememll(lptm + 0x04, 13);  /* tmAscent = 13 */
        writememll(lptm + 0x08, 3);   /* tmDescent = 3 */
        writememll(lptm + 0x0C, 0);   /* tmInternalLeading = 0 */
        writememll(lptm + 0x10, 3);   /* tmExternalLeading = 3 */
        writememll(lptm + 0x14, 7);   /* tmAveCharWidth = 7 */
        writememll(lptm + 0x18, 14);  /* tmMaxCharWidth = 14 */
        writememll(lptm + 0x1C, 400); /* tmWeight = 400 (normal) */
        writememll(lptm + 0x20, 0);   /* tmOverhang = 0 */
        writememll(lptm + 0x24, 96);  /* tmDigitizedAspectX = 96 */
        writememll(lptm + 0x28, 96);  /* tmDigitizedAspectY = 96 */
        writememwl(lptm + 0x2C, ' '); /* tmFirstChar */
        writememwl(lptm + 0x2E, 0xFF);/* tmLastChar */
        writememwl(lptm + 0x30, '?'); /* tmDefaultChar */
        writememwl(lptm + 0x32, ' '); /* tmBreakChar */
        writemembl(lptm + 0x34, 0);   /* tmItalic = 0 */
        writemembl(lptm + 0x35, 0);   /* tmUnderlined = 0 */
        writemembl(lptm + 0x36, 0);   /* tmStruckOut = 0 */
        writemembl(lptm + 0x37, 0);   /* tmPitchAndFamily */
        writemembl(lptm + 0x38, 1);   /* tmCharSet = DEFAULT_CHARSET */

        EAX = 1;  /* Success */
    } else {
        EAX = 0;  /* Failure - invalid pointer */
    }

    return STATUS_SUCCESS;
}

/*
 * NtUserShowWindowAsync - show window asynchronously
 * Syscall number: 558
 *
 * Parameters:
 *   arg0: HWND hwnd
 *   arg1: int nCmdShow
 *
 * Returns: BOOL previous visibility
 */
ntstatus_t sys_NtUserShowWindowAsync(void)
{
    uint32_t hwnd = read_stack_arg(0);
    uint32_t nCmdShow = read_stack_arg(1);

    (void)hwnd;
    (void)nCmdShow;

    /* Return TRUE (was visible before) */
    EAX = 1;
    return STATUS_SUCCESS;
}

/*
 * NtUserDeferWindowPos - defer window positioning
 * Syscall number: 353
 *
 * Parameters:
 *   arg0: HDWP hWinPosInfo
 *   arg1: HWND hwnd
 *   arg2: HWND hwndInsertAfter
 *   arg3: int x
 *   arg4: int y
 *   arg5: int cx
 *   arg6: int cy
 *   arg7: UINT uFlags
 *
 * Returns: HDWP (handle to defer structure)
 */
ntstatus_t sys_NtUserDeferWindowPos(void)
{
    uint32_t hWinPosInfo = read_stack_arg(0);

    /* Return the same handle (no actual deferral) */
    EAX = hWinPosInfo ? hWinPosInfo : 1;
    return STATUS_SUCCESS;
}

/*
 * NtUserGetWOWClass - get WOW16 window class info
 * Syscall number: 446
 *
 * Parameters:
 *   arg0: HINSTANCE hInstance
 *   arg1: PUNICODE_STRING pClassName
 *
 * Returns: PCLS (class pointer) or NULL
 */
ntstatus_t sys_NtUserGetWOWClass(void)
{
    /* Return NULL - no WOW16 class support */
    EAX = 0;
    return STATUS_SUCCESS;
}

/* Additional FNID value for NtUserMessageCall (not in user_class.h) */
#define FNID_SENDMESSAGE            0x02B1

/*
 * NtUserDefSetText - set window text (internal kernel function)
 * Syscall number: 348 (0x115C)
 *
 * Parameters:
 *   arg0: HWND hwnd            - Window handle
 *   arg1: PLARGE_STRING Text   - Window text (LARGE_STRING pointer)
 *
 * Returns: BOOL (TRUE on success)
 */
ntstatus_t sys_NtUserDefSetText(void)
{
    uint32_t hwnd = read_stack_arg(0);
    uint32_t pText = read_stack_arg(1);

    WBOX_WND *wnd = user_window_from_hwnd(hwnd);
    if (!wnd) {
        EAX = 0;
        return STATUS_SUCCESS;
    }

    /* Free existing title */
    if (wnd->strName) {
        free(wnd->strName);
        wnd->strName = NULL;
    }

    /* Read text if provided */
    if (pText != 0) {
        wchar_t textBuf[256];
        read_guest_large_string(pText, textBuf, 256);

        /* Allocate and copy window text */
        size_t len = wcslen(textBuf);
        wnd->strName = (wchar_t *)malloc((len + 1) * sizeof(wchar_t));
        if (wnd->strName) {
            wcscpy(wnd->strName, textBuf);
        }

        fprintf(stderr, "USER: DefSetText(hwnd=0x%X, text='%ls')\n", hwnd, textBuf);
    } else {
        fprintf(stderr, "USER: DefSetText(hwnd=0x%X, text=NULL)\n", hwnd);
    }

    EAX = 1;  /* TRUE */
    return STATUS_SUCCESS;
}

/*
 * Default window procedure - kernel side message handling
 */
static uint32_t def_wndproc_kernel(uint32_t hwnd, uint32_t msg, uint32_t wParam, uint32_t lParam)
{
    WBOX_WND *wnd = user_window_from_hwnd(hwnd);

    switch (msg) {
        case WM_NCCREATE:
            /* Default handling: return TRUE to allow window creation */
            return 1;

        case WM_CREATE:
            /* Default handling: return 0 to continue creation */
            return 0;

        case WM_NCDESTROY:
        case WM_DESTROY:
            /* Default handling: return 0 */
            return 0;

        case WM_NCCALCSIZE:
            /* Default: do nothing, return 0 */
            return 0;

        case WM_GETMINMAXINFO:
            /* Default: let the caller handle it */
            return 0;

        case WM_ERASEBKGND:
            /* Default: don't erase (return 0) */
            return 0;

        case WM_PAINT:
            /* Default: just validate the region */
            return 0;

        case WM_CLOSE:
            /* Default: destroy the window */
            if (wnd) {
                msg_queue_post(hwnd, WM_DESTROY, 0, 0);
            }
            return 0;

        case WM_GETTEXT:
            /* Default: return 0 (no text) */
            return 0;

        case WM_GETTEXTLENGTH:
            /* Default: return 0 */
            return 0;

        case WM_SETTEXT:
            /* Default: accept text, return TRUE */
            return 1;

        case WM_SETCURSOR:
            /* Default: let child set cursor */
            return 0;

        case WM_MOUSEACTIVATE:
            /* Default: activate window */
            return 1;  /* MA_ACTIVATE */

        case WM_WINDOWPOSCHANGING:
        case WM_WINDOWPOSCHANGED:
            /* Default: do nothing */
            return 0;

        case WM_SHOWWINDOW:
            /* Default: do nothing */
            return 0;

        case WM_ACTIVATE:
        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            /* Default: do nothing */
            return 0;

        case WM_NCPAINT:
            /* Default: paint non-client area (we skip this for now) */
            return 0;

        case WM_NCACTIVATE:
            /* Default: allow activation change, return TRUE */
            return 1;

        case WM_NCHITTEST:
            /* Default: return HTCLIENT */
            return 1;  /* HTCLIENT */

        case WM_QUERYOPEN:
            /* Default: allow open */
            return 1;

        case WM_SYSCOMMAND:
            /* Default: do nothing */
            return 0;

        default:
            /* Default: return 0 */
            return 0;
    }
}

/*
 * NtUserMessageCall - message passing and default window procedure
 * Syscall number: 459
 *
 * Parameters:
 *   arg0: HWND hWnd
 *   arg1: UINT Msg
 *   arg2: WPARAM wParam
 *   arg3: LPARAM lParam
 *   arg4: ULONG_PTR ResultInfo (output pointer for result)
 *   arg5: DWORD dwType (FNID_*)
 *   arg6: BOOL Ansi
 *
 * Returns: BOOL (TRUE on success)
 */
ntstatus_t sys_NtUserMessageCall(void)
{
    uint32_t hwnd       = read_stack_arg(0);
    uint32_t msg        = read_stack_arg(1);
    uint32_t wParam     = read_stack_arg(2);
    uint32_t lParam     = read_stack_arg(3);
    uint32_t resultInfo = read_stack_arg(4);
    uint32_t dwType     = read_stack_arg(5);
    uint32_t ansi       = read_stack_arg(6);

    (void)ansi;

    uint32_t result = 0;
    bool handled = false;

    switch (dwType) {
        case FNID_DEFWINDOWPROC:
            /* Handle default window procedure */
            result = def_wndproc_kernel(hwnd, msg, wParam, lParam);
            handled = true;
            break;

        case FNID_SENDMESSAGE:
            /* SendMessage - call the window procedure directly */
            {
                WBOX_WND *wnd = user_window_from_hwnd(hwnd);
                if (wnd) {
                    vm_context_t *vm = vm_get_context();
                    result = user_call_wndproc(vm, wnd, msg, wParam, lParam);
                    handled = true;
                }
            }
            break;

        case FNID_SCROLLBAR:
        case FNID_BUTTON:
        case FNID_EDIT:
        case FNID_LISTBOX:
        case FNID_COMBOBOX:
        case FNID_STATIC:
            /* Control window procedures - just call DefWindowProc for now */
            result = def_wndproc_kernel(hwnd, msg, wParam, lParam);
            handled = true;
            break;

        default:
            /* Unknown type - try to handle as DefWindowProc */
            fprintf(stderr, "NtUserMessageCall: Unknown dwType 0x%X (hwnd=0x%X, msg=0x%X)\n",
                    dwType, hwnd, msg);
            result = def_wndproc_kernel(hwnd, msg, wParam, lParam);
            handled = true;
            break;
    }

    /* Write result to output pointer */
    if (resultInfo != 0) {
        writememll(resultInfo, result);
    }

    EAX = handled ? 1 : 0;
    return STATUS_SUCCESS;
}

/*
 * NtUserOpenWindowStation - open an existing window station
 * Syscall number: 477
 *
 * Parameters:
 *   arg0: POBJECT_ATTRIBUTES ObjectAttributes
 *   arg1: ACCESS_MASK dwDesiredAccess
 *
 * Returns: HWINSTA (window station handle) or NULL on failure
 */
ntstatus_t sys_NtUserOpenWindowStation(void)
{
    uint32_t objAttr = read_stack_arg(0);
    uint32_t accessMask = read_stack_arg(1);

    (void)objAttr;
    (void)accessMask;

    /* Return a fake window station handle.
     * Windows uses handles in the 0x30-0x100 range typically.
     * We just need a non-zero value that identifies this as valid. */
    static uint32_t g_winsta_handle = 0x50;  /* Fake WinSta0 handle */

    fprintf(stderr, "NtUserOpenWindowStation: returning handle 0x%X\n", g_winsta_handle);

    EAX = g_winsta_handle;
    return STATUS_SUCCESS;
}

/*
 * NtUserOpenDesktop - open an existing desktop
 * Syscall number: 475
 *
 * Parameters:
 *   arg0: POBJECT_ATTRIBUTES ObjectAttributes
 *   arg1: DWORD dwFlags
 *   arg2: ACCESS_MASK dwDesiredAccess
 *
 * Returns: HDESK (desktop handle) or NULL on failure
 */
ntstatus_t sys_NtUserOpenDesktop(void)
{
    uint32_t objAttr = read_stack_arg(0);
    uint32_t flags = read_stack_arg(1);
    uint32_t accessMask = read_stack_arg(2);

    (void)objAttr;
    (void)flags;
    (void)accessMask;

    /* Return a fake desktop handle */
    static uint32_t g_desktop_handle = 0x60;  /* Fake Default desktop handle */

    fprintf(stderr, "NtUserOpenDesktop: returning handle 0x%X\n", g_desktop_handle);

    EAX = g_desktop_handle;
    return STATUS_SUCCESS;
}

/*
 * NtUserOpenInputDesktop - open the input desktop
 * Syscall number: 476
 *
 * Parameters:
 *   arg0: DWORD dwFlags
 *   arg1: BOOL fInherit
 *   arg2: ACCESS_MASK dwDesiredAccess
 *
 * Returns: HDESK (desktop handle) or NULL on failure
 */
ntstatus_t sys_NtUserOpenInputDesktop(void)
{
    uint32_t flags = read_stack_arg(0);
    uint32_t inherit = read_stack_arg(1);
    uint32_t accessMask = read_stack_arg(2);

    (void)flags;
    (void)inherit;
    (void)accessMask;

    /* Return a fake input desktop handle (same as default desktop) */
    static uint32_t g_input_desktop_handle = 0x60;

    fprintf(stderr, "NtUserOpenInputDesktop: returning handle 0x%X\n", g_input_desktop_handle);

    EAX = g_input_desktop_handle;
    return STATUS_SUCCESS;
}
